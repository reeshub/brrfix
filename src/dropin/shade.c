/* Smooth shading (2026-11-15, the director's request: the game's gradients, fixed at their
 * source; [display] smooth_shading, with full_colour), and [debug] span_census.
 *
 * Every polygon is filled by a span routine picked through a fill descriptor (16 bytes: mode,
 * lighting set-up, edge routine 0x442CE0, span routine): the table fill_descriptors
 * (0x5430E0, 40 descriptors) and 16 more inside engine_draw_polygon's code (polygon types
 * 3-12). The census swaps each descriptor's span routine for a stub that counts the call and
 * jumps on, and logs the counts every 10 seconds. In races nearly every shaded polygon is a
 * flat-coloured one lit per corner: poly_light_smooth (0x446D70) gives each corner the shade
 * table's palette index for its light level (0-255), and span_gouraud_z (0x4438D0; or
 * span_gouraud 0x443710 without depth) interpolates that index across the polygon in 16.16
 * and writes its whole part. Each colour's shades are consecutive palette entries, one for
 * every 28-37 light levels, so the game's gradients step a whole shade at a time; everything
 * else in a race is flat colour or a texture lit once per polygon.
 *
 * So, with full colour, sites fill_flat_smooth, fill_seethrough_smooth and fill_gouraud point
 * those descriptors at copies in thunks.asm that do exactly what the originals do, plus:
 * the spans store each drawn pixel's index and the fraction the original drops (a word, in
 * a side buffer laid out like the game's picture), and the lighting gives each corner a
 * fraction too, from where its light level falls within its shade's run of levels (the
 * table below). When the frame is shown (render.c), a pixel whose side word still matches
 * it is blended between its palette shade and the next. */
#include "brrfix.h"
#include <string.h>

#define VIEW_BUFFER    (*(uint8_t *volatile *)0x44BE68)
#define VIEW_PICTURE   (*(uint8_t *volatile *)0x542030)
#define VIEW_WIDTH     (*(volatile int32_t *)0x542014)
#define VIEW_HEIGHT    (*(volatile int32_t *)0x542024)
#define VIEW_STRIDE    (*(volatile int32_t *)0x542018)
#define SHADE_TABLE    (*(const uint8_t *volatile *)0x54A35C)  /* [level x 256 + colour] */
#define SIDE_PIXELS    ((uint32_t)RENDER_MAX_W * RENDER_MAX_H)

#define FILL_DESCRIPTORS 0x5430E0u
#define FILL_COUNT       40
#define SLOTS            (FILL_COUNT + 16)

static const uint32_t inline_spans[16] = {
    0x44211B, 0x44212B, 0x44223B, 0x44224B, 0x44235B, 0x44236B, 0x44247A, 0x44248A,
    0x44259D, 0x4425AD, 0x4426CA, 0x4426DA, 0x4427DA, 0x4427EA, 0x4428FB, 0x44290B,
};

static const struct { uint32_t address; const char *name; } span_names[] = {
    {0x443590, "flat"}, {0x443640, "flat_z"}, {0x443710, "gouraud"}, {0x4437C0, "gouraud_lit"},
    {0x4438D0, "gouraud_z"}, {0x4439C0, "texture"}, {0x443AB0, "texture_z"},
    {0x443C00, "texture_lit"}, {0x443D10, "texture_lit_z"}, {0x443E70, "texture_gouraud"},
    {0x443FC0, "texture_gouraud_z"}, {0x444220, "gouraud_lit_dither"},
    {0x444350, "gouraud_stipple"}, {0x444430, "gouraud_stipple_z"}, {0x4446C0, "persp_z"},
    {0x444870, "persp"}, {0x4449E0, "persp_lit_z"}, {0x444B70, "persp_lit"},
    {0x444CE0, "persp_gouraud_z"}, {0x444EA0, "persp_gouraud"},
    {0x445470, "persp_gouraud_opaque"}, {0x445620, "gouraud_remap"}, {0x4456E0, "table_2d"},
    {0x445800, "translucent"}, {0x4458B0, "translucent_gouraud"},
    {0x445970, "texture_gouraud_opaque"}, {0x445AB0, "gouraud_lit_blend"},
};
#define NAMES (sizeof span_names / sizeof span_names[0])

static uint32_t *slot_field[SLOTS];      /* where each descriptor holds its span routine */
static uint32_t slot_original[SLOTS];
static volatile uint32_t slot_calls[SLOTS];
static uint32_t slot_reported[SLOTS];
static int slots;
static DWORD census_since;

static const char *span_name(uint32_t address)
{
    unsigned i;
    for (i = 0; i < NAMES; i++)
        if (span_names[i].address == address)
            return span_names[i].name;
    return NULL;
}

void brr_shade_census_setup(void)
{
    uint8_t *stubs;
    int i;
    if (!cfg.debug_span_census)
        return;
    stubs = (uint8_t *)VirtualAlloc(NULL, SLOTS * 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stubs)
        return;
    for (i = 0; i < SLOTS; i++) {
        uint32_t *field = i < FILL_COUNT ? (uint32_t *)(FILL_DESCRIPTORS + 16 * i + 12)
                                         : (uint32_t *)inline_spans[i - FILL_COUNT];
        uint8_t *stub = stubs + 16 * slots;
        DWORD old;
        if (!span_name(*field))
            continue;
        slot_field[slots] = field;
        slot_original[slots] = *field;
        stub[0] = 0xFF;  /* inc dword ptr [calls] */
        stub[1] = 0x05;
        *(uint32_t *)(stub + 2) = (uint32_t)&slot_calls[slots];
        stub[6] = 0xFF;  /* jmp dword ptr [original] */
        stub[7] = 0x25;
        *(uint32_t *)(stub + 8) = (uint32_t)&slot_original[slots];
        VirtualProtect(field, 4, PAGE_EXECUTE_READWRITE, &old);
        *field = (uint32_t)stub;
        VirtualProtect(field, 4, old, &old);
        slots++;
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    census_since = GetTickCount();
    brr_log("span census: %d descriptors counted", slots);
}

/* once a frame: every 10 seconds, the calls per span routine (and per descriptor) since the last report */
void brr_shade_census_report(void)
{
    char line[600];
    int i, n;
    unsigned k;
    if (!slots || GetTickCount() - census_since < 10000)
        return;
    census_since = GetTickCount();
    for (k = 0; k < NAMES; k++) {
        uint32_t total = 0;
        n = 0;
        line[0] = 0;
        for (i = 0; i < slots; i++) {
            uint32_t calls = slot_calls[i] - slot_reported[i];
            if (slot_original[i] != span_names[k].address || !calls)
                continue;
            total += calls;
            if (n < 520)
                n += wsprintfA(line + n, " %08lX:%lu", (unsigned long)(uint32_t)slot_field[i], (unsigned long)calls);
        }
        if (total)
            brr_log("span census: %-24s %8lu calls;%s", span_names[k].name, (unsigned long)total, line);
    }
    for (i = 0; i < slots; i++)
        slot_reported[i] = slot_calls[i];
    brr_log("span census: (screen 0x%02X)", (unsigned)*(volatile uint8_t *)0x4B13C0);
}

/* ---- smooth shading ---- */

int brr_shade_active;
uint16_t *brr_shade_side;            /* per picture pixel: the fraction (low byte), the index (high byte) */
uint32_t brr_shade_lo, brr_shade_hi; /* the picture's memory the side buffer covers (thunks.asm) */
uint32_t brr_shade_side_off;         /* side word of the pixel at address A: side_off + 2 A */
uint32_t brr_shade_delta;            /* the same from span_gouraud_z's depth pointer */
int16_t *brr_shade_vertex_delta;     /* [level x 256 + colour]: the corner's fraction, 16.16 */
uint32_t brr_light_level_fn = 0x4468E0;  /* light_level, called by brr_poly_light_smooth */

static const uint8_t *built_table;
static uint32_t built_sum;

/* The corner fractions for a shade table: for colour c, the levels map to runs of one index
 * each (40 for levels 0-28, 39 for 29-56 ...). A level in a run is placed between the run's
 * index (at its middle) and the neighbouring run's (at the run's ends), so the shade moves
 * smoothly with the light; an end of the ramp, or a jump of more than one index, gets none. */
static void build_vertex_deltas(const uint8_t *table)
{
    int c;
    for (c = 0; c < 256; c++) {
        int start = 0;
        while (start < 256) {
            int v = table[start * 256 + c], end = start, n, o, before, after;
            while (end < 256 && table[end * 256 + c] == v)
                end++;
            n = end - start;
            before = start > 0 ? table[(start - 1) * 256 + c] - v : 0;
            after = end < 256 ? table[end * 256 + c] - v : 0;
            for (o = 0; o < n; o++) {
                int32_t twice = 2 * o + 1 - n, d = 0;  /* (position - middle) x 2, in levels */
                if (twice < 0 && (before == 1 || before == -1))
                    d = (int32_t)(-twice * 32768L / n) * before;
                else if (twice > 0 && (after == 1 || after == -1))
                    d = (int32_t)(twice * 32768L / n) * after;
                if (d > 32767) d = 32767;
                if (d < -32767) d = -32767;
                brr_shade_vertex_delta[(start + o) * 256 + c] = (int16_t)d;
            }
            start = end;
        }
    }
}

void brr_shade_setup(void)
{
    if (!brr_shade_active)
        return;
    brr_shade_side = (uint16_t *)VirtualAlloc(NULL, SIDE_PIXELS * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    brr_shade_vertex_delta = (int16_t *)VirtualAlloc(NULL, 65536 * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!brr_shade_side || !brr_shade_vertex_delta) {
        brr_log("shading: out of memory; the game's own shading");
        brr_shade_active = 0;
    }
}

/* Called as each 3D view starts (render_view's sky_draw call): the side buffer follows the
 * game's picture, and the view's part of it is cleared; the corner fractions follow the
 * shade table. */
void brr_shade_view_begin(void)
{
    static int logged;
    uint8_t *buffer = VIEW_BUFFER, *picture = VIEW_PICTURE;
    const uint8_t *table = SHADE_TABLE;
    int32_t width = VIEW_WIDTH, height = VIEW_HEIGHT, stride = VIEW_STRIDE, row;
    uint32_t offset, sum = 0;
    int i;
    if (!brr_shade_active)
        return;
    if (!buffer || picture < buffer || width <= 0 || height <= 0 || stride < width
        || (uint32_t)(picture - buffer) + (uint32_t)(height - 1) * (uint32_t)stride + (uint32_t)width > SIDE_PIXELS) {
        brr_shade_lo = brr_shade_hi = 0;  /* the copies leave this view to the original spans */
        return;
    }
    brr_shade_lo = (uint32_t)buffer;
    brr_shade_hi = (uint32_t)buffer + SIDE_PIXELS;
    brr_shade_side_off = (uint32_t)brr_shade_side - 2 * (uint32_t)buffer;
    offset = (uint32_t)(picture - buffer);
    for (row = 0; row < height; row++)
        memset(brr_shade_side + offset + (uint32_t)row * (uint32_t)stride, 0, (size_t)width * 2);
    if (table) {
        for (i = 0; i < 16384; i++)
            sum += ((const uint32_t *)table)[i] * (uint32_t)(i | 1);
        if ((table != built_table || sum != built_sum) && !cfg.debug_shade_no_corners) {
            build_vertex_deltas(table);
            built_table = table;
            built_sum = sum;
            if (logged++ < 4)
                brr_log("shading: corner fractions worked out from the shade table at %p", (const void *)table);
        }
    }
}

/* The side words of the picture's row that starts at the given pixel, or NULL (render.c) */
const uint16_t *brr_shade_row(const uint8_t *pixel, int count)
{
    uint32_t a = (uint32_t)pixel;
    if (!brr_shade_active || !brr_shade_lo || a < brr_shade_lo || a + (uint32_t)count > brr_shade_hi)
        return NULL;
    return brr_shade_side + (a - brr_shade_lo);
}
