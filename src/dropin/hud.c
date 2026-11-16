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
 * Each HUD element has a layer of its own (site sprite_scaled_entry: the caller says which):
 * the map, the place, the nitro, the laps, the messages (damage, FINAL LAP and the like), the
 * ticker and the playback icon. Each sprite that defines an element's edges (not the map's
 * moving dots) adds its visible pixels to the element's box. When the race picture is
 * composed (render.c), each layer is laid over it enlarged by whole numbers (with
 * [display] hud_size=small, the default, about a 400th of the picture's height; large, a
 * 200th, as the 1996 low resolution filled the screen; with hud_aspect43=1 the columns
 * nearest the 4:3 monitor's shape, pixels 1.2 times taller than wide), and placed by its box:
 * the map at the top left and the place at the top right, the same margin from each edge; the
 * nitro and the laps right-aligned with the place below it, evenly spaced; the messages and
 * the ticker centred at the top; the playback icon at the bottom left. The 3D view itself
 * never holds the HUD. */
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
#define MARGIN 8                   /* HUD pixels from the picture's edges (to the art's light pixels) */
#define GAP 4                      /* HUD pixels between the place, the nitro and the laps */

enum { MAP, PLACE, NITRO, LAPS, MESSAGE, TICKER, ICON, OTHER, LAYERS };

struct box { int x0, y0, x1, y1; };  /* HUD pixels, x1 and y1 past the end; empty if x1 <= x0 */

int brr_hud_active;                /* decided at patch time */
int brr_hud_drawing;               /* inside hud_draw: thunks.asm's sprite hook reads it */
uint32_t brr_orig_hud_draw = 0x423658, brr_orig_hud_icon = 0x4236F0;
uint32_t brr_sprite_scaled_continue = 0x426F96;

static uint8_t *layers;
static int touched[LAYERS];
static struct box boxes[LAYERS];
static int place_height;           /* the tallest place sprite, so the nitro and laps don't move */
static int valid;                  /* the layers hold the HUD of the race picture below */
static const uint8_t *for_picture; /* that picture: the view buffer, its size, the screen */
static int for_w, for_h;
static unsigned for_screen;
static int16_t saved_w, saved_h;
static int32_t saved_hires;
static uint8_t *saved_target;

/* Each of sprite_draw_scaled's HUD callers, by its return address (the CALL's address + 5;
 * WINRACE.EXE v2.25): its element, and whether it defines the element's edges. Anything else
 * goes to OTHER, drawn where the game put it. */
static const struct { uint32_t ret; uint8_t element, edges; } callers[] = {
    {0x422F81, PLACE, 1}, {0x422FB7, PLACE, 1},   /* hud_draw_position: the place and its suffix */
    {0x42307C, NITRO, 1}, {0x4230A7, NITRO, 1},   /* hud_draw_nitro: bottles, or the bar's frame (its bar follows) */
    {0x423180, MAP, 1}, {0x4231E0, MAP, 0},       /* hud_draw_map: the map, and the vehicles' dots on it */
    {0x42345A, MESSAGE, 1},                       /* hud_draw_messages: FINAL LAP, MISSED CHECK, WRONG WAY */
    {0x423497, LAPS, 1}, {0x4234B0, LAPS, 1}, {0x4234D2, LAPS, 1},  /* hud_draw_messages: lap N / laps */
    {0x423250, MESSAGE, 1}, {0x423276, MESSAGE, 1}, {0x4232B8, MESSAGE, 1}, {0x4232F0, MESSAGE, 1},
    {0x423320, MESSAGE, 1},                       /* hud_draw_damage: DAMAGE: N% */
    {0x42361A, TICKER, 1},                        /* credits_tick: the ticker's box (its text follows) */
    {0x423757, ICON, 1},                          /* hud_draw_playback_icon */
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
            "whole numbers (%s, %s) and laid out by element ([display] hud=lowres)",
            cfg.hud_large ? "large" : "small", cfg.hud_aspect43 ? "the 4:3 monitor's shape" : "square pixels");
}

static uint8_t *layer_of(int element)
{
    return layers + (size_t)element * HUD_PIXELS;
}

/* Before hud_draw (first = 1, a new frame's HUD) or hud_draw_playback_icon (first = 0) in
 * render_view: the 2D routines pointed at our OTHER layer, at 320x200, low resolution.
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
        for (i = 0; i < LAYERS; i++) {
            if (touched[i]) {
                memset(layer_of(i), 0, HUD_PIXELS);
                touched[i] = 0;
            }
            boxes[i].x0 = boxes[i].y0 = HUD_W + HUD_H;
            boxes[i].x1 = boxes[i].y1 = -1;
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
    VIEW_TARGET = layer_of(OTHER);
    touched[OTHER] = 1;  /* anything drawn before a sprite (text) lands here */
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

/* A sprite's light pixels (not its dark outline or shadow, whose thickness differs from one
 * piece of art to another, so the spacing looks even), as sprite_draw_scaled draws it into a
 * 320-wide view: header (16-bit width and height, four bytes; byte 5 bit 7 = drawn for 640,
 * halved here), then width x height bytes, colour 0 transparent. Light: a channel of its
 * colour in the game's own palette (0x44A008, 6 bits a channel; not the one on screen, which
 * is black while the race fades in, as the HUD is first drawn) at least 32 of 63 (the
 * outlines are about 16; the art white, red, yellow); if none is, every pixel counts. Kept
 * for the sprites seen. */
static int light_colour(uint8_t v)
{
    const uint8_t *entry = (const uint8_t *)0x44A008 + (size_t)v * 3;
    return entry[0] >= 32 || entry[1] >= 32 || entry[2] >= 32;
}

static struct box sprite_box(const uint8_t *sprite)
{
    static struct { const uint8_t *sprite; struct box box; } cache[64];
    static int count;
    struct box b;
    int w = *(const int16_t *)sprite, h = *(const int16_t *)(sprite + 2), half = (sprite[5] & 0x80) != 0;
    int x, y, i;
    for (i = 0; i < count; i++)
        if (cache[i].sprite == sprite)
            return cache[i].box;
    for (i = 0; i < 2; i++) {  /* the light pixels; failing those, any */
        b.x0 = w;
        b.y0 = h;
        b.x1 = b.y1 = 0;
        for (y = 0; y < h && w > 0 && w < 1024 && h < 1024; y++)
            for (x = 0; x < w; x++) {
                uint8_t v = sprite[8 + y * w + x];
                if (v && (i || light_colour(v))) {
                    if (x < b.x0) b.x0 = x;
                    if (y < b.y0) b.y0 = y;
                    if (x + 1 > b.x1) b.x1 = x + 1;
                    if (y + 1 > b.y1) b.y1 = y + 1;
                }
            }
        if (b.x1 > b.x0)
            break;
    }
    if (half) {
        b.x0 /= 2;
        b.y0 /= 2;
        b.x1 = (b.x1 + 1) / 2;
        b.y1 = (b.y1 + 1) / 2;
    }
    if (count < 64) {
        cache[count].sprite = sprite;
        cache[count].box = b;
        count++;
    }
    return b;
}

/* The tallest of the place sprites (1-7, a place each, and the suffixes 10-13: hud_draw_position),
 * so that the nitro and the laps below the place stay put whatever place is shown */
static void measure_place(const uint8_t *bank)
{
    int i, tallest = 0;
    for (i = 1; i <= 13; i++) {
        struct box b;
        if (i == 8 || i == 9)
            continue;
        b = sprite_box(bank + *(const int32_t *)(bank + i * 4 + 2));
        if (b.y1 - b.y0 > tallest)
            tallest = b.y1 - b.y0;
    }
    place_height = tallest;
}

/* sprite_draw_scaled's entry while the HUD draws (thunks.asm): the sprite (bank, index, at x,
 * y in 640-wide units) goes to its caller's element's layer; a rectangle or text after it
 * follows. Sprites that define an element's edges add their visible pixels to its box. */
void __cdecl brr_hud_sprite(uint32_t ret, const uint8_t *bank, int32_t index, int32_t x, int32_t y)
{
    int element = OTHER, edges = 0;
    size_t i;
    for (i = 0; i < sizeof callers / sizeof callers[0]; i++)
        if (callers[i].ret == ret) {
            element = callers[i].element;
            edges = callers[i].edges;
            break;
        }
    VIEW_TARGET = layer_of(element);
    touched[element] = 1;
    if (edges && bank && index >= 0 && index < 256) {
        const uint8_t *sprite = bank + *(const int32_t *)(bank + index * 4 + 2);
        struct box b = sprite_box(sprite);
        int left = x * HUD_W / 640, top = y * HUD_W / 640;  /* as sprite_draw_scaled places it */
        struct box *e = &boxes[element];
        if (b.x1 > b.x0 && b.y1 > b.y0) {
            if (left + b.x0 < e->x0) e->x0 = left + b.x0;
            if (top + b.y0 < e->y0) e->y0 = top + b.y0;
            if (left + b.x1 > e->x1) e->x1 = left + b.x1;
            if (top + b.y1 > e->y1) e->y1 = top + b.y1;
        }
        if (element == PLACE && !place_height)
            measure_place(bank);
    }
}

/* The scale: kx x ky screen pixels per HUD pixel */
static void hud_scale(int h, int w, int *kx, int *ky)
{
    int y = cfg.hud_large ? (h + HUD_H / 2) / HUD_H : (h + HUD_H) / (2 * HUD_H);
    if (!cfg.hud_large && y < 2 && h >= 480)
        y = 2;  /* 640x480 keeps the 1996 low resolution's size */
    if (y < 1)
        y = 1;
    *ky = y;
    *kx = cfg.hud_aspect43 ? (y * 5 + 3) / 6 : y;
    if (*kx * HUD_W > w)
        *kx = w / HUD_W;
    if (*kx < 1)
        *kx = 1;
}

/* Where each element's layer goes: its origin in the picture's pixels */
static void layout(int w, int h, int kx, int ky, int *ox, int *oy)
{
    const struct box *b;
    int i, right_top;
    for (i = 0; i < LAYERS; i++) {  /* by default, where the game put it, the layer centred */
        ox[i] = (w - HUD_W * kx) / 2;
        oy[i] = 0;
    }
    b = &boxes[MAP];
    if (b->x1 > b->x0) {
        ox[MAP] = (MARGIN - b->x0) * kx;
        oy[MAP] = (MARGIN - b->y0) * ky;
    }
    right_top = MARGIN;
    b = &boxes[PLACE];
    if (b->x1 > b->x0) {
        ox[PLACE] = w - (MARGIN + b->x1) * kx;
        oy[PLACE] = (MARGIN - b->y0) * ky;
        right_top = MARGIN + (place_height > b->y1 - b->y0 ? place_height : b->y1 - b->y0) + GAP;
    }
    b = &boxes[NITRO];
    if (b->x1 > b->x0) {
        ox[NITRO] = w - (MARGIN + b->x1) * kx;
        oy[NITRO] = (right_top - b->y0) * ky;
        right_top += b->y1 - b->y0 + GAP;
    }
    b = &boxes[LAPS];
    if (b->x1 > b->x0) {
        ox[LAPS] = w - (MARGIN + b->x1) * kx;
        oy[LAPS] = (right_top - b->y0) * ky;
    }
    for (i = MESSAGE; i <= TICKER; i++) {  /* centred on what's drawn, at the top */
        b = &boxes[i];
        if (b->x1 > b->x0) {
            ox[i] = (w - (b->x1 - b->x0) * kx) / 2 - b->x0 * kx;
            oy[i] = (MARGIN - b->y0) * ky;
        }
    }
    b = &boxes[ICON];
    if (b->x1 > b->x0) {
        ox[ICON] = (MARGIN - b->x0) * kx;
        oy[ICON] = h - (MARGIN + b->y1) * ky;
    }
}

/* Lay the HUD over a composed race picture. origin, w x h: the game's picture (it must be the
 * one the HUD was drawn for, unless again = 1: a fade's step re-showing the last picture);
 * it was composed into out (rows out_pitch pixels apart, out_w x out_h) at px, py, each of its
 * pixels sx x sy. colours: the palette in 32 bits, or NULL for an 8-bit out. 1 if laid. */
int brr_hud_overlay(const uint8_t *origin, int w, int h, void *out, int out_w, int out_h, int out_pitch,
                    int px, int py, int sx, int sy, const uint32_t *colours, int again)
{
    static int logged_w, logged_h;
    int kx, ky, i, ox[LAYERS], oy[LAYERS];
    if (!brr_hud_active || !valid)
        return 0;
    if (!again && (origin != for_picture || w != for_w || h != for_h || SCREEN != for_screen))
        return 0;
    hud_scale(h, w, &kx, &ky);
    layout(w, h, kx, ky, ox, oy);
    if (w != logged_w || h != logged_h) {
        static const char *const names[LAYERS] = {"map", "place", "nitro", "laps", "message", "ticker", "icon", "other"};
        logged_w = w;
        logged_h = h;
        brr_log("hud: on the %dx%d race picture: the 320x200 HUD's pixels %dx%d, laid out by element", w, h, kx, ky);
        for (i = 0; i < LAYERS; i++)
            if (boxes[i].x1 > boxes[i].x0)
                brr_log("hud: %s: drawn at %d,%d-%d,%d in the game's layout; placed at %d,%d in the picture", names[i],
                        boxes[i].x0, boxes[i].y0, boxes[i].x1, boxes[i].y1, ox[i] + boxes[i].x0 * kx,
                        oy[i] + boxes[i].y0 * ky);
    }
    for (i = 0; i < LAYERS; i++) {
        const uint8_t *layer = layer_of(i);
        int x, y;
        if (!touched[i])
            continue;
        for (y = 0; y < HUD_H; y++) {
            const uint8_t *p = layer + (size_t)y * HUD_W;
            int gy = oy[i] + y * ky, rows, fy, r;
            if (gy >= h || gy + ky <= 0)
                continue;
            for (x = 0; x < HUD_W; x++) {
                int gx, cols, fx, c, top, left;
                uint8_t v = p[x];
                if (!v)
                    continue;
                gx = ox[i] + x * kx;
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
