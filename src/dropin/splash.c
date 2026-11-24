/* The start-up splash (the director, 2026-11-24). game_init (0x423FF8) shows BRS2.RAW, Big Red
 * Racing's title, fades it in and loads the rest; then the first screen's set-up fades it out
 * before its own picture (name_entry_enter 0x40998C: palette_fade_out, then palette_fade_in).
 * On a 1996 PC the loading kept it up for a few seconds; on today's it flashes by.
 *
 * Site splash_present (game_init's CALL present_view, 0x4240D4) notes when it appears. Site
 * frame_message (WndProc's JZ to the frame message 0x9000, 0x401749) then holds the first
 * screen back, returning 0 from WndProc without a frame, until [display] splash_seconds (5)
 * have passed since it appeared, or Esc, Space, Enter (either), a mouse click in the window or
 * any controller button is pressed and let go (so the first screen never sees it): then the
 * first screen starts as in 1996, fading the splash out. Meanwhile the messages keep flowing
 * (pump_messages posts the frame message whenever its queue is empty), so the window answers
 * and KEY.DLL's hook sees the keys. A key or button already down when the splash appears (from
 * the launcher) counts once it's let go and pressed again. The widescreen version: render.c. */
#include "brrfix.h"

#define GAME_HWND   (*(HWND *)0x451014)
#define KEYS        (*(volatile unsigned char **)0x56C648)  /* KEY.DLL's _KEY_pressed, by scan code */
#define KEY_DOWN    (*(volatile unsigned char **)0x56C644)  /* KEY.DLL's _KEY_down: the last key typed */
#define KEY_ESC     0x01
#define KEY_RETURN  0x1C
#define KEY_SPACE   0x39
#define KEY_PAD_ENTER 0x70
#define LET_GO_MS   2000    /* the longest wait for the skipping key or button to be let go */

int brr_splash_on;
static int holding, armed, skipped, pad_down;
static int64_t shown_at, skipped_at, next_pad;

/* Site splash_present: BRS2.RAW loaded, about to be shown */
void __cdecl brr_splash_shown(void)
{
    brr_splash_on = 1;
    if (cfg.splash_seconds > 0) {
        holding = 1;
        shown_at = brr_qpc();
    }
    brr_log("splash: shown; held %d s, or until Esc, Space, Enter, a mouse click or a controller button",
            cfg.splash_seconds);
}

/* Which skipping input is down now, or NULL */
static const char *skip_input(void)
{
    const volatile unsigned char *keys = KEYS;
    HWND hwnd = GAME_HWND;
    int64_t now = brr_qpc();
    if (keys && (keys[KEY_ESC] || keys[KEY_RETURN] || keys[KEY_SPACE] || keys[KEY_PAD_ENTER]))
        return "a key";
    if (hwnd && GetForegroundWindow() == hwnd
        && ((GetAsyncKeyState(VK_LBUTTON) | GetAsyncKeyState(VK_RBUTTON) | GetAsyncKeyState(VK_MBUTTON)) & 0x8000)) {
        POINT p;
        RECT r;
        if (GetCursorPos(&p) && GetWindowRect(hwnd, &r) && PtInRect(&r, p))
            return "a mouse click";
    }
    if (now >= next_pad) {  /* the pads at the game's tick rate */
        next_pad = now + brr_qpc_freq() / 70;
        pad_down = brr_controller_any_button();
    }
    return pad_down ? "a controller button" : NULL;
}

/* Site frame_message: 1 while the splash holds the first screen back */
int __cdecl brr_splash_hold(void)
{
    const char *input;
    int64_t now, freq;
    if (!holding) {
        brr_splash_on = 0;
        return 0;
    }
    now = brr_qpc();
    freq = brr_qpc_freq();
    input = skip_input();
    if (!skipped) {
        if (!input) {
            armed = 1;
        } else if (armed) {
            int ms = (int)((now - shown_at) * 1000 / freq);
            skipped = 1;
            skipped_at = now;
            brr_log("splash: skipped by %s after %d.%02d s", input, ms / 1000, ms % 1000 / 10);
        }
        if (!skipped && now - shown_at >= freq * cfg.splash_seconds) {
            brr_log("splash: %d s up", cfg.splash_seconds);
            holding = 0;
        }
    } else if (!input || now - skipped_at >= freq * LET_GO_MS / 1000) {
        holding = 0;
        if (KEY_DOWN)
            *KEY_DOWN = 0;  /* the skipping key isn't typed into the first screen */
    }
    if (!holding) {
        brr_splash_on = 0;
        return 0;
    }
    Sleep(5);  /* the frame message comes straight back while the queue is empty */
    return 1;
}
