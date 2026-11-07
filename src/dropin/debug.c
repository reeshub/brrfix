/* Diagnostics for the stutter hunt (docs/reports/stutter-hunt.md). Off unless set in
 * [debug] in BRRFIX.INI:
 *  - capture_frames=N, capture_from_tick=T: from race tick T, keep every presented
 *    frame (the game's own 8-bit buffer, before any scaling, with its palette and the
 *    times, tick, interpolation fraction and drawn camera) in memory, and write all N
 *    to BRRFIX-CAPTURE.BIN once the last one is in, so the disk isn't touched while
 *    capturing. Read by "python3 brr.py stutter-meter".
 *  - input=NAME:FIRST-LAST,...: before each race tick, hold keys in the game's key table
 *    for those race ticks (inclusive), so runs are repeatable tick for tick. NAME is
 *    accel, brake, left, right, nitro or reset (player 1's control set at 0x44B00A, in
 *    that order: likely), or kXX for key-table index XX in hex.
 * Neither changes how the game runs: input is what a player would press. */
#include "brrfix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_TABLE_IMPORT 0x56C648u   /* WINRACE.EXE's import slot for KEY.DLL's _KEY_pressed */
#define CONTROL_SET      0x44B00Au   /* player 1's six key-table indices */
#define FRAME_BUFFER_PTR 0x52D738u   /* the buffer the present copies */
#define FRAME_WIDTH      0x44BFB8u
#define FRAME_HEIGHT     0x44BFBCu
#define PALETTE          0x52D758u   /* 256 PALETTEENTRY (r, g, b, flags) */
#define MAX_INPUTS 16

struct capture_header {             /* 64 bytes, little-endian */
    char magic[4];                  /* "BRRC" */
    uint32_t version, frame, width, height;
    int32_t tick_count;
    uint32_t fraction_milli;
    uint32_t draw_start_us, draw_end_us;  /* QueryPerformanceCounter, microseconds since load */
    int32_t cam_pos[3];
    int16_t cam_angles[3];
    uint8_t interpolation, frame_ticks;
    uint8_t reserved[8];
};
_Static_assert(sizeof(struct capture_header) == 64, "capture header must be 64 bytes");

struct input_span { int index; int32_t first, last; };

static struct input_span inputs[MAX_INPUTS];
static int input_count, input_held[MAX_INPUTS];
static uint8_t *capture;
static uint32_t capture_record, capture_count, capture_width, capture_height, capture_done;
static uint32_t draw_start_us;

static int control_index(const char *name)
{
    static const char *names[6] = {"accel", "brake", "left", "right", "nitro", "reset"};
    for (int i = 0; i < 6; i++)
        if (!lstrcmpiA(name, names[i]))
            return ((uint8_t *)CONTROL_SET)[i];
    if ((name[0] == 'k' || name[0] == 'K') && name[1])
        return (int)strtol(name + 1, NULL, 16) & 0xFF;
    return -1;
}

void brr_debug_setup(void)
{
    char text[512], *p, *next;
    if (cfg.debug_input[0]) {
        lstrcpynA(text, cfg.debug_input, sizeof text);
        for (p = text; p && *p && input_count < MAX_INPUTS; p = next) {
            next = strchr(p, ',');
            if (next)
                *next++ = 0;
            char *colon = strchr(p, ':'), *dash = colon ? strchr(colon, '-') : NULL;
            if (!colon || !dash) {
                brr_log("debug input: can't read \"%s\"", p);
                continue;
            }
            *colon = 0;
            struct input_span *s = &inputs[input_count];
            s->index = control_index(p);
            s->first = atoi(colon + 1);
            s->last = atoi(dash + 1);
            if (s->index < 0) {
                brr_log("debug input: unknown key \"%s\"", p);
                continue;
            }
            brr_log("debug input: key-table index 0x%02X (%s) held for race ticks %ld-%ld",
                    s->index, p, (long)s->first, (long)s->last);
            input_count++;
        }
    }
    if (cfg.debug_capture_frames > 0)
        brr_log("debug capture: %d frames from race tick %d", cfg.debug_capture_frames,
                cfg.debug_capture_from_tick);
}

/* Before each race tick: the scheduled keys. */
void brr_debug_before_tick(void)
{
    if (!input_count)
        return;
    uint8_t *keys = *(uint8_t **)KEY_TABLE_IMPORT;
    int32_t tick = *GAME_TICK_COUNT;
    for (int i = 0; i < input_count; i++) {
        int on = tick >= inputs[i].first && tick <= inputs[i].last;
        if (on)
            keys[inputs[i].index] = 1;
        else if (input_held[i])
            keys[inputs[i].index] = 0;
        input_held[i] = on;
    }
}

void brr_debug_before_draw(void)
{
    draw_start_us = (uint32_t)(brr_qpc() * 1000000 / brr_qpc_freq());
}

static void write_capture(void)
{
    char path[MAX_PATH];
    brr_path_beside_dll(path, sizeof path, "BRRFIX-CAPTURE.BIN");
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0, total = 0;
    uint64_t size = (uint64_t)capture_record * capture_count;
    if (file != INVALID_HANDLE_VALUE) {
        for (uint64_t at = 0; at < size; at += written) {
            DWORD chunk = (DWORD)(size - at > (1u << 24) ? (1u << 24) : size - at);
            if (!WriteFile(file, capture + at, chunk, &written, NULL) || !written)
                break;
            total += written;
        }
        CloseHandle(file);
    }
    brr_log("debug capture: wrote %lu frames of %lux%lu (%lu bytes) to %s", (unsigned long)capture_count,
            (unsigned long)capture_width, (unsigned long)capture_height, (unsigned long)total, path);
    VirtualFree(capture, 0, MEM_RELEASE);
    capture = NULL;
}

/* After each race draw, before anything is restored: keep the frame just presented. */
void brr_debug_after_draw(void)
{
    if (cfg.debug_capture_frames <= 0 || capture_done || *GAME_TICK_COUNT < cfg.debug_capture_from_tick)
        return;
    uint32_t width = *(uint32_t *)FRAME_WIDTH, height = *(uint32_t *)FRAME_HEIGHT & 0xFFFF;
    const uint8_t *pixels = *(const uint8_t **)FRAME_BUFFER_PTR;
    if (!pixels || width == 0 || width > 1024 || height == 0 || height > 768)
        return;
    if (!capture) {
        capture_width = width;
        capture_height = height;
        capture_record = sizeof(struct capture_header) + 1024 + width * height;
        capture = VirtualAlloc(NULL, (SIZE_T)capture_record * (SIZE_T)cfg.debug_capture_frames,
                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!capture) {
            brr_log("debug capture: couldn't allocate %lu bytes", (unsigned long)(capture_record * cfg.debug_capture_frames));
            capture_done = 1;
            return;
        }
        brr_log("debug capture: started at race tick %ld, %lux%lu", (long)*GAME_TICK_COUNT,
                (unsigned long)width, (unsigned long)height);
    }
    if (width != capture_width || height != capture_height) {
        brr_log("debug capture: the frame size changed; stopping early");
        capture_done = 1;
        write_capture();
        return;
    }
    uint8_t *record = capture + (size_t)capture_record * capture_count;
    struct capture_header h;
    memset(&h, 0, sizeof h);
    memcpy(h.magic, "BRRC", 4);
    h.version = 1;
    h.frame = capture_count;
    h.width = width;
    h.height = height;
    h.tick_count = *GAME_TICK_COUNT;
    h.fraction_milli = (uint32_t)(brr_clock_frame_fraction() * 1000.0f);
    h.draw_start_us = draw_start_us;
    h.draw_end_us = (uint32_t)(brr_qpc() * 1000000 / brr_qpc_freq());
    memcpy(h.cam_pos, (void *)GAME_CAMERA_POS, 12);
    memcpy(h.cam_angles, (void *)GAME_CAMERA_ANGLES, 6);
    h.interpolation = (uint8_t)cfg.fix_interpolation;
    h.frame_ticks = *GAME_FRAME_TICKS;
    memcpy(record, &h, sizeof h);
    memcpy(record + sizeof h, (void *)PALETTE, 1024);
    memcpy(record + sizeof h + 1024, pixels, width * height);
    if (++capture_count >= (uint32_t)cfg.debug_capture_frames) {
        capture_done = 1;
        write_capture();
    }
}
