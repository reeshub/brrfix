/* Big Red Racing drop-in fix: a replacement TIMER.DLL for the player's own v2.25
 * WINRACE.EXE. Our own code only; it patches the game in memory, never on disk.
 * See docs/reports/phase-3a-drop-in-fix.md and data/dropin-patches.json. */
#ifndef BRRFIX_H
#define BRRFIX_H

#include <windows.h>
#include <stdint.h>

#define BRRFIX_VERSION "0.1.0"

/* Addresses in WINRACE.EXE v2.25 (image base 0x400000, never relocated). */
#define GAME_FRAME_TICKS   ((volatile uint8_t *)0x44BF80)
#define GAME_TICK_COUNT    ((volatile int32_t *)0x44BF84)
#define GAME_VEHICLES      0x452A90u
#define GAME_VEHICLE_SIZE  0x395u
#define GAME_VEHICLE_COUNT 7
#define GAME_CAMERA_POS    0x451310u
#define GAME_CAMERA_ANGLES 0x45131Cu
#define GAME_RACE_TICK     0x415204u
#define GAME_RACE_DRAW     0x414DBCu

struct brr_config {
    int fix_vga, fix_clock, fix_cap, fix_interpolation, fix_camera;
    int clock_original;      /* emulate the original 14 ms callback timer instead */
    uint32_t rate_milli;     /* ticks per 1000 s: 70000 = 70.0 Hz */
    int cap;                 /* catch-up cap in ticks (original 35) */
    int frame_limit;         /* frames per second; 0 = display refresh, -1 = none */
    int interp_delay_milli;  /* display delay in thousandths of a tick (2000) */
    int camera_smoothing;    /* experimental, 0 = off; time constant in tenths of a tick */
    int camera_roll_fix;     /* experimental, 0 = off: mode-4 roll over 7 ticks */
    int log, log_clock, log_state, log_camera, check_restore;
    int debug_ignore_exe_checksum;  /* tests only: check the patch sites even if the file differs */
    int debug_camera_mode;          /* diagnostics: request this camera mode each tick (-1 = off) */
};

extern struct brr_config cfg;
extern volatile LONG brr_timer_count;  /* exported as _TIMER_Count */
extern int brr_patched;                /* 1 when every check passed and patches are in */
extern uint32_t brr_orig_race_tick, brr_orig_race_draw, brr_orig_camera_update;

/* main.c */
void brr_log(const char *format, ...);
void brr_path_beside_dll(char *out, DWORD size, const char *name);

/* clock.c */
void brr_clock_setup(void);
void brr_clock_start(void);
void brr_clock_stop(void);
uint32_t brr_clock_ticks_now(void);
uint32_t __cdecl brr_clock_read(void);
void __cdecl brr_retrace(void);
float brr_clock_frame_fraction(void);
int64_t brr_qpc(void);
int64_t brr_qpc_freq(void);
void brr_clock_report(int final);

/* patch.c */
void brr_patch_all(void);
void brr_sha256(const void *data, size_t size, uint8_t out[32]);

/* interp.c */
void brr_interp_setup(void);
void brr_interp_close(void);
void __cdecl brr_before_tick(void);
void __cdecl brr_after_tick(void);
void __cdecl brr_before_draw(void);
void __cdecl brr_after_draw(void);

/* thunks.S */
void brr_hook_retrace(void);
void brr_hook_clock_read(void);
void brr_hook_race_tick(void);
void brr_hook_race_draw(void);
void brr_hook_cam4(void);
void brr_hook_camera_update(void);

/* camera.c */
void __cdecl brr_after_camera_update(uint32_t record);
void brr_camera_before_draw(void);
void brr_camera_restore(void);

#endif
