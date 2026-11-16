/* Phase 3c, the Modern renderer (SPEC Phase 3c). Since 2026-11-15 (the director: no black
 * borders, "everything handed to cnc-ddraw super crisp and ready for scaling", a deliberate
 * relaxation of the golden rule) DirectDraw's mode is always the size of the game's picture,
 * and cnc-ddraw enlarges it with the launcher's scaling preset (4:3 sharp by default):
 *   - the menus: their own 320x200 (the doubling into 640x480, 0x4262B4, is removed: site
 *     menu_double_call), with the menu's mouse 1:1 (site menu_mouse);
 *   - races: the game's 640x480 (or 320x200 at low resolution), or with Max the bigger view
 *     below, chosen for the screen's height.
 * The game's set-up (0x42C590) gets the size through site dd_setup_entry (site dd_mode_check
 * lets any size through its 320x200-or-640x480 check). When the picture's size changes (a race
 * starts or ends), the present re-creates DirectDraw at the new size through the game's own
 * dd_set_mode (0x42D044), as its high and low resolution commands do. Each present, our copy
 * of the picture takes the place of the game's buffer, with the frame size the copy uses
 * (0x44BFB8) set to it, only for the present. The game itself still runs at its own 320x200
 * or 640x480 display size. In a window the game runs full screen too, and cnc-ddraw shows it
 * in a window of the size the launcher writes into ddraw.ini (Alt+Enter: full screen).
 * Experimental, [display] render_resolution=max ("Max" in the launcher): single-view races
 * and the attract-mode demo drawn bigger than 640x480, at the screen's (or the window's) own
 * height, 4:3 (race_view_size, worked out once at start-up): the recip_* sites point the 3D
 * engine at a reciprocal table of ours, bigger than its own 1,024 entries; site view_max raises
 * the view record's maximum before the engine allocates its picture, depth buffer and row
 * table from it, and site set_view_entry turns the game's 640x480 race view into ours. The game's display stays 640x480 (menus,
 * the window, DirectDraw, split screen and the menu mouse are unchanged); the HUD is laid over
 * the composed picture by hud.c ([display] hud=lowres), or with hud=game is the game's own,
 * scaled by its own code to the view's width.
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
#define DETAIL_LEVEL   (*(volatile int32_t *)0x44B044)  /* the vision distance */
#define SPLIT_SCREEN   (*(volatile int32_t *)0x44BE74)  /* -1: two views, one above the other */

#define MENU_W 320
#define MENU_H 200

int brr_renderer_active;               /* the new renderer's sites are patched */
int brr_full_colour;                   /* it presents 32-bit pictures */
uint32_t brr_dd_setup_continue = 0x42C599;
static int screen_w, screen_h;         /* the screen's own resolution */
static int mode_w, mode_h;             /* the mode we gave DirectDraw (0: the game's own window) */
static int want_w, want_h;             /* the size the next set-up is for (0: the menus') */
static int switching;                  /* re-creating DirectDraw, from the present */
static int mode_switches;
static int window_w, window_h;         /* cnc-ddraw's window (ddraw.ini width, height; 0: the game's size) */
static int final_w, final_h;           /* the last final picture's size, for the mouse */
uint32_t brr_set_view_continue = 0x425F7D;  /* set_view after the 5 bytes site set_view_entry replaces */
static int race_w, race_h;             /* Max: the race view we last gave the game (0: none) */
static int view_held, screenshot_held; /* Max: a screenshot is being taken at 640x480 */
uint32_t brr_orig_screenshot_race = 0x423B5C, brr_orig_screenshot_tape = 0x423C40;

/* ---- deciding (at patch time) ---- */

uint32_t brr_reciprocals[RECIPROCALS];       /* the 3D engine's reciprocal table, bigger (recip_* sites) */
int brr_view_max_w = 640, brr_view_max_h = 480;  /* the engine's buffers' size (site view_max) */
/* Native widescreen ([display] widescreen, with Max, full screen on a screen wider than 4:3):
 * races drawn at the screen's own shape, with the field of view from the height (sites
 * proj_scale_store and landscape_width), and every picture handed to cnc-ddraw at that size,
 * the menus enlarged into it at 4:3 by whole numbers, with black bars at the sides. */
int brr_widescreen;
int32_t brr_proj_scale;  /* set_view's horizontal scale for the view being set (0: the game's) */
static void race_view_size(int out_w, int out_h, int *w, int *h);

/* 2^32 / n, as the engine's own table at 0x54A380 (engine_init_core 0x447950: entry 1 is
 * 0xFFFFFFFF, entry 0 is never written), for n up to RECIPROCALS - 1 instead of 1,023 */
void brr_reciprocals_fill(void)
{
    uint32_t n;
    brr_reciprocals[0] = 0;
    brr_reciprocals[1] = 0xFFFFFFFFu;
    for (n = 2; n < RECIPROCALS; n++)  /* floor(2^32 / n), from floor((2^32 - 1) / n) */
        brr_reciprocals[n] = 0xFFFFFFFFu / n + (0xFFFFFFFFu % n == n - 1);
}

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

/* Whether cnc-ddraw shows the picture at its own shape, as native widescreen needs: not with
 * a fixed aspect_ratio (the 4:3 presets), which would squeeze it; the launcher writes it
 * empty for widescreen. Logs why not. */
static int keeps_shape(void)
{
    char path[MAX_PATH], aspect[32], maintas[16], boxing[16];
    brr_path_beside_dll(path, sizeof path, "ddraw.ini");
    GetPrivateProfileStringA("ddraw", "aspect_ratio", "", aspect, sizeof aspect, path);
    GetPrivateProfileStringA("ddraw", "maintas", "false", maintas, sizeof maintas, path);
    GetPrivateProfileStringA("ddraw", "boxing", "false", boxing, sizeof boxing, path);
    if (aspect[0] && !lstrcmpiA(maintas, "true") && lstrcmpiA(boxing, "true")) {
        brr_log("render: native widescreen: off, as ddraw.ini keeps the picture at %s (aspect_ratio); "
                "Big Red Launcher writes it empty for widescreen", aspect);
        return 0;
    }
    return 1;
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
    if (cfg.display_mode == DISPLAY_WINDOWED) {
        char path[MAX_PATH];
        brr_path_beside_dll(path, sizeof path, "ddraw.ini");
        window_w = (int)GetPrivateProfileIntA("ddraw", "width", 0, path);
        window_h = (int)GetPrivateProfileIntA("ddraw", "height", 0, path);
    }
    brr_log("render: the Modern renderer: DirectDraw at the picture's own size (menus 320x200, races "
            "their own), enlarged by cnc-ddraw's scaling preset%s",
            cfg.display_mode == DISPLAY_WINDOWED ? ", in cnc-ddraw's window (Alt+Enter: full screen)" : "");
    if (window_w > 0 && window_h > 0)
        brr_log("render: cnc-ddraw's window: %dx%d (ddraw.ini)", window_w, window_h);
    if (cfg.render_max) {  /* Max's view, for the engine's buffers (it can't change later) */
        int windowed = cfg.display_mode == DISPLAY_WINDOWED && window_w > 0 && window_h > 0;
        int out_w = windowed ? window_w : screen_w, out_h = windowed ? window_h : screen_h;
        brr_widescreen = cfg.widescreen && out_w * 3 > out_h * 4 && keeps_shape();
        race_view_size(out_w, out_h, &brr_view_max_w, &brr_view_max_h);
        if (brr_widescreen)
            brr_log("render: native widescreen: races at %dx%d (the %dx%d %s's shape), the field of view from "
                    "the height; menus 4:3 in the middle of that picture", brr_view_max_w, brr_view_max_h,
                    out_w, out_h, windowed ? "window" : "screen");
        else if (cfg.widescreen)
            brr_log("render: native widescreen: off, as the %s is %dx%d (no wider than 4:3)",
                    windowed ? "window" : "screen", out_w, out_h);
    }
    return 1;
}

/* ---- the layout: a picture enlarged by whole numbers, centred ---- */
struct layout { int sx, sy, x, y; };

/* All the game's pictures were made for a 4:3 screen: 640x480 has square pixels, 320x200
 * pixels 1.2 times taller than wide. With aspect=4:3: as many rows as fit, then the whole
 * number across nearest that shape; with square: the same whole number both ways. */
static void fit(int w, int h, int out_w, int out_h, struct layout *l)
{
    int sx, sy, aspect43 = cfg.window_aspect43 || brr_widescreen;  /* widescreen: menus at 4:3 */
    sy = out_h / h;
    if (sy < 1)
        sy = 1;
    if (aspect43)  /* sx / sy = 4h / 3w, rounded to the nearest */
        sx = (sy * 8 * h + 3 * w) / (6 * w);
    else
        sx = sy;
    if (sx < 1)
        sx = 1;
    if (sx * w > out_w) {  /* too wide for the screen (rare: tall screens): width decides */
        sx = out_w / w;
        if (sx < 1)
            sx = 1;
        sy = aspect43 ? (sx * 6 * w + 4 * h) / (8 * h) : sx;
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
    if (*w <= 0 || *h <= 0 || *w > brr_view_max_w || *h > brr_view_max_h)
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
    brr_hud_overlay(pixels, w, h, out, out_w, out_h, out_pitch, l.x, l.y, l.sx, l.sy, NULL, 0);
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
static int shown_w, shown_h, replaying, shown_hud;

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
            shown_pixels = (uint8_t *)HeapAlloc(GetProcessHeap(), 0, (size_t)brr_view_max_w * brr_view_max_h);
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
    /* the race HUD (hud.c) on top; a fade's steps keep the HUD the picture was shown with */
    if (!replaying)
        shown_hud = brr_hud_overlay(origin, w, h, out, out_w, out_h, out_pitch, l.x, l.y, l.sx, l.sy, colours, 0);
    else if (shown_hud)
        brr_hud_overlay(origin, w, h, out, out_w, out_h, out_pitch, l.x, l.y, l.sx, l.sy, colours, 1);
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

/* The race view for a final picture of out_w x out_h, which cnc-ddraw enlarges to the screen:
 * 4:3 (the game's own field of view, and the shape its landscape grid is drawn for), a multiple
 * of 16 wide, as tall as the picture but at most [display] render_max_height rows (1080 by
 * default; the game draws in software) and RENDER_LIMIT_W x RENDER_LIMIT_H, and never smaller
 * than the game's 640x480. For example 1056x792 on 1280x800 and 1440x1080 on 1920x1080; on
 * 2560x1440 and 3840x2160, 1440x1080, or 1920x1440 with a limit of 1440, and with none
 * 2880x2160 on 3840x2160. */
static void race_view_size(int out_w, int out_h, int *w, int *h)
{
    int hh = out_h, ww;
    if (cfg.render_max_rows > 0 && hh > cfg.render_max_rows)
        hh = cfg.render_max_rows;
    if (hh > RENDER_LIMIT_H)
        hh = RENDER_LIMIT_H;
    if (brr_widescreen) {  /* the screen's own shape: 1280x800, 1920x1080 (and on 2560x1440 and 4K) */
        ww = hh * out_w / out_h;
        if (ww > out_w)
            ww = out_w;
        if (ww > RENDER_LIMIT_W)
            ww = RENDER_LIMIT_W;
        ww &= ~15;
        hh = ww * out_h / out_w;
        *w = ww >= 640 && hh >= 480 ? ww : 640;
        *h = ww >= 640 && hh >= 480 ? hh : 480;
        return;
    }
    ww = hh * 4 / 3;
    if (ww > out_w)
        ww = out_w;
    if (ww > RENDER_LIMIT_W)
        ww = RENDER_LIMIT_W;
    ww &= ~15;
    hh = ww * 3 / 4;
    *w = ww >= 640 && hh >= 480 ? ww : 640;
    *h = ww >= 640 && hh >= 480 ? hh : 480;
}

/* Site set_view_entry: every call of the game's set_view (0x425F78; width in EAX, height in
 * EDX). The single-view 640x480 race view becomes ours; anything else is left alone. Only the
 * race screens ask for 640x480 (menus are 320x200, split screen 640x240 or 640x238); not only
 * with the high-resolution flag set, as the race's set-up clears it for its first frame
 * (0x426014). Returns height << 16 | width. */
uint32_t __cdecl brr_view_size(uint32_t width, uint32_t height)
{
    static int logged_w, logged_h;
    int out_w = screen_w, out_h = screen_h, w, h;  /* races chosen for the screen's height */
    if (brr_modern_window && window_w > 0 && window_h > 0) {  /* or the window's: cnc-ddraw only enlarges */
        out_w = window_w;
        out_h = window_h;
    }
    brr_proj_scale = 0;
    if ((width & 0xFFFF) != 640 || (height & 0xFFFF) != 480 || SPLIT_SCREEN || out_w <= 0 || view_held)
        return (height & 0xFFFF) << 16 | (width & 0xFFFF);
    race_view_size(out_w, out_h, &w, &h);
    if (w > brr_view_max_w || h > brr_view_max_h) {  /* the engine's buffers were made for this */
        w = brr_view_max_w;
        h = brr_view_max_h;
    }
    race_w = w;
    race_h = h;
    /* widescreen: the scale of the 4:3 view as tall (width x 256 / 320 with width = height x 4 / 3),
     * so the picture shows as much from top to bottom as a 4:3 one, and more at the sides */
    if (brr_widescreen && w * 3 != h * 4)
        brr_proj_scale = h * 16 / 15;
    if (w != logged_w || h != logged_h) {
        logged_w = w;
        logged_h = h;
        brr_log("render: Max: races drawn at %dx%d instead of 640x480, for the %dx%d %s", w, h,
                out_w, out_h, out_w == screen_w && out_h == screen_h ? "screen" : "window");
    }
    return (uint32_t)h << 16 | (uint32_t)w;
}

/* Site landscape_width (landscape_setup 0x41CC80, through thunks.asm): the visible ground's
 * half-width at its far edge, less one, for the vision distance d: the game's 7d / 10 (q)
 * suits the 4:3 view; a widescreen view needs it wider by its shape against 4:3. The shape is
 * drawn into a 64x64 grid around the camera, so its far corners must stay within 31 cells
 * (d^2 + width^2 <= 961, as at the game's maximum of 25 with 4:3: KNOWLEDGE, draw distance);
 * where they wouldn't, the width stops there and the farthest corners go undrawn. */
int __cdecl brr_landscape_width(int q)
{
    static int logged_w, logged_d;
    int d = DETAIL_LEVEL, w, cap;
    if (!brr_widescreen || !race_w || VIEW_W != race_w || VIEW_H != race_h || race_w * 3 <= race_h * 4
        || d < 1 || d > 30)
        return q;
    w = 7 * d * race_w * 3 / (10 * race_h * 4);
    for (cap = 0; d * d + (cap + 2) * (cap + 2) <= 961; cap++)  /* the largest width - 1 that fits */
        ;
    if (w > cap)
        w = cap;
    if (w < q)
        w = q;
    if (w != logged_w || d != logged_d) {
        static int logs;
        logged_w = w;
        logged_d = d;
        if (logs++ < 6)
            brr_log("landscape: widescreen: the visible ground %d cells either side at vision %d (4:3: %d)%s",
                    w + 1, d, q + 1, w == cap && 7 * d * race_w * 3 / (10 * race_h * 4) > cap
                    ? ", the most the 64x64 grid holds" : "");
    }
    return w;
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

/* ---- DirectDraw at the picture's own size ---- */

/* the game's dd_set_mode (0x42D044; EAX width, EDX height, BL 0: no window swap), thunks.asm */
void __cdecl brr_call_dd_set_mode(int width, int height);

/* Site dd_setup_entry: the DirectDraw set-up's mode, given its windowed flag. Returns the
 * mode as height << 16 | width, or 0 to keep the game's (its own window: the original
 * renderer). The first set-up is for the menus' 320x200 (the first screen is a menu). */
uint32_t __cdecl brr_render_mode(uint32_t windowed)
{
    static int logged;
    if (!brr_renderer_active || (windowed & 0xFF)) {
        mode_w = mode_h = 0;
        return 0;
    }
    mode_w = want_w > 0 ? want_w : MENU_W;
    mode_h = want_h > 0 ? want_h : MENU_H;
    if (brr_widescreen) {  /* one size for every picture: the race's */
        mode_w = brr_view_max_w;
        mode_h = brr_view_max_h;
    }
    if (logged++ < 12)
        brr_log("render: DirectDraw at %dx%d, the picture's own size%s (screen 0x%02X; the game's display %ldx%ld)",
                mode_w, mode_h, brr_modern_window ? ", in cnc-ddraw's window" : "",
                (unsigned)*(volatile uint16_t *)0x4B13C0, (long)DISPLAY_W, (long)DISPLAY_H);
    return (uint32_t)mode_h << 16 | (uint32_t)mode_w;
}

/* Before a present: DirectDraw re-created at the picture's size when it has changed */
static void follow_picture_size(void)
{
    const uint8_t *pixels;
    int w, h, pitch;
    if (switching || !game_picture(&pixels, &w, &h, &pitch))
        return;
    if (brr_widescreen) {
        w = brr_view_max_w;
        h = brr_view_max_h;
    }
    if (w == mode_w && h == mode_h)
        return;
    switching = 1;
    want_w = w;
    want_h = h;
    mode_switches++;
    brr_call_dd_set_mode(w, h);
    switching = 0;
}

static uint8_t *game_source;
static int32_t game_frame_w, game_frame_h;

/* Called on entry to the present (0x42CE78), before the game copies its frame. Returns 1
 * when it has presented the frame itself (full colour, full screen). */
/* ---- the display list's fill, for the clock log (KNOWLEDGE: the display list) ----
 * Read after each race frame is drawn (nothing resets them until the next view): the node pool
 * (objects, vehicles, effects: 999 nodes of 17 bytes from 0x5268D1, full at 0x52AB28) and the
 * landscape records (0xBD bytes from 0x4CA430; the walk stops at 0x523980, 1,936 records). */
#define NODE_NEXT    (*(volatile uint32_t *)0x52D6F0)
#define RECORD_NEXT  (*(volatile uint32_t *)0x52D6F4)
static uint32_t list_frames, list_full_frames, nodes_max, records_max, records_sum, nodes_sum;

/* the buffers in use: the game's, or ours (sites node_first and record_rewind, distance.c) */
static void list_buffers(uint32_t *node_base, uint32_t *node_count, uint32_t *record_base, uint32_t *record_count)
{
    int ours = brr_record_limit != 0x523980u;
    *node_base = ours ? (uint32_t)brr_nodes : 0x5268D1u;
    *node_count = ours ? BRR_NODES : 999;
    *record_base = ours ? (uint32_t)brr_records : 0x4CA430u;
    *record_count = ours ? BRR_RECORDS - 64 : 1936;
}

static void display_list_sample(void)
{
    uint32_t nodes, records, node_base, node_count, record_base, record_count;
    list_buffers(&node_base, &node_count, &record_base, &record_count);
    if (*(volatile uint16_t *)0x4B13C0 != 0x80 || NODE_NEXT < node_base || RECORD_NEXT < record_base)
        return;
    nodes = (NODE_NEXT - node_base) / 17;
    records = (RECORD_NEXT - record_base) / 0xBD;
    list_frames++;
    nodes_sum += nodes;
    records_sum += records;
    if (nodes > nodes_max) nodes_max = nodes;
    if (records > records_max) records_max = records;
    if (nodes + 1 >= node_count || records >= record_count)
        list_full_frames++;
}

/* thunks.asm counts what the game's 32-bit maths would have got wrong (precise_3d) */
uint32_t brr_count_project, brr_count_backface, brr_count_clip, brr_count_unlit;

/* Site shading_threshold: the distance in squares inside which display_list_draw lights objects
 * smoothly (vertex normals), flat (face normals) beyond: the game's clamp(vision - 9, 3, 8), or
 * every object smooth, or every object flat ([display] object_shading). */
int __cdecl brr_shading_threshold(void)
{
    int d = DETAIL_LEVEL - 9;
    if (cfg.object_shading == 1)
        return 4096;
    if (cfg.object_shading == 2)
        return -4096;
    return d < 3 ? 3 : d > 8 ? 8 : d;
}

void brr_display_list_report(void)
{
    if (brr_count_project || brr_count_backface || brr_count_clip)
        brr_log("precise 3D: the game's 32-bit maths would have got %lu projected corners, %lu faces' facing and "
                "%lu screen-edge clips wrong", (unsigned long)brr_count_project, (unsigned long)brr_count_backface,
                (unsigned long)brr_count_clip);
    if (brr_count_unlit)
        brr_log("object lighting: %lu times the game would have left far objects unlit after an effect",
                (unsigned long)brr_count_unlit);
    brr_count_project = brr_count_backface = brr_count_clip = brr_count_unlit = 0;
    if (list_frames) {
        uint32_t node_base, node_count, record_base, record_count;
        list_buffers(&node_base, &node_count, &record_base, &record_count);
        brr_log("display list: %lu race frames: object nodes %lu on average, %lu at most (of %lu), landscape "
                "records %lu on average, %lu at most (of %lu); a buffer full in %lu frames",
                (unsigned long)list_frames, (unsigned long)(nodes_sum / list_frames), (unsigned long)nodes_max,
                (unsigned long)node_count, (unsigned long)(records_sum / list_frames), (unsigned long)records_max,
                (unsigned long)record_count, (unsigned long)list_full_frames);
    }
    list_frames = list_full_frames = nodes_max = records_max = records_sum = nodes_sum = 0;
}

static int64_t present_mark;  /* for the clock log's presenting time */
extern int64_t brr_prof_present;

int __cdecl brr_render_before_present(void)
{
    const uint8_t *out;
    present_mark = brr_qpc();
    game_source = NULL;
    display_list_sample();
    brr_shade_census_report();
    brr_debug_every_frame();
    if (!brr_renderer_active || *GAME_WINDOWED || !mode_w || switching)
        return 0;  /* the game's own window: display.c */
    follow_picture_size();
    if (!DD_READY || !mode_w)
        return 0;
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
    brr_prof_present += brr_qpc() - present_mark;
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
