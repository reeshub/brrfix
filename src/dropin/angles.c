/* Finer angles for drawing (stutter hunt H2, docs/reports/stutter-hunt.md).
 *
 * The 3D engine builds every rotation from a 1,024-entry sine table (FUN_00446670,
 * 32767 x sin(2 pi i / 1024) at 0x542080; cosine = the same table 256 entries on), so the
 * picture can only turn in steps of 64 angle units (0.35 degrees), and the follow
 * camera's pitch and yaw come from an arctangent that also returns multiples of 64. As
 * the camera moves, the picture drifts, then snaps 2-3 pixels.
 *
 * With [camera] fine_angles=1, during the race draw only:
 *  - brr_rotation_fine replaces FUN_00446670. It repeats the original's arithmetic step
 *    by step (the same sums, products and arithmetic shifts), but reads the table at the
 *    full 16-bit angle, interpolating between neighbouring entries. When the low 6 bits
 *    are zero the interpolation adds exactly nothing, so for every angle that's a multiple
 *    of 64 the result is the original's, bit for bit ([debug] prove_fine_angles checks all
 *    1,024^3 combinations against the original routine). The trucks' interpolated angles
 *    go through the same routine.
 *  - brr_fine_camera_angles recomputes the follow camera's (mode 4) pitch and yaw from the
 *    same inputs as FUN_00404778, without rounding to 64.
 * Outside the race draw, or with the setting off, the original routine itself runs.
 *
 * Plain, portable C (builds with GCC and Open Watcom): no compiler extensions, no maths
 * library (msvcrt.dll doesn't exist on Windows 95), and right shifts of negative numbers
 * done explicitly, as C leaves them to the compiler. */
#include "brrfix.h"
#include <string.h>

#define SINE_TABLE   ((const int16_t *)0x542080)  /* 1,280 entries */
#define COSINE_TABLE ((const int16_t *)0x542280)  /* = SINE_TABLE + 256 */
#define ROT_A        ((volatile uint32_t *)0x54A308)  /* the original's byte offsets */
#define ROT_B        ((volatile uint32_t *)0x54A30C)
#define ROT_C        ((volatile uint32_t *)0x54A310)
#define ROT_SIN_A    ((volatile int32_t *)0x54A304)
#define PI 3.14159265358979323846

int brr_fine_angles_active;  /* set by the race draw hook; read by the thunk */
static uint32_t camera_fine_frames, camera_kept_game, camera_like_game, kept_lines, rotations, rotations_fine;
static int last_kept;

/* x >> n for signed x, as the x86 SAR instruction does it */
static int32_t sar(int32_t x, int n)
{
    return x >= 0 ? x >> n : -(int32_t)((uint32_t)(-(x + 1)) >> n) - 1;
}

static int32_t table_at(const int16_t *table, uint32_t angle)
{
    uint32_t i = (angle >> 6) & 1023, f = angle & 63;
    int32_t a = table[i], b = table[(i + 1) & 1023];
    return a + sar((b - a) * (int32_t)f, 6);
}

#define S(x) table_at(SINE_TABLE, (uint32_t)(x) & 0xFFFFu)
#define K(x) table_at(COSINE_TABLE, (uint32_t)(x) & 0xFFFFu)

/* FUN_00446670 with fine angles. pitch, yaw, roll as the original receives them in EAX,
 * EBX, ECX (only the low 16 bits count); m is the 3x3 matrix at EBP; regs gets the values
 * the original leaves in EAX, EBX, ECX, EDX. Products stay within 32 bits: table values
 * are at most 32767 in size, so (s1 + s2) * sA is at most 65534 x 32767 < 2^31. */
void __cdecl brr_rotation_fine(uint32_t pitch, uint32_t yaw, uint32_t roll, int32_t *m, uint32_t *regs)
{
    int32_t sa = S(pitch), s1, s2, t1, t2;
    rotations++;
    if ((pitch | yaw | roll) & 63)
        rotations_fine++;
    *ROT_A = (pitch >> 5) & 0x7FE;
    *ROT_B = (yaw >> 5) & 0x7FE;
    *ROT_C = (roll >> 5) & 0x7FE;
    *ROT_SIN_A = sa;

    s1 = S(yaw + roll);                  /* ESI */
    s2 = S(yaw - roll);                  /* EDI */
    t1 = sar((s1 + s2) * sa, 15);
    t2 = sar((s1 - s2) * sa, 15);
    m[6] = sar(t2 - s1 - s2, 1);
    m[1] = sar(-t1 + s1 - s2, 1);

    s1 = K(yaw + roll);
    s2 = K(yaw - roll);
    t1 = sar((s1 + s2) * sa, 15);
    t2 = sar((s2 - s1) * sa, 15);
    m[7] = -sar(t1 + s2 - s1, 1);
    m[0] = sar(t2 + s1 + s2, 1);

    m[3] = -sar(S(pitch + roll) - S(pitch - roll), 1);
    m[4] = sar(K(pitch + roll) + K(pitch - roll), 1);

    m[2] = sar(S(pitch + yaw) - S(pitch - yaw), 1);
    m[5] = sa;
    m[8] = sar(K(pitch - yaw) + K(pitch + yaw), 1);

    regs[0] = (uint32_t)m[2];            /* EAX */
    regs[1] = (uint32_t)sa;              /* EBX */
    regs[2] = 0x7FE;                     /* ECX */
    regs[3] = (uint32_t)S(pitch - yaw);  /* EDX */
}

/* ---- arctangent and square root without a maths library ---- */

static double atan_01(double t)  /* 0 <= t <= 1; Abramowitz & Stegun 4.4.49, error < 1e-5 rad */
{
    double t2 = t * t;
    return t * (0.9998660 + t2 * (-0.3302995 + t2 * (0.1801410 + t2 * (-0.0851330 + t2 * 0.0208351))));
}

/* atan2(y, x) in the game's angle units (65,536 per turn), the same convention as the
 * game's FUN_004470c0: y is the "sine" side, x the "cosine" side */
static double atan2_units(double y, double x)
{
    double ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, r;
    if (ax == 0 && ay == 0)
        return 0;
    r = ax >= ay ? atan_01(ay / ax) : PI / 2 - atan_01(ax / ay);
    if (x < 0)
        r = PI - r;
    if (y < 0)
        r = -r;
    return r * (32768.0 / PI);
}

static double square_root(double v)
{
    double scale = 1, r;
    int i;
    if (v <= 0)
        return 0;
    while (v > 4) { v /= 4; scale *= 2; }
    while (v < 1) { v *= 4; scale /= 2; }
    r = 1.5;
    for (i = 0; i < 6; i++)
        r = 0.5 * (r + v / r);
    return r * scale;
}

static int16_t to_angle(double units)
{
    long n = (long)(units >= 0 ? units + 0.5 : units - 0.5);
    return (int16_t)(uint16_t)((unsigned long)n & 0xFFFFu);
}

/* What the game's arctangent (FUN_004470e0) would return for this exact angle: it floors to
 * a multiple of 64 within the first octant and mirrors that into the others, so it always
 * rounds toward the nearest axis (0, 90, 180 or 270 degrees). */
static int16_t like_game(double units)
{
    double a = units < 0 ? -units : units;
    int back = a > 16384, high;
    int32_t q;
    if (back)
        a = 32768 - a;
    high = a > 8192;
    if (high)
        a = 16384 - a;
    q = (int32_t)(a / 64) * 64;
    if (high)
        q = 16384 - q;
    if (back)
        q = 32768 - q;
    return (int16_t)(uint16_t)(uint32_t)(units < 0 ? -q : q);
}

static int16_t clamp_pitch(int32_t pitch)
{
    return (int16_t)(pitch > 0x2000 ? 0x2000 : pitch < -0x2000 ? -0x2000 : pitch);
}

static int32_t read32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static int16_t read16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }

/* After camera_update for a view, during the race draw: the follow camera's (mode 4) pitch
 * and yaw, unrounded, into the drawn camera angles (int16 pitch, yaw, roll at angles).
 * camera_mode4 (0x403b58) aims the camera at the truck with FUN_00404778 (the arctangents
 * above, rounded to 64 units), then calls 0x403ae0, the Elasticam's look-ahead: it turns
 * the camera part of the way toward the truck's heading (vehicle +0xE), by a quarter of
 * the difference (folded back beyond a right angle) times speed (vehicle +0x230, at most
 * 4000) / 4000. Both are repeated here without the rounding. */
void brr_fine_camera_angles(uint32_t record, int16_t *angles)
{
    const uint8_t *camera = (const uint8_t *)record;
    const uint8_t *vehicle;
    int32_t offset, look_pitch, look_yaw, speed, heading, aim, d, mode;
    int16_t game_pitch, game_yaw, fine_pitch, fine_yaw;
    double dx, dy, dz, pitch, yaw, diff;
    int kept;
    mode = read16(camera);
    /* mode 4 (follow camera), unless held by its dead band; mode 3 (trackside cameras) */
    if (!(mode == 4 && camera[0x45] != 0xFF) && mode != 3)
        return;
    vehicle = (const uint8_t *)(uintptr_t)(uint32_t)read32(camera + 0x40);
    if (!vehicle)
        return;
    if (mode == 4) {
        offset = read32(camera + 0x54);
        if (camera[0x44] & 0x7F)
            offset += (read32(camera + 0x48) - offset) * (int32_t)(camera[0x44] & 0x7F) / 100;
        offset = -offset;
    } else  /* FUN_004046fc: a height per vehicle type, from the table at 0x451230 */
        offset = (int16_t)(*(const uint32_t *)(0x451230 + (uint32_t)(read32(vehicle + 0x24) >> 16) * 8) >> 16);
    dx = (double)(read32(vehicle) - read32(camera + 2));
    dz = (double)(read32(vehicle + 8) - read32(camera + 0x0A));
    dy = (double)(read32(vehicle + 4) + offset - read32(camera + 6));
    pitch = atan2_units(dy, square_root(dx * dx + dz * dz));
    yaw = atan2_units(dx, dz);
    speed = read32(vehicle + 0x230);
    if (speed < 0)
        speed = -speed;
    if (speed > 4000)
        speed = 4000;
    heading = read16(vehicle + 0x0E);
    look_pitch = read16(camera + 0x22);  /* as camera_update (camera_mode4 zeroes them) */
    look_yaw = read16(camera + 0x24);

    /* a check that this reads the game's camera correctly: done the game's way, with its
     * rounding, ours should be exactly its values */
    aim = like_game(yaw);
    d = (int16_t)(uint16_t)(uint32_t)(heading - aim);
    d = d >= 0x4000 ? 0x7FFF - d : d < -0x4000 ? -0x8000 - d : d;
    if (mode != 4)
        d = 0;  /* the look-ahead is the follow camera's only */
    game_pitch = clamp_pitch(like_game(pitch) - look_pitch);
    game_yaw = (int16_t)(uint16_t)(uint32_t)(aim + sar(d, 2) * speed / 4000 - look_yaw);
    if (game_pitch == angles[0] && game_yaw == angles[1])
        camera_like_game++;

    diff = (double)heading - yaw;
    while (diff >= 32768)
        diff -= 65536;
    while (diff < -32768)
        diff += 65536;
    diff = diff >= 16384 ? 32767 - diff : diff < -16384 ? -32768 - diff : diff;
    if (mode == 4)
        yaw += diff / 4 * speed / 4000;
    pitch -= look_pitch;
    if (pitch > 0x2000)
        pitch = 0x2000;
    if (pitch < -0x2000)
        pitch = -0x2000;
    fine_pitch = to_angle(pitch);
    fine_yaw = to_angle(yaw - look_yaw);

    /* only replace the game's values when they're close to ours */
    d = (int16_t)(uint16_t)(fine_pitch - angles[0]);
    kept = d > 96 || d < -96;
    d = (int16_t)(uint16_t)(fine_yaw - angles[1]);
    kept = kept || d > 96 || d < -96;
    if (kept != last_kept && kept_lines < 20) {
        kept_lines++;
        if (kept)
            brr_log("fine angles: keeping the game's camera angles from race tick %lu: game pitch %d yaw %d, "
                    "ours %d %d (dx %ld dy %ld dz %ld)", (unsigned long)*GAME_TICK_COUNT, angles[0], angles[1],
                    fine_pitch, fine_yaw, (long)dx, (long)dy, (long)dz);
        else
            brr_log("fine angles: refining the camera angles again from race tick %lu",
                    (unsigned long)*GAME_TICK_COUNT);
    }
    last_kept = kept;
    if (kept) {
        camera_kept_game++;
        return;
    }
    angles[0] = fine_pitch;
    angles[1] = fine_yaw;
    camera_fine_frames++;
}

void brr_fine_angles_report(void)
{
    if (cfg.fine_angles)
        brr_log("fine angles: camera pitch and yaw refined in %lu view frames, game values kept in %lu; "
                "ours quantised as the game does = the game's values in %lu; rotations built %lu, "
                "%lu of them with angles finer than 64 units", (unsigned long)camera_fine_frames,
                (unsigned long)camera_kept_game, (unsigned long)camera_like_game, (unsigned long)rotations,
                (unsigned long)rotations_fine);
}

/* ---- [debug] prove_fine_angles: the proof against the original routine ---- */

void brr_prove_fine_angles(void)
{
    int32_t m1[9], m2[9];
    uint32_t r1[4], r2[4], g1[4], g2[4], saved[4];
    uint32_t a, b, c, mismatches = 0, combinations = 0, atan_samples = 0, atan_like_game = 0;
    long atan_max = 0;
    int64_t started = brr_qpc(), seed = 12345;
    saved[0] = *ROT_A; saved[1] = *ROT_B; saved[2] = *ROT_C; saved[3] = (uint32_t)*ROT_SIN_A;
    brr_log("prove fine angles: comparing all 1024^3 multiples of 64 with the original routine...");
    for (a = 0; a < 65536; a += 64) {
        if (a % 8192 == 0)
            brr_log("prove fine angles: pitch %lu of 65536, %lu mismatches so far (%ld s)", (unsigned long)a,
                    (unsigned long)mismatches, (long)((brr_qpc() - started) / brr_qpc_freq()));
        for (b = 0; b < 65536; b += 64)
            for (c = 0; c < 65536; c += 64) {
                brr_call_rotation_original(a, b, c, m1, r1);
                g1[0] = *ROT_A; g1[1] = *ROT_B; g1[2] = *ROT_C; g1[3] = (uint32_t)*ROT_SIN_A;
                brr_rotation_fine(a, b, c, m2, r2);
                g2[0] = *ROT_A; g2[1] = *ROT_B; g2[2] = *ROT_C; g2[3] = (uint32_t)*ROT_SIN_A;
                combinations++;
                if (memcmp(m1, m2, sizeof m1) || memcmp(r1, r2, sizeof r1) || memcmp(g1, g2, sizeof g1)) {
                    if (mismatches < 10)
                        brr_log("prove fine angles: MISMATCH at pitch %lu yaw %lu roll %lu",
                                (unsigned long)a, (unsigned long)b, (unsigned long)c);
                    mismatches++;
                }
            }
    }
    brr_log("prove fine angles: %lu combinations, %lu mismatches (%ld s)", (unsigned long)combinations,
            (unsigned long)mismatches, (long)((brr_qpc() - started) / brr_qpc_freq()));
    /* the arctangent: the game's FUN_00412a20 against ours, on pseudo-random inputs */
    for (atan_samples = 0; atan_samples < 1000000; atan_samples++) {
        int32_t y, x;
        long game, fine, d;
        double exact;
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        y = (int32_t)(seed % 20001) - 10000;
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        x = (int32_t)(seed % 40001) - 20000;
        if (y == 0 && x == 0)
            continue;
        game = (int16_t)brr_call_game_atan(y, x);
        exact = atan2_units((double)y, (double)x);
        fine = to_angle(exact);
        d = (long)(int16_t)(uint16_t)(fine - game);
        if (d < 0) d = -d;
        if (d > atan_max) atan_max = d;
        if (like_game(exact) == (int16_t)game)
            atan_like_game++;
    }
    brr_log("prove fine angles: arctangent, %lu samples: largest difference from the game %ld units; "
            "ours quantised as the game does = the game's in %lu", (unsigned long)atan_samples,
            atan_max, (unsigned long)atan_like_game);
    *ROT_A = saved[0]; *ROT_B = saved[1]; *ROT_C = saved[2]; *ROT_SIN_A = (int32_t)saved[3];
}

/* ---- shared with sky.c ---- */
double brr_atan2_units(double y, double x) { return atan2_units(y, x); }
double brr_square_root(double v) { return square_root(v); }
/* sin of a game angle (65,536 per turn), from the game's own table, interpolated */
double brr_sine_units(int32_t angle) { return (double)table_at(SINE_TABLE, (uint32_t)angle & 0xFFFFu) / 32767.0; }
