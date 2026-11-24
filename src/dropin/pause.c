/* Pausing a race, and the End Race? question (the director, 2026-11-24).
 *
 * The game's pause: Break (key-table 0x72; a pad's Start, controller.c) fires the race's key
 * hotspot action 11, which race_keys_tick (0x414240) turns into race_paused (0x44B744); while
 * it's set, race_tick runs only the camera keys, Esc (action 10: race_quit) and Break again
 * (carry on). Esc (0x01; a pad's Back) fires action 10, race_quit at once. Each tick
 * menu_hotspots_tick (0x427E28) sets the action (0x44BF2C) and the race tick reads it; this
 * runs between the two (site menu_hotspots_call, keys.c's brr_after_menu_hotspots).
 *
 * [fixes] pause=1 (both renderers):
 *  - no pause before the start: action 11 is dropped while race_countdown (0x44B74C) is above
 *    0 (the lights not yet green), as the camera's zoom (camera_tick runs paused or not) and
 *    the start's commentary carried on through a pause;
 *  - the sound effects and commentary pause with the race: every effect channel's DirectSound
 *    buffer that is playing is stopped (Stop keeps its place) and played again, looping if it
 *    was, when the race carries on. The game touches its channels only from
 *    sound_update_vehicles, every 8 ticks of a running race, so nothing restarts them while
 *    paused. Its buffers' pointers (0x52F240, or brr_ds_buffers with more than 8 channels) are
 *    cleared whenever they are released (ds_load_sample, ds_shutdown). The music is ours
 *    (music.c), and plays on.
 *
 * [display] quit_confirm=1 (the Modern renderer's HUD, hud.c; single races and tournaments once
 * the lights are green: not split screen, network play, the demo or recordings): Esc or Back
 * doesn't end the race but pauses it (through the game's own action 11) with END RACE?, YES and
 * NO on the screen (NO chosen). Left and right (the arrow keys, the keypad's, joystick 1's stick
 * or D-pad: key-table 0x4B/0x6B, 0x4D/0x6D, 0x77/0x78) choose; button 1 (A, Return, keypad Enter:
 * 0x73, 0x1C, 0x70) does it: YES ends the race (action 10, race_quit), NO goes back; button 2
 * (B, Esc, Back: 0x74, 0x01) goes back at once. Back means running again, or still paused if
 * the race was paused before. While it's up the game sees no action: no pause, no camera keys.
 * Each input counts when pressed, so the press that opened it doesn't close it. */
#include "brrfix.h"

#define SCREEN          (*(volatile uint16_t *)0x4B13C0)
#define RACE_PAUSED     (*(volatile int32_t *)0x44B744)
#define RACE_COUNTDOWN  (*(volatile int32_t *)0x44B74C)  /* ticks to the green light; then negative */
#define HOTSPOT_ACTION  (*(volatile int16_t *)0x44BF2C)
#define GAME_MODE       (*(volatile int16_t *)0x44AF58)  /* 0 normal, 2 demo, 4 -REC, 5 a recording */
#define GAME_TYPE       (*(volatile int16_t *)0x49C5C0)  /* 0x1E race, 0x1F tournament, 0x20 split, 0x23 network */
#define SPLIT_SCREEN    (*(volatile int32_t *)0x44BE74)
#define DS_READY        (*(volatile uint8_t *)0x52F234)
#define DS_BUFFERS      ((void *const *)0x52F240)        /* the game's 8 effect channels */
#define KEY_TABLE       (*(uint8_t *volatile *)0x56C648) /* KEY.DLL's _KEY_pressed */
#define SCREEN_RACE     0x80
#define ACTION_QUIT     10
#define ACTION_PAUSE    11
#define KEY_ESC         0x01
#define KEY_BREAK       0x72
#define KEY_RETURN      0x1C
#define KEY_PAD_ENTER   0x70
#define KEY_JS_B1       0x73
#define KEY_JS_B2       0x74
#define KEY_JS_LEFT     0x77
#define KEY_JS_RIGHT    0x78
/* IDirectSoundBuffer's methods, by vtable slot (dsound.h) */
#define DS_GET_STATUS   9
#define DS_PLAY         12
#define DS_STOP         18
#define DSBSTATUS_PLAYING 1u
#define DSBSTATUS_LOOPING 4u
#define DSBPLAY_LOOPING   1u

int brr_quit_open, brr_quit_choice, brr_quit_serial;  /* hud.c draws it */
int brr_quit_ending;  /* YES: the question stays up while the race fades out */

static int was_paused;                    /* the race was paused when the question came up */
static int held_b1, held_b2, held_left, held_right;
static int hold_quit, hold_pause;         /* Esc or Back, Break or Start, still down from the question */
static int logged_early;
static void *stopped[SOUND_CHANNELS_MAX]; /* the buffers we stopped, and whether they looped */
static uint8_t stopped_looping[SOUND_CHANNELS_MAX];
static int sounds_stopped;
static int32_t last_tick, ending_since;

typedef HRESULT (WINAPI *ds_status_fn)(void *self, DWORD *status);
typedef HRESULT (WINAPI *ds_play_fn)(void *self, DWORD reserved, DWORD priority, DWORD flags);
typedef HRESULT (WINAPI *ds_stop_fn)(void *self);
#define METHOD(buffer, slot) ((*(void ***)(buffer))[slot])

static void *const *channel_buffers(int *count)
{
    *count = cfg.sound_channels > 8 ? cfg.sound_channels : 8;
    return cfg.sound_channels > 8 ? (void *const *)brr_ds_buffers : DS_BUFFERS;
}

static void sounds_stop(void)
{
    void *const *buffers;
    int count, i, n = 0;
    sounds_stopped = 1;
    if (!DS_READY)
        return;
    buffers = channel_buffers(&count);
    for (i = 0; i < count; i++) {
        void *b = buffers[i];
        DWORD status = 0;
        stopped[i] = NULL;
        if (!b || ((ds_status_fn)METHOD(b, DS_GET_STATUS))(b, &status) != 0 || !(status & DSBSTATUS_PLAYING))
            continue;
        ((ds_stop_fn)METHOD(b, DS_STOP))(b);
        stopped[i] = b;
        stopped_looping[i] = (status & DSBSTATUS_LOOPING) != 0;
        n++;
    }
    brr_log("pause: the race is paused; %d sound effect%s stopped with it (tick %ld)", n, n == 1 ? "" : "s",
            (long)*GAME_TICK_COUNT);
}

/* again = 1: the race carries on, so the buffers we stopped play on from where they were (if
 * they're still the channels'); 0: the race has ended, and they're the game's to release */
static void sounds_resume(int again)
{
    void *const *buffers;
    int count, i, n = 0;
    sounds_stopped = 0;
    buffers = channel_buffers(&count);
    for (i = 0; i < count; i++) {
        void *b = stopped[i];
        stopped[i] = NULL;
        if (!again || !b || !DS_READY || buffers[i] != b)
            continue;
        ((ds_play_fn)METHOD(b, DS_PLAY))(b, 0, 0, stopped_looping[i] ? DSBPLAY_LOOPING : 0);
        n++;
    }
    if (again)
        brr_log("pause: the race carries on; %d sound effect%s with it (tick %ld)", n, n == 1 ? "" : "s",
                (long)*GAME_TICK_COUNT);
}

/* The question is for a race of ours to draw it in, under way */
static int question_allowed(void)
{
    return cfg.quit_confirm && brr_hud_active && !SPLIT_SCREEN && GAME_MODE == 0 && GAME_TYPE != 0x20
        && GAME_TYPE != 0x23 && RACE_COUNTDOWN <= 0;
}

static void question_close(const char *how, int action, int *out)
{
    brr_quit_open = 0;
    hold_quit = hold_pause = 1;  /* until Esc or Back, Break or Start are let go */
    *out = action;
    brr_log("quit: %s (tick %ld)", how, (long)*GAME_TICK_COUNT);
}

static int button1(const uint8_t *k) { return k[KEY_JS_B1] || k[KEY_RETURN] || k[KEY_PAD_ENTER]; }
static int button2(const uint8_t *k) { return k[KEY_JS_B2] || k[KEY_ESC]; }
static int go_left(const uint8_t *k) { return k[KEY_JS_LEFT] || k[0x4B] || k[0x6B]; }
static int go_right(const uint8_t *k) { return k[KEY_JS_RIGHT] || k[0x4D] || k[0x6D]; }

/* With the question up, this tick's inputs; returns the action for the race tick */
static int question_tick(const uint8_t *keys)
{
    int b1 = button1(keys), b2 = button2(keys), left = go_left(keys), right = go_right(keys);
    int back = was_paused ? -1 : ACTION_PAUSE, action = -1;
    if (left && !held_left)
        brr_quit_choice = 0;
    if (right && !held_right)
        brr_quit_choice = 1;
    if (b2 && !held_b2)
        question_close(was_paused ? "button 2: back to the pause" : "button 2: the race carries on", back, &action);
    else if (b1 && !held_b1 && brr_quit_choice == 0) {
        question_close("YES: the race ends", ACTION_QUIT, &action);
        brr_quit_ending = 1;
        ending_since = *GAME_TICK_COUNT;
    }
    else if (b1 && !held_b1)
        question_close(was_paused ? "NO: back to the pause" : "NO: the race carries on", back, &action);
    held_b1 = b1;
    held_b2 = b2;
    held_left = left;
    held_right = right;
    return action;
}

/* Each tick, after menu_hotspots_tick and before the screen's tick (keys.c) */
void brr_pause_tick(void)
{
    const uint8_t *keys = KEY_TABLE;
    int action;
    int32_t now = *GAME_TICK_COUNT;
    int new_screen = now < last_tick;  /* tick_count starts again with each screen */
    last_tick = now;
    if (SCREEN != SCREEN_RACE || new_screen || !keys) {
        if (sounds_stopped)
            sounds_resume(0);
        brr_quit_open = brr_quit_ending = 0;
        hold_quit = hold_pause = 0;
        logged_early = 0;
        if (SCREEN != SCREEN_RACE || !keys)
            return;
    }
    action = HOTSPOT_ACTION;
    if (brr_quit_ending && now - ending_since > 5 * 70) {  /* never seen: the screen didn't change */
        brr_quit_ending = 0;
        brr_log("quit: the race didn't end after YES; the game's keys again (tick %ld)", (long)now);
    }
    if (brr_quit_ending) {
        action = -1;  /* race_quit has asked for the next screen: nothing else now */
    } else if (brr_quit_open) {
        action = question_tick(keys);
    } else {
        if (hold_quit && !keys[KEY_ESC])
            hold_quit = 0;
        if (hold_pause && !keys[KEY_BREAK])
            hold_pause = 0;
        if ((action == ACTION_QUIT && hold_quit) || (action == ACTION_PAUSE && hold_pause))
            action = -1;
        if (action == ACTION_QUIT && question_allowed()) {
            was_paused = RACE_PAUSED != 0;
            brr_quit_open = 1;
            brr_quit_choice = 1;
            brr_quit_serial++;
            /* the inputs down now (Esc or Back among them) count when pressed again */
            held_b1 = button1(keys);
            held_b2 = button2(keys);
            held_left = go_left(keys);
            held_right = go_right(keys);
            action = was_paused ? -1 : ACTION_PAUSE;
            brr_log("quit: END RACE? (YES or NO; the race %s) (tick %ld)", was_paused ? "was paused" : "pauses",
                    (long)now);
        } else if (action == ACTION_PAUSE && cfg.fix_pause && !RACE_PAUSED && RACE_COUNTDOWN > 0) {
            action = -1;
            if (!logged_early++)
                brr_log("pause: not before the start (%ld ticks to the green light)", (long)RACE_COUNTDOWN);
        }
    }
    HOTSPOT_ACTION = (int16_t)action;
    if (cfg.fix_pause) {
        if ((RACE_PAUSED || brr_quit_open) && !sounds_stopped)
            sounds_stop();
        else if (!RACE_PAUSED && !brr_quit_open && !brr_quit_ending && sounds_stopped)
            sounds_resume(1);
    }
}
