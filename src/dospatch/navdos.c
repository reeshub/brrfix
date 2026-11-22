/* Joystick menu navigation: DOS's side (the shared core is ../dropin/menunav.c, compiled
 * here with BRR_DOS). The hooks are in thunks.asm: run_screen's CALL menu_hotspots_tick
 * (site menu_tick_call), the end of menu_mouse_update (menu_mouse_end) and menu_draw_group's
 * CALL menu_hotspot_draw (hotspot_draw_call). */
#include "../dropin/menunav.h"
#include "../dropin/joyclick.h"

static int click_ready;

/* SAMPLES\JOYCLICK.WAV, written from our copy the first time, so the game can play it as
 * it plays BEEP2 */
void nav_install_click(void)
{
    s32 handle = dos_open_read(NAV_CLICK_FILE);
    if (handle >= 0) {
        dos_close(handle);
        click_ready = 1;
        return;
    }
    handle = dos_create(NAV_CLICK_FILE);
    if (handle < 0)
        return;
    click_ready = dos_write(handle, joyclick_wav, JOYCLICK_WAV_SIZE) == JOYCLICK_WAV_SIZE;
    dos_close(handle);
}

void nav_play_click(void)
{
    if (click_ready)
        game_sound_play_sample(NAV_CLICK_NAME);
}

int nav_name_load(char *name, int size)
{
    s32 handle = dos_open_read(NAV_NAME_FILE), got;
    int n;
    if (handle < 0)
        return 0;
    got = dos_read(handle, name, (u32)(size - 1));
    dos_close(handle);
    if (got < 0)
        got = 0;
    for (n = 0; n < got && name[n] != '\r' && name[n] != '\n' && name[n]; n++)
        ;
    name[n] = 0;
    return n;
}

void nav_name_save(const char *name)
{
    s32 handle = dos_create(NAV_NAME_FILE);
    u32 n;
    if (handle < 0)
        return;
    for (n = 0; n < 8 && name[n]; n++)
        ;
    dos_write(handle, name, n);
    dos_write(handle, "\r\n", 2);
    dos_close(handle);
}

void nav_note(const char *what, int value)
{
    if (OPTION(OPT_LOG))  /* the logging build: BRRLOG.TXT */
        log_note(what, value);
}

/* The end of menu_mouse_update: the game has added the mouse's movement to the menu's
 * position and read its buttons (if a mouse driver answered) */
void brr_dos_menu_mouse(void)
{
    int moved = 0, buttons = 0;
    if (game_mouse_present) {
        moved = game_mouse_move_x != 0 || game_mouse_move_y != 0;
        buttons = game_mouse_buttons_now;
    }
    nav_mouse(moved, buttons, (short *)&game_menu_mouse_x, (short *)&game_menu_mouse_y,
              (short *)&game_mouse_buttons_now);
}
