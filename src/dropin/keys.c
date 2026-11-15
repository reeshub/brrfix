/* The keyboard in menus: one key press, one menu action.
 *
 * KEY.DLL's keyboard hook keeps the game's key table (_KEY_pressed, 1 while a key is
 * down, updated as the game reads its messages) and the last key pressed (_KEY_down,
 * for typing names). Each tick, menu_hotspots_tick (0x427E28) fires a menu's key
 * hotspot (the .SCN files' "key" lines: hotspot type 6, key-table index at +4, action
 * at +6) when its key is down and isn't the one key it remembers as already fired (the
 * latch, 0x44BF30); the screen's tick reads the action from 0x44BF2C. Entering a screen
 * resets the latch (FUN_00427D80, from run_screen_frame), so a key that is still down
 * on the new screen's first tick fires again there:
 *  - full screen: the new screen's set-up fades out and in (21 + 21 waits for the
 *    vertical blank, about 0.6 s) before its first tick, without reading messages, so
 *    the key's release is still waiting in the queue: any press fires twice.
 *  - in a window, the fade (FUN_00412FB8) clears the key table instead, but the
 *    keyboard's auto-repeat then sets the held key again: a press longer than the
 *    repeat delay fires again on the next screen, and the next.
 * At 1996 frame rates (about 10 a second in the menus) the release was nearly always
 * read before the screen changed; at 60 to 1,000 frames a second it almost never is.
 *
 * [fixes] menu_keys=1: a key that fired a menu action can't fire another until it is
 * released or pressed afresh (not auto-repeat), as our own WH_KEYBOARD hook sees it
 * (it runs just before KEY.DLL's and passes everything on). Until then it is hidden
 * from menu_hotspots_tick only, for that call; nothing else sees a change. Keys our
 * hook never sees (joystick buttons, 0x73-0x7E, set each tick by joystick_to_keys) are
 * released when the key table shows them up, which also covers a missing hook.
 * A key still held when the screen changes counts as having fired too: it was pressed
 * for the previous screen, which may have acted on it without a key hotspot. The name
 * screen (0x0F) finishes on Return through _KEY_down (name_entry_keys, 0x4098DC), and
 * with an existing player or no name the main menu comes straight after, whose own
 * Return hotspot would otherwise fire on its first tick (two screens for one press).
 *
 * [debug] log_keys=1 logs, to BRRFIX.LOG, every keyboard event the game's thread sees,
 * and on each tick the key table's changes, _KEY_down, any hotspot action and each
 * screen's key hotspots. Diagnostics only. */
#include "brrfix.h"
#include <string.h>

#define KEY_GENERATE_IMPORT 0x56C640u  /* WINRACE.EXE's import slots for KEY.DLL's data */
#define KEY_DOWN_IMPORT     0x56C644u
#define KEY_PRESSED_IMPORT  0x56C648u
#define HOTSPOTS            0x4B03B0u  /* 0x24 bytes each: +2 type, +4 key (type 6), +6 action */
#define HOTSPOT_COUNT       ((volatile int16_t *)0x44BF38)
#define HOTSPOT_ACTION      ((volatile int16_t *)0x44BF2C)
#define HOTSPOT_LATCH       ((volatile int16_t *)0x44BF30)
#define SCREEN              ((volatile uint16_t *)0x4B13C0)

uint32_t brr_orig_menu_hotspots = 0x427E28;
static HHOOK hook;
static int hook_failed, index_mismatch_logged;
static uint8_t fired[256];   /* fired a menu action during this press */
static uint8_t held[256];    /* down, as our hook last saw it (in the game's message order) */
static uint8_t hidden[256];  /* hidden from this call of menu_hotspots_tick */
static uint8_t noted[256];   /* this press's stopped repeat is in the log */
static uint32_t stopped;     /* repeat firings stopped (for the log) */
static int fix_screen = -1;  /* the screen at the previous tick */

/* debug logging */
static uint8_t last_keys[256];
static int last_screen = -1, last_count = -1, last_down = -1, ticks_left;
static uint32_t frame_serial;

/* KEY.DLL's key-table index for an event: the scan code, with the extended keys moved
 * to their own indices (_KEY_Handler@12; e.g. the arrow keys 0x48/0x4B/0x4D/0x50 to
 * 0x68/0x6B/0x6D/0x64). Checked against _KEY_down on every key press. */
static int key_index(LPARAM lparam)
{
    static const uint8_t from[] = {0x1C, 0x1D, 0x38, 0x46, 0x47, 0x48, 0x49, 0x4B, 0x4D, 0x4F, 0x50, 0x51, 0x52, 0x53};
    static const uint8_t to[]   = {0x55, 0x59, 0x5A, 0x72, 0x67, 0x68, 0x69, 0x6B, 0x6D, 0x6F, 0x64, 0x65, 0x62, 0x63};
    int scan = (int)((lparam >> 16) & 0xFF);
    size_t i;
    if (lparam & 0x1000000)
        for (i = 0; i < sizeof from; i++)
            if (scan == from[i])
                return to[i];
    return scan;
}

static LRESULT CALLBACK keys_hook(int code, WPARAM wparam, LPARAM lparam)
{
    LRESULT result;
    if (code >= 0 && cfg.debug_log_keys)
        brr_log("keys: event vk 0x%02X scan 0x%02X%s %s%s (hook code %d) at tick %ld",
                (unsigned)wparam, (unsigned)((lparam >> 16) & 0xFF), lparam & 0x1000000 ? " ext" : "",
                lparam & 0x80000000 ? "up" : "down", (lparam & 0xC0000000) == 0x40000000 ? " (auto-repeat)" : "",
                code, (long)*GAME_TICK_COUNT);
    result = CallNextHookEx(hook, code, wparam, lparam);  /* KEY.DLL's handler */
    if (code >= 0) {
        int k = key_index(lparam);
        if (lparam & 0x80000000) {
            held[k] = 0;
            fired[k] = noted[k] = 0;
        } else {
            int seen = **(const uint8_t **)KEY_DOWN_IMPORT;
            held[k] = 1;
            if (!(lparam & 0x40000000))  /* a new press, not the keyboard's auto-repeat */
                fired[k] = noted[k] = 0;
            if (seen != k && !index_mismatch_logged) {
                index_mismatch_logged = 1;
                brr_log("keys: KEY.DLL put scan 0x%02X%s at index 0x%02X, not 0x%02X as expected",
                        (unsigned)((lparam >> 16) & 0xFF), lparam & 0x1000000 ? " ext" : "", seen, k);
            }
        }
    }
    return result;
}

static void log_hotspots(void)
{
    char line[400];
    int n = 0, count = *HOTSPOT_COUNT, i, keys = 0;
    for (i = 0; i < count && i < 128 && n < (int)sizeof line - 24; i++) {
        const int16_t *h = (const int16_t *)(HOTSPOTS + 0x24u * (uint32_t)i);
        if (h[1] == 6) {
            n += wsprintfA(line + n, " %02X>%d", (unsigned)(uint16_t)h[2], h[3]);
            keys++;
        }
    }
    line[n] = 0;
    brr_log("keys: screen 0x%02X, %d hotspots, %d keys (index>action):%s", *SCREEN, count, keys, line);
}

static void log_tick(const uint8_t *keys)
{
    int down = **(const uint8_t **)KEY_DOWN_IMPORT, i, k;
    if (ticks_left == 0) {  /* the first tick of a frame */
        ticks_left = *GAME_FRAME_TICKS;
        frame_serial++;
    }
    ticks_left--;
    if (*SCREEN != last_screen || *HOTSPOT_COUNT != last_count) {
        last_screen = *SCREEN;
        last_count = *HOTSPOT_COUNT;
        log_hotspots();
    }
    for (i = 0; i < 256; i++) {
        if (keys[i] != last_keys[i]) {
            char line[300];
            int n = 0;
            for (k = 0; k < 256 && n < (int)sizeof line - 6; k++)
                if (keys[k])
                    n += wsprintfA(line + n, " %02X", k);
            line[n] = 0;
            brr_log("keys: tick %ld (frame %lu, %d of %d) key table:%s", (long)*GAME_TICK_COUNT,
                    (unsigned long)frame_serial, *GAME_FRAME_TICKS - ticks_left, *GAME_FRAME_TICKS, line);
            for (k = 0; k < 256; k++)
                last_keys[k] = keys[k];
            break;
        }
    }
    if (down != last_down) {
        brr_log("keys: tick %ld KEY_down 0x%02X", (long)*GAME_TICK_COUNT, down);
        last_down = down;
    }
}

static int is_hotspot_key(int k)
{
    int count = *HOTSPOT_COUNT, i;
    for (i = 0; i < count && i < 128; i++) {
        const int16_t *h = (const int16_t *)(HOTSPOTS + 0x24u * (uint32_t)i);
        if (h[1] == 6 && h[2] == k)
            return 1;
    }
    return 0;
}

/* Each tick, just before menu_hotspots_tick (on the game's thread). */
void __cdecl brr_before_menu_hotspots(void)
{
    uint8_t *keys = *(uint8_t **)KEY_PRESSED_IMPORT;
    int k, latch;
    if (!hook && !hook_failed) {
        /* after KEY_Init_ (WinMain), so ours is called first */
        hook = SetWindowsHookExA(WH_KEYBOARD, keys_hook, NULL, GetCurrentThreadId());
        hook_failed = hook == NULL;
        brr_log("keys: %s%s; our keyboard hook %s", cfg.fix_menu_keys ? "one action per key press in menus" : "",
                cfg.debug_log_keys ? (cfg.fix_menu_keys ? ", logging" : "logging (fix off)") : "",
                hook ? "installed" : "couldn't be installed (key releases come from the key table only)");
    }
    if (cfg.debug_log_keys)
        log_tick(keys);
    if (!cfg.fix_menu_keys)
        return;
    if (*SCREEN != fix_screen) {
        if (fix_screen != -1)
            for (k = 0; k < 256; k++)
                if (held[k])
                    fired[k] = 1;
        fix_screen = *SCREEN;
    }
    latch = (uint16_t)*HOTSPOT_LATCH;
    for (k = 0; k < 256; k++) {
        if (!keys[k] && !held[k])  /* up: joystick buttons, or no hook */
            fired[k] = noted[k] = 0;
        hidden[k] = 0;
        /* The game's own latch already stops the key it holds. Any other key that has
         * fired is used: the original would fire it again here, because a new screen
         * reset the latch, the window's fade cleared the key table (and auto-repeat set
         * it again), or another key took the latch. */
        if (fired[k] && keys[k] && k != latch) {
            keys[k] = 0;
            hidden[k] = 1;
            if (!noted[k] && is_hotspot_key(k)) {
                noted[k] = 1;
                if (stopped++ < 100)
                    brr_log("keys: stopped key 0x%02X firing again on screen 0x%02X (tick %ld)", k, *SCREEN,
                            (long)*GAME_TICK_COUNT);
            }
        }
    }
}

/* Each tick, just after menu_hotspots_tick. */
void __cdecl brr_after_menu_hotspots(void)
{
    uint8_t *keys = *(uint8_t **)KEY_PRESSED_IMPORT;
    int16_t latch = *HOTSPOT_LATCH;
    int k;
    if (cfg.fix_menu_keys) {
        for (k = 0; k < 256; k++)
            if (hidden[k])
                keys[k] = 1;
        if (latch != -1)  /* menu_hotspots_tick latches only a key that fired */
            fired[(uint8_t)latch] = 1;
    }
    if (cfg.debug_log_keys && *HOTSPOT_ACTION != -1)
        brr_log("keys: tick %ld fired action %d (latch 0x%02X)", (long)*GAME_TICK_COUNT, *HOTSPOT_ACTION,
                (unsigned)(uint16_t)latch);
}

/* ---- Controller menu navigation ([controller] menu_navigation; both renderers) ----
 * The game's own joystick support reaches the menus through the same key hotspots: its
 * joystick code sets key-table entries 0x73-0x7E (buttons 1 and 2 of joystick 1 are 0x73
 * and 0x74, its stick left and right 0x77 and 0x78), and every screen script that has an
 * OK lists 0x73 beside Return, so A (button 1) already chooses. Here every key hotspot a
 * script adds for Esc (0x01) gets a twin for button 2 (B: back, or quit where Esc quits),
 * and every one for the cursor or keypad left and right keys (0x4B, 0x6B; 0x4D, 0x6D) a
 * twin for the stick (left and right on the race, logo and car screens). Not on the keys
 * screen (0x0E), where the joystick's buttons and directions can be given to a control,
 * nor on the joystick screen (0x17), whose script lists no joystick key at all.
 * Site scn_commit_call (scn_load's CALL menu_hotspot_commit) notes the twins; they're
 * added at the script's end (site scn_end_call), after its own hotspots, as scripts refer
 * to their hotspots by number (stext chains). */
#define HOTSPOT_SIZE 0x24
#define HOTSPOT_MAX  100
#define HOTSPOT_ROOM 90      /* leave the rest for the screens' own code (the credits ticker) */
#define TWINS_MAX    16

static uint8_t twins[TWINS_MAX][HOTSPOT_SIZE];
static int twin_count;

static int twin_key(int key)
{
    if (key == 0x01) return 0x74;
    if (key == 0x4B || key == 0x6B) return 0x77;
    if (key == 0x4D || key == 0x6D) return 0x78;
    return 0;
}

/* After scn_load's menu_hotspot_commit; before = the count of hotspots before it */
void __cdecl brr_menu_pad_commit(int before)
{
    const uint8_t *h;
    int twin;
    if (*HOTSPOT_COUNT != before + 1 || before < 0 || before >= HOTSPOT_MAX || twin_count == TWINS_MAX
        || *SCREEN == 0x0E || *SCREEN == 0x17)
        return;
    h = (const uint8_t *)HOTSPOTS + before * HOTSPOT_SIZE;
    if (*(const int16_t *)(h + 2) != 6 || !(twin = twin_key(*(const int16_t *)(h + 4))))
        return;
    memcpy(twins[twin_count], h, HOTSPOT_SIZE);
    *(int16_t *)(twins[twin_count] + 4) = (int16_t)twin;
    twin_count++;
}

/* At scn_load's end: the twins after the script's own hotspots */
void __cdecl brr_menu_pad_append(void)
{
    static int logged;
    int i, added = 0;
    for (i = 0; i < twin_count; i++) {
        int n = *HOTSPOT_COUNT;
        if (n < 0 || n >= HOTSPOT_ROOM)
            break;
        memcpy((uint8_t *)HOTSPOTS + n * HOTSPOT_SIZE, twins[i], HOTSPOT_SIZE);
        *HOTSPOT_COUNT = (int16_t)(n + 1);
        added++;
    }
    if (added && logged++ < 40)
        brr_log("menu pad: screen 0x%02X: %d joystick key(s) added (B for Esc, the stick for left and right)",
                (unsigned)*SCREEN, added);
    twin_count = 0;
}
