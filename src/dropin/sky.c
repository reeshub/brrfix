/* The smooth sky (2026-11-15, the director's request; [display] smooth_sky, with full_colour).
 *
 * The game draws the sky as flat bands of 16 palette colours (239 down to 224) and the haze
 * below the horizon in two (222, 223): sky_draw (0x41A4D0), called once per view by
 * render_view (0x4176C4, at 0x4176F6) with two (colour, angle) tables, sky_bands 0x44B86C
 * (239 from 7,000 angle units above the horizon, ... 224 from 0) and ground_bands 0x44B8AC
 * (222 to 1,300 below, 223 to the horizon); the first band reaches 13,000. Each band edge at
 * angle A is a line in the engine's view space: y = -D - 32767 sin(A + pitch), z = 32767
 * cos(A + pitch), turned by the camera's roll, where D is the camera's height (0x54A2D8, set
 * by view_setup) and pitch and roll are camera_angles 0x45131C and 0x451320; projected as
 * the engine does (x * proj_scale / z + view centre). Checked on 3,718 band edges in 200
 * captured race frames: the drawn edges fall a median 0.33 rows from that model.
 *
 * So, with the new renderer presenting in full colour (render.c), site sky_draw_call sends
 * the call here: the view's camera and projection are recorded, and the game draws its bands
 * all in one marker colour (palette index 255, which the game never uses: Windows' own
 * colours, 246-255, aren't in its palettes), as the scenery reuses the sky's colours. When
 * the frame is presented, every marker pixel gets the sky's colour for its own direction:
 * the elevation is worked out from the model above, at the final picture's resolution, and
 * the colour is blended between the bands' colours (the live palette's, at each band's
 * middle), so the sky is a smooth gradient however much it is enlarged. A PrtSc screenshot
 * (drawn again by the game for its PCX file) keeps the game's own bands. */
#include "brrfix.h"
#include <stddef.h>
#include <string.h>

#define DEFAULT_SKY    ((int16_t *)0x44B980)
#define DEFAULT_GROUND ((int16_t *)0x44B9A8)
#define CAMERA_PITCH   (*(volatile int16_t *)0x45131C)
#define CAMERA_ROLL    (*(volatile int16_t *)0x451320)
#define CAMERA_HEIGHT  (*(volatile int32_t *)0x54A2D8)
#define PROJ_SCALE_X   (*(volatile int32_t *)0x542000)
#define PROJ_SCALE_Y   (*(volatile int32_t *)0x542004)
#define VIEW_CENTRE_X  (*(volatile int32_t *)0x54201C)
#define VIEW_CENTRE_Y  (*(volatile int32_t *)0x542028)
#define VIEW_WIDTH     (*(volatile int32_t *)0x542014)
#define VIEW_HEIGHT    (*(volatile int32_t *)0x542024)
#define VIEW_STRIDE    (*(volatile int32_t *)0x542018)
#define VIEW_PICTURE   (*(uint8_t *volatile *)0x542030)
#define GAME_PALETTE   ((const PALETTEENTRY *)0x52D758)
#define BANDS          20
#define LUT_SIZE       4096
#define VIEWS          2
#define TABLE_SCALE    32767.0  /* the game's sine table: 32767 x sin */

double brr_atan2_units(double y, double x);
double brr_square_root(double v);
double brr_sine_units(int32_t angle);

int brr_sky_active, brr_screenshot_drawing;

struct sky_view {
    const uint8_t *picture;            /* where the engine drew this view */
    int stride, width, height;
    double cx, cy, psx, psy, nx, ny;   /* projection; the rolled "up" direction */
    double height_d, pitch;
    int bands;
    uint8_t colour[BANDS];             /* palette index, by angle from the lowest */
    double middle[BANDS];              /* angle units at the band's middle */
    /* for the present: s = the "up" coordinate, as a LUT index in 16.16 fixed point */
    int32_t ds_dx, ds_dy, s0;
    int ox, oy;                        /* the view's top-left in the picture being presented */
    uint32_t lut[LUT_SIZE];
};
static struct sky_view views[VIEWS];
static int view_count, presented;      /* presented: the next sky_draw starts a new frame */
static uint32_t frames_logged;

/* sky_draw (0x41A4D0, stdcall), called from thunks.asm: Watcom compiles a call through a
 * constant address cast to a function pointer wrongly (call dword ptr [eax] with the wrong EAX) */
void __cdecl brr_call_sky_draw(int16_t *ground, int16_t *sky);

/* the (colour, angle) pairs of a table, up to and including the one at angle 0 */
static int table_length(const int16_t *table)
{
    int n = 0;
    while (n < BANDS && table[2 * n + 1] != 0)
        n++;
    return n < BANDS ? n + 1 : BANDS;
}

static void record(int16_t *ground, int16_t *sky)
{
    struct sky_view *v;
    double top = 13000, roll, low;
    int n_sky = table_length(sky), n_ground = table_length(ground), i, k = 0;
    if (presented) {
        view_count = 0;
        presented = 0;
    }
    if (view_count == VIEWS || n_sky + n_ground > BANDS)
        return;
    v = &views[view_count++];
    v->picture = VIEW_PICTURE;
    v->stride = VIEW_STRIDE;
    v->width = VIEW_WIDTH;
    v->height = VIEW_HEIGHT;
    v->cx = VIEW_CENTRE_X;
    v->cy = VIEW_CENTRE_Y;
    v->psx = PROJ_SCALE_X;
    v->psy = PROJ_SCALE_Y;
    roll = -(double)CAMERA_ROLL;  /* sky_draw turns by -roll */
    v->nx = brr_sine_units((int32_t)roll);
    v->ny = brr_sine_units((int32_t)roll + 16384);
    v->height_d = CAMERA_HEIGHT;
    v->pitch = CAMERA_PITCH;
    /* the bands from the lowest angle up: the ground's (its table runs from 13,000 below the
     * horizon up to it), then the sky's (its table runs from 13,000 above down to the horizon) */
    low = -13000;
    for (i = 0; i < n_ground; i++) {
        double edge = -(double)ground[2 * i + 1];
        v->colour[i] = (uint8_t)ground[2 * i];
        v->middle[i] = (low + edge) / 2;
        low = edge;
    }
    k = n_ground;
    for (i = 0; i < n_sky; i++) {
        double edge = sky[2 * i + 1];
        v->colour[k + n_sky - 1 - i] = (uint8_t)sky[2 * i];
        v->middle[k + n_sky - 1 - i] = (top + edge) / 2;
        top = edge;
    }
    v->bands = n_ground + n_sky;
}

/* Site sky_draw_call (through brr_hook_sky_draw, thunks.asm): render_view's call of sky_draw */
void __cdecl brr_sky_draw(int16_t *ground, int16_t *sky)
{
    int16_t *g = ground ? ground : DEFAULT_GROUND, *s = sky ? sky : DEFAULT_SKY;
    int16_t saved[2 * BANDS];
    int n_sky, n_ground, i;
    brr_shade_view_begin();  /* smooth shading (shade.c): this view's part of the side buffer */
    if (!brr_sky_active || brr_screenshot_drawing) {
        brr_call_sky_draw(ground, sky);
        return;
    }
    record(g, s);
    n_sky = table_length(s);
    n_ground = table_length(g);
    for (i = 0; i < n_sky; i++) {
        saved[i] = s[2 * i];
        s[2 * i] = SKY_MARKER;
    }
    for (i = 0; i < n_ground; i++) {
        saved[BANDS + i] = g[2 * i];
        g[2 * i] = SKY_MARKER;
    }
    brr_call_sky_draw(ground, sky);
    for (i = 0; i < n_sky; i++)
        s[2 * i] = saved[i];
    for (i = 0; i < n_ground; i++)
        g[2 * i] = saved[BANDS + i];
}

/* The angle above the horizon (game units) of the direction whose "up" coordinate is s */
static double elevation(const struct sky_view *v, double s)
{
    double root = brr_square_root(1 + s * s), r = -v->height_d / (TABLE_SCALE * root);
    if (r > 1)
        r = 1;
    if (r < -1)
        r = -1;
    return brr_atan2_units(r, brr_square_root(1 - r * r)) - brr_atan2_units(s, 1) - v->pitch;
}

static uint32_t rgb(const PALETTEENTRY *p) { return (uint32_t)p->peRed << 16 | (uint32_t)p->peGreen << 8 | p->peBlue; }

static uint32_t blend(uint32_t a, uint32_t b, double t)
{
    int i;
    uint32_t out = 0;
    for (i = 0; i < 24; i += 8) {
        double ca = (a >> i) & 0xFF, cb = (b >> i) & 0xFF;
        out |= (uint32_t)(ca + (cb - ca) * t + 0.5) << i;
    }
    return out;
}

/* The colour at an angle: between the middles of the two bands around it */
static uint32_t colour_at(const struct sky_view *v, double e)
{
    int k;
    if (e <= v->middle[0])
        return rgb(&GAME_PALETTE[v->colour[0]]);
    for (k = 1; k < v->bands; k++)
        if (e < v->middle[k])
            return blend(rgb(&GAME_PALETTE[v->colour[k - 1]]), rgb(&GAME_PALETTE[v->colour[k]]),
                         (e - v->middle[k - 1]) / (v->middle[k] - v->middle[k - 1]));
    return rgb(&GAME_PALETTE[v->colour[v->bands - 1]]);
}

/* Before a present: each recorded view's colours along its "up" coordinate, and how that
 * coordinate follows the final picture's pixels. picture: the game's picture being shown
 * (rows pitch bytes apart), enlarged sx by sy and placed at (lx, ly). Returns the views. */
int brr_sky_begin(const uint8_t *picture, int pic_w, int pic_h, int pitch, int sx, int sy, int lx, int ly)
{
    int i, j;
    (void)pic_w;
    presented = 1;
    if (!brr_sky_active)
        return 0;
    for (i = 0; i < view_count; i++) {
        struct sky_view *v = &views[i];
        ptrdiff_t offset = v->picture - picture;
        double s_min = 1e9, s_max = -1e9, scale, ax, ay, c;
        int corner;
        if (offset < 0 || offset >= (ptrdiff_t)pitch * pic_h || v->psx <= 0 || v->psy <= 0) {
            v->bands = 0;  /* not drawn into this picture */
            continue;
        }
        v->oy = (int)(offset / pitch);
        v->ox = (int)(offset % pitch);
        /* a final pixel (fx, fy) is at the game picture's ((fx - lx + 0.5) / sx - 0.5, ...);
         * the view's own pixel is that less (ox, oy); s = X nx + Y ny with X = (vx - cx) / psx */
        ax = v->nx / (sx * v->psx);
        ay = v->ny / (sy * v->psy);
        c = (((0.5 - lx) / sx - 0.5 - v->ox - v->cx) / v->psx) * v->nx
          + (((0.5 - ly) / sy - 0.5 - v->oy - v->cy) / v->psy) * v->ny;
        for (corner = 0; corner < 4; corner++) {
            double fx = lx + (corner & 1 ? (v->ox + v->width) * sx : v->ox * sx);
            double fy = ly + (corner & 2 ? (v->oy + v->height) * sy : v->oy * sy);
            double s = ax * fx + ay * fy + c;
            if (s < s_min) s_min = s;
            if (s > s_max) s_max = s;
        }
        if (s_max - s_min < 1e-6)
            s_max = s_min + 1e-6;
        for (j = 0; j < LUT_SIZE; j++)
            v->lut[j] = colour_at(v, elevation(v, s_min + (s_max - s_min) * j / (LUT_SIZE - 1)));
        scale = (LUT_SIZE - 1) / (s_max - s_min) * 65536.0;
        v->ds_dx = (int32_t)(ax * scale);
        v->ds_dy = (int32_t)(ay * scale);
        v->s0 = (int32_t)((c - s_min) * scale);
    }
    if (view_count && frames_logged++ < 3)
        brr_log("sky: %d view(s); the first: camera height %.0f, pitch %.0f, roll %d, centre %.0f,%.0f, scale %.0f,%.0f",
                view_count, views[0].height_d, views[0].pitch, (int)CAMERA_ROLL, views[0].cx, views[0].cy,
                views[0].psx, views[0].psy);
    return view_count;
}

/* The view (or -1) that the game picture's row gy belongs to */
int brr_sky_view_of_row(int gy)
{
    int i;
    for (i = 0; i < view_count; i++)
        if (views[i].bands && gy >= views[i].oy && gy < views[i].oy + views[i].height)
            return i;
    return -1;
}

/* count final pixels from (fx, fy) in the sky of view i */
void brr_sky_span(uint32_t *out, int fx, int fy, int count, int i)
{
    const struct sky_view *v = &views[i];
    int32_t s = (int32_t)((double)v->s0 + (double)v->ds_dx * fx + (double)v->ds_dy * fy);
    while (count-- > 0) {
        int32_t k = s >> 16;
        *out++ = v->lut[k < 0 ? 0 : k >= LUT_SIZE ? LUT_SIZE - 1 : k];
        s += v->ds_dx;
    }
}
