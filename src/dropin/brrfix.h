/* Big Red Launcher's TIMER.DLL (the drop-in fix): a replacement TIMER.DLL for the player's own v2.25
 * WINRACE.EXE. Our own code only; it patches the game in memory, never on disk.
 * See docs/reports/phase-3a-drop-in-fix.md and data/dropin-patches.json. */
#ifndef BRRFIX_H
#define BRRFIX_H

#include <windows.h>
#include <stdint.h>

#include "version.h"

/* Addresses in WINRACE.EXE v2.25 (image base 0x400000, never relocated). */
#define GAME_FRAME_TICKS   ((volatile uint8_t *)0x44BF80)
#define GAME_TICK_COUNT    ((volatile int32_t *)0x44BF84)
#define GAME_WINDOWED      ((volatile uint8_t *)0x52D72C)  /* the present blits into a window */
#define GAME_VEHICLES      0x452A90u
#define GAME_VEHICLE_SIZE  0x395u
#define GAME_VEHICLE_COUNT 7
#define GAME_CAMERA_POS    0x451310u
#define GAME_CAMERA_ANGLES 0x45131Cu
#define GAME_RACE_TICK     0x415204u
#define GAME_RACE_DRAW     0x414DBCu

struct brr_config {
    int fix_vga, fix_clock, fix_cap, fix_interpolation, fix_camera, fix_keys;
    int clock_original;      /* emulate the original 14 ms callback timer instead */
    uint32_t rate_milli;     /* ticks per 1000 s: 70000 = 70.0 Hz */
    int cap;                 /* catch-up cap in ticks (original 35) */
    int frame_limit;         /* frames per second; FRAME_LIMIT_AUTO = display refresh, 0 = unlocked */
    int window_scale;        /* 0 = auto (largest whole number that fits), else 1..8 */
    int window_aspect43;     /* stretch the height by 1.2, as a 4:3 monitor showed 320x200 */
    int debug_log_mouse;     /* log mouse positions in a window (tests) */
    int music, music_volume, music_follow_game;  /* [music] */
    int controller, pad_deadzone, pad_sensitivity, pad_curve, pad_triggers, pad_analogue;  /* [controller] */
    int debug_pad_test;      /* tests: a simulated pad (half right, full throttle) */
    char music_folder[MAX_PATH];
    int show_version;        /* the drop-in's version on the main menu */
    int display_mode;        /* DISPLAY_WINDOWED, DISPLAY_FULLSCREEN or DISPLAY_ASK */
    int vsync;               /* wait for the vertical blank before the windowed present */
    int pacing_dropin;       /* 1 = the drop-in paces frames (default); 0 = leave it to cnc-ddraw if present */
    int interp_delay_milli;  /* display delay in thousandths of a tick (2000) */
    int camera_smoothing;    /* experimental, 0 = off; time constant in tenths of a tick */
    int camera_roll_fix;     /* experimental, 0 = off: mode-4 roll over 7 ticks */
    int log, log_clock, log_state, log_camera, check_restore;
    int debug_ignore_exe_checksum;  /* tests only: check the patch sites even if the file differs */
    int debug_camera_mode;          /* diagnostics: request this camera mode each tick (-1 = off) */
    int debug_capture_frames, debug_capture_from_tick;  /* stutter meter frame capture */
    char debug_input[256];          /* scheduled keys, e.g. accel:300-1200 */
    int debug_writeset_frames, debug_writeset_from_tick;  /* tick write-set measurement */
    int debug_dump_tick, debug_dump_count;  /* raw vehicle records after these race ticks */
    int debug_hold_pitch;           /* diagnostic: hold the drawn pitch at its first race value */
    int debug_hold_roll;            /* diagnostic: draw the follow camera with no roll */
    int fine_angles;                /* [camera] fine_angles: draw with angles finer than 64 units */
    int debug_prove_fine_angles;    /* run the proof against the original routine at the first race draw */
};

extern struct brr_config cfg;
extern volatile LONG brr_timer_count;  /* exported as _TIMER_Count */
extern int brr_patched;                /* 1 when every check passed and patches are in */
extern uint32_t brr_orig_race_tick, brr_orig_race_draw, brr_orig_camera_update;
extern uint32_t brr_orig_playback_tick, brr_orig_playback_draw;
void brr_hook_playback_tick(void);
void brr_hook_playback_draw(void);

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

#define FRAME_LIMIT_AUTO (-2)
#define DISPLAY_WINDOWED 0
#define DISPLAY_FULLSCREEN 1
#define DISPLAY_ASK 2

/* controller.c */
extern uint32_t brr_orig_joystick_to_keys;
void brr_controller_start(void);
void __cdecl brr_before_joystick(void);
void brr_hook_joystick(void);

/* music.c */
int brr_music_check(void);
void brr_music_start(void);

/* display.c */
extern uint32_t brr_display_windowed, brr_display_fullscreen, brr_display_prompt;
int __cdecl brr_display_choice(uint32_t depth);
int __cdecl brr_present_gdi(void);
void brr_install_key_filter(void);
void brr_hook_present(void);
extern int brr_gdi_present;
extern uint32_t brr_present_continue;
void brr_hook_display_choice(void);

/* clock.c: frame pacing, decided at the first frame */
extern int brr_cnc_ddraw, brr_vsync_active;
void __cdecl brr_before_present_window(void);
void brr_hook_present_window(void);
extern uint32_t brr_orig_present_window;

/* angles.c */
extern int brr_fine_angles_active;
extern uint32_t brr_rotation_continue, brr_game_atan;
void __cdecl brr_rotation_fine(uint32_t pitch, uint32_t yaw, uint32_t roll, int32_t *m, uint32_t *regs);
void brr_fine_camera_angles(uint32_t record, int16_t *angles);
void brr_fine_angles_report(void);
void brr_prove_fine_angles(void);
void brr_hook_rotation(void);
void __cdecl brr_call_rotation_original(uint32_t pitch, uint32_t yaw, uint32_t roll, int32_t *m, uint32_t *regs);
int __cdecl brr_call_game_atan(int32_t y, int32_t x);

/* debug.c */
void brr_debug_setup(void);
void brr_debug_before_tick(void);
void brr_debug_before_draw(void);
void brr_debug_after_draw(void);
void brr_debug_end_of_draw(void);

/* thunks.asm */
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
