/* Camera fixes and smoothing, at draw time only. render_view's call to camera_update
 * is redirected here: the game computes its camera as usual, then, for drawing only,
 *  - the follow camera's roll (mode 4, the default "Elasticam") is recomputed from the
 *    yaw rate over 7 ticks of real time. The game banks it by 4 x (yaw change since the
 *    last frame that ran a tick) / ticks: an average over 7 ticks at the 10 fps of 1996,
 *    but over a single tick of a yaw quantised to 1/1024 of a turn at today's frame
 *    rates, so it flicks between 0 and 2 degrees about 35 times a second;
 *  - with camera_smoothing > 0, a time-based low-pass filter on height, pitch and roll
 *    (yaw follows four times faster) takes out the truck's own jitter, which the camera
 *    is welded to.
 * The result goes into the global camera position and angles the renderer reads (no
 * tick code reads them); the game's own values are put back after the race is drawn. */
#include "brrfix.h"
#include <string.h>

#define CAMERA_POS    ((int32_t *)GAME_CAMERA_POS)     /* x, y, z */
#define CAMERA_ANGLES ((int16_t *)GAME_CAMERA_ANGLES)  /* pitch, yaw, roll */
#define HISTORY 64
#define ROLL_WINDOW 7.0f  /* ticks: one frame at the developers' 10 fps */

struct view_state {
    int valid, mode;
    float last_time;           /* ticks */
    float y, pitch, yaw, roll; /* filtered */
    float unwrapped_yaw;       /* continuous yaw for rates */
    int16_t last_yaw;
    float hist_time[HISTORY], hist_yaw[HISTORY];
    int hist_next, hist_count;
};

static struct view_state views[2];
static int32_t game_pos[3];
static int16_t game_angles[3];
static int have_game_values;
static uint32_t frames_smoothed;

static float wrap_angle(float a)  /* to -32768..32767 */
{
    while (a >= 32768.0f) a -= 65536.0f;
    while (a < -32768.0f) a += 65536.0f;
    return a;
}

/* first-order low-pass, time-based: the same feel at any frame rate */
static float follow(float current, float target, float dt_ticks, float tau_ticks)
{
    if (tau_ticks <= 0.0f)
        return target;
    return current + (target - current) * (dt_ticks / (tau_ticks + dt_ticks));
}

static float yaw_at(struct view_state *v, float when)
{
    /* the recorded continuous yaw at time `when`, interpolated; oldest if too old */
    int n = v->hist_count, newest = (v->hist_next + HISTORY - 1) % HISTORY;
    float best = v->hist_yaw[(v->hist_next + HISTORY - n) % HISTORY];
    for (int k = 0; k < n - 1; k++) {
        int b = (newest - k + HISTORY) % HISTORY, a = (b + HISTORY - 1) % HISTORY;
        if (v->hist_time[a] <= when && when <= v->hist_time[b]) {
            float span = v->hist_time[b] - v->hist_time[a];
            float t = span > 0.0f ? (when - v->hist_time[a]) / span : 1.0f;
            return v->hist_yaw[a] + (v->hist_yaw[b] - v->hist_yaw[a]) * t;
        }
    }
    return best;
}

void __cdecl brr_after_camera_update(uint32_t record)
{
    int index = record == 0x4529C0u ? 0 : record == 0x452A27u ? 1 : -1;
    if (index < 0)
        return;
    memcpy(game_pos, CAMERA_POS, sizeof game_pos);  /* the game's own, for after the draw */
    memcpy(game_angles, CAMERA_ANGLES, sizeof game_angles);
    have_game_values = 1;
    struct view_state *v = &views[index];
    int mode = *(uint16_t *)record;
    float now = (float)*GAME_TICK_COUNT + brr_clock_frame_fraction();
    float y = (float)CAMERA_POS[1];
    int16_t yaw16 = CAMERA_ANGLES[1];
    if (!v->valid || v->mode != mode || now < v->last_time || now - v->last_time > 35.0f) {
        memset(v, 0, sizeof *v);
        v->valid = 1;
        v->mode = mode;
        v->y = y;
        v->pitch = CAMERA_ANGLES[0];
        v->yaw = yaw16;
        v->roll = CAMERA_ANGLES[2];
        v->unwrapped_yaw = yaw16;
        v->last_yaw = yaw16;
        v->last_time = now;
    }
    float dt = now - v->last_time;
    v->unwrapped_yaw += (float)(int16_t)(yaw16 - v->last_yaw);
    v->last_yaw = yaw16;
    v->hist_time[v->hist_next] = now;
    v->hist_yaw[v->hist_next] = v->unwrapped_yaw;
    v->hist_next = (v->hist_next + 1) % HISTORY;
    if (v->hist_count < HISTORY)
        v->hist_count++;

    float roll_target = CAMERA_ANGLES[2];
    if (cfg.fix_camera && mode == 4) {
        /* 4 x yaw change per tick, measured over the last 7 ticks (or what we have) */
        float oldest = v->hist_time[(v->hist_next + HISTORY - v->hist_count) % HISTORY];
        float span = now - oldest < ROLL_WINDOW ? now - oldest : ROLL_WINDOW;
        roll_target = span > 0.5f ? 4.0f * (yaw_at(v, now - span) - v->unwrapped_yaw) / span : 0.0f;
    }
    float tau = (float)cfg.camera_smoothing / 10.0f;  /* time constant in ticks: 50 -> 5 ticks = 71 ms */
    float y_target = y, pitch_target = CAMERA_ANGLES[0];
    if (cfg.camera_smoothing > 0) {
        v->y = follow(v->y, y_target, dt, tau);
        v->pitch = v->pitch + wrap_angle(pitch_target - v->pitch) * (tau > 0 ? dt / (tau + dt) : 1.0f);
        v->roll = follow(v->roll, roll_target, dt, tau);
        v->yaw = v->yaw + wrap_angle((float)yaw16 - v->yaw) * (dt / (tau / 4.0f + dt));
    } else {
        v->y = y_target;
        v->pitch = pitch_target;
        v->roll = roll_target;
        v->yaw = yaw16;
    }
    v->last_time = now;
    if (cfg.fix_camera || cfg.camera_smoothing > 0) {
        CAMERA_POS[1] = (int32_t)(v->y >= 0 ? v->y + 0.5f : v->y - 0.5f);
        CAMERA_ANGLES[0] = (int16_t)(int32_t)wrap_angle(v->pitch);
        CAMERA_ANGLES[1] = (int16_t)(int32_t)wrap_angle(v->yaw);
        CAMERA_ANGLES[2] = (int16_t)(int32_t)wrap_angle(v->roll);
        frames_smoothed++;
    }
}

/* After the race is drawn: put back the game's own last camera values. */
void brr_camera_restore(void)
{
    if (!have_game_values)
        return;
    memcpy(CAMERA_POS, game_pos, sizeof game_pos);
    memcpy(CAMERA_ANGLES, game_angles, sizeof game_angles);
    have_game_values = 0;
}

void brr_camera_before_draw(void) { have_game_values = 0; }
