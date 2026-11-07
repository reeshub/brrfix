/* Smooth drawing without touching the physics. After each race tick, each vehicle's
 * drawn state (world position, angles, wheel heights) is compared with
 * the last snapshot; when it has changed, the old snapshot becomes "previous". Before
 * the race is drawn, a blend of previous and current (at the time of drawing, minus
 * a delay of two ticks, the physics step) is written into the vehicle records; after
 * drawing, the blended fields are put back exactly, so the physics never sees the
 * blend. (The draw itself also writes each vehicle's level-of-detail model, +0x13 and
 * +0x18..+0x57, chosen by distance from the camera: that is the game's own render
 * state and is left as the draw wrote it, as in the original.)
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

/* The follow camera (mode 4, the default) keeps its own position in its record
 * (+2/+6/+0xA), moved during the tick by camera_control; the draw only aims it at
 * the vehicle. That position is interpolated too, and put back after the draw. Other
 * modes compute their position from the (blended) vehicle while drawing. */
static const uint32_t camera_records[2] = {0x4529C0, 0x452A27};
struct cam_sample {
    int valid;
    int32_t prev[3], cur[3];
    int32_t prev_tick, cur_tick;
};
static struct cam_sample cams[2];
static int32_t cam_saved[2][3];
static int cam_saved_valid[2];

static int camera_mode(int c) { return *(uint16_t *)camera_records[c]; }
static void camera_pos(int c, int32_t out[3]) { memcpy(out, (void *)(camera_records[c] + 2), 12); }
static void set_camera_pos(int c, const int32_t in[3]) { memcpy((void *)(camera_records[c] + 2), in, 12); }
static int saved_valid[GAME_VEHICLE_COUNT];
static uint8_t changed_offsets[GAME_VEHICLE_SIZE];  /* restore check: offsets the draw changed */
static uint8_t before_draw_copy[GAME_VEHICLE_COUNT][GAME_VEHICLE_SIZE];
static int32_t last_tick_seen = -1;
static FILE *state_csv, *camera_csv;
static uint32_t frames_drawn, restore_problems;
/* why vehicles were or weren't blended, reported every 1,400 race frames */
static uint32_t n_blended, n_inactive, n_changed, n_current, n_teleport;

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
            fprintf(camera_csv, "frame,qpc_us,tick_count,fraction_milli,cam_x,cam_y,cam_z,cam_a1,cam_a2,cam_a3,cam_mode,cam_vehicle,drawn_x,drawn_z\n");
    }
}

void brr_interp_close(void)
{
    if (brr_patched && cfg.fix_interpolation)
        brr_log("interpolation: %lu race frames drawn; restore check: %lu vehicle offsets changed by the draw",
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
        memset(cams, 0, sizeof cams);
        brr_log("interpolation: race screen restarted at tick %ld", (long)tick);
    }
    last_tick_seen = tick;
    if (state_csv)
        log_state(tick);
    if (!cfg.fix_interpolation)
        return;
    for (int c = 0; c < 2; c++) {
        struct cam_sample *s = &cams[c];
        int32_t now[3];
        camera_pos(c, now);
        if (!s->valid) {
            s->valid = 1;
            memcpy(s->prev, now, 12);
            memcpy(s->cur, now, 12);
            s->prev_tick = s->cur_tick = tick;
        } else if (memcmp(now, s->cur, 12) != 0) {
            memcpy(s->prev, s->cur, 12);
            s->prev_tick = s->cur_tick;
            memcpy(s->cur, now, 12);
            s->cur_tick = tick;
        }
    }
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
        } else if (memcmp(now.pos, s->cur.pos, sizeof now.pos) != 0 ||
                   memcmp(now.angle, s->cur.angle, sizeof now.angle) != 0) {
            /* a physics step (position and angles change only then; wheel values change
             * on other ticks too, so they don't count as a new step) */
            s->prev = s->cur;
            s->prev_tick = s->cur_tick;
            s->cur = now;
            s->cur_tick = tick;
        } else {
            memcpy(s->cur.wheel, now.wheel, sizeof now.wheel);
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
        if (!s->valid || !active(i)) {
            n_inactive++;
            continue;
        }
        struct look now;
        read_look(i, &now);
        if (memcmp(&now, &s->cur, sizeof now) != 0) {
            n_changed++;
            continue;
        }  /* changed outside the tick (e.g. a reset): draw it as it is */
        int32_t t0 = s->prev_tick, t1 = s->cur_tick;
        if (t1 - t0 > 2)
            t0 = t1 - 2;  /* it had been still: it started moving one physics step ago */
        if (t1 <= t0 || shown >= (float)t1) {
            n_current++;
            continue;
        }  /* nothing to blend: the current state is right */
        float t = shown <= (float)t0 ? 0.0f : (shown - (float)t0) / (float)(t1 - t0);
        const struct look *a = &s->prev, *b = &s->cur;
        int teleport = 0;
        for (int k = 0; k < 3; k++) {
            int32_t d = b->pos[k] - a->pos[k];
            teleport |= d > TELEPORT || d < -TELEPORT;
        }
        if (teleport) {
            n_teleport++;
            continue;
        }
        n_blended++;
        struct look blend;
        memset(&blend, 0, sizeof blend);
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
    for (int c = 0; c < 2; c++) {
        struct cam_sample *s = &cams[c];
        cam_saved_valid[c] = 0;
        int32_t now[3];
        camera_pos(c, now);
        if (!s->valid || camera_mode(c) != 4 || memcmp(now, s->cur, 12) != 0)
            continue;
        int32_t t0 = s->prev_tick, t1 = s->cur_tick;
        if (t1 - t0 > 2)
            t0 = t1 - 2;
        if (t1 <= t0 || shown >= (float)t1)
            continue;
        float t = shown <= (float)t0 ? 0.0f : (shown - (float)t0) / (float)(t1 - t0);
        int32_t blend[3];
        int jump = 0;
        for (int k = 0; k < 3; k++) {
            int32_t d = s->cur[k] - s->prev[k];
            jump |= d > TELEPORT || d < -TELEPORT;
            blend[k] = lerp32(s->prev[k], s->cur[k], t);
        }
        if (jump)
            continue;
        memcpy(cam_saved[c], now, 12);
        cam_saved_valid[c] = 1;
        set_camera_pos(c, blend);
    }
}

/* After race_draw: put the true values back, and optionally check that the draw left
 * the vehicle records exactly as they were. */
void __cdecl brr_after_draw(void)
{
    uint8_t *camera = (uint8_t *)0x4529C0;  /* camera_view1 */
    uint32_t followed = *(uint32_t *)(camera + 0x40);
    int slot = (followed >= GAME_VEHICLES && followed < GAME_VEHICLES + GAME_VEHICLE_COUNT * GAME_VEHICLE_SIZE)
             ? (int)((followed - GAME_VEHICLES) / GAME_VEHICLE_SIZE) : -1;
    int32_t drawn_x = slot >= 0 ? *(int32_t *)vehicle(slot) : 0;
    int32_t drawn_z = slot >= 0 ? *(int32_t *)(vehicle(slot) + 8) : 0;
    if (cfg.fix_interpolation && frames_drawn % 1400 == 0) {
        brr_log("interpolation: last 1400 frames: vehicles blended %lu, current %lu, "
                "changed outside a tick %lu, inactive %lu, jumped %lu", (unsigned long)n_blended,
                (unsigned long)n_current, (unsigned long)n_changed, (unsigned long)n_inactive,
                (unsigned long)n_teleport);
        n_blended = n_current = n_changed = n_inactive = n_teleport = 0;
    }
    for (int i = 0; i < GAME_VEHICLE_COUNT; i++)
        if (saved_valid[i]) {
            write_look(i, &saved[i]);
            saved_valid[i] = 0;
        }
    for (int c = 0; c < 2; c++)
        if (cam_saved_valid[c]) {
            set_camera_pos(c, cam_saved[c]);
            cam_saved_valid[c] = 0;
        }
    if (camera_csv) {
        int32_t *pos = (int32_t *)GAME_CAMERA_POS;
        int16_t *ang = (int16_t *)GAME_CAMERA_ANGLES;
        int64_t us = brr_qpc() * 1000000 / brr_qpc_freq();
        fprintf(camera_csv, "%lu,%lld,%ld,%lu,%ld,%ld,%ld,%d,%d,%d,%d,%d,%ld,%ld\n", (unsigned long)frames_drawn,
                (long long)us, (long)*GAME_TICK_COUNT,
                (unsigned long)(brr_clock_frame_fraction() * 1000.0f), (long)pos[0], (long)pos[1],
                (long)pos[2], ang[0], ang[1], ang[2], *(uint16_t *)camera, slot, (long)drawn_x,
                (long)drawn_z);
    }
    if (cfg.check_restore) {
        for (int i = 0; i < GAME_VEHICLE_COUNT; i++)
            for (uint32_t b = 0; b < GAME_VEHICLE_SIZE; b++)
                if (vehicle(i)[b] != before_draw_copy[i][b] && !changed_offsets[b]) {
                    changed_offsets[b] = 1;
                    restore_problems++;
                    brr_log("restore check: frame %lu: the draw changed vehicle %d byte +0x%03lX "
                            "(first time this offset changed)", (unsigned long)frames_drawn, i,
                            (unsigned long)b);
                }
    }
}
