/* Smooth drawing without touching the physics. After each race tick, each vehicle's
 * drawn state (world position, angles, wheel heights) is compared with
 * the last snapshot; when it has changed, the old snapshot becomes "previous". Before
 * the race is drawn, a blend of previous and current (at the time of drawing, minus
 * a delay of two ticks, the physics step) is written into the vehicle records; after
 * drawing, the true values are put back, so the physics never sees the blend.
 * Also writes the optional verification logs: per-tick vehicle state (BRRFIX-STATE.CSV)
 * and per-frame camera (BRRFIX-CAMERA.CSV). */
#include "brrfix.h"
#include <stdio.h>
#include <string.h>

#define WHEELS 8
#define TELEPORT 4000  /* world units between snapshots: a reset, not motion */

struct look {
    int32_t pos[3];        /* +0x000 world position */
    int16_t angle[3];      /* +0x00C angles */
    int32_t wheel[WHEELS]; /* +0x252, every 8 bytes: wheel heights */
};

struct sample {
    int valid;
    struct look prev, cur;
    int32_t prev_tick, cur_tick;
};

static struct sample samples[GAME_VEHICLE_COUNT];
static struct look saved[GAME_VEHICLE_COUNT];
static int saved_valid[GAME_VEHICLE_COUNT];
static uint8_t before_draw_copy[GAME_VEHICLE_COUNT][GAME_VEHICLE_SIZE];
static int32_t last_tick_seen = -1;
static FILE *state_csv, *camera_csv;
static uint32_t frames_drawn, restore_problems;

static uint8_t *vehicle(int i) { return (uint8_t *)(GAME_VEHICLES + (uint32_t)i * GAME_VEHICLE_SIZE); }
static int active(int i) { return *(int16_t *)(vehicle(i) + 0x26) >= 0; }

static void read_look(int i, struct look *out)
{
    uint8_t *v = vehicle(i);
    memset(out, 0, sizeof *out);  /* padding too, for memcmp */
    memcpy(out->pos, v, 12);
    memcpy(out->angle, v + 0x0C, 6);
    for (int w = 0; w < WHEELS; w++)
        memcpy(&out->wheel[w], v + 0x252 + 8 * w, 4);
}

static void write_look(int i, const struct look *in)
{
    uint8_t *v = vehicle(i);
    memcpy(v, in->pos, 12);
    memcpy(v + 0x0C, in->angle, 6);
    for (int w = 0; w < WHEELS; w++)
        memcpy(v + 0x252 + 8 * w, &in->wheel[w], 4);
}

static int32_t lerp32(int32_t a, int32_t b, float t) { return a + (int32_t)((float)(b - a) * t); }
static int16_t lerp_angle(int16_t a, int16_t b, float t)
{
    int16_t d = (int16_t)(b - a);  /* shortest way round */
    return (int16_t)(a + (int16_t)((float)d * t));
}

static uint32_t fnv1a(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    while (n--)
        h = (h ^ *p++) * 16777619u;
    return h;
}

void brr_interp_setup(void)
{
    char path[MAX_PATH];
    if (!brr_patched)
        return;
    if (cfg.log_state) {
        brr_path_beside_dll(path, sizeof path, "BRRFIX-STATE.CSV");
        state_csv = fopen(path, "w");
        if (state_csv)
            fprintf(state_csv, "tick,slot,state,x,y,z,o1,o2,o3,fwd,side,spin1,spin2,spin3,contacts,record_fnv1a\n");
    }
    if (cfg.log_camera) {
        brr_path_beside_dll(path, sizeof path, "BRRFIX-CAMERA.CSV");
        camera_csv = fopen(path, "w");
        if (camera_csv)
            fprintf(camera_csv, "frame,qpc_us,tick_count,fraction_milli,cam_x,cam_y,cam_z,cam_a1,cam_a2,cam_a3\n");
    }
}

void brr_interp_close(void)
{
    if (brr_patched && cfg.fix_interpolation)
        brr_log("interpolation: %lu race frames drawn; restore check problems: %lu",
                (unsigned long)frames_drawn, (unsigned long)restore_problems);
    if (state_csv) { fclose(state_csv); state_csv = NULL; }
    if (camera_csv) { fclose(camera_csv); camera_csv = NULL; }
}

static void log_state(int32_t tick)
{
    for (int i = 0; i < GAME_VEHICLE_COUNT; i++) {
        if (!active(i))
            continue;
        uint8_t *v = vehicle(i);
        fprintf(state_csv, "%ld,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%d,%08lx\n",
                (long)tick, i, *(int16_t *)(v + 0x30),
                (long)*(int32_t *)(v + 0x242), (long)*(int32_t *)(v + 0x246), (long)*(int32_t *)(v + 0x24A),
                (long)*(int32_t *)(v + 0x224), (long)*(int32_t *)(v + 0x228), (long)*(int32_t *)(v + 0x22C),
                (long)*(int32_t *)(v + 0x23A), (long)*(int32_t *)(v + 0x23E),
                (long)*(int32_t *)(v + 0x218), (long)*(int32_t *)(v + 0x21C), (long)*(int32_t *)(v + 0x220),
                *(uint16_t *)(v + 0x2D7), (unsigned long)fnv1a(v, GAME_VEHICLE_SIZE));
    }
}

/* After every race tick (called by brr_hook_race_tick). */
void __cdecl brr_after_tick(void)
{
    int32_t tick = *GAME_TICK_COUNT;
    if (tick < last_tick_seen) {  /* a new race screen: start again */
        memset(samples, 0, sizeof samples);
        brr_log("interpolation: race screen restarted at tick %ld", (long)tick);
    }
    last_tick_seen = tick;
    if (state_csv)
        log_state(tick);
    if (!cfg.fix_interpolation)
        return;
    for (int i = 0; i < GAME_VEHICLE_COUNT; i++) {
        struct sample *s = &samples[i];
        struct look now;
        if (!active(i)) {
            s->valid = 0;
            continue;
        }
        read_look(i, &now);
        if (!s->valid) {
            s->valid = 1;
            s->prev = s->cur = now;
            s->prev_tick = s->cur_tick = tick;
        } else if (memcmp(&now, &s->cur, sizeof now) != 0) {
            s->prev = s->cur;
            s->prev_tick = s->cur_tick;
            s->cur = now;
            s->cur_tick = tick;
        }
    }
}

/* Before race_draw (called by brr_hook_race_draw). */
void __cdecl brr_before_draw(void)
{
    frames_drawn++;
    if (cfg.check_restore)
        for (int i = 0; i < GAME_VEHICLE_COUNT; i++)
            memcpy(before_draw_copy[i], vehicle(i), GAME_VEHICLE_SIZE);
    if (!cfg.fix_interpolation)
        return;
    /* the moment being drawn, in ticks since the race screen started */
    float shown = (float)*GAME_TICK_COUNT + brr_clock_frame_fraction()
                - (float)cfg.interp_delay_milli / 1000.0f;
    for (int i = 0; i < GAME_VEHICLE_COUNT; i++) {
        struct sample *s = &samples[i];
        saved_valid[i] = 0;
        if (!s->valid || !active(i))
            continue;
        struct look now;
        read_look(i, &now);
        if (memcmp(&now, &s->cur, sizeof now) != 0)
            continue;  /* changed outside the tick (e.g. a reset): draw it as it is */
        int32_t t0 = s->prev_tick, t1 = s->cur_tick;
        if (t1 - t0 > 2)
            t0 = t1 - 2;  /* it had been still: it started moving one physics step ago */
        if (t1 <= t0 || shown >= (float)t1)
            continue;  /* nothing to blend: the current state is right */
        float t = shown <= (float)t0 ? 0.0f : (shown - (float)t0) / (float)(t1 - t0);
        const struct look *a = &s->prev, *b = &s->cur;
        int teleport = 0;
        for (int k = 0; k < 3; k++) {
            int32_t d = b->pos[k] - a->pos[k];
            teleport |= d > TELEPORT || d < -TELEPORT;
        }
        if (teleport)
            continue;
        struct look blend;
        for (int k = 0; k < 3; k++) {
            blend.pos[k] = lerp32(a->pos[k], b->pos[k], t);
            blend.angle[k] = lerp_angle(a->angle[k], b->angle[k], t);
        }
        for (int w = 0; w < WHEELS; w++)
            blend.wheel[w] = lerp32(a->wheel[w], b->wheel[w], t);
        saved[i] = now;
        saved_valid[i] = 1;
        write_look(i, &blend);
    }
}

/* After race_draw: put the true values back, and optionally check that the draw left
 * the vehicle records exactly as they were. */
void __cdecl brr_after_draw(void)
{
    for (int i = 0; i < GAME_VEHICLE_COUNT; i++)
        if (saved_valid[i]) {
            write_look(i, &saved[i]);
            saved_valid[i] = 0;
        }
    if (camera_csv) {
        int32_t *pos = (int32_t *)GAME_CAMERA_POS;
        int16_t *ang = (int16_t *)GAME_CAMERA_ANGLES;
        int64_t us = brr_qpc() * 1000000 / brr_qpc_freq();
        fprintf(camera_csv, "%lu,%lld,%ld,%lu,%ld,%ld,%ld,%d,%d,%d\n", (unsigned long)frames_drawn,
                (long long)us, (long)*GAME_TICK_COUNT,
                (unsigned long)(brr_clock_frame_fraction() * 1000.0f), (long)pos[0], (long)pos[1],
                (long)pos[2], ang[0], ang[1], ang[2]);
    }
    if (cfg.check_restore)
        for (int i = 0; i < GAME_VEHICLE_COUNT; i++)
            for (uint32_t b = 0; b < GAME_VEHICLE_SIZE; b++)
                if (vehicle(i)[b] != before_draw_copy[i][b]) {
                    restore_problems++;
                    if (restore_problems <= 20)
                        brr_log("restore check: frame %lu vehicle %d byte +0x%03lX changed during the draw",
                                (unsigned long)frames_drawn, i, (unsigned long)b);
                    break;
                }
}
