/* The controller layer (round 2, item 9): modern pads (XInput) as the game's two joysticks.
 *
 * The game reads two joysticks through Windows' joystick messages (joySetCapture in
 * joystick_start 0x41A7E0; joystick1_message and joystick2_message store them only while
 * their "captured" bytes, 0x44B9C8 and 0x44B9C9, are set): X and Y, 0..65535, at 0x44B9CA
 * and 0x44B9CC (joystick 2: 0x44B9D0 and 0x44B9D2), buttons at 0x44B9D6 (0x44B9D7). Each
 * tick, run_screen_frame calls joystick_to_keys (0x41A950, at 0x428D1D), which turns them
 * into key-table entries 0x73-0x7E (joystick 1: buttons 0x73/0x74, axes 0x77-0x7A;
 * joystick 2: 0x75/0x76, 0x7B-0x7E) and inverted analogue values (0x4546D0...) that the
 * steering reads proportionally (0x41AA64, 0x41AAE8), using the calibration at 0x44B9E4
 * (max, centre, min per axis, 6 bytes each; axes 0-1 joystick 1, 2-3 joystick 2). Each
 * player picks Keys, JS1, JS2 or pedals in the game's controls menu (0x44B010, 0x44B028).
 *
 * The pads: XInput's four slots are read (connected ones every tick, an empty one now and
 * then, as XInputGetState is slow on an empty slot on Windows). A pad becomes JS1 when it's
 * first used, the next one used becomes JS2, as "press a button to join": an idle device
 * (Steam's own pad, the Steam Deck's controls) never takes a joystick, and a slot that only
 * mirrors JS1's pad (the same pad seen twice) doesn't become JS2. While any XInput pad is
 * connected the game's own joystick messages are switched off (both captured bytes 0), so
 * the same pad can't also drive a joystick through Windows (under Proton the pad was both
 * JS1, through XInput, and JS2, through Windows' messages: 2026-11-15); with none, Windows'
 * own joysticks (a pad without XInput, such as an 8BitDo in D-input mode) are read each tick
 * and driven the same way (drive_windows_joysticks), and without XInput at all (Windows
 * 95-ME) they work as in 1996. Each tick, just before joystick_to_keys, each pad's state goes
 * into its joystick's variables: left stick X (or the D-pad's left and right) steers
 * (deadzone, sensitivity and a response curve), the triggers (or stick Y) are the pedals, A
 * and B are buttons 1 and 2, the calibration matches, and with analogue=1 the per-axis
 * analogue flags (0x44B9D8) are set once the pad has shown it's analogue (a stick or trigger
 * held part of the way; a pad with only a D-pad and digital triggers never does, so it
 * steers as the keyboard does). A joystick without a pad reads as centred. In a race, any pad's Start is Break (key-table index 0x72: pause, and
 * again to carry on) and Back is Esc (0x01: end the race), as race_keys_tick (0x414240)
 * reads them through the key hotspots; each is set while the button is held and cleared on
 * release, so a key held on the keyboard is left alone. The game's code is unchanged: only what it's given.
 *
 * Vibration, read only, for each player's vehicle in a race (+0x30: 1 player one, 2 player
 * two), on the pad of the joystick in that player's controls (JS1 or pedals: JS1; JS2:
 * JS2; keys: none), looked at the same moment each tick:
 * - [controller] vibration=1: when its damage points (+0x38D: only impacts over 1,499 add
 *   any, and only with the game's damage option on; -1 when wrecked) have gone up, a
 *   buzz of both motors at vibration_intensity for 0.2 s (0.45 s for the wreck);
 * - nitro_rumble=1: while its exhaust flames show (+0x34D: the boost, boost_ticks +0x342,
 *   then up to 16 steps dying away), a steady rumble at half that (the low-frequency
 *   motor; the high one at a quarter).
 * Each motor runs at the stronger of the two. A timer (every 50 ms) applies the end of a
 * buzz, repeats a running command every 0.5 s and turns the motors off once the race's
 * ticks stop advancing or it's paused (race_paused 0x44B744: Break; the tick count carries
 * on), on Alt-Tab and at the end of the race. Under Wine and Proton each command lasts
 * 1 s (winebus cutoff_time_ms) and Wine's XInputSetState drops one equal to the last
 * (xinput1_3 HID_set_state), so each repeat changes a motor by one step in 65,535. */
#include "brrfix.h"
#include <mmsystem.h>
#include <string.h>

#define JOY_CAPTURED  ((volatile uint8_t *)0x44B9C8)   /* [j]: joystick j+1's messages are stored */
#define JOY_X(j)      ((volatile uint16_t *)(0x44B9CA + 6 * (j)))
#define JOY_Y(j)      ((volatile uint16_t *)(0x44B9CC + 6 * (j)))
#define JOY_BUTTONS   ((volatile uint8_t *)0x44B9D6)   /* [j] */
#define JOY_CALIBRATE ((volatile uint16_t *)0x44B9E4)  /* axis n: [3n] max, [3n+1] centre, [3n+2] min */
#define JOY_ANALOGUE  ((volatile uint8_t *)0x44B9D8)   /* per axis: use the position, not just on/off */
#define PLAYER_TYPE(p) (*(volatile uint8_t *)((p) ? 0x44B028 : 0x44B010))  /* 0 keys, 1 JS1, 2 JS2, 3-4 pedals */
#define SCREEN        ((volatile uint16_t *)0x4B13C0)  /* screen_current: 0x80 a race */
#define RACE_PAUSED   ((volatile int32_t *)0x44B744)   /* race_paused: Break; the tick count goes on */
#define V_MODEL       0x26   /* short: -1 an unused slot */
#define V_STATE       0x30   /* short: 1 player one, 2 player two */
#define V_DAMAGE      0x38D  /* int: damage points; -1 at the wreck, then counting down */
#define V_FLAME       0x34D  /* byte: the exhaust flames, 1-0x20 boosting, then dying away; 0x80+ an empty puff */
#define BUZZ_MS       200
#define WRECK_MS      450
#define STALL_MS      100    /* no race tick for this long: the motors stop */
#define REFRESH_MS    500    /* Wine and Proton end each rumble after 1 s (winebus cutoff_time_ms) */
#define SLOTS         4
#define JOYSTICKS     2
#define PAD_UP        0x0001  /* the D-pad */
#define PAD_DOWN      0x0002
#define PAD_LEFT      0x0004
#define PAD_RIGHT     0x0008
#define PAD_START     0x0010
#define PAD_BACK      0x0020
#define KEY_BREAK     0x72   /* key-table index of Break: action 11, pause (race_add_keys) */
#define KEY_ESC       0x01   /* Esc: action 10, race_quit */

typedef struct {
    WORD buttons;
    BYTE left_trigger, right_trigger;
    SHORT left_x, left_y, right_x, right_y;
} pad_state;
typedef struct { DWORD packet; pad_state pad; } xinput_state;
typedef DWORD (WINAPI *xinput_get_state_fn)(DWORD, xinput_state *);
typedef struct { WORD left_motor, right_motor; } xinput_vibration;  /* low and high frequency */
typedef DWORD (WINAPI *xinput_set_state_fn)(DWORD, xinput_vibration *);

struct rumble {
    int64_t buzz_until;    /* QPC: the end of the damage buzz */
    int boosting;          /* a player of this joystick has nitro flames */
    WORD low, high;        /* what the pad was last told to do */
    int64_t last_send;     /* QPC */
    int nudge;             /* the last repeat was one step lower */
    DWORD result;          /* the pad's answer: 0 done, else not connected */
};

uint32_t brr_orig_joystick_to_keys = 0x41A950;
static void vibration_start(void);
static xinput_get_state_fn get_state;
static xinput_set_state_fn set_state;
static int simulated = -1;  /* tests: the simulated pad has been logged */
static int slot_connected[SLOTS], probe_slot, probe_wait, probed, mirror_logged[SLOTS];
static volatile int joystick_slot[JOYSTICKS] = {-1, -1};  /* each joystick's XInput slot (read by the timer) */
static uint8_t windows_captured[JOYSTICKS];  /* the game's own captured bytes, while switched off */
static int windows_off = -1;                 /* the game's own joysticks switched off (-1: not yet known) */
static UINT vibration_timer;
static CRITICAL_SECTION motor_lock;
static struct rumble rumble[JOYSTICKS];
static int64_t last_tick_time;               /* QPC: the last running race tick */
static int damage_known[2];                  /* per player: damage_seen is from the last tick */
static int32_t damage_seen[2], last_tick;
static int analogue_seen[JOYSTICKS];          /* each joystick's pad has moved part of the way */

void brr_controller_start(void)
{
    static const char *const libraries[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
    size_t i;
    if (!cfg.controller && !cfg.debug_pad_test)
        return;
    for (i = 0; i < sizeof libraries / sizeof libraries[0] && !get_state; i++) {
        HMODULE x = LoadLibraryA(libraries[i]);
        if (x) {
            get_state = (xinput_get_state_fn)GetProcAddress(x, "XInputGetState");
            set_state = (xinput_set_state_fn)GetProcAddress(x, "XInputSetState");
        }
    }
    vibration_start();
    brr_log("controller: %s (deadzone %d%%, sensitivity %d%%, curve %d, layout %s; vibration on damage %s at "
            "%d%%, nitro rumble %s at %d%%)",
            get_state ? "XInput found; pads drive the game's joysticks, JS1 the first used, JS2 the next"
                      : "no XInput here (Windows 9x?): off",
            cfg.pad_deadzone, cfg.pad_sensitivity, cfg.pad_curve, cfg.pad_triggers ? "triggers" : "stick",
            cfg.pad_vibration && set_state ? "on" : "off", cfg.pad_damage_strength,
            cfg.pad_nitro_rumble && set_state ? "on" : "off", cfg.pad_nitro_strength);
}

/* ---- vibration ---- */
static int64_t ms_from_now(int ms) { return brr_qpc() + brr_qpc_freq() * ms / 1000; }

/* Each joystick's motors from the state: the stronger of the damage buzz and the nitro
 * rumble, nothing once the race's ticks have stopped. Tells the pad when it changes, and
 * again every REFRESH_MS while it runs, one step different (Wine drops an equal command). */
static void apply_motors(void)
{
    int64_t now = brr_qpc();
    int running, j;
    EnterCriticalSection(&motor_lock);
    running = now - last_tick_time < brr_qpc_freq() * STALL_MS / 1000;
    for (j = 0; j < JOYSTICKS; j++) {
        struct rumble *r = &rumble[j];
        long low = 0, high = 0;
        int slot = joystick_slot[j], changed, due;
        xinput_vibration v;
        if (slot < 0)
            continue;
        if (running) {
            if (now < r->buzz_until)
                low = high = 65535L * cfg.pad_damage_strength / 100;
            if (r->boosting) {
                long level = 65535L * cfg.pad_nitro_strength / 100;
                if (level > low)
                    low = level;
                if (level / 2 > high)
                    high = level / 2;
            }
        }
        changed = (WORD)low != r->low || (WORD)high != r->high;
        due = (low || high) && now - r->last_send >= brr_qpc_freq() * REFRESH_MS / 1000;
        if (!changed && !due)
            continue;
        r->nudge = changed ? 0 : !r->nudge;
        v.left_motor = (WORD)(low && r->nudge ? low - 1 : low);
        v.right_motor = (WORD)(!low && high && r->nudge ? high - 1 : high);
        r->result = set_state((DWORD)slot, &v);
        r->low = (WORD)low;
        r->high = (WORD)high;
        r->last_send = now;
        if (changed)
            brr_log("controller: JS%d's pad (XInput slot %d): motors %ld%% (low frequency), %ld%% (high)%s", j + 1,
                    slot, low * 100 / 65535, high * 100 / 65535, r->result == 0 ? "" : "; not connected");
    }
    LeaveCriticalSection(&motor_lock);
}

static void CALLBACK vibration_tick(UINT id, UINT msg, DWORD_PTR user, DWORD_PTR a, DWORD_PTR b)
{
    (void)id; (void)msg; (void)user; (void)a; (void)b;
    apply_motors();
}

static void vibration_start(void)
{
    if (!set_state || (!cfg.pad_vibration && !cfg.pad_nitro_rumble))
        return;
    InitializeCriticalSection(&motor_lock);
    vibration_timer = timeSetEvent(50, 10, vibration_tick, 0, TIME_PERIODIC);
}

/* A joystick's pad goes still (it's leaving the joystick, or the game is ending) */
static void motors_off(int j)
{
    xinput_vibration v;
    if (!vibration_timer || joystick_slot[j] < 0)
        return;
    EnterCriticalSection(&motor_lock);
    v.left_motor = v.right_motor = 0;
    set_state((DWORD)joystick_slot[j], &v);
    rumble[j].low = rumble[j].high = 0;
    rumble[j].boosting = 0;
    rumble[j].buzz_until = 0;
    LeaveCriticalSection(&motor_lock);
}

void brr_controller_stop(void)
{
    int j;
    if (vibration_timer) {
        timeKillEvent(vibration_timer);
        for (j = 0; j < JOYSTICKS; j++)
            motors_off(j);
        vibration_timer = 0;
    }
    if (windows_off == 1)  /* the game's own captures, so joystick_stop releases them */
        for (j = 0; j < JOYSTICKS; j++)
            JOY_CAPTURED[j] = windows_captured[j];
}

/* The joystick in player p's controls: JS1 (or pedals, which steer with JS1), JS2, or none */
static int player_joystick(int p)
{
    uint8_t type = PLAYER_TYPE(p);
    return type == 2 ? 1 : type == 1 || type == 3 || type == 4 ? 0 : -1;
}

/* Both players' damage and nitro this tick: a buzz on their pad when the damage has gone up */
static void check_vehicles(void)
{
    int32_t tick = *GAME_TICK_COUNT;
    int boosting[JOYSTICKS] = {0, 0}, seen[2] = {0, 0};
    int i, j;
    if (tick != last_tick) {
        last_tick = tick;
        if (*SCREEN == 0x80 && !*RACE_PAUSED) {  /* the race is running */
            EnterCriticalSection(&motor_lock);  /* 64-bit: shared with the timer's thread */
            last_tick_time = brr_qpc();
            LeaveCriticalSection(&motor_lock);
        }
    }
    for (i = 0; i < GAME_VEHICLE_COUNT && *SCREEN == 0x80; i++) {
        const uint8_t *v = (const uint8_t *)(GAME_VEHICLES + (uint32_t)i * GAME_VEHICLE_SIZE);
        int16_t state = *(const int16_t *)(v + V_STATE);
        int p = state - 1, joystick;
        int32_t damage;
        uint8_t flame;
        if (*(const int16_t *)(v + V_MODEL) < 0 || (state != 1 && state != 2) || seen[p])
            continue;
        seen[p] = 1;
        joystick = player_joystick(p);
        memcpy(&damage, v + V_DAMAGE, sizeof damage);
        /* as long as the flames show: the boost (boost_ticks +0x342), then up to 16 steps dying away */
        flame = v[V_FLAME];
        if (joystick >= 0 && cfg.pad_nitro_rumble && flame > 0 && flame < 0x80)
            boosting[joystick] = 1;
        /* up, or wrecked (-1); not the wreck's countdown, or a new race's reset to 0 */
        if (cfg.pad_vibration && joystick >= 0 && damage_known[p] && damage_seen[p] >= 0
            && (damage > damage_seen[p] || damage < 0)) {
            EnterCriticalSection(&motor_lock);
            rumble[joystick].buzz_until = ms_from_now(damage < 0 ? WRECK_MS : BUZZ_MS);
            LeaveCriticalSection(&motor_lock);
            brr_log("controller: vibration: player %d's damage %ld -> %ld%s (tick %ld; JS%d)", p + 1,
                    (long)damage_seen[p], (long)damage, damage < 0 ? ", wrecked" : "", (long)tick, joystick + 1);
        }
        damage_seen[p] = damage;
        damage_known[p] = 1;
    }
    for (i = 0; i < 2; i++)
        if (!seen[i])
            damage_known[i] = 0;
    for (j = 0; j < JOYSTICKS; j++)
        if (boosting[j] != rumble[j].boosting) {
            brr_log("controller: nitro flames %s on JS%d's pad (tick %ld)", boosting[j] ? "on: rumble" : "out", j + 1,
                    (long)tick);
            EnterCriticalSection(&motor_lock);
            rumble[j].boosting = boosting[j];
            LeaveCriticalSection(&motor_lock);
        }
    apply_motors();
}

/* ---- the pads ---- */
/* -1..1 with deadzone (scaled so movement starts smoothly), curve and sensitivity */
static float shape(float v, int deadzone, int curve, int sensitivity)
{
    float dz = (float)deadzone / 100.0f, a = v < 0 ? -v : v, out, power = (float)curve / 100.0f;
    if (a <= dz)
        return 0.0f;
    a = (a - dz) / (1.0f - dz);
    /* curve 100 = straight, 200 = squared (gentler near the centre); no maths library */
    out = a + (a * a - a) * (power - 1.0f);
    if (out < 0.0f) out = 0.0f;
    out *= (float)sensitivity / 100.0f;
    if (out > 1.0f) out = 1.0f;
    return v < 0 ? -out : out;
}

static uint16_t to_axis(float v)  /* -1..1 -> 0..65535, 32767 at the centre */
{
    long n = 32767L + (long)(v * 32767.0f);
    return (uint16_t)(n < 0 ? 0 : n > 65535 ? 65535 : n);
}

static int pad_in_use(const pad_state *p)
{
    return p->buttons || p->left_trigger > 30 || p->right_trigger > 30 || p->left_x > 16000 || p->left_x < -16000
        || p->left_y > 16000 || p->left_y < -16000;
}

static int close_to(int a, int b, int by) { return a - b <= by && b - a <= by; }

/* The same pad seen in two slots: the same buttons, triggers and sticks at the same moment */
static int mirrors(const pad_state *a, const pad_state *b)
{
    return a->buttons == b->buttons && close_to(a->left_trigger, b->left_trigger, 16)
        && close_to(a->right_trigger, b->right_trigger, 16) && close_to(a->left_x, b->left_x, 2048)
        && close_to(a->left_y, b->left_y, 2048) && close_to(a->right_x, b->right_x, 2048) && close_to(a->right_y, b->right_y, 2048);
}

/* The game's own joysticks off while any XInput pad is connected, back on without one */
static void windows_joysticks(int off)
{
    int j;
    if (windows_off == off)
        return;
    if (off) {
        for (j = 0; j < JOYSTICKS; j++) {
            windows_captured[j] = JOY_CAPTURED[j];
            JOY_CAPTURED[j] = 0;
        }
    } else if (windows_off == 1)
        for (j = 0; j < JOYSTICKS; j++)
            JOY_CAPTURED[j] = windows_captured[j];
    if (windows_off != -1 || off)
        brr_log("controller: the game's own joystick messages %s", off ? "off: XInput pads drive JS1 and JS2"
                                                                         : "back on: no XInput pad");
    windows_off = off;
}

/* Reads every connected slot into states (ok[slot] set), finds new and gone pads, and gives
 * JS1 and then JS2 to pads as they're first used */
static void read_pads(xinput_state states[SLOTS], int ok[SLOTS])
{
    int i, j, any = 0;
    if (--probe_wait <= 0) {  /* every slot at the start, then one empty slot every 18 ticks */
        probe_wait = 18;
        for (i = 0; i < SLOTS; i++) {
            probe_slot = (probe_slot + 1) % SLOTS;
            if (!slot_connected[probe_slot] && get_state((DWORD)probe_slot, &states[0]) == 0) {
                slot_connected[probe_slot] = 1;
                brr_log("controller: a pad in XInput slot %d", probe_slot);
            }
            if (probed)
                break;
        }
        probed = 1;
    }
    for (i = 0; i < SLOTS; i++) {
        ok[i] = 0;
        if (!slot_connected[i])
            continue;
        if (get_state((DWORD)i, &states[i]) != 0) {
            slot_connected[i] = 0;
            mirror_logged[i] = 0;
            brr_log("controller: the pad in XInput slot %d is gone", i);
            for (j = 0; j < JOYSTICKS; j++)
                if (joystick_slot[j] == i) {
                    joystick_slot[j] = -1;
                    brr_log("controller: JS%d has no pad", j + 1);
                }
            continue;
        }
        ok[i] = any = 1;
    }
    windows_joysticks(any);
    for (i = 0; i < SLOTS; i++) {
        if (!ok[i] || !pad_in_use(&states[i].pad) || joystick_slot[0] == i || joystick_slot[1] == i)
            continue;
        for (j = 0; j < JOYSTICKS && joystick_slot[j] >= 0; j++)
            ;
        if (j == JOYSTICKS)
            break;
        if (j == 1 && ok[joystick_slot[0]] && mirrors(&states[i].pad, &states[joystick_slot[0]].pad)) {
            if (!mirror_logged[i])
                brr_log("controller: XInput slot %d mirrors JS1's pad (slot %d): left out", i, joystick_slot[0]);
            mirror_logged[i] = 1;
            continue;
        }
        motors_off(j);
        joystick_slot[j] = i;
        analogue_seen[j] = 0;
        brr_log("controller: JS%d is the pad in XInput slot %d (first used)", j + 1, i);
    }
}

/* In a race, Start and Back on any pad (the buttons of every joystick's pad) are Break and Esc
 * in the game's key table */
static void race_buttons(WORD buttons)
{
    static int held_start, held_back;
    uint8_t *keys = *(uint8_t **)0x56C648;  /* KEY.DLL's _KEY_pressed */
    int start, back;
    start = *SCREEN == 0x80 && (buttons & PAD_START);
    back = *SCREEN == 0x80 && (buttons & PAD_BACK);
    if (start || held_start)  /* the game clears Break when it pauses: held, it stays set */
        keys[KEY_BREAK] = (uint8_t)start;
    if (back || held_back)
        keys[KEY_ESC] = (uint8_t)back;
    if (start && !held_start)
        brr_log("controller: Start: Break (pause, or carry on) (tick %ld)", (long)*GAME_TICK_COUNT);
    if (back && !held_back)
        brr_log("controller: Back: Esc (end the race) (tick %ld)", (long)*GAME_TICK_COUNT);
    held_start = start;
    held_back = back;
    {
        static int32_t was_paused;
        if (*RACE_PAUSED != was_paused && *SCREEN == 0x80)
            brr_log("controller: the race is %s (tick %ld)", *RACE_PAUSED ? "paused" : "running again",
                    (long)*GAME_TICK_COUNT);
        was_paused = *RACE_PAUSED;
    }
}

/* Part of the way: a stick between a quarter and nine tenths over, a trigger between an
 * eighth and seven eighths pulled; a D-pad, or a digital trigger, is never seen there */
static int part_way(long v) { return (v > 8191 && v < 29491) || (v < -8191 && v > -29491); }
static int part_pulled(int t) { return t > 31 && t < 224; }

/* A pad's state into joystick j's variables. triggers: the pad's triggers are the pedals (an
 * XInput pad's, an 8BitDo's L2 and R2), with layout=triggers; else stick up and down */
static void drive_joystick(int j, const pad_state *pad, int triggers)
{
    float steer, pedal;
    int i;
    steer = shape((float)pad->left_x / 32767.0f, cfg.pad_deadzone, cfg.pad_curve, cfg.pad_sensitivity);
    /* the D-pad's left and right push the stick all the way: in the menus (which read its
     * left and right through the game's joystick keys, keys.c) and in races, where it steers
     * as the keyboard does (a pad without a stick: the director, 2026-11-24) */
    if (pad->buttons & PAD_LEFT)
        steer = -1.0f;
    else if (pad->buttons & PAD_RIGHT)
        steer = 1.0f;
    if (triggers)
        pedal = ((float)pad->left_trigger - (float)pad->right_trigger) / 255.0f;  /* up = accelerate */
    else
        pedal = -shape((float)pad->left_y / 32767.0f, cfg.pad_deadzone, 100, 100);
    /* menus: up and down are the stick's (or the D-pad's), whichever the layout, for the
     * joystick menu navigation (menunav.c), which reads them as the game's joystick keys */
    if (cfg.menu_navigation && *SCREEN != 0x80) {
        if (pad->buttons & PAD_UP)
            pedal = -1.0f;
        else if (pad->buttons & PAD_DOWN)
            pedal = 1.0f;
        else
            pedal = -shape((float)pad->left_y / 32767.0f, cfg.pad_deadzone, 100, 100);
    }
    *JOY_X(j) = to_axis(steer);
    *JOY_Y(j) = to_axis(pedal);
    JOY_BUTTONS[j] = (uint8_t)((pad->buttons & 0x1000 ? 1 : 0) | (pad->buttons & 0x2000 ? 2 : 0));
    /* an analogue pad shows itself by a stick or trigger held part of the way: until then (and
     * always, on a pad with only a D-pad and digital triggers, as the 8BitDo Micro) its steering
     * and pedals are on or off, ramping as the keyboard's do, and the joysticks screen's
     * Analogue box is unticked (the director, 2026-11-24) */
    if (part_way(pad->left_x) || part_way(pad->left_y)
        || (triggers && (part_pulled(pad->left_trigger) || part_pulled(pad->right_trigger)))) {
        if (!analogue_seen[j])
            brr_log("controller: JS%d moved part of the way: analogue%s", j + 1,
                    cfg.pad_analogue ? "" : " (off in BRRFIX.INI: analogue=0)");
        analogue_seen[j] = 1;
    }
    for (i = 2 * j; i < 2 * j + 2; i++) {  /* the calibration of a perfect stick, on the inverted values */
        JOY_CALIBRATE[3 * i] = 65535;
        JOY_CALIBRATE[3 * i + 1] = 32767;
        JOY_CALIBRATE[3 * i + 2] = 0;
        /* analogue steering and pedals, through the game's own analogue code (vehicle_control_axis
         * 0x4183EC); with analogue=0 the game's own Analogue box decides, as in 1996 */
        if (cfg.pad_analogue)
            JOY_ANALOGUE[i] = (uint8_t)analogue_seen[j];
    }
}

/* tests: the joysticks as found before this tick's writes (what Windows' messages left), when changed */
static void log_joysticks(void)
{
    static uint16_t last[6] = {1, 1, 1, 1, 1, 1};
    uint16_t now[6];
    int j;
    for (j = 0; j < JOYSTICKS; j++) {
        now[3 * j] = *JOY_X(j);
        now[3 * j + 1] = *JOY_Y(j);
        now[3 * j + 2] = JOY_BUTTONS[j];
    }
    if (memcmp(now, last, sizeof now)) {
        brr_log("joysticks: JS1 %u,%u buttons %u; JS2 %u,%u buttons %u (tick %ld; captured %u %u)", now[0], now[1],
                now[2], now[3], now[4], now[5], (long)*GAME_TICK_COUNT, JOY_CAPTURED[0], JOY_CAPTURED[1]);
        memcpy(last, now, sizeof now);
    }
}

/* A joystick that Windows doesn't deliver (its captured byte 0: joySetCapture found none) and
 * no pad drives reads 0,0 in the game, which joystick_to_keys (0x41A950) turns into the stick
 * held left and up (key-table 0x77, 0x79) on every tick: unnoticed in 1996, as no menu used
 * them, but the menu navigation's left (keys.c) would fire with every key press. Centred. */
static void centre_absent_joysticks(void)
{
    int j;
    if (windows_off == 1)  /* pads connected: their joysticks are driven, the others centred */
        return;
    for (j = 0; j < JOYSTICKS; j++)
        if (!JOY_CAPTURED[j]) {
            *JOY_X(j) = 32767;
            *JOY_Y(j) = 32767;
        }
}

/* Windows' own joystick functions, looked up (Windows 95 has them) */
typedef MMRESULT (WINAPI *getposex_fn)(UINT, JOYINFOEX *);
typedef MMRESULT (WINAPI *getdevcaps_fn)(UINT, JOYCAPSA *, UINT);

static MMRESULT joystick_read(int j, DWORD flags, JOYINFOEX *info)
{
    static getposex_fn getposex;
    static int looked;
    if (!looked) {
        looked = 1;
        getposex = (getposex_fn)GetProcAddress(GetModuleHandleA("winmm.dll"), "joyGetPosEx");
    }
    memset(info, 0, sizeof *info);
    info->dwSize = sizeof *info;
    info->dwFlags = flags;
    return getposex ? getposex(JOYSTICKID1 + j, info) : JOYERR_NOCANDO;
}

/* A hat's position (joyGetPosEx's POV, in hundredths of a degree from up; diagonals count
 * both ways) as the D-pad's buttons */
static WORD hat_buttons(DWORD pov)
{
    if (pov >= 36000)
        return 0;
    return (WORD)((pov >= 31500 || pov <= 4500 ? PAD_UP : pov >= 13500 && pov <= 22500 ? PAD_DOWN : 0)
                  | (pov >= 4500 && pov <= 13500 ? PAD_RIGHT : pov >= 22500 && pov <= 31500 ? PAD_LEFT : 0));
}

/* The D-pad of a joystick Windows delivers itself, without the controller layer (Windows
 * 95-ME, or [controller] enabled=0) is a hat, which the game's joystick messages don't carry:
 * in the menus, with the menu navigation, it pushes joystick 1's stick all the way, as an
 * XInput pad's D-pad does (drive_joystick; the director, 2026-11-24: "I like to use the D-pad
 * to navigate menus"). The stick's own position is put back when the hat is let go, unless a
 * joystick message has brought a new one. */
static void hat_to_stick(void)
{
    static int pushed;
    static uint16_t saved_x, saved_y, forced_x, forced_y;
    int dx = 0, dy = 0;
    JOYINFOEX info;
    if (*SCREEN != 0x80 && JOY_CAPTURED[0] && joystick_read(0, JOY_RETURNPOV, &info) == JOYERR_NOERROR) {
        WORD d = hat_buttons(info.dwPOV);
        dy = d & PAD_UP ? -1 : d & PAD_DOWN ? 1 : 0;
        dx = d & PAD_LEFT ? -1 : d & PAD_RIGHT ? 1 : 0;
    }
    if (dx || dy) {
        if (!pushed || *JOY_X(0) != forced_x)
            saved_x = *JOY_X(0);
        if (!pushed || *JOY_Y(0) != forced_y)
            saved_y = *JOY_Y(0);
        forced_x = (uint16_t)(dx < 0 ? 0 : dx > 0 ? 65535 : saved_x);
        forced_y = (uint16_t)(dy < 0 ? 0 : dy > 0 ? 65535 : saved_y);
        *JOY_X(0) = forced_x;
        *JOY_Y(0) = forced_y;
        if (!pushed)
            brr_log("controller: the joystick's hat (D-pad) pushes the stick in the menus (POV %d,%d)", dx, dy);
        pushed = 1;
    } else if (pushed) {
        if (*JOY_X(0) == forced_x)
            *JOY_X(0) = saved_x;
        if (*JOY_Y(0) == forced_y)
            *JOY_Y(0) = saved_y;
        pushed = 0;
    }
}

/* Windows' own joysticks, with the controller layer, while no XInput pad is connected: a pad
 * without XInput, such as an 8BitDo in its D-input mode (the director's Micro, 2026-11-24).
 * Read each tick (joyGetPosEx) and driven as a pad is (drive_joystick): the D-pad (a hat,
 * which the game's joystick messages don't carry) navigates and steers, the stick gets the
 * deadzone, curve and sensitivity, and the Analogue box follows what the pad shows. An
 * 8BitDo (USB vendor 0x2DC8: joyGetDevCaps' wMid) numbers its buttons one way in D-input
 * mode (SDL's controller database: L2 9, R2 10, Select 11, Start 12): its L2 and R2 are the
 * pedals with layout=triggers, its Select and Start a race's Back and Start. Other pads'
 * extra buttons have no fixed numbers, so they're left alone; their stick's up and down stay
 * the pedals. Returns the D-pad's, Back's and Start's buttons (race_buttons). */
static WORD drive_windows_joysticks(void)
{
    static getdevcaps_fn getdevcaps;
    static int known[JOYSTICKS], eightbitdo[JOYSTICKS];
    WORD all = 0;
    int j;
    for (j = 0; j < JOYSTICKS; j++) {
        JOYINFOEX info;
        pad_state pad;
        DWORD b;
        if (!JOY_CAPTURED[j]
            || joystick_read(j, JOY_RETURNX | JOY_RETURNY | JOY_RETURNPOV | JOY_RETURNBUTTONS, &info) != JOYERR_NOERROR)
            continue;
        if (!known[j]) {
            JOYCAPSA caps;
            known[j] = 1;
            analogue_seen[j] = 0;
            if (!getdevcaps)
                getdevcaps = (getdevcaps_fn)GetProcAddress(GetModuleHandleA("winmm.dll"), "joyGetDevCapsA");
            if (getdevcaps && getdevcaps(JOYSTICKID1 + j, &caps, sizeof caps) == JOYERR_NOERROR) {
                eightbitdo[j] = caps.wMid == 0x2DC8;
                brr_log("controller: JS%d is a joystick Windows reads itself (USB %04X:%04X, %u axes, %u buttons%s)%s",
                        j + 1, caps.wMid, caps.wPid, caps.wNumAxes, caps.wNumButtons,
                        caps.wCaps & JOYCAPS_HASPOV ? ", a hat" : "",
                        eightbitdo[j] ? ": an 8BitDo, L2 and R2 its triggers, Select and Start" : "");
            }
        }
        memset(&pad, 0, sizeof pad);
        pad.left_x = (SHORT)((long)info.dwXpos - 32768L);
        pad.left_y = (SHORT)(32767L - (long)info.dwYpos);
        b = info.dwButtons;
        pad.buttons = (WORD)((b & 1 ? 0x1000 : 0) | (b & 2 ? 0x2000 : 0) | hat_buttons(info.dwPOV));
        if (eightbitdo[j]) {
            pad.left_trigger = (BYTE)(b & 0x100 ? 255 : 0);
            pad.right_trigger = (BYTE)(b & 0x200 ? 255 : 0);
            pad.buttons |= (WORD)((b & 0x400 ? PAD_BACK : 0) | (b & 0x800 ? PAD_START : 0));
        }
        drive_joystick(j, &pad, cfg.pad_triggers && eightbitdo[j]);
        all |= pad.buttons;
    }
    return all;
}

/* A button on any pad or joystick, read now (the start-up splash's skip, splash.c, before any
 * tick has read them): an XInput pad's (not the D-pad's) or a trigger pulled, as a tick reads
 * them (a pad used becomes JS1, as it would on the first tick); or one of Windows' own
 * joysticks' (joyGetPosEx: a gameport stick on Windows 95-ME, a pad without XInput), only
 * those the game captured, so absent ones are never asked */
int brr_controller_any_button(void)
{
    xinput_state states[SLOTS];
    int ok[SLOTS], i, any = 0;
    if (cfg.controller && get_state && !cfg.debug_pad_test) {
        read_pads(states, ok);
        for (i = 0; i < SLOTS; i++)
            if (ok[i] && ((states[i].pad.buttons & ~0x000Fu) || states[i].pad.left_trigger > 64
                          || states[i].pad.right_trigger > 64))
                any = 1;
    }
    for (i = 0; i < JOYSTICKS; i++)
        if (JOY_CAPTURED[i]) {
            JOYINFOEX info;
            if (joystick_read(i, JOY_RETURNBUTTONS, &info) == JOYERR_NOERROR && info.dwButtons)
                any = 1;
        }
    return any;
}

void __cdecl brr_before_joystick(void)
{
    xinput_state states[SLOTS];
    int ok[SLOTS], j;
    static const pad_state centred;
    if (cfg.debug_log_joysticks)
        log_joysticks();
    for (j = 0; j < 2; j++)  /* tests: the players' controls, as chosen in the game's menu */
        if (cfg.debug_player_type[j] >= 0 && *SCREEN == 0x80)
            PLAYER_TYPE(j) = (uint8_t)cfg.debug_player_type[j];
    if (cfg.debug_pad_test) {  /* tests: a pad on JS1 steering right (pad_test=1 half, 2 full lock) with full throttle from race tick 380 */
        int32_t tick = *GAME_TICK_COUNT;
        uint8_t *map = *(uint8_t **)0x4546BC;
        memset(&states[0], 0, sizeof states[0]);
        if (*SCREEN == 0x80 && map) {
            /* as choosing the joystick in the controls menu: accelerate, brake, left, right */
            map[0] = 0x79; map[1] = 0x7A; map[2] = 0x77; map[3] = 0x78;
        }
        if (*SCREEN == 0x80 && tick >= 380 && tick < 1500) {
            states[0].pad.left_x = (SHORT)(cfg.debug_pad_test == 2 ? 32767 : 16384);
            states[0].pad.right_trigger = 255;
        }
        if (simulated != 1) {
            brr_log("controller: pad connected (simulated, JS1; vibration to XInput slot 0)");
            simulated = 1;
            joystick_slot[0] = 0;
            PLAYER_TYPE(0) = 1;
        }
        if (tick == 400) {  /* what the game sees */
            const uint8_t *keys = *(const uint8_t **)0x56C648;
            char line[160];
            int k, n = 0;
            for (k = 0; k < 12 && map; k++)
                n += wsprintfA(line + n, "%02X ", map[k]);
            brr_log("controller test: player 1's control map %s; keys 0x73-0x7E %d%d%d%d%d%d%d%d%d%d%d%d", line,
                    keys[0x73], keys[0x74], keys[0x75], keys[0x76], keys[0x77], keys[0x78], keys[0x79], keys[0x7A],
                    keys[0x7B], keys[0x7C], keys[0x7D], keys[0x7E]);
        }
        analogue_seen[0] = 1;  /* the simulated pad is analogue */
        drive_joystick(0, &states[0].pad, cfg.pad_triggers);
    } else {
        WORD buttons = 0;
        if (!cfg.controller || !get_state) {
            if (cfg.menu_navigation) {
                centre_absent_joysticks();
                hat_to_stick();
            }
            return;
        }
        read_pads(states, ok);
        for (j = 0; j < JOYSTICKS; j++)
            if (joystick_slot[j] >= 0 && ok[joystick_slot[j]]) {
                drive_joystick(j, &states[joystick_slot[j]].pad, cfg.pad_triggers);
                buttons |= states[joystick_slot[j]].pad.buttons;
            } else if (windows_off == 1)  /* no pad yet: a centred joystick, not Windows' last message */
                drive_joystick(j, &centred, cfg.pad_triggers);
        centre_absent_joysticks();
        if (windows_off != 1)  /* Windows' own joysticks, driven as pads */
            buttons |= drive_windows_joysticks();
        race_buttons(buttons);
    }
    if (vibration_timer)
        check_vehicles();
}
