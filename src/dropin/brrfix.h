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
    int fix_vga, fix_clock, fix_cap, fix_interpolation, fix_camera, fix_mode_switch;
    int fix_menu_keys;       /* one menu action per key press (keys.c) */
    int resolution_high;     /* [display] resolution=high: the game's 640x480 mode (keypad +) */
    int renderer_new;        /* [display] renderer=new: Phase 3c's renderer (render.c); original = the game's */
    int render_max;          /* [display] render_resolution=max: experimental, races drawn at up to
                              * 1008x756 (render.c; the new renderer only) */
    int language;            /* [display] language: 1-7 (ENG, USA, FRE, GER, ITA, SPA, JAP) */
    int clock_original;      /* emulate the original 14 ms callback timer instead */
    uint32_t rate_milli;     /* ticks per 1000 s: 70000 = 70.0 Hz */
    int cap;                 /* catch-up cap in ticks (original 35) */
    int frame_limit;         /* frames per second; FRAME_LIMIT_AUTO = display refresh, 0 = unlocked */
    int window_scale;        /* 0 = auto (largest whole number that fits), else 1..8 */
    int window_aspect43;     /* [display] aspect=4:3: 320x200 pictures at the 4:3 shape (window; new renderer) */
    int max_draw_distance;   /* [display] max_draw_distance: experimental; 0 = the game's own, 25 = the
                              * furthest its grid holds, 26-60 = further, with extra passes (distance.c) */
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
    int debug_log_keys;             /* log keyboard events, the key table and menu key hotspots */
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
extern uint32_t brr_orig_credit_lookup;
const char *__cdecl brr_credit_text(char *name);
void brr_hook_credit_lookup(void);

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

/* render.c (Phase 3c) */
extern int brr_renderer_active;
extern uint32_t brr_dd_setup_continue;
int brr_renderer_decide(int cnc_ddraw);
const uint8_t *brr_render_compose(int out_w, int out_h, int out_pitch);
uint32_t __cdecl brr_render_mode(uint32_t windowed);
void brr_hook_dd_setup(void);
/* Experimental draw distance: site render_view_entry (thunks.asm) draws each view with the
 * vision distance at 25 (the most the game's 64x64 visible-area grid holds; the game's own
 * limit is 22) and the texture distance at brr_draw_texture, then puts the player's own back */
extern uint32_t brr_render_view_continue;
extern uint32_t brr_draw_vision, brr_draw_texture;
#define DRAW_DISTANCE_GRID_MAX 25
#define DRAW_DISTANCE_FAR_MAX 60   /* keeps depths under 32,767 and the tables' sums in 32 bits */
void brr_hook_render_view(void);

/* distance.c: the far passes beyond the grid (site landscape_walk_call) */
extern int brr_far_distance;
extern uint32_t brr_landscape_walk_fn, brr_landscape_walk_loop, brr_landscape_tables_fn;
uint32_t __cdecl brr_landscape_walk(void);
void brr_hook_landscape_walk(void);
uint32_t brr_call_landscape_walk(void);     /* thunks.asm: the game's walk, 0x41D5E4 */
void brr_call_landscape_tables(void);       /* the game's camera-space tables, 0x41D24C */
void brr_call_landscape_far_walk(void);     /* the walk's loop (0x41D618) on our grid */
/* Experimental "Max" render resolution (render.c): site set_view_entry sends the game's
 * set_view (0x425F78) through brr_view_size, which enlarges the single-view 640x480 race view */
extern uint32_t brr_set_view_continue;
uint32_t __cdecl brr_view_size(uint32_t width, uint32_t height);
void brr_hook_set_view(void);
void __cdecl brr_call_set_view(uint32_t width, uint32_t height);  /* thunks.asm: the game's set_view */
extern uint32_t brr_orig_screenshot_race, brr_orig_screenshot_tape;
void __cdecl brr_screenshot_begin(void);
void __cdecl brr_screenshot_end(void);
void brr_hook_screenshot_race(void);
void brr_hook_screenshot_tape(void);
#define RENDER_MAX_W 1008   /* the engine's reciprocal table covers spans up to 1,022 pixels; rows */
#define RENDER_MAX_H 756    /* must be a multiple of 16 wide (its row stride and the 2D code's agree) */
void __cdecl brr_render_before_present(void);
void __cdecl brr_render_after_present(void);
uint32_t __cdecl brr_menu_mouse(void);
void brr_hook_menu_mouse(void);

/* controller.c */
extern uint32_t brr_orig_joystick_to_keys;
void brr_controller_start(void);
void __cdecl brr_before_joystick(void);
void brr_hook_joystick(void);

/* keys.c */
extern uint32_t brr_orig_menu_hotspots;
void __cdecl brr_before_menu_hotspots(void);
void __cdecl brr_after_menu_hotspots(void);
void brr_hook_menu_hotspots(void);

/* music.c */
int brr_music_check(void);
void brr_music_start(void);
void __cdecl brr_music_reassert(void);
void brr_hook_music_check(void);

/* display.c */
extern uint32_t brr_display_windowed, brr_display_fullscreen, brr_display_prompt;
int __cdecl brr_display_choice(uint32_t depth);
int __cdecl brr_present_gdi(void);
BOOL WINAPI brr_set_menu(HWND window, HMENU menu);
void brr_hook_present(void);
extern int brr_gdi_present;
extern uint32_t brr_present_continue;
void brr_hook_display_choice(void);

/* clock.c: frame pacing, decided at the first frame */
extern int brr_cnc_ddraw, brr_vsync_active;
void __cdecl brr_before_present_window(void);
void brr_hook_present_window(void);
extern uint32_t brr_orig_present_window;
extern uint32_t brr_orig_music_check;

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
