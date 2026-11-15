/* The race HUD on layers of its own (the Modern renderer; [display] hud=lowres, the default).
 *
 * The game draws its HUD (the map, place, nitro, damage, messages and laps; the attract mode's
 * credits ticker; a recording's playback icon) with hud_draw (0x423658) and
 * hud_draw_playback_icon (0x4236F0) at the end of render_view, straight into the 3D view,
 * scaled by the view's width: at high resolution from GAMESVGA.SPR, the 1996 art enlarged
 * about 1.9 times. Here those two calls draw instead into 320x200 layers of ours, with the
 * view's size set to 320x200 and the high-resolution flag clear for the duration, so the game
 * draws exactly the HUD of its 320x200 mode (GAMEVGA.SPR, the 64x64 map), with its own code.
 * Its 2D routines use only the view's width and height (0x44BE60/62), the high-resolution flag
 * (0x44BE5C) and the buffer at 0x542030 (KNOWLEDGE, "The HUD on its own layers").
 *
 * Each HUD sprite goes to the layer of its corner (site sprite_scaled_entry: the caller says
 * which): the map left, the place, nitro and laps right, damage and the messages centred; top
 * or bottom by its height. When the race picture is composed (render.c), each layer is laid
 * over it enlarged by whole numbers (as the menus were: as many rows as the picture's height
 * is 200s, the columns nearest the 4:3 shape) and pinned to its corner, so a wider view leaves
 * the HUD unstretched. The 3D view itself never holds the HUD. */
#include "brrfix.h"
#include <string.h>

#define VIEW_W        (*(volatile int16_t *)0x44BE60)
#define VIEW_H        (*(volatile int16_t *)0x44BE62)
#define VIEW_HIRES    (*(volatile int32_t *)0x44BE5C)
#define VIEW_TARGET   (*(uint8_t *volatile *)0x542030)  /* where the 2D routines draw */
#define SPLIT_SCREEN  (*(volatile int32_t *)0x44BE74)
#define SCREEN        (*(volatile uint16_t *)0x4B13C0)
#define HUD_W 320
#define HUD_H 200
#define HUD_PIXELS (HUD_W * HUD_H)

enum { LEFT, CENTRE, RIGHT };      /* across */
enum { TOP, BOTTOM };              /* down: layer = across + 3 x down */
#define LAYERS 6

int brr_hud_active;                /* decided at patch time */
int brr_hud_drawing;               /* inside hud_draw: thunks.asm's sprite hook reads it */
uint32_t brr_orig_hud_draw = 0x423658, brr_orig_hud_icon = 0x4236F0;
uint32_t brr_sprite_scaled_continue = 0x426F96;

static uint8_t *layers;
static int touched[LAYERS];
static int valid;                  /* the layers hold the HUD of the race picture below */
static const uint8_t *for_picture; /* that picture: the view buffer, its size, the screen */
static int for_w, for_h;
static unsigned for_screen;
static int16_t saved_w, saved_h;
static int32_t saved_hires;
static uint8_t *saved_target;

/* Which corner each of sprite_draw_scaled's HUD callers draws in, by its return address
 * (the CALL's address + 5; WINRACE.EXE v2.25). Anything else is centred. */
static const struct { uint32_t ret; uint8_t across; } callers[] = {
    {0x422F81, RIGHT}, {0x422FB7, RIGHT},   /* hud_draw_position: the place and its suffix */
    {0x42307C, RIGHT}, {0x4230A7, RIGHT},   /* hud_draw_nitro: bottles, or the bar's frame (its bar follows) */
    {0x423180, LEFT}, {0x4231E0, LEFT},     /* hud_draw_map: the map and the vehicles' dots */
    {0x42345A, CENTRE},                     /* hud_draw_messages: FINAL LAP, MISSED CHECK, WRONG WAY */
    {0x423497, RIGHT}, {0x4234B0, RIGHT}, {0x4234D2, RIGHT},  /* hud_draw_messages: lap N / laps */
    {0x423250, CENTRE}, {0x423276, CENTRE}, {0x4232B8, CENTRE}, {0x4232F0, CENTRE},
    {0x423320, CENTRE},                     /* hud_draw_damage: DAMAGE: N% */
    {0x42361A, CENTRE},                     /* credits_tick: the ticker's box (its text follows) */
    {0x423757, LEFT},                       /* hud_draw_playback_icon */
};

void brr_hud_setup(void)
{
    brr_hud_active = brr_renderer_active && cfg.hud_lowres;
    if (!brr_hud_active)
        return;
    layers = (uint8_t *)VirtualAlloc(NULL, LAYERS * HUD_PIXELS, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!layers) {
        brr_log("hud: out of memory; the game's own HUD");
        brr_hud_active = 0;
        return;
    }
    brr_log("hud: the race HUD drawn from the game's 320x200 art on layers of its own, enlarged by "
            "whole numbers and pinned to the picture's corners ([display] hud=lowres)");
}

/* Before hud_draw (first = 1, a new frame's HUD) or hud_draw_playback_icon (first = 0) in
 * render_view: the 2D routines pointed at our centre-top layer, at 320x200, low resolution.
 * Split screen and the race redrawn for a screenshot (PCX) keep the game's own HUD. */
void __cdecl brr_hud_begin(int first)
{
    int i;
    if (!brr_hud_active || brr_screenshot_drawing)
        return;
    if (SPLIT_SCREEN) {
        valid = 0;
        return;
    }
    if (first) {
        for (i = 0; i < LAYERS; i++)
            if (touched[i]) {
                memset(layers + (size_t)i * HUD_PIXELS, 0, HUD_PIXELS);
                touched[i] = 0;
            }
        for_picture = VIEW_TARGET;
        for_w = VIEW_W;
        for_h = VIEW_H;
        for_screen = SCREEN;
        valid = 0;
    } else if (!valid) {
        return;
    }
    saved_w = VIEW_W;
    saved_h = VIEW_H;
    saved_hires = VIEW_HIRES;
    saved_target = VIEW_TARGET;
    VIEW_W = HUD_W;
    VIEW_H = HUD_H;
    VIEW_HIRES = 0;
    VIEW_TARGET = layers + (size_t)(CENTRE + 3 * TOP) * HUD_PIXELS;
    touched[CENTRE + 3 * TOP] = 1;  /* anything drawn before a sprite (text) lands here */
    brr_hud_drawing = 1;
}

void __cdecl brr_hud_end(void)
{
    if (!brr_hud_drawing)
        return;
    VIEW_W = saved_w;
    VIEW_H = saved_h;
    VIEW_HIRES = saved_hires;
    VIEW_TARGET = saved_target;
    brr_hud_drawing = 0;
    valid = 1;
}

/* sprite_draw_scaled's entry while the HUD draws (thunks.asm): the sprite (its top at y, in
 * 640-wide units) goes to its caller's corner's layer; a rectangle or text after it follows. */
void __cdecl brr_hud_sprite(uint32_t ret, int32_t y)
{
    int across = CENTRE, layer;
    size_t i;
    for (i = 0; i < sizeof callers / sizeof callers[0]; i++)
        if (callers[i].ret == ret) {
            across = callers[i].across;
            break;
        }
    layer = across + 3 * (y >= 240 ? BOTTOM : TOP);
    VIEW_TARGET = layers + (size_t)layer * HUD_PIXELS;
    touched[layer] = 1;
}

/* Lay the HUD over a composed race picture. origin, w x h: the game's picture (it must be the
 * one the HUD was drawn for, unless again = 1: a fade's step re-showing the last picture);
 * it was composed into out (rows out_pitch pixels apart, out_w x out_h) at px, py, each of its
 * pixels sx x sy. colours: the palette in 32 bits, or NULL for an 8-bit out. 1 if laid. */
int brr_hud_overlay(const uint8_t *origin, int w, int h, void *out, int out_w, int out_h, int out_pitch,
                    int px, int py, int sx, int sy, const uint32_t *colours, int again)
{
    static int logged_w, logged_h;
    int kx, ky, i;
    if (!brr_hud_active || !valid)
        return 0;
    if (!again && (origin != for_picture || w != for_w || h != for_h || SCREEN != for_screen))
        return 0;
    /* as many rows as the picture has 200s (rounded), the columns nearest the 1996 pixels'
     * shape (1.2 times taller than wide), and never wider than the picture */
    ky = (h + HUD_H / 2) / HUD_H;
    if (ky < 1)
        ky = 1;
    kx = (ky * 5 + 3) / 6;
    if (kx * HUD_W > w)
        kx = w / HUD_W;
    if (kx < 1)
        kx = 1;
    if (w != logged_w || h != logged_h) {
        logged_w = w;
        logged_h = h;
        brr_log("hud: on the %dx%d race picture: the 320x200 HUD enlarged %dx%d (%dx%d), pinned to its corners",
                w, h, kx, ky, HUD_W * kx, HUD_H * ky);
    }
    for (i = 0; i < LAYERS; i++) {
        const uint8_t *layer = layers + (size_t)i * HUD_PIXELS;
        int across = i % 3, down = i / 3, ox, oy, x, y;
        if (!touched[i])
            continue;
        ox = across == LEFT ? 0 : across == RIGHT ? w - HUD_W * kx : (w - HUD_W * kx) / 2;
        oy = down == TOP ? 0 : h - HUD_H * ky;
        for (y = 0; y < HUD_H; y++) {
            const uint8_t *p = layer + (size_t)y * HUD_W;
            int gy = oy + y * ky, rows, fy, r;
            if (gy >= h || gy + ky <= 0)
                continue;
            for (x = 0; x < HUD_W; x++) {
                int gx, cols, fx, c, top, left;
                uint8_t v = p[x];
                if (!v)
                    continue;
                gx = ox + x * kx;
                /* the block, clipped to the picture, in out's pixels */
                left = gx < 0 ? 0 : gx;
                cols = (gx + kx > w ? w : gx + kx) - left;
                top = gy < 0 ? 0 : gy;
                rows = (gy + ky > h ? h : gy + ky) - top;
                if (cols <= 0 || rows <= 0)
                    continue;
                fx = px + left * sx;
                fy = py + top * sy;
                cols *= sx;
                rows *= sy;
                if (fx + cols > out_w)
                    cols = out_w - fx;
                if (fy + rows > out_h)
                    rows = out_h - fy;
                for (r = 0; r < rows; r++) {
                    if (colours) {
                        uint32_t *o = (uint32_t *)out + (size_t)(fy + r) * (size_t)out_pitch + fx;
                        uint32_t value = colours[v];
                        for (c = 0; c < cols; c++)
                            o[c] = value;
                    } else {
                        memset((uint8_t *)out + (size_t)(fy + r) * (size_t)out_pitch + fx, v, (size_t)cols);
                    }
                }
            }
        }
    }
    return 1;
}
