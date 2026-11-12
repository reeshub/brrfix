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

#define MENU_W 320
#define MENU_H 200

int brr_renderer_active;               /* the new renderer's sites are patched */
uint32_t brr_dd_setup_continue = 0x42C599;
static int screen_w, screen_h;         /* the screen's own resolution */
static int mode_w, mode_h;             /* full screen: the mode we gave DirectDraw (0 in a window) */
static int final_w, final_h;           /* the last final picture's size, for the mouse */

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

/* Whether the new renderer can run; logs why not. cnc_ddraw: the loaded ddraw.dll is cnc-ddraw. */
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
    if (cfg.display_mode != DISPLAY_WINDOWED && !cnc_ddraw) {
        brr_log("render: the new renderer's full screen needs cnc-ddraw (ddraw.dll beside the game); "
                "the original renderer instead");
        return 0;
    }
    brr_log("render: the new renderer: the picture composed at %s and shown 1:1; menus %s",
            cfg.display_mode == DISPLAY_WINDOWED ? "the window's size" : "the screen's resolution",
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
    } else {
        *w = DISPLAY_W;
        *h = DISPLAY_H;
    }
    if (*w <= 0 || *h <= 0 || *w > 640 || *h > 480)
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

/* ---- full screen: DirectDraw at the screen's resolution ---- */

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
    if (logged++ < 4)
        brr_log("render: full screen: DirectDraw at %dx%d (the screen's own), shown 1:1; the game runs at %ldx%ld",
                mode_w, mode_h, (long)DISPLAY_W, (long)DISPLAY_H);
    return (uint32_t)mode_h << 16 | (uint32_t)mode_w;
}

static uint8_t *game_source;
static int32_t game_frame_w, game_frame_h;

/* Called on entry to the present (0x42CE78), before the game copies its frame. */
void __cdecl brr_render_before_present(void)
{
    const uint8_t *out;
    game_source = NULL;
    if (!brr_renderer_active || *GAME_WINDOWED || !mode_w)
        return;  /* in a window, display.c composes */
    out = brr_render_compose(mode_w, mode_h, mode_w);
    if (!out)
        return;
    game_source = PRESENT_SOURCE;
    game_frame_w = FRAME_W;
    game_frame_h = FRAME_H;
    PRESENT_SOURCE = (uint8_t *)out;
    FRAME_W = mode_w;
    FRAME_H = mode_h;
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
