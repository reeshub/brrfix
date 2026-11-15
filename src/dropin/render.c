/* Phase 3c, the new renderer (SPEC Phase 3c, milestone 3). The golden rule (director,
 * 2026-11-12): nothing is scaled twice, and integer scaling is the gold standard. So the
 * final picture is composed here, at the screen's own resolution (full screen) or the
 * window's (in a window), and shown 1:1:
 *   - full screen: the game's DirectDraw set-up (0x42C590) asks cnc-ddraw for a mode of the
 *     screen's own size (site dd_setup_entry; site dd_mode_check lets that size through the
 *     game's 320x200-or-640x480 check), and cnc-ddraw, rendering at the desktop's size,
 *     shows it 1:1. Each present, our picture of that size takes the place of the game's
 *     buffer, and the frame size the copy uses (0x44BFB8) is the mode's, both only for the
 *     present (WndProc sets the frame size back to the game's display size every frame);
 *   - in a window: display.c draws our picture of the window's size with GDI, 1:1.
 * The game itself still runs at its own 320x200 or 640x480, so nothing else changes.
 * Its picture is enlarged once, by whole numbers, and centred on black (palette index 0,
 * as the game's own bands): the 320x200 menus (the doubling into 640x480, 0x4262B4, is
 * removed: site menu_double_call) at the 4:3 shape a 1996 monitor gave them, or with
 * square pixels ([display] aspect); 640x480 pictures (races in high resolution) with
 * square pixels, as the game drew them. The menu's mouse is mapped back the same way
 * (site menu_mouse). Races at the screen's resolution are milestone 4.
 * Experimental, [display] render_resolution=max ("Max" in the launcher): single-view races
 * and the attract-mode demo drawn bigger than 640x480, up to RENDER_MAX_W x RENDER_MAX_H
 * (1008x756), the most the 3D engine's 1,024-entry reciprocal table allows without a bigger
 * one of ours: site view_max raises the view record's maximum before the engine allocates
 * its picture, depth buffer and row table from it, and site set_view_entry turns the game's
 * 640x480 race view into ours (race_view_size). The game's display stays 640x480 (menus,
 * the window, DirectDraw, split screen and the menu mouse are unchanged); the HUD is still
 * the game's own, scaled by its own code to the view's width.
 * Full colour ([display] full_colour, 2026-11-15): the final picture is made in 32 bits, each
 * of the game's pixels through its live palette (0x52D758), so effects beyond its 256 colours
 * can be added; the first is the smooth sky (sky.c). In full screen the game asks DirectDraw
 * for a 32-bit mode (site dd_mode_bpp, its PUSH 8 at 0x42C7D7), and the present (site
 * present_entry) composes straight into the back surface and flips, instead of the game's
 * 8-bit copy (dd_copy_frame 0x42CAC0). A 32-bit surface takes no palette, so the game's
 * dd_set_palette (0x42D1FC), which fills its palette entries first, would create a new
 * palette object each time while dd_palette_pending (0x44BFC4) stayed set: it's cleared here.
 * In a window, display.c draws the 32-bit picture with GDI.
 * Windows NT family only (Wine, Proton, Windows 2000 onwards): on Windows 95/98/ME the
 * original renderer stays. Docs: KNOWLEDGE "The renderer (Phase 3c)". */
#include "brrfix.h"
#include <string.h>

#define VIEW_W         (*(volatile int16_t *)0x44BE60)
#define VIEW_H         (*(volatile int16_t *)0x44BE62)
#define VIEW_BUFFER    (*(uint8_t *volatile *)0x44BE68)
#define DISPLAY_W      (*(volatile int32_t *)0x44A618)
#define DISPLAY_H      (*(volatile int32_t *)0x44A61C)
#define FRAME_W        (*(volatile int32_t *)0x44BFB8)
#define FRAME_H        (*(volatile int32_t *)0x44BFBC)
#define PRESENT_SOURCE (*(uint8_t *volatile *)0x52D738)
#define MOUSE_X        (*(volatile int16_t *)0x44BBEC)  /* in the final picture's pixels */
#define MOUSE_Y        (*(volatile int16_t *)0x44BBEE)
#define SPLIT_SCREEN   (*(volatile int32_t *)0x44BE74)  /* -1: two views, one above the other */

#define MENU_W 320
#define MENU_H 200

int brr_renderer_active;               /* the new renderer's sites are patched */
int brr_full_colour;                   /* it presents 32-bit pictures */
uint32_t brr_dd_setup_continue = 0x42C599;
static int screen_w, screen_h;         /* the screen's own resolution */
static int mode_w, mode_h;             /* full screen: the mode we gave DirectDraw (0 in a window) */
static int final_w, final_h;           /* the last final picture's size, for the mouse */
uint32_t brr_set_view_continue = 0x425F7D;  /* set_view after the 5 bytes site set_view_entry replaces */
static int race_w, race_h;             /* Max: the race view we last gave the game (0: none) */
static int view_held, screenshot_held; /* Max: a screenshot is being taken at 640x480 */
uint32_t brr_orig_screenshot_race = 0x423B5C, brr_orig_screenshot_tape = 0x423C40;

/* ---- deciding (at patch time) ---- */

/* The screen's own resolution. cnc-ddraw replaces EnumDisplaySettingsA in our imports and
 * in GetProcAddress's answers (it reports the game's mode); the Ex version it leaves alone. */
static int find_screen_size(void)
{
    typedef BOOL (WINAPI *enum_ex_fn)(LPCSTR, DWORD, DEVMODEA *, DWORD);
    enum_ex_fn enum_ex = (enum_ex_fn)GetProcAddress(GetModuleHandleA("user32.dll"), "EnumDisplaySettingsExA");
    DEVMODEA mode;
    memset(&mode, 0, sizeof mode);
    mode.dmSize = sizeof mode;
    if (!enum_ex || !enum_ex(NULL, ENUM_CURRENT_SETTINGS, &mode, 0))
        return 0;
    screen_w = (int)mode.dmPelsWidth;
    screen_h = (int)mode.dmPelsHeight;
    return screen_w >= 640 && screen_h >= 480;
}

/* Whether the new renderer can run; logs why not. cnc_ddraw: the loaded DirectDraw is cnc-ddraw. */
int brr_renderer_decide(int cnc_ddraw)
{
    if (!cfg.renderer_new) {
        brr_log("render: the original renderer ([display] renderer=original)");
        return 0;
    }
    if (GetVersion() & 0x80000000u) {
        brr_log("render: the new renderer is for Windows 2000 onwards, Wine and Proton; "
                "on Windows 95, 98 and ME the game keeps its original renderer");
        return 0;
    }
    if (!find_screen_size()) {
        brr_log("render: can't read the screen's resolution; the original renderer instead");
        return 0;
    }
    if (!cnc_ddraw) {
        brr_log("render: the Modern renderer needs cnc-ddraw (CNCDD.DLL beside the launcher); "
                "the original renderer instead");
        return 0;
    }
    brr_log("render: the Modern renderer: the picture composed at %s and shown 1:1; menus %s",
            cfg.display_mode == DISPLAY_WINDOWED ? "the window's size (cnc-ddraw's window; Alt+Enter: full screen)"
                                                 : "the screen's resolution",
            cfg.window_aspect43 ? "at the 4:3 shape of a 1996 monitor" : "with square pixels");
    return 1;
}

/* ---- the layout: a picture enlarged by whole numbers, centred ---- */
struct layout { int sx, sy, x, y; };

/* All the game's pictures were made for a 4:3 screen: 640x480 has square pixels, 320x200
 * pixels 1.2 times taller than wide. With aspect=4:3: as many rows as fit, then the whole
 * number across nearest that shape; with square: the same whole number both ways. */
static void fit(int w, int h, int out_w, int out_h, struct layout *l)
{
    int sx, sy;
    sy = out_h / h;
    if (sy < 1)
        sy = 1;
    if (cfg.window_aspect43)  /* sx / sy = 4h / 3w, rounded to the nearest */
        sx = (sy * 8 * h + 3 * w) / (6 * w);
    else
        sx = sy;
    if (sx < 1)
        sx = 1;
    if (sx * w > out_w) {  /* too wide for the screen (rare: tall screens): width decides */
        sx = out_w / w;
        if (sx < 1)
            sx = 1;
        sy = cfg.window_aspect43 ? (sx * 6 * w + 4 * h) / (8 * h) : sx;
        if (sy < 1)
            sy = 1;
        if (sy * h > out_h && out_h / h >= 1)
            sy = out_h / h;
    }
    l->sx = sx;
    l->sy = sy;
    l->x = (out_w - sx * w) / 2;
    l->y = (out_h - sy * h) / 2;
    if (l->x < 0) l->x = 0;
    if (l->y < 0) l->y = 0;
}

/* What the game is showing: the 320x200 picture its menus (and low resolution, and recording
 * playback) draw at the start of the view buffer, or else the display-sized picture the
 * original present would have copied. */
static int game_picture(const uint8_t **pixels, int *w, int *h, int *pitch)
{
    const uint8_t *source = PRESENT_SOURCE;
    if (!source)
        return 0;
    if (source == VIEW_BUFFER && VIEW_W == MENU_W && VIEW_H == MENU_H) {
        *w = MENU_W;
        *h = MENU_H;
    } else if (race_w && source == VIEW_BUFFER && VIEW_W == race_w && VIEW_H == race_h) {
        *w = race_w;  /* Max: our race view (a multiple of 16 wide, so the rows are packed) */
        *h = race_h;
    } else {
        *w = DISPLAY_W;
        *h = DISPLAY_H;
    }
    if (*w <= 0 || *h <= 0 || *w > RENDER_MAX_W || *h > RENDER_MAX_H)
        return 0;
    *pixels = source;
    *pitch = *w;
    return 1;
}

static void log_layout(int w, int h, int out_w, int out_h, const struct layout *l)
{
    static struct { int w, h, out_w, out_h; } seen[8];
    static int count;
    int i;
    for (i = 0; i < count; i++)
        if (seen[i].w == w && seen[i].h == h && seen[i].out_w == out_w && seen[i].out_h == out_h)
            return;
    if (count == 8)
        return;
    seen[count].w = w; seen[count].h = h; seen[count].out_w = out_w; seen[count].out_h = out_h;
    count++;
    brr_log("render: %dx%d picture enlarged %dx%d to %dx%d, at %d,%d in the %dx%d final picture", w, h, l->sx,
            l->sy, w * l->sx, h * l->sy, l->x, l->y, out_w, out_h);
}

/* Compose the final picture: the game's picture enlarged by whole numbers, centred, on black. */
static void compose(const uint8_t *in, int w, int h, int in_pitch, uint8_t *out, int out_w, int out_h,
                    int out_pitch, const struct layout *l)
{
    int y, row = 0, cols = w, rows = h;
    int left = l->x, width = w * l->sx;
    if (left + width > out_w) {  /* never more than fits (smaller than the picture: cropped) */
        cols = (out_w - left) / l->sx;
        width = cols * l->sx;
    }
    if (l->y + rows * l->sy > out_h)
        rows = (out_h - l->y) / l->sy;
    for (y = 0; y < out_h; y++) {
        uint8_t *o = out + (size_t)y * (size_t)out_pitch;
        int inside = y >= l->y && row < rows;
        if (!inside) {
            memset(o, 0, (size_t)out_w);
            continue;
        }
        if ((y - l->y) % l->sy) {  /* a repeat of the row above */
            memcpy(o, o - out_pitch, (size_t)out_w);
        } else {
            const uint8_t *p = in + (size_t)row * (size_t)in_pitch;
            uint8_t *q = o + left;
            int x, k;
            memset(o, 0, (size_t)left);
            if (l->sx == 1)
                memcpy(q, p, (size_t)cols);
            else
                for (x = 0; x < cols; x++) {
                    uint8_t v = p[x];
                    for (k = 0; k < l->sx; k++)
                        *q++ = v;
                }
            memset(o + left + width, 0, (size_t)(out_w - left - width));
        }
        if ((y - l->y) % l->sy == l->sy - 1)
            row++;
    }
}

static uint8_t *picture;
static size_t picture_size;

static uint8_t *picture_of(size_t need)
{
    if (need > picture_size) {
        uint8_t *bigger = (uint8_t *)HeapAlloc(GetProcessHeap(), 0, need);
        if (!bigger)
            return NULL;
        if (picture)
            HeapFree(GetProcessHeap(), 0, picture);
        picture = bigger;
        picture_size = need;
    }
    return picture;
}

/* The final picture at out_w x out_h (rows out_pitch bytes apart), or NULL when there's
 * nothing to compose: then the present shows the game's frame as it is. */
const uint8_t *brr_render_compose(int out_w, int out_h, int out_pitch)
{
    static int failed;
    const uint8_t *pixels;
    int w, h, pitch;
    struct layout l;
    uint8_t *out;
    if (!brr_renderer_active || out_w <= 0 || out_h <= 0 || !game_picture(&pixels, &w, &h, &pitch))
        return NULL;
    out = picture_of((size_t)out_pitch * (size_t)out_h);
    if (!out) {
        if (!failed++)
            brr_log("render: no memory for a %dx%d picture", out_w, out_h);
        return NULL;
    }
    fit(w, h, out_w, out_h, &l);
    compose(pixels, w, h, pitch, out, out_w, out_h, out_pitch, &l);
    final_w = out_w;
    final_h = out_h;
    log_layout(w, h, out_w, out_h, &l);
    return out;
}

/* ---- full colour ---- */
static uint32_t colours[256];

static PALETTEENTRY shown_palette[256];  /* the colours of the last full-colour picture */

static void palette_colours(void)
{
    const PALETTEENTRY *p = (const PALETTEENTRY *)0x52D758;
    int i;
    memcpy(shown_palette, p, sizeof shown_palette);
    for (i = 0; i < 256; i++)
        colours[i] = (uint32_t)p[i].peRed << 16 | (uint32_t)p[i].peGreen << 8 | p[i].peBlue;
}

static void fill32(uint32_t *o, uint32_t value, int count)
{
    while (count-- > 0)
        *o++ = value;
}

/* a + (b - a) x t / 256, per channel */
static uint32_t blend256(uint32_t a, uint32_t b, uint32_t t)
{
    return (((a & 0xFF00FFu) * (256 - t) + (b & 0xFF00FFu) * t) >> 8 & 0xFF00FFu)
         | (((a & 0x00FF00u) * (256 - t) + (b & 0x00FF00u) * t) >> 8 & 0x00FF00u);
}

/* As compose, in 32 bits: each game pixel through the palette, a smoothly shaded one (shade.c)
 * blended between its shade and the next, and the sky's marker pixels (sky.c) coloured per
 * final pixel, so a row with sky in it is worked out afresh each time. origin: where the
 * picture is in the game's memory (in may be a copy of it). */
static void compose32(const uint8_t *in, const uint8_t *origin, int w, int h, int in_pitch, uint32_t *out,
                      int out_w, int out_h, int out_pitch, const struct layout *l, int sky)
{
    int y, row = 0, cols = w, rows = h, view = -1, marked = 0;
    int left = l->x, width = w * l->sx;
    if (left + width > out_w) {
        cols = (out_w - left) / l->sx;
        width = cols * l->sx;
    }
    if (l->y + rows * l->sy > out_h)
        rows = (out_h - l->y) / l->sy;
    for (y = 0; y < out_h; y++) {
        uint32_t *o = out + (size_t)y * (size_t)out_pitch;
        int first = (y - l->y) % l->sy == 0;
        if (!(y >= l->y && row < rows)) {
            fill32(o, 0, out_w);
            continue;
        }
        if (first) {
            view = sky ? brr_sky_view_of_row(row) : -1;
            marked = view >= 0 && memchr(in + (size_t)row * (size_t)in_pitch, SKY_MARKER, (size_t)cols) != NULL;
        }
        if (!first && !marked) {  /* a repeat of the row above */
            memcpy(o, o - out_pitch, (size_t)out_w * 4);
        } else {
            const uint8_t *p = in + (size_t)row * (size_t)in_pitch;
            const uint16_t *side = brr_shade_row(origin + (size_t)row * (size_t)in_pitch, cols);
            uint32_t *q = o + left;
            int x, k;
            fill32(o, 0, left);
            for (x = 0; x < cols; x++, q += l->sx) {
                uint8_t v = p[x];
                if (v == SKY_MARKER && marked) {
                    brr_sky_span(q, left + x * l->sx, y, l->sx, view);
                } else {
                    uint32_t c = colours[v];
                    if (side && (side[x] & 0xFF) && (side[x] >> 8) == v && v != 255
                        && (!cfg.debug_shade_split || x < cols / 2))
                        c = blend256(c, colours[v + 1], side[x] & 0xFF);
                    for (k = 0; k < l->sx; k++)
                        q[k] = c;
                }
            }
            fill32(o + left + width, 0, out_w - left - width);
        }
        if ((y - l->y) % l->sy == l->sy - 1)
            row++;
    }
}

/* The game picture last shown, kept for a fade's steps: an 8-bit screen keeps showing the
 * last picture presented while the palette changes, even once the game has drawn the next
 * screen's picture into its buffer (the screens' set-ups draw it, then fade the old palette
 * out and the new one in). */
static uint8_t *shown_pixels;
static const uint8_t *shown_origin;
static int shown_w, shown_h, replaying;

/* The final picture in 32 bits at out_w x out_h into out (rows out_pitch pixels apart);
 * 0 when there's nothing to compose. */
int brr_render_compose32(uint32_t *out, int out_w, int out_h, int out_pitch)
{
    const uint8_t *pixels, *origin;
    int w, h, pitch, sky;
    struct layout l;
    if (!brr_full_colour || out_w <= 0 || out_h <= 0)
        return 0;
    if (replaying && shown_origin) {
        pixels = shown_pixels;
        origin = shown_origin;
        w = pitch = shown_w;
        h = shown_h;
    } else {
        if (!game_picture(&origin, &w, &h, &pitch))
            return 0;
        pixels = origin;
        if (!shown_pixels)
            shown_pixels = (uint8_t *)HeapAlloc(GetProcessHeap(), 0, (size_t)RENDER_MAX_W * RENDER_MAX_H);
        shown_origin = NULL;
        if (shown_pixels && pitch == w) {
            memcpy(shown_pixels, origin, (size_t)w * (size_t)h);
            shown_origin = origin;
            shown_w = w;
            shown_h = h;
        }
    }
    fit(w, h, out_w, out_h, &l);
    palette_colours();
    sky = brr_sky_begin(origin, w, h, pitch, l.sx, l.sy, l.x, l.y) > 0;
    compose32(pixels, origin, w, h, pitch, out, out_w, out_h, out_pitch, &l, sky);
    final_w = out_w;
    final_h = out_h;
    log_layout(w, h, out_w, out_h, &l);
    return 1;
}

/* Full screen in full colour: our picture straight into the back surface, then the flip
 * (as dd_present 0x42CE78 does, without dd_copy_frame's 8-bit copy). 1 when done. */
#define DD_PRIMARY         (*(void *volatile *)0x52D710)
#define DD_BACK            (*(void *volatile *)0x52D714)
#define DD_READY           (*(volatile uint8_t *)0x52D720)
#define DD_PALETTE_PENDING (*(volatile uint8_t *)0x44BFC4)
#define DD_CLEAR_PENDING   (*(volatile uint8_t *)0x52D754)
#define DDERR_LOST         ((HRESULT)0x887601C2L)
#define METHOD(object, offset) (*(void **)(*(uint8_t **)(object) + (offset)))
typedef HRESULT (__stdcall *lock_fn)(void *self, RECT *rect, void *desc, DWORD flags, HANDLE event);
typedef HRESULT (__stdcall *unlock_fn)(void *self, void *rect);
typedef HRESULT (__stdcall *flip_fn)(void *self, void *target, DWORD flags);
typedef HRESULT (__stdcall *restore_fn)(void *self);

static int present_full_colour(void)
{
    static int failures, logged;
    uint8_t desc[0x6C];  /* DDSURFACEDESC */
    void *back = DD_BACK, *primary = DD_PRIMARY;
    HRESULT result;
    uint32_t bits, width, height;
    int32_t pitch;
    uint8_t *surface;
    if (!DD_READY || !back || !primary)
        return 0;
    DD_PALETTE_PENDING = 0;  /* no palette on a 32-bit surface: the game just keeps its entries */
    DD_CLEAR_PENDING = 0;    /* the whole picture is drawn each time */
    memset(desc, 0, sizeof desc);
    *(DWORD *)desc = sizeof desc;
    result = ((lock_fn)METHOD(back, 0x64))(back, NULL, desc, 1 /* DDLOCK_WAIT */, NULL);
    if (result == DDERR_LOST) {
        ((restore_fn)METHOD(back, 0x6C))(back);
        return 1;  /* this frame is skipped */
    }
    if (result != 0) {
        if (failures++ < 4)
            brr_log("render: full colour: Lock failed (0x%08lX)", (unsigned long)result);
        return 1;
    }
    height = *(DWORD *)(desc + 0x08);
    width = *(DWORD *)(desc + 0x0C);
    pitch = *(int32_t *)(desc + 0x10);
    surface = *(uint8_t **)(desc + 0x24);
    bits = *(DWORD *)(desc + 0x54);  /* ddpfPixelFormat.dwRGBBitCount */
    if (bits == 32 && surface) {
        if (brr_render_compose32((uint32_t *)surface, (int)width, (int)height, pitch / 4))
            brr_debug_frame_shot((const uint32_t *)surface, (int)width, (int)height, pitch / 4);
        else
            for (; height > 0; height--)
                memset(surface + (size_t)(height - 1) * (size_t)pitch, 0, (size_t)width * 4);
    }
    ((unlock_fn)METHOD(back, 0x80))(back, NULL);
    if (bits != 32) {
        if (failures++ < 4)
            brr_log("render: full colour: the surface has %lu bits per pixel, not 32", (unsigned long)bits);
        return 1;
    }
    result = ((flip_fn)METHOD(primary, 0x2C))(primary, NULL, 0);
    if (result == DDERR_LOST)
        ((restore_fn)METHOD(primary, 0x6C))(primary);
    if (!logged++)
        brr_log("render: full colour: %lux%lu, 32 bits, drawn straight into DirectDraw's back surface",
                (unsigned long)width, (unsigned long)*(DWORD *)(desc + 0x08));
    return 1;
}

/* Called by brr_retrace (vga_wait_retrace): full-screen fades step the palette once per
 * retrace without presenting, as an 8-bit screen shows new colours at once (palette_set_level
 * 0x412F48); in full colour the last picture is presented again in the new colours. */
void brr_render_palette_check(void)
{
    static int fade_logged;
    if (!brr_full_colour || *GAME_WINDOWED || !mode_w
        || !memcmp(shown_palette, (const void *)0x52D758, sizeof shown_palette))
        return;
    if (!fade_logged++)
        brr_log("render: full colour: a palette fade, shown a step at a time (on the picture last shown)");
    {
        static int ahead_logged;
        const uint8_t *now;
        int w, h, pitch;
        if (ahead_logged < 3 && shown_origin && game_picture(&now, &w, &h, &pitch)
            && (now != shown_origin || w != shown_w || h != shown_h
                || memcmp(now, shown_pixels, (size_t)w * (size_t)h))) {
            ahead_logged++;
            brr_log("render: full colour: a fade step while the game's buffer already holds a picture not yet "
                    "shown (screen 0x%02X); the one on screen is used", (unsigned)*(volatile uint16_t *)0x4B13C0);
        }
    }
    replaying = 1;
    present_full_colour();
    replaying = 0;
}

/* ---- Max: races drawn bigger (experimental) ---- */

/* The race view for a final picture of out_w x out_h: 4:3 (the game's own field of view, and
 * the shape its landscape grid is drawn for), a multiple of 16 wide, at most 1008x756 and never
 * smaller than the game's 640x480, enlarged d times; the d that fills the most rows of the final
 * picture, the smaller (the more detailed) on a tie. For example 1008x756 at 1x on 1280x800 or
 * 1600x900, 720x540 at 2x on 1920x1080, 960x720 at 2x on 2560x1440 and at 3x on 3840x2160. */
static void race_view_size(int out_w, int out_h, int *w, int *h)
{
    int d, best = 0;
    *w = 640;
    *h = 480;
    for (d = 1; d <= 8; d++) {
        int hh = out_h / d, ww = out_w / d;
        if (hh > RENDER_MAX_H)
            hh = RENDER_MAX_H;
        if (ww > RENDER_MAX_W)
            ww = RENDER_MAX_W;
        if (ww > hh * 4 / 3)
            ww = hh * 4 / 3;
        ww &= ~15;
        hh = ww * 3 / 4;
        if (ww < 640 || hh < 480)
            continue;
        if (d * hh > best) {
            best = d * hh;
            *w = ww;
            *h = hh;
        }
    }
}

/* Site set_view_entry: every call of the game's set_view (0x425F78; width in EAX, height in
 * EDX). The single-view 640x480 race view becomes ours; anything else is left alone. Only the
 * race screens ask for 640x480 (menus are 320x200, split screen 640x240 or 640x238); not only
 * with the high-resolution flag set, as the race's set-up clears it for its first frame
 * (0x426014). Returns height << 16 | width. */
uint32_t __cdecl brr_view_size(uint32_t width, uint32_t height)
{
    static int logged_w, logged_h;
    int out_w = mode_w ? mode_w : final_w, out_h = mode_w ? mode_h : final_h, w, h;
    if ((width & 0xFFFF) != 640 || (height & 0xFFFF) != 480 || SPLIT_SCREEN || out_w <= 0 || view_held)
        return (height & 0xFFFF) << 16 | (width & 0xFFFF);
    race_view_size(out_w, out_h, &w, &h);
    race_w = w;
    race_h = h;
    if (w != logged_w || h != logged_h) {
        logged_w = w;
        logged_h = h;
        brr_log("render: Max: races drawn at %dx%d instead of 640x480, for the %dx%d final picture", w, h,
                out_w, out_h);
    }
    return (uint32_t)h << 16 | (uint32_t)w;
}

/* Sites screenshot_race_call and screenshot_tape_call (run_screen_frame 0x428D9E, the race's
 * PrtSc, and 0x428D62, the numbered TAPEDECK pictures): the game redraws the race and saves
 * it as a PCX (0x4239BC), packing each row into a 1,280-byte buffer on the stack, which is
 * just enough for 640 pixels (PCX packing can double a row). So with Max, the view goes back
 * to 640x480 for the screenshot, and to ours after it. */
void __cdecl brr_screenshot_begin(void)
{
    static int logged;
    brr_screenshot_drawing = 1;  /* the game's own sky bands in the PCX (sky.c) */
    screenshot_held = race_w > 640 && VIEW_W == race_w && VIEW_H == race_h;
    if (screenshot_held) {
        if (logged++ < 4)
            brr_log("render: Max: the game's screenshot drawn at 640x480 (its PCX writer's row buffer)");
        view_held = 1;
        brr_call_set_view(640, 480);
    }
}

void __cdecl brr_screenshot_end(void)
{
    brr_screenshot_drawing = 0;
    if (screenshot_held) {
        view_held = 0;
        brr_call_set_view(640, 480);  /* ours again, through site set_view_entry */
        screenshot_held = 0;
    }
}

/* ---- full screen: DirectDraw at the screen's resolution ---- */

/* The Modern renderer in a window: the game runs full screen and cnc-ddraw shows its picture
 * in a window (Alt+Enter for full screen), so the mode is the window's size: the game's
 * picture (640x480, or 320x200 a fifth taller with 4:3) times window_scale, or with auto the
 * largest whole number that fits 85% of the desktop's work area, as the game's own window. */
static void window_mode_size(int *w, int *h)
{
    RECT work;
    int frame_w = DISPLAY_W, frame_h = DISPLAY_H, scale = cfg.window_scale, stretch;
    if (frame_w <= 0 || frame_h <= 0) {
        frame_w = 640;
        frame_h = 480;
    }
    stretch = cfg.window_aspect43 && frame_w * 3 != frame_h * 4;
    if (scale <= 0) {
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
        for (scale = 8; scale > 1; scale--)
            if (frame_w * scale <= (work.right - work.left) * 85 / 100
                && frame_h * scale * (stretch ? 6 : 5) / 5 <= (work.bottom - work.top) * 85 / 100)
                break;
    }
    *w = frame_w * scale;
    *h = stretch ? frame_h * scale * 6 / 5 : frame_h * scale;
    if (*w > screen_w || *h > screen_h) {
        *w = screen_w;
        *h = screen_h;
    }
}

/* Site dd_setup_entry: the DirectDraw set-up's mode, given its windowed flag. Returns the
 * mode for full screen as height << 16 | width, or 0 to keep the game's. */
uint32_t __cdecl brr_render_mode(uint32_t windowed)
{
    static int logged;
    if (!brr_renderer_active || (windowed & 0xFF)) {
        mode_w = mode_h = 0;
        return 0;
    }
    mode_w = screen_w;
    mode_h = screen_h;
    if (brr_modern_window)
        window_mode_size(&mode_w, &mode_h);
    if (logged++ < 4)
        brr_log("render: %s: DirectDraw at %dx%d, shown 1:1; the game runs at %ldx%ld",
                brr_modern_window ? "in cnc-ddraw's window" : "full screen (the screen's own size)",
                mode_w, mode_h, (long)DISPLAY_W, (long)DISPLAY_H);
    return (uint32_t)mode_h << 16 | (uint32_t)mode_w;
}

static uint8_t *game_source;
static int32_t game_frame_w, game_frame_h;

/* Called on entry to the present (0x42CE78), before the game copies its frame. Returns 1
 * when it has presented the frame itself (full colour, full screen). */
int __cdecl brr_render_before_present(void)
{
    const uint8_t *out;
    game_source = NULL;
    brr_shade_census_report();
    brr_debug_every_frame();
    if (!brr_renderer_active || *GAME_WINDOWED || !mode_w)
        return 0;  /* in a window, display.c composes */
    if (brr_full_colour)
        return present_full_colour();
    out = brr_render_compose(mode_w, mode_h, mode_w);
    if (!out)
        return 0;
    game_source = PRESENT_SOURCE;
    game_frame_w = FRAME_W;
    game_frame_h = FRAME_H;
    PRESENT_SOURCE = (uint8_t *)out;
    FRAME_W = mode_w;
    FRAME_H = mode_h;
    return 0;
}

void __cdecl brr_render_after_present(void)
{
    if (game_source) {
        PRESENT_SOURCE = game_source;
        FRAME_W = game_frame_w;
        FRAME_H = game_frame_h;
        game_source = NULL;
    }
}

/* Site menu_mouse (in 0x4274FC, only when the race view isn't set): the menu's mouse
 * position from the pointer's position in the final picture, the inverse of the layout
 * above (the original mapped only at 640x480, to its doubled picture: x / 2, (y - 40) / 2).
 * Returns x | y << 16. */
uint32_t __cdecl brr_menu_mouse(void)
{
    int x = MOUSE_X, y = MOUSE_Y;
    if (final_w > 0 && final_h > 0) {
        struct layout l;
        fit(MENU_W, MENU_H, final_w, final_h, &l);
        x = x >= l.x ? (x - l.x) / l.sx : -1;
        y = y >= l.y ? (y - l.y) / l.sy : -1;
    }
    if (x < 0) x = 0;
    if (x > MENU_W - 1) x = MENU_W - 1;
    if (y < 0) y = 0;
    if (y > MENU_H - 1) y = MENU_H - 1;
    if (cfg.debug_log_mouse) {  /* each new position (the same one is asked for every tick) */
        static int logged, last_x = -1, last_y = -1;
        if ((MOUSE_X != last_x || MOUSE_Y != last_y) && logged++ < 40)
            brr_log("mouse: pointer %d,%d in the %dx%d picture -> menu %d,%d", MOUSE_X, MOUSE_Y, final_w, final_h, x, y);
        last_x = MOUSE_X;
        last_y = MOUSE_Y;
    }
    return (uint32_t)(uint16_t)x | (uint32_t)(uint16_t)y << 16;
}
