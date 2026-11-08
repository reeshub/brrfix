/* Big Red Racing DOS patch (Phase 3b): the hooks, interpolation, the roll fix and the
 * retrace wait. The design is Phase 3a's (src/dropin/interp.c and camera.c), in integers.
 *
 * Interpolation: after each race tick, each vehicle's drawn state (world position,
 * angles, wheel heights) is compared with the last snapshot; when it has changed (a
 * physics step: every other tick), the old snapshot becomes "previous". Before the race
 * is drawn, a blend of previous and current, two ticks (one physics step) behind, is
 * written into the vehicle records; straight after drawing, the true values are put back,
 * so the physics never sees the blend. The follow camera's own position is blended the
 * same way. The DOS timer only counts whole ticks, so the blend moves in half steps: 70
 * different pictures a second instead of 35.
 *
 * Retrace: before the game copies a finished frame to video memory, wait for the start
 * of the screen's vertical retrace (port 0x3DA, bit 3), so the copy never tears. Only
 * after a frame that took at most one tick (one refresh, at 320x200's 70 Hz): a machine
 * just short of 70 frames a second would otherwise wait a whole refresh every frame and
 * drop to 35. It gives up after two ticks of the 70 Hz timer, so it can't hang.
 *
 * Speed: brr_frame_start runs at the start of every frame (from run_screen, after it has
 * counted the ticks since the last frame). It keeps an average of ticks per frame and
 * calls the machine "fast" while that's 3 or fewer (over 23 frames a second; it becomes
 * "slow" again above 4). Only on a fast machine do the lower catch-up cap, interpolation
 * and the retrace wait apply. A 486, at 5 to 18 ticks a frame, runs exactly as the
 * original: the cap of 8 would slow its game down, there's nothing to blend between, and
 * waiting for the retrace would only cost it time. */
#include "brrdos.h"

#define WHEELS   8
#define TELEPORT 4000   /* world units between snapshots: a reset, not motion */
#define DELAY    2      /* ticks: one physics step */
#define SUB      16     /* time in 1/16 tick */
#define HISTORY  64
#define ROLL_WINDOW (7 * SUB)   /* the 7 ticks of a frame at the developers' 10 fps */

int brr_in_race_draw, brr_fine_active, brr_fast;
u32 brr_frames_drawn;
static s32 average_ticks;   /* ticks per frame, in 1/16, a moving average; 0 = not yet */

struct look {
    s32 pos[3];          /* +0x000 world position */
    s16 angle[3];        /* +0x00C angles */
    s32 wheel[WHEELS];   /* +0x252, every 8 bytes: wheel heights */
};

struct sample {
    int valid;
    struct look prev, cur;
    s32 prev_tick, cur_tick;
};

struct cam_sample {
    int valid;
    s32 prev[3], cur[3];
    s32 prev_tick, cur_tick;
};

struct view_state {      /* the roll fix: the drawn yaw over the last ticks */
    int valid, mode;
    s32 last_time;       /* 1/16 ticks */
    s16 last_yaw;
    s32 unwrapped;
    s32 hist_time[HISTORY], hist_yaw[HISTORY];
    int hist_next, hist_count;
};

static struct sample samples[VEHICLE_COUNT];
static struct look saved[VEHICLE_COUNT];
static int saved_valid[VEHICLE_COUNT];
static struct cam_sample cams[2];
static s32 cam_saved[2][3];
static int cam_saved_valid[2];
static s32 last_tick_seen;
static struct view_state views[2];
static s32 game_pos[3];
static s16 game_angles[3];
static int have_game_camera;

s32 sar(s32 x, int n)    /* arithmetic shift right, as the x86 SAR does it */
{
    return x >= 0 ? x >> n : -(s32)((u32)(-(x + 1)) >> n) - 1;
}

static void copy(void *to, const void *from, u32 n)
{
    u8 *d = (u8 *)to;
    const u8 *s = (const u8 *)from;
    while (n--)
        *d++ = *s++;
}

static int same(const void *a, const void *b, u32 n)
{
    const u8 *x = (const u8 *)a, *y = (const u8 *)b;
    while (n--)
        if (*x++ != *y++)
            return 0;
    return 1;
}

static void clear(void *to, u32 n)
{
    u8 *d = (u8 *)to;
    while (n--)
        *d++ = 0;
}

static u8 *vehicle(int i) { return game_vehicles + (u32)i * VEHICLE_SIZE; }
static int active(int i) { return *(s16 *)(vehicle(i) + 0x26) >= 0; }
static u8 *camera_record(int c) { return c ? game_camera_view2 : game_camera_view1; }

static void read_look(int i, struct look *out)
{
    u8 *v = vehicle(i);
    int w;
    clear(out, sizeof *out);
    copy(out->pos, v, 12);
    copy(out->angle, v + 0x0C, 6);
    for (w = 0; w < WHEELS; w++)
        copy(&out->wheel[w], v + 0x252 + 8 * w, 4);
}

static void write_look(int i, const struct look *in)
{
    u8 *v = vehicle(i);
    int w;
    copy(v, in->pos, 12);
    copy(v + 0x0C, in->angle, 6);
    for (w = 0; w < WHEELS; w++)
        copy(v + 0x252 + 8 * w, &in->wheel[w], 4);
}

/* a + (b - a) * t / 256, rounded down, without overflowing 32 bits */
static s32 lerp32(s32 a, s32 b, s32 t)
{
    s32 d = b - a;
    return a + sar(d, 8) * t + (((d & 255) * t) >> 8);
}

static s16 lerp_angle(s16 a, s16 b, s32 t)
{
    s32 d = (s16)(u16)(b - a);  /* the shortest way round */
    return (s16)(u16)(a + sar(d * t, 8));
}

static void reset_samples(void)
{
    clear(samples, sizeof samples);
    clear(cams, sizeof cams);
    clear(views, sizeof views);
    clear(saved_valid, sizeof saved_valid);
    clear(cam_saved_valid, sizeof cam_saved_valid);
    last_tick_seen = 0;
}

/* ---- the race screen's four routines, wrapped ---- */

void brr_race_enter(void)
{
    reset_samples();
    if (OPTION(OPT_LOG))
        log_race_enter();
    game_race_enter();
}

void brr_race_leave(void)
{
    if (OPTION(OPT_LOG))
        log_race_leave();
    game_race_leave();
}

static void after_tick(void)
{
    s32 tick = game_tick_count;
    int i, c;
    if (tick < last_tick_seen)
        reset_samples();
    last_tick_seen = tick;
    for (c = 0; c < 2; c++) {
        struct cam_sample *s = &cams[c];
        s32 now[3];
        copy(now, camera_record(c) + 2, 12);
        if (!s->valid) {
            s->valid = 1;
            copy(s->prev, now, 12);
            copy(s->cur, now, 12);
            s->prev_tick = s->cur_tick = tick;
        } else if (!same(now, s->cur, 12)) {
            copy(s->prev, s->cur, 12);
            s->prev_tick = s->cur_tick;
            copy(s->cur, now, 12);
            s->cur_tick = tick;
        }
    }
    for (i = 0; i < VEHICLE_COUNT; i++) {
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
        } else if (!same(now.pos, s->cur.pos, sizeof now.pos) ||
                   !same(now.angle, s->cur.angle, sizeof now.angle)) {
            /* a physics step (position and angles change only then; wheel values
             * change on other ticks too, so they don't count as a new step) */
            s->prev = s->cur;
            s->prev_tick = s->cur_tick;
            s->cur = now;
            s->cur_tick = tick;
        } else {
            copy(s->cur.wheel, now.wheel, sizeof now.wheel);
        }
    }
}

void brr_race_tick(void)
{
    if (OPTION(OPT_LOG))
        log_before_tick();
    game_race_tick();
    after_tick();
    if (OPTION(OPT_LOG))
        log_after_tick();
}

/* the blend fraction for a sample, in 1/256, or -1 for "draw it as it is" */
static s32 fraction(s32 shown, s32 t0, s32 t1)
{
    if (t1 - t0 > DELAY)
        t0 = t1 - DELAY;  /* it had been still: it started moving one physics step ago */
    if (t1 <= t0 || shown >= t1 * SUB)
        return -1;
    if (shown <= t0 * SUB)
        return 0;
    return (shown - t0 * SUB) * (256 / SUB) / (t1 - t0);
}

static int before_draw(void)
{
    s32 shown = (game_tick_count - DELAY) * SUB;
    int i, c, blended = 0;
    for (i = 0; i < VEHICLE_COUNT; i++)
        saved_valid[i] = 0;
    for (c = 0; c < 2; c++)
        cam_saved_valid[c] = 0;
    if (!OPTION(OPT_INTERPOLATION) || !brr_fast)
        return 0;
    for (i = 0; i < VEHICLE_COUNT; i++) {
        struct sample *s = &samples[i];
        struct look now, blend;
        s32 t;
        int k, w, jump = 0;
        if (!s->valid || !active(i))
            continue;
        read_look(i, &now);
        if (!same(&now, &s->cur, sizeof now))
            continue;  /* changed outside the tick (a reset): draw it as it is */
        t = fraction(shown, s->prev_tick, s->cur_tick);
        if (t < 0)
            continue;
        for (k = 0; k < 3; k++) {
            s32 d = s->cur.pos[k] - s->prev.pos[k];
            jump |= d > TELEPORT || d < -TELEPORT;
        }
        if (jump)
            continue;
        clear(&blend, sizeof blend);
        for (k = 0; k < 3; k++) {
            blend.pos[k] = lerp32(s->prev.pos[k], s->cur.pos[k], t);
            blend.angle[k] = lerp_angle(s->prev.angle[k], s->cur.angle[k], t);
        }
        for (w = 0; w < WHEELS; w++)
            blend.wheel[w] = lerp32(s->prev.wheel[w], s->cur.wheel[w], t);
        saved[i] = now;
        saved_valid[i] = 1;
        write_look(i, &blend);
        blended++;
    }
    for (c = 0; c < 2; c++) {
        struct cam_sample *s = &cams[c];
        u8 *record = camera_record(c);
        s32 now[3], blend[3], t;
        int k, jump = 0;
        copy(now, record + 2, 12);
        if (!s->valid || *(u16 *)record != 4 || !same(now, s->cur, 12))
            continue;
        t = fraction(shown, s->prev_tick, s->cur_tick);
        if (t < 0)
            continue;
        for (k = 0; k < 3; k++) {
            s32 d = s->cur[k] - s->prev[k];
            jump |= d > TELEPORT || d < -TELEPORT;
            blend[k] = lerp32(s->prev[k], s->cur[k], t);
        }
        if (jump)
            continue;
        copy(cam_saved[c], now, 12);
        cam_saved_valid[c] = 1;
        copy(record + 2, blend, 12);
    }
    return blended;
}

static void restore(void)
{
    int i, c;
    for (i = 0; i < VEHICLE_COUNT; i++)
        if (saved_valid[i]) {
            write_look(i, &saved[i]);
            saved_valid[i] = 0;
        }
    for (c = 0; c < 2; c++)
        if (cam_saved_valid[c]) {
            copy(camera_record(c) + 2, cam_saved[c], 12);
            cam_saved_valid[c] = 0;
        }
    if (have_game_camera) {
        copy(game_camera_pos, game_pos, sizeof game_pos);
        copy(game_camera_angles, game_angles, sizeof game_angles);
        have_game_camera = 0;
    }
}

void brr_race_draw(void)
{
    int blended;
    brr_frames_drawn++;
    have_game_camera = 0;
    if (OPTION(OPT_LOG))
        log_before_draw();
    blended = before_draw();
    brr_in_race_draw = 1;
    brr_fine_active = OPTION(OPT_FINE_ANGLES) != 0;
    game_race_draw();
    brr_fine_active = 0;
    brr_in_race_draw = 0;
    if (OPTION(OPT_LOG))
        log_after_draw(blended);
    restore();
    if (OPTION(OPT_LOG))
        log_after_draw(-1);
}

/* ---- the camera, at draw time: fine angles and the roll fix ---- */

static s32 yaw_at(struct view_state *v, s32 when)
{
    /* the recorded continuous yaw at time `when`, interpolated; the oldest if too old */
    int n = v->hist_count, newest = (v->hist_next + HISTORY - 1) % HISTORY, k;
    s32 best = v->hist_yaw[(v->hist_next + HISTORY - n) % HISTORY];
    for (k = 0; k < n - 1; k++) {
        int b = (newest - k + HISTORY) % HISTORY, a = (b + HISTORY - 1) % HISTORY;
        if (v->hist_time[a] <= when && when <= v->hist_time[b]) {
            s32 span = v->hist_time[b] - v->hist_time[a];
            if (span <= 0)
                return v->hist_yaw[b];
            return v->hist_yaw[a] + (v->hist_yaw[b] - v->hist_yaw[a]) * (when - v->hist_time[a]) / span;
        }
    }
    return best;
}

/* render_view's call to camera_update comes here. The game works out its camera as
 * usual; then, during the race draw only, the follow camera's pitch and yaw are refined
 * (fine angles) and its roll is worked out from the yaw's change over 7 ticks (roll fix).
 * The game's own values are put back after the draw. */
void brr_camera_update(u8 *record)
{
    struct view_state *v;
    int index, mode;
    s32 now;
    s16 yaw;
    game_camera_update(record);
    if (!brr_in_race_draw)
        return;
    index = record == game_camera_view1 ? 0 : record == game_camera_view2 ? 1 : -1;
    if (index < 0)
        return;
    if (!have_game_camera) {
        copy(game_pos, game_camera_pos, sizeof game_pos);
        copy(game_angles, game_camera_angles, sizeof game_angles);
        have_game_camera = 1;
    }
    if (OPTION(OPT_FINE_ANGLES))
        brr_fine_camera_angles(record, game_camera_angles);
    if (!OPTION(OPT_ROLL_FIX))
        return;
    v = &views[index];
    mode = *(u16 *)record;
    now = game_tick_count * SUB;
    yaw = game_camera_angles[1];
    if (!v->valid || v->mode != mode || now < v->last_time || now - v->last_time > 35 * SUB) {
        clear(v, sizeof *v);
        v->valid = 1;
        v->mode = mode;
        v->last_yaw = yaw;
        v->unwrapped = yaw;
        v->last_time = now;
    }
    v->unwrapped += (s16)(u16)(yaw - v->last_yaw);
    v->last_yaw = yaw;
    v->last_time = now;
    v->hist_time[v->hist_next] = now;
    v->hist_yaw[v->hist_next] = v->unwrapped;
    v->hist_next = (v->hist_next + 1) % HISTORY;
    if (v->hist_count < HISTORY)
        v->hist_count++;
    if (mode == 4) {
        /* 4 x yaw change per tick, measured over the last 7 ticks (or what we have) */
        s32 oldest = v->hist_time[(v->hist_next + HISTORY - v->hist_count) % HISTORY];
        s32 span = now - oldest < ROLL_WINDOW ? now - oldest : ROLL_WINDOW;
        s32 roll = span > SUB / 2 ? 4 * SUB * (yaw_at(v, now - span) - v->unwrapped) / span : 0;
        if (roll > 32767)
            roll = 32767;
        if (roll < -32768)
            roll = -32768;
        game_camera_angles[2] = (s16)roll;
    }
}

/* ---- every frame: the catch-up cap and the machine's speed ---- */

/* run_screen has just stored the ticks since the last frame (8-bit) in frame_ticks; the
 * original then capped them at 35. Called through brr_cap in thunks.asm, which keeps
 * every register as it was. */
void brr_frame_start(void)
{
    u32 ticks = game_frame_ticks, cap = OPTION(OPT_CAP);
    if (ticks > 35)
        ticks = 35;
    if (brr_fast && ticks > cap)
        ticks = cap;
    game_frame_ticks = (u8)ticks;
    if (!average_ticks)
        average_ticks = 8 * 16;   /* start as slow: a few frames tell */
    average_ticks += ((s32)ticks * 16 - average_ticks) / 8;
    if (brr_fast && average_ticks > 4 * 16)
        brr_fast = 0;
    else if (!brr_fast && average_ticks <= 3 * 16)
        brr_fast = 1;
}

/* ---- the retrace wait, before the frame is copied to video memory ---- */

static void wait_retrace(void)
{
    u16 start = game_timer_ticks;
    u32 polls = 0;
    /* if a retrace is under way, let it end, so the copy starts at the beginning of one */
    while ((port_in(0x3DA) & 8) && (u16)(game_timer_ticks - start) < 3 && ++polls < 0x400000u)
        ;
    while (!(port_in(0x3DA) & 8) && (u16)(game_timer_ticks - start) < 3 && ++polls < 0x400000u)
        ;
}

void brr_present(void)
{
    if (OPTION(OPT_RETRACE) && brr_fast && game_frame_ticks <= 1)
        wait_retrace();
    game_copy_frame();
}

void brr_present_split(void)
{
    if (OPTION(OPT_RETRACE) && brr_fast && game_frame_ticks <= 1)
        wait_retrace();
    game_copy_frame_split();
}
