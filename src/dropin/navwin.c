/* Joystick menu navigation: Windows' side (the shared core is menunav.c). The hooks: keys.c's
 * calls either side of menu_hotspots_tick (site menu_hotspots_call), render.c's menu mouse
 * (site menu_mouse, both renderers with the navigation on), thunks.asm's
 * brr_hook_hotspot_draw (site hotspot_draw_call) and brr_sound_sprite_* (sites
 * sound_button_*). The game runs in its own folder (Game\),
 * so the click and the last name are files there, shared with the DOS version. */
#include "menunav.h"
#include "joyclick.h"

uint32_t brr_orig_hotspot_draw = 0x427C7C;  /* menu_hotspot_draw */
uint32_t brr_orig_sprite_draw = 0x426D48;   /* sprite_draw */
static int click_ready, cursor_hidden;

/* Windows' pointer, in a window with the Original renderer (the game draws its own only full
 * screen, and none in a window): hidden while the joystick has the menus. ShowCursor counts,
 * as the game's own calls do (dd_setup, dd_release), so each hide is undone exactly once. */
void nav_pointer(int shown)
{
    if (!shown && !cursor_hidden && *GAME_WINDOWED) {
        ShowCursor(FALSE);
        cursor_hidden = 1;
    } else if (shown && cursor_hidden) {
        ShowCursor(TRUE);
        cursor_hidden = 0;
    }
}

/* SAMPLES\JOYCLICK.WAV, written from our copy when it isn't there (or isn't ours), so the
 * game can play it as it plays BEEP2 */
void nav_install_click(void)
{
    HANDLE f = CreateFileA(NAV_CLICK_FILE, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD written = 0;
    if (f != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(f, NULL);
        CloseHandle(f);
        if (size == JOYCLICK_WAV_SIZE) {
            click_ready = 1;
            return;
        }
    }
    f = CreateFileA(NAV_CLICK_FILE, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        brr_log("menu nav: couldn't write %s (error %lu): no click as the selection moves", NAV_CLICK_FILE,
                (unsigned long)GetLastError());
        return;
    }
    WriteFile(f, nav_click_wav, JOYCLICK_WAV_SIZE, &written, NULL);
    CloseHandle(f);
    click_ready = written == JOYCLICK_WAV_SIZE;
    brr_log("menu nav: wrote %s, the selection's click (%lu bytes)", NAV_CLICK_FILE, (unsigned long)written);
}

void nav_play_click(void)
{
    if (click_ready)
        brr_call_play_sample(NAV_CLICK_NAME);
}

int nav_name_load(char *name, int size)
{
    HANDLE f = CreateFileA(NAV_NAME_FILE, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD got = 0;
    int n;
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    if (!ReadFile(f, name, (DWORD)(size - 1), &got, NULL))
        got = 0;
    CloseHandle(f);
    for (n = 0; n < (int)got && name[n] != '\r' && name[n] != '\n' && name[n]; n++)
        ;
    name[n] = 0;
    return n;
}

void nav_name_save(const char *name)
{
    HANDLE f = CreateFileA(NAV_NAME_FILE, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written;
    int n;
    if (f == INVALID_HANDLE_VALUE)
        return;
    for (n = 0; n < 8 && name[n]; n++)
        ;
    WriteFile(f, name, (DWORD)n, &written, NULL);
    WriteFile(f, "\r\n", 2, &written, NULL);
    CloseHandle(f);
}

void nav_note(const char *what, int value)
{
    brr_log("menu nav: %s 0x%02X", what, value);
}
