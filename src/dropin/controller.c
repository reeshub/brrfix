/* The controller layer (round 2, item 9): a modern pad (XInput) as the game's joystick.
 *
 * The game reads its joystick through Windows' joystick messages (MM_JOY1MOVE and the
 * button messages, from joySetCapture): position 0..65535 per axis at 0x44B9CA (X) and
 * 0x44B9CC (Y), buttons at 0x44B9D6. Each tick, run_screen_frame calls joystick_to_keys
 * (0x41A950, at 0x428D1D), which turns them into key-table entries 0x73-0x7E (left below
 * 0x4000, right above 0xC000, up, down, buttons 1 and 2) and inverted analogue values
 * (0x4546D0...) that the steering reads proportionally (0x41AA64, 0x41AAE8), using the
 * calibration at 0x44B9E4 (max, centre, min per axis, 6 bytes each).
 *
 * With [controller] enabled=1 and a pad connected, just before that call the pad's
 * state goes into those variables: left stick X steers (deadzone, sensitivity and a
 * response curve), the triggers (or stick Y) are the pedals, A and B are buttons 1 and
 * 2, and the calibration matches. With analogue=1 the game's per-axis analogue flags
 * (0x44B9D8) are set, so it steers in proportion (as with an analogue joystick set up in
 * its menus) instead of on/off at a quarter from either end. The game's code is unchanged: only what it's given.
 *
 * Vibration, read only, for player one's vehicle in a race, looked at the same moment each tick:
 * - [controller] vibration=1: when its damage points (+0x38D: only impacts over 1,499 add
 *   any, and only with the game's damage option on; -1 when wrecked) have gone up, a
 *   buzz of both motors at damage_strength for 0.2 s (0.45 s for the wreck);
 * - nitro_rumble=1: while its nitro boost runs (boost_ticks +0x342 above 0), a steady
 *   rumble at nitro_strength (the low-frequency motor; the high one at half).
 * Each motor runs at the stronger of the two. A timer (every 50 ms) applies the end of a
 * buzz and turns the motors off once the race's ticks stop advancing or it's paused
 * (race_paused 0x44B744: Break; the tick count carries on), on Alt-Tab and at the end of the
 * race, so the pad never keeps rumbling on its own. */
#include "brrfix.h"
#include <string.h>

#define JOY1_X        ((volatile uint16_t *)0x44B9CA)
#define JOY1_Y        ((volatile uint16_t *)0x44B9CC)
#define JOY1_BUTTONS  ((volatile uint8_t *)0x44B9D6)
#define JOY1_PRESENT  ((volatile uint8_t *)0x44B9C8)
#define JOY_CALIBRATE ((volatile uint16_t *)0x44B9E4)  /* axis n: [3n] max, [3n+1] centre, [3n+2] min */
#define JOY_ANALOGUE  ((volatile uint8_t *)0x44B9D8)   /* per axis: use the position, not just on/off */
#define SCREEN        ((volatile uint16_t *)0x4B13C0)  /* screen_current: 0x80 a race */
#define RACE_PAUSED   ((volatile int32_t *)0x44B744)   /* race_paused: Break; the tick count goes on */
#define V_MODEL       0x26   /* short: -1 an unused slot */
#define V_STATE       0x30   /* short: 1 player one */
#define V_DAMAGE      0x38D  /* int: damage points; -1 at the wreck, then counting down */
#define V_BOOST       0x342  /* int: nitro boost steps left (vehicle_nitro, vehicle_physics) */
#define BUZZ_MS       200
#define WRECK_MS      450
#define STALL_MS      100    /* no race tick for this long: the motors stop */

typedef struct {
    WORD buttons;
    BYTE left_trigger, right_trigger;
    SHORT left_x, left_y, right_x, right_y;
} pad_state;
typedef struct { DWORD packet; pad_state pad; } xinput_state;
typedef DWORD (WINAPI *xinput_get_state_fn)(DWORD, xinput_state *);
typedef struct { WORD left_motor, right_motor; } xinput_vibration;  /* low and high frequency */
typedef DWORD (WINAPI *xinput_set_state_fn)(DWORD, xinput_vibration *);

uint32_t brr_orig_joystick_to_keys = 0x41A950;
static void vibration_start(void);
static xinput_get_state_fn get_state;
static xinput_set_state_fn set_state;
static int connected = -1;
static UINT vibration_timer;
static CRITICAL_SECTION motor_lock;
static int64_t buzz_until, last_tick_time;  /* QPC: the end of the damage buzz; the last race tick */
static volatile int boosting;                /* player one's nitro boost, at the last race tick */
static WORD motor_low, motor_high;          /* what the pad was last told */
static DWORD motor_result;                  /* and its answer (0: done; else not connected) */
static int damage_known;     /* damage_seen is player one's damage at the last tick */
static int32_t damage_seen, last_tick;

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
            get_state ? "XInput found; a pad drives the game's joystick" : "no XInput here (Windows 9x?): off",
            cfg.pad_deadzone, cfg.pad_sensitivity, cfg.pad_curve, cfg.pad_triggers ? "triggers" : "stick",
            cfg.pad_vibration && set_state ? "on" : "off", cfg.pad_damage_strength,
            cfg.pad_nitro_rumble && set_state ? "on" : "off", cfg.pad_nitro_strength);
}

static int64_t ms_from_now(int ms) { return brr_qpc() + brr_qpc_freq() * ms / 1000; }

/* The motors from the state: the stronger of the damage buzz and the nitro rumble, nothing
 * once the race's ticks have stopped. Tells the pad only when it changes. */
static DWORD apply_motors(void)
{
    int64_t now = brr_qpc();
    long low = 0, high = 0;
    DWORD result;
    xinput_vibration v;
    EnterCriticalSection(&motor_lock);
    if (now - last_tick_time < brr_qpc_freq() * STALL_MS / 1000) {
        if (now < buzz_until)
            low = high = 65535L * cfg.pad_damage_strength / 100;
        if (boosting) {
            long rumble = 65535L * cfg.pad_nitro_strength / 100;
            if (rumble > low)
                low = rumble;
            if (rumble / 2 > high)
                high = rumble / 2;
        }
    }
    if ((WORD)low != motor_low || (WORD)high != motor_high) {
        v.left_motor = (WORD)low;
        v.right_motor = (WORD)high;
        motor_result = set_state(0, &v);
        motor_low = (WORD)low;
        motor_high = (WORD)high;
        brr_log("controller: motors %ld%% (low frequency), %ld%% (high)%s", low * 100 / 65535, high * 100 / 65535,
                motor_result == 0 ? "" : "; no pad");
    }
    result = motor_result;
    LeaveCriticalSection(&motor_lock);
    return result;
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

void brr_controller_stop(void)
{
    xinput_vibration v;
    if (!vibration_timer)
        return;
    timeKillEvent(vibration_timer);
    vibration_timer = 0;
    v.left_motor = v.right_motor = 0;
    set_state(0, &v);
}

/* Player one's damage and nitro this tick: a buzz when the damage has gone up */
static void check_vehicle(void)
{
    const uint8_t *player = NULL;
    int32_t damage, boost, tick = *GAME_TICK_COUNT;
    int i, was_boosting = boosting;
    if (*SCREEN == 0x80)
        for (i = 0; i < GAME_VEHICLE_COUNT && !player; i++) {
            const uint8_t *v = (const uint8_t *)(GAME_VEHICLES + (uint32_t)i * GAME_VEHICLE_SIZE);
            if (*(const int16_t *)(v + V_MODEL) >= 0 && *(const int16_t *)(v + V_STATE) == 1)
                player = v;
        }
    if (!player) {
        damage_known = 0;
        boosting = 0;
        return;
    }
    if (tick != last_tick) {
        last_tick = tick;
        if (!*RACE_PAUSED) {  /* the race is running */
            EnterCriticalSection(&motor_lock);  /* 64-bit: shared with the timer's thread */
            last_tick_time = brr_qpc();
            LeaveCriticalSection(&motor_lock);
        }
    }
    memcpy(&damage, player + V_DAMAGE, sizeof damage);
    memcpy(&boost, player + V_BOOST, sizeof boost);
    boosting = cfg.pad_nitro_rumble && boost > 0;
    if (boosting != was_boosting)
        brr_log("controller: nitro boost %s (tick %ld, %ld steps)", boosting ? "on: rumble" : "over",
                (long)tick, (long)boost);
    /* up, or wrecked (-1); not the wreck's countdown, or a new race's reset to 0 */
    if (cfg.pad_vibration && damage_known && damage_seen >= 0 && (damage > damage_seen || damage < 0)) {
        DWORD result;
        EnterCriticalSection(&motor_lock);
        buzz_until = ms_from_now(damage < 0 ? WRECK_MS : BUZZ_MS);
        LeaveCriticalSection(&motor_lock);
        result = apply_motors();
        brr_log("controller: vibration: player one's damage %ld -> %ld%s (tick %ld; pad %s)", (long)damage_seen,
                (long)damage, damage < 0 ? ", wrecked" : "", (long)tick,
                result == 0 ? "buzzed" : "not connected");
    } else if (boosting != was_boosting)
        apply_motors();
    damage_seen = damage;
    damage_known = 1;
}

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

void __cdecl brr_before_joystick(void)
{
    xinput_state state;
    float steer, pedal;
    int i;
    if (cfg.debug_pad_test) {  /* tests: a pad steering right (pad_test=1 half, 2 full lock) with full throttle from race tick 380 */
        int32_t tick = *GAME_TICK_COUNT;
        memset(&state, 0, sizeof state);
        uint8_t *map = *(uint8_t **)0x4546BC;
        if (*(volatile uint16_t *)0x4B13C0 == 0x80 && map) {
            /* as choosing the joystick in the controls menu: accelerate, brake, left, right */
            map[0] = 0x79; map[1] = 0x7A; map[2] = 0x77; map[3] = 0x78;
        }
        if (*(volatile uint16_t *)0x4B13C0 == 0x80 && tick >= 380 && tick < 1500) {
            state.pad.left_x = (SHORT)(cfg.debug_pad_test == 2 ? 32767 : 16384);
            state.pad.right_trigger = 255;
        }
    } else if (!cfg.controller || !get_state || get_state(0, &state) != 0) {
        if (connected == 1)
            brr_log("controller: pad disconnected");
        connected = get_state && cfg.controller ? 0 : connected;
        return;
    }
    if (cfg.debug_pad_test && *GAME_TICK_COUNT == 400) {  /* what the game sees */
        const uint8_t *map = *(const uint8_t **)0x4546BC, *keys = *(const uint8_t **)0x56C648;
        char line[160];
        int k, n = 0;
        for (k = 0; k < 12 && map; k++)
            n += wsprintfA(line + n, "%02X ", map[k]);
        brr_log("controller test: player 1's control map %s; keys 0x73-0x7E %d%d%d%d%d%d%d%d%d%d%d%d", line,
                keys[0x73], keys[0x74], keys[0x75], keys[0x76], keys[0x77], keys[0x78], keys[0x79], keys[0x7A],
                keys[0x7B], keys[0x7C], keys[0x7D], keys[0x7E]);
    }
    if (connected != 1) {
        brr_log("controller: pad connected");
        connected = 1;
    }
    if (vibration_timer)
        check_vehicle();
    steer = shape((float)state.pad.left_x / 32767.0f, cfg.pad_deadzone, cfg.pad_curve, cfg.pad_sensitivity);
    if (cfg.pad_triggers)
        pedal = ((float)state.pad.left_trigger - (float)state.pad.right_trigger) / 255.0f;  /* up = accelerate */
    else
        pedal = -shape((float)state.pad.left_y / 32767.0f, cfg.pad_deadzone, 100, 100);
    *JOY1_X = to_axis(steer);
    *JOY1_Y = to_axis(pedal);
    *JOY1_BUTTONS = (uint8_t)((state.pad.buttons & 0x1000 ? 1 : 0) | (state.pad.buttons & 0x2000 ? 2 : 0));
    *JOY1_PRESENT = 1;
    for (i = 0; i < 2; i++) {  /* the calibration of a perfect stick, on the inverted values */
        JOY_CALIBRATE[3 * i] = 65535;
        JOY_CALIBRATE[3 * i + 1] = 32767;
        JOY_CALIBRATE[3 * i + 2] = 0;
        /* analogue steering and pedals, through the game's own analogue code (FUN_004183ec) */
        if (cfg.pad_analogue)
            JOY_ANALOGUE[i] = 1;
    }
}
