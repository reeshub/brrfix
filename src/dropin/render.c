/* Phase 3c, the renderer: menus that fill the picture (SPEC Phase 3c, milestone 2).
 * Menus always draw a 320x200 picture at the start of the game's buffer (view record
 * 0x44BE60 at 320x200); at 640x480 the game doubled it into rows 40-439 (0x4262B4, called
 * only from 0x426358), leaving black bands above and below. With [display] menu_scaling=fill
 * that call is removed (site menu_double_call) and the present (brr_hook_present) shows our
 * own picture instead: the 320x200 menu scaled to the whole frame by repeating pixels. The
 * game's buffer is left as drawn, so a frame shown again (WM_PAINT) is scaled again from the
 * same picture. The menu's mouse position is mapped back the same way (site menu_mouse).
 * Docs: KNOWLEDGE "The renderer (Phase 3c)". */
#include "brrfix.h"
#include <string.h>

#define VIEW_W         (*(volatile int16_t *)0x44BE60)
#define VIEW_H         (*(volatile int16_t *)0x44BE62)
#define VIEW_BUFFER    (*(uint8_t *volatile *)0x44BE68)
#define RACE_VIEW_SET  (*(volatile uint8_t *)0x44BE8C)  /* set by race_enter, cleared on entering any screen */
#define DISPLAY_W      (*(volatile int32_t *)0x44A618)
#define DISPLAY_H      (*(volatile int32_t *)0x44A61C)
#define FRAME_W        (*(volatile int32_t *)0x44BFB8)
#define FRAME_H        (*(volatile int32_t *)0x44BFBC)
#define PRESENT_SOURCE (*(uint8_t *volatile *)0x52D738)
#define MOUSE_X        (*(volatile int16_t *)0x44BBEC)  /* in the display's pixels */
#define MOUSE_Y        (*(volatile int16_t *)0x44BBEE)

#define MENU_W 320
#define MENU_H 200

int brr_menu_fill_active;  /* set when site menu_double_call is patched */

static uint8_t *picture;
static size_t picture_size;
static int *columns, columns_w;
static uint8_t *game_source;  /* the game's buffer while our picture is shown */

static int scale_menu(const uint8_t *menu, int w, int h)
{
    int x, y, last = -1;
    size_t need = (size_t)w * (size_t)h;
    if (need > picture_size) {
        uint8_t *bigger = (uint8_t *)HeapAlloc(GetProcessHeap(), 0, need);
        if (!bigger)
            return 0;
        if (picture)
            HeapFree(GetProcessHeap(), 0, picture);
        picture = bigger;
        picture_size = need;
    }
    if (columns_w != w) {
        int *table = (int *)HeapAlloc(GetProcessHeap(), 0, sizeof(int) * (size_t)w);
        if (!table)
            return 0;
        if (columns)
            HeapFree(GetProcessHeap(), 0, columns);
        columns = table;
        for (x = 0; x < w; x++)
            columns[x] = x * MENU_W / w;
        columns_w = w;
    }
    for (y = 0; y < h; y++) {
        int source_row = y * MENU_H / h;
        uint8_t *row = picture + (size_t)y * (size_t)w;
        if (source_row == last) {
            memcpy(row, row - w, (size_t)w);
            continue;
        }
        {
            const uint8_t *in = menu + (size_t)source_row * MENU_W;
            for (x = 0; x < w; x++)
                row[x] = in[columns[x]];
        }
        last = source_row;
    }
    return 1;
}

/* Called on entry to the present (0x42CE78), before the game copies its frame. */
void __cdecl brr_render_before_present(void)
{
    static int logged, failed;
    uint8_t *source = PRESENT_SOURCE;
    int w = FRAME_W, h = FRAME_H;
    game_source = NULL;
    if (!brr_menu_fill_active || !source || source != VIEW_BUFFER || VIEW_W != MENU_W || VIEW_H != MENU_H
        || RACE_VIEW_SET || w <= MENU_W || h <= MENU_H)
        return;
    if (!scale_menu(source, w, h)) {
        if (!failed++)
            brr_log("render: no memory for a %dx%d menu picture", w, h);
        return;
    }
    game_source = source;
    PRESENT_SOURCE = picture;
    if (!logged++)
        brr_log("render: menus scaled from 320x200 to fill the %dx%d frame", w, h);
}

void __cdecl brr_render_after_present(void)
{
    if (game_source) {
        PRESENT_SOURCE = game_source;
        game_source = NULL;
    }
}

/* Site menu_mouse (in 0x4274FC, only when the race view isn't set): the menu's mouse
 * position from the display's, the inverse of the scaling above. The original mapped only
 * at 640x480, to its doubled picture: x / 2, (y - 40) / 2. Returns x | y << 16. */
uint32_t __cdecl brr_menu_mouse(void)
{
    int x = MOUSE_X, y = MOUSE_Y, w = DISPLAY_W, h = DISPLAY_H;
    if (w > MENU_W && h > MENU_H) {
        x = x * MENU_W / w;
        y = y * MENU_H / h;
        if (x < 0) x = 0;
        if (x > MENU_W - 1) x = MENU_W - 1;
        if (y < 0) y = 0;
        if (y > MENU_H - 1) y = MENU_H - 1;
    }
    return (uint32_t)(uint16_t)x | (uint32_t)(uint16_t)y << 16;
}
