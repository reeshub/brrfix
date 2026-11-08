/* Big Red Racing DOS patch (Phase 3b): the logging build, for verification only.
 * Patched in only with dos-patch --log (or --input, --seed, --capture, --prove); a
 * player's build never runs any of this. Files, in the game's folder:
 *  - BRRSTATE.CSV: every active vehicle's physics state after every race tick (the same
 *    columns as Phase 3a's BRRFIX-STATE.CSV, so "brr.py state-compare" reads it);
 *  - BRRCAM.CSV: the drawn camera, every race frame;
 *  - BRRCAP.BIN: --capture N@T: N frames from race tick T, kept in memory and written at
 *    the end (the stutter meter's format: 64-byte header, 1,024-byte palette, pixels);
 *  - BRRLOG.TXT: what happened, including the restore check: the vehicle records after
 *    each race draw (and the patch's restore) compared with before it, logging each byte
 *    offset the first time it differs.
 * Writes go straight to DOS (int 21h), so they don't touch the game's C library state. */
#include "brrdos.h"

#define BUFFER 4096

struct out {
    s32 handle;
    u32 used;
    char data[BUFFER];
};

static struct out log_file, state_file, camera_file;
static int files_open, input_held[INPUT_MAX], proved;
static u8 before_copy[VEHICLE_COUNT][VEHICLE_SIZE];
static u8 offset_seen[VEHICLE_SIZE];
static u32 restore_changes, race_frames, capture_count, capture_record;
static u8 *capture;

static void flush(struct out *o)
{
    if (o->handle > 0 && o->used)
        dos_write(o->handle, o->data, o->used);
    o->used = 0;
}

static void commit(struct out *o)   /* flush, and make DOS write it to disk */
{
    flush(o);
    if (o->handle > 0)
        dos_commit(o->handle);
}

static void put(struct out *o, const char *text)
{
    while (*text) {
        if (o->used == BUFFER)
            flush(o);
        o->data[o->used++] = *text++;
    }
}

static void put_number(struct out *o, s32 value)
{
    char digits[12], *p = digits + sizeof digits - 1;
    u32 v = value < 0 ? (u32)-value : (u32)value;
    *p = 0;
    do {
        *--p = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    if (value < 0)
        *--p = '-';
    put(o, p);
}

static void put_hex(struct out *o, u32 value)
{
    char digits[9];
    int i;
    for (i = 7; i >= 0; i--, value >>= 4)
        digits[i] = "0123456789abcdef"[value & 15];
    digits[8] = 0;
    put(o, digits);
}

static void field(struct out *o, s32 value)
{
    put_number(o, value);
    put(o, ",");
}

void log_text(const char *text) { put(&log_file, text); }
void log_number(s32 value) { put_number(&log_file, value); }
void log_line(void)
{
    put(&log_file, "\r\n");
    commit(&log_file);
}

static void open_files(void)
{
    if (files_open)
        return;
    files_open = 1;
    log_file.handle = dos_create("BRRLOG.TXT");
    state_file.handle = dos_create("BRRSTATE.CSV");
    camera_file.handle = dos_create("BRRCAM.CSV");
    put(&state_file, "tick,slot,state,x,y,z,o1,o2,o3,fwd,side,spin1,spin2,spin3,contacts,record_fnv1a\r\n");
    put(&camera_file, "frame,tick_count,frame_ticks,blended,cam_x,cam_y,cam_z,cam_a1,cam_a2,cam_a3,"
                      "cam_mode,cam_vehicle,drawn_x,drawn_z,fast\r\n");
    log_text("Big Red Racing DOS patch, logging build: ");
    log_text(brr_tag);
    log_line();
}

static u8 *vehicle(int i) { return game_vehicles + (u32)i * VEHICLE_SIZE; }

void log_race_enter(void)
{
    u32 i;
    open_files();
    if (OPTION(OPT_SEED_SET))
        game_rand_seed = OPTION(OPT_SEED);
    log_text("race: enter, random seed ");
    log_number((s32)game_rand_seed);
    log_line();
    for (i = 0; i < brr_input_count && i < INPUT_MAX; i++) {
        log_text("input: key ");
        log_number((s32)brr_input[i].key);
        log_text(" held for race ticks ");
        log_number(brr_input[i].first);
        log_text("-");
        log_number(brr_input[i].last);
        log_line();
    }
}

void log_race_leave(void)
{
    log_text("race: leave after ");
    log_number((s32)race_frames);
    log_text(" race frames; restore check: ");
    log_number((s32)restore_changes);
    log_text(" vehicle byte offsets changed by the draw");
    log_line();
    commit(&state_file);
    commit(&camera_file);
}

/* before each race tick: the scheduled keys, as a player would press them */
void log_before_tick(void)
{
    s32 tick = game_tick_count;   /* already counts the tick about to run */
    u32 i;
    for (i = 0; i < brr_input_count && i < INPUT_MAX; i++) {
        u32 key = brr_input[i].key;
        int on = tick >= brr_input[i].first && tick <= brr_input[i].last;
        u8 code = key >= 0x100 ? game_control_set[key - 0x100] : (u8)key;
        if (on)
            game_key_table[code] = code ? code : 1;
        else if (input_held[i])
            game_key_table[code] = 0;
        input_held[i] = on;
    }
}

static u32 fnv1a(const u8 *p, u32 n)
{
    u32 h = 2166136261UL;
    while (n--)
        h = (h ^ *p++) * 16777619UL;
    return h;
}

void log_after_tick(void)
{
    s32 tick = game_tick_count;
    int i;
    for (i = 0; i < VEHICLE_COUNT; i++) {
        u8 *v = vehicle(i);
        if (*(s16 *)(v + 0x26) < 0)
            continue;
        field(&state_file, tick);
        field(&state_file, i);
        field(&state_file, *(s16 *)(v + 0x30));
        field(&state_file, *(s32 *)(v + 0x242));
        field(&state_file, *(s32 *)(v + 0x246));
        field(&state_file, *(s32 *)(v + 0x24A));
        field(&state_file, *(s32 *)(v + 0x224));
        field(&state_file, *(s32 *)(v + 0x228));
        field(&state_file, *(s32 *)(v + 0x22C));
        field(&state_file, *(s32 *)(v + 0x23A));
        field(&state_file, *(s32 *)(v + 0x23E));
        field(&state_file, *(s32 *)(v + 0x218));
        field(&state_file, *(s32 *)(v + 0x21C));
        field(&state_file, *(s32 *)(v + 0x220));
        field(&state_file, *(u16 *)(v + 0x2D7));
        put_hex(&state_file, fnv1a(v, VEHICLE_SIZE));
        put(&state_file, "\r\n");
    }
    if (tick % 70 == 0) {
        commit(&state_file);
        commit(&camera_file);
    }
}

void log_before_draw(void)
{
    int i;
    if (OPTION(OPT_PROVE) && !proved) {
        proved = 1;
        prove_fine_angles();
    }
    race_frames++;
    for (i = 0; i < VEHICLE_COUNT; i++) {
        const u8 *v = vehicle(i);
        u32 b;
        for (b = 0; b < VEHICLE_SIZE; b++)
            before_copy[i][b] = v[b];
    }
}

static void capture_frame(int blended)
{
    u32 width = game_frame_width, height = game_frame_height, size, i;
    u8 *at;
    if (!capture) {
        capture_record = 64 + 1024 + width * height;
        capture = (u8 *)dpmi_alloc(capture_record * OPTION(OPT_CAPTURE_FRAMES));
        log_text(capture ? "capture: started at race tick " : "capture: not enough memory at race tick ");
        log_number(game_tick_count);
        log_line();
        if (!capture) {
            capture_count = OPTION(OPT_CAPTURE_FRAMES);
            return;
        }
    }
    if (64 + 1024 + width * height != capture_record)
        return;
    at = capture + capture_record * capture_count;
    for (i = 0; i < 64; i++)
        at[i] = 0;
    at[0] = 'B'; at[1] = 'R'; at[2] = 'R'; at[3] = 'C';
    *(u32 *)(at + 4) = 1;
    *(u32 *)(at + 8) = capture_count;
    *(u32 *)(at + 12) = width;
    *(u32 *)(at + 16) = height;
    *(s32 *)(at + 20) = game_tick_count;
    *(u32 *)(at + 24) = blended > 0 ? 500 : 0;
    *(u32 *)(at + 28) = (u32)game_tick_count * 14286UL;   /* microseconds, from ticks */
    *(u32 *)(at + 32) = (u32)game_tick_count * 14286UL + capture_count % 14;
    for (i = 0; i < 3; i++) {
        *(s32 *)(at + 36 + 4 * i) = game_camera_pos[i];
        *(s16 *)(at + 48 + 2 * i) = game_camera_angles[i];
    }
    at[54] = (u8)(OPTION(OPT_INTERPOLATION) != 0);
    at[55] = game_frame_ticks;
    port_out(0x3C7, 0);
    for (i = 0; i < 256; i++) {
        at[64 + 4 * i] = (u8)(port_in(0x3C9) << 2);
        at[65 + 4 * i] = (u8)(port_in(0x3C9) << 2);
        at[66 + 4 * i] = (u8)(port_in(0x3C9) << 2);
        at[67 + 4 * i] = 0;
    }
    size = width * height;
    for (i = 0; i < size; i++)
        at[1088 + i] = game_frame_buffer[i];
    if (++capture_count == OPTION(OPT_CAPTURE_FRAMES)) {
        s32 handle = dos_create("BRRCAP.BIN");
        u32 done = 0, total = capture_record * capture_count;
        while (handle > 0 && done < total) {
            u32 chunk = total - done > 0x8000 ? 0x8000 : total - done;
            if (dos_write(handle, capture + done, chunk) <= 0)
                break;
            done += chunk;
        }
        if (handle > 0)
            dos_close(handle);
        log_text("capture: wrote ");
        log_number((s32)capture_count);
        log_text(" frames to BRRCAP.BIN, up to race tick ");
        log_number(game_tick_count);
        log_line();
    }
}

/* blended >= 0: after the race draw, before the patch puts the true values back (the
 * picture's camera); -1: after that (the restore check) */
void log_after_draw(int blended)
{
    int i;
    if (blended >= 0) {
        u8 *camera = game_camera_view1;
        u32 followed = *(u32 *)(camera + 0x40), base = (u32)game_vehicles;
        int slot = followed >= base && followed < base + VEHICLE_COUNT * VEHICLE_SIZE
                 ? (int)((followed - base) / VEHICLE_SIZE) : -1;
        field(&camera_file, (s32)race_frames);
        field(&camera_file, game_tick_count);
        field(&camera_file, game_frame_ticks);
        field(&camera_file, blended);
        for (i = 0; i < 3; i++)
            field(&camera_file, game_camera_pos[i]);
        for (i = 0; i < 3; i++)
            field(&camera_file, game_camera_angles[i]);
        field(&camera_file, *(u16 *)camera);
        field(&camera_file, slot);
        field(&camera_file, slot >= 0 ? *(s32 *)vehicle(slot) : 0);
        field(&camera_file, slot >= 0 ? *(s32 *)(vehicle(slot) + 8) : 0);
        put_number(&camera_file, brr_fast);
        put(&camera_file, "\r\n");
        if (OPTION(OPT_CAPTURE_FRAMES) && game_tick_count >= (s32)OPTION(OPT_CAPTURE_TICK) &&
                capture_count < OPTION(OPT_CAPTURE_FRAMES))
            capture_frame(blended);
        return;
    }
    for (i = 0; i < VEHICLE_COUNT; i++) {
        const u8 *v = vehicle(i);
        u32 b;
        for (b = 0; b < VEHICLE_SIZE; b++)
            if (v[b] != before_copy[i][b] && !offset_seen[b]) {
                offset_seen[b] = 1;
                restore_changes++;
                log_text("restore check: race frame ");
                log_number((s32)race_frames);
                log_text(": the draw changed vehicle ");
                log_number(i);
                log_text(" byte +0x");
                put_hex(&log_file, b);
                log_text(" (first time this offset changed)");
                log_line();
            }
    }
}

/* --prove: rotation_matrix against brr_rotation_fine for every multiple of 64 */
void prove_fine_angles(void)
{
    static s32 m1[9], m2[9];
    struct rot_args a1, a2;
    u32 saved[4], g1[4], g2[4], pitch, yaw, roll, mismatches = 0, combinations = 0;
    u16 started = game_timer_ticks;
    int k, bad;
    saved[0] = game_rot_a; saved[1] = game_rot_b; saved[2] = game_rot_c; saved[3] = (u32)game_rot_sin_a;
    log_text("prove fine angles: comparing all 1024^3 multiples of 64 with the original routine");
    log_line();
    for (pitch = 0; pitch < 65536; pitch += 64) {
        if (pitch % 4096 == 0) {
            log_text("prove fine angles: pitch ");
            log_number((s32)pitch);
            log_text(" of 65536, mismatches so far ");
            log_number((s32)mismatches);
            log_line();
        }
        for (yaw = 0; yaw < 65536; yaw += 64)
            for (roll = 0; roll < 65536; roll += 64) {
                a1.pitch = a2.pitch = pitch;
                a1.yaw = a2.yaw = yaw;
                a1.roll = a2.roll = roll;
                a1.m = m1;
                a2.m = m2;
                brr_call_rotation_original(&a1);
                g1[0] = game_rot_a; g1[1] = game_rot_b; g1[2] = game_rot_c; g1[3] = (u32)game_rot_sin_a;
                brr_rotation_fine(&a2);
                g2[0] = game_rot_a; g2[1] = game_rot_b; g2[2] = game_rot_c; g2[3] = (u32)game_rot_sin_a;
                combinations++;
                bad = 0;
                for (k = 0; k < 9; k++)
                    bad |= m1[k] != m2[k];
                for (k = 0; k < 4; k++)
                    bad |= a1.regs[k] != a2.regs[k] || g1[k] != g2[k];
                if (bad) {
                    if (mismatches < 10) {
                        log_text("prove fine angles: MISMATCH at pitch ");
                        log_number((s32)pitch);
                        log_text(" yaw ");
                        log_number((s32)yaw);
                        log_text(" roll ");
                        log_number((s32)roll);
                        log_line();
                    }
                    mismatches++;
                }
            }
    }
    log_text("prove fine angles: ");
    log_number((s32)combinations);
    log_text(" combinations, ");
    log_number((s32)mismatches);
    log_text(" mismatches, ");
    log_number((s32)(u16)(game_timer_ticks - started));
    log_text(" ticks (the 70 Hz timer wraps every 936 s)");
    log_line();
    game_rot_a = saved[0]; game_rot_b = saved[1]; game_rot_c = saved[2]; game_rot_sin_a = (s32)saved[3];
}
