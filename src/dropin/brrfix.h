/* Big Red Launcher's PATCH.DLL (the drop-in fix): it stands in for the game's TIMER.DLL (the
 * launcher's WINRACEP.EXE imports it instead) for the player's own v2.25 WINRACE.EXE. Our own
 * code only; it patches the game in memory, never on disk.
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
    int render_max;          /* [display] render_resolution=max: experimental, races drawn at the
                              * screen's (or window's) own height, 4:3 (render.c; the new renderer only) */
    int render_max_part;     /* [display] render_max_height=1/N: Max's rows are the screen's (or
                              * window's) height / N (2 by default; 1 = all of it): the game draws in
                              * software, so rows cost frame rate */
    int render_max_rows;     /* [display] render_max_height=ROWS (before 2026-11-16): at most ROWS; 0 = no limit */
    int full_colour;         /* [display] full_colour: the new renderer presents 32-bit pictures */
    int smooth_sky;          /* [display] smooth_sky: the sky as a smooth gradient (sky.c; full colour) */
    int smooth_shading;      /* [display] smooth_shading: lit polygons shaded smoothly (shade.c; full colour) */
    int hud_lowres;          /* [display] hud=lowres: the race HUD from the 320x200 art on its own layers,
                              * enlarged by whole numbers (hud.c; the new renderer only); game = the game's own */
    int hud_aspect43;        /* [display] hud_aspect43: the HUD's blocks the 4:3 monitor's shape (1) or square */
    int hud_large;           /* [display] hud_size=large: the 1996 low resolution's size; small (half) the default */
    int hud_map_large;       /* [display] hud_map=large: the map twice the rest of the HUD's size (hud.c) */
    int widescreen;          /* [display] widescreen: Max races at the screen's shape (render.c) */
    int widescreen_menus;    /* [display] widescreen_menus: menu backgrounds across the whole width (render.c) */
    int splash_seconds;      /* [display] splash_seconds: the start-up splash held this long, unless skipped (splash.c) */
    int precise_3d;          /* [display] precise_3d: the 3D maths in 64 bits and culling by the real scale */
    int object_lighting;     /* [display] object_lighting: far objects lit after effects (the game left them unlit) */
    int sort_footprint;      /* [display] sort_footprint: objects drawn in order of their footprint, not their height */
    int ground_perspective;  /* [display] ground_perspective: every textured ground square perspective-correct (persp.c) */
    int close_gaps;          /* [display] close_gaps: level pieces one unit apart drawn together; exact edge slopes */
    int horizon_culling;     /* [display] horizon_culling: skip what nearer ground will paint over (horizon.c) */
    int distance_volume;     /* [sound] distance_volume: the effects' full volume range, nearest vehicles first (sound.c) */
    int stereo_pan;          /* [sound] stereo_pan: pans as a balance, not near hard left or right (sound.c) */
    int sound_channels;      /* [sound] channels: 8 (the game's), 16 or 32 (the default; sound.c) */
    int object_shading;      /* [display] object_shading: 0 game (smooth near, flat beyond 8 squares), 1 smooth, 2 flat */
    int texture_distance;    /* [display] texture_distance with max_draw_distance: 0 the slider's, else squares */
    int object_detail;       /* [display] object_detail: 0 original, 1 matched to the resolution, 2 highest */
    int colour_animation;    /* [display] colour_animation: the levels' palette cycling, as in DOS (palanim.c) */
    int menu_navigation;     /* [controller] menu_navigation: the joystick and arrow keys in menus (menunav.c) */
    int menu_animations;     /* [controller] menu_animations: 0 original (the selection still), 1 inverted */
    int menu_still_titles;   /* [controller] still_titles: menu headings held still in the navigation */
    int cd_volume;           /* [sound] cd_volume: the sound screen's CD volume, as in DOS (music.c) */
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
    int pad_vibration, pad_nitro_rumble;        /* [controller] vibration (a buzz on damage), nitro_rumble */
    int pad_damage_strength, pad_nitro_strength; /* % of the motors: vibration_intensity, and half of it */
    int debug_pad_test;      /* tests: a simulated pad (half right, full throttle) */
    int debug_player_type[2];  /* tests: each player's controller type in a race (-1: the game's) */
    int debug_log_joysticks;   /* tests: the game's two joysticks in BRRFIX.LOG when they change */
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
    int debug_span_census;          /* log which span routines (polygon fills) are called */
    char debug_level[16];           /* tests: race this level (e.g. CITY1) whatever the menus chose */
    int debug_shade_split;          /* tests: smooth shading on the picture's left half only */
    int debug_lod_log;              /* tests: each model's level-of-detail distances, once (render.c) */
    int debug_sound_log;            /* tests: each vehicle's pan and distance every 2 s (sound.c) */
    int debug_horizon_log;          /* tests: N: the coverage line for N frames from tick 300; 999: no culling, timed (horizon.c) */
    int debug_shot_tick;            /* tests: save the full-colour picture at this race tick */
    int debug_shade_no_corners;     /* tests: smooth shading without the corners' fractions */
};

extern struct brr_config cfg;
extern volatile LONG brr_timer_count;  /* exported as _TIMER_Count */
extern int brr_patched;                /* 1 when every check passed and patches are in */
extern uint32_t brr_orig_race_tick, brr_orig_race_draw, brr_orig_camera_update;
extern uint32_t brr_orig_playback_tick, brr_orig_playback_draw;
void brr_hook_playback_tick(void);
void brr_hook_playback_draw(void);
extern uint32_t brr_orig_preview_tick, brr_orig_preview_draw;
void brr_hook_preview_tick(void);
void brr_hook_preview_draw(void);

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
void brr_display_list_report(void);
void brr_mouse_settle(void);            /* render.c: no mouse-camera jump at a race's start */  /* render.c: the display list's fill, every report */

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
HMODULE brr_ddraw_module(void);
int brr_is_cnc_ddraw(HMODULE ddraw);
const uint8_t *brr_render_compose(int out_w, int out_h, int out_pitch);
/* full colour (render.c, 2026-11-15): the final picture in 32 bits (0x00RRGGBB), from the
 * game's palette, the sky smooth (sky.c); full screen asks DirectDraw for 32 bits (site dd_mode_bpp) */
extern int brr_full_colour;
int brr_render_compose32(uint32_t *out, int out_w, int out_h, int out_pitch);
void brr_render_palette_check(void);
void brr_shade_census_setup(void);
void brr_shade_setup(void);
void brr_colour_anim(uint32_t ticks);
void __cdecl brr_hook_scn_commit(void);
void __cdecl brr_hook_scn_end(void);
extern int brr_modern_window;
void brr_shade_view_begin(void);
const uint16_t *brr_shade_row(const uint8_t *pixel, int count);
extern int brr_shade_active;
void __cdecl brr_span_gouraud(void);
void __cdecl brr_span_gouraud_z(void);
void __cdecl brr_poly_light_smooth(void);
void brr_shade_census_report(void);
void brr_debug_every_frame(void);
void brr_debug_frame_shot(const uint32_t *picture, int width, int height, int pitch);
/* hud.c: the race HUD on layers of its own */
extern int brr_hud_active;
void brr_hud_setup(void);
void brr_hook_hud_draw(void);
void brr_hook_hud_icon(void);
void brr_hook_raw_name(void);              /* thunks.asm: site raw_name_call */
void brr_hook_splash_present(void);        /* thunks.asm: site splash_present */
void brr_hook_frame_message(void);         /* thunks.asm: site frame_message */
void __cdecl brr_splash_shown(void);       /* splash.c */
int __cdecl brr_splash_hold(void);
extern int brr_splash_on;                  /* the start-up splash is the game's picture (splash.c, render.c) */
void brr_hook_persp_setup(void);           /* thunks.asm: site persp_setup_hook */
void brr_hook_scenery_pos(void);           /* thunks.asm: site scenery_snap */
void brr_hook_display_list(void);          /* thunks.asm: site display_list_call */
void __cdecl brr_horizon_cull(void);       /* horizon.c */
void brr_hook_sound_nearest(void);         /* thunks.asm: site sound_nearest_first */
extern int32_t brr_ds_volumes[128];          /* sound.c: site ds_volume_curve */
extern int32_t brr_ds_pans[255];             /* sound.c: site ds_pan_curve */
#define SOUND_CHANNELS_MAX 32  /* sound.c: sites snd@ */
extern int32_t brr_snd_keep[SOUND_CHANNELS_MAX], brr_snd_data[SOUND_CHANNELS_MAX], brr_snd_busy[SOUND_CHANNELS_MAX];
extern uint8_t brr_snd_requests[SOUND_CHANNELS_MAX * 0x1B], brr_snd_channels[SOUND_CHANNELS_MAX * 0x1B];
extern void *brr_ds_buffers[SOUND_CHANNELS_MAX];
extern int brr_sound_debug_on;
void __cdecl brr_sound_debug(const int32_t *frame);
void brr_hook_ds_pan(void);                 /* thunks.asm: site ds_pan_curve */
void brr_sound_setup(void);
extern uint32_t brr_horizon_records, brr_horizon_records_culled, brr_horizon_objects, brr_horizon_objects_culled;
extern int64_t brr_horizon_time, brr_horizon_frames, brr_horizon_draw_time;
void __cdecl brr_horizon_drawn(void);
int __cdecl brr_persp_setup(void);         /* persp.c */
extern uint32_t brr_persp_setup_original, brr_count_persp;
void brr_hook_sprite_scaled(void);
int brr_hud_overlay(const uint8_t *origin, int w, int h, void *out, int out_w, int out_h, int out_pitch,
                    int px, int py, int sx, int sy, const uint32_t *colours, int again);
/* sky.c */
#define SKY_MARKER 255       /* the colour the game's sky bands are drawn in, with smooth_sky */
extern int brr_sky_active;
void __cdecl brr_sky_draw(int16_t *ground, int16_t *sky);
void brr_hook_sky_draw(void);
int brr_sky_begin(const uint8_t *picture, int pic_w, int pic_h, int pitch, int sx, int sy, int lx, int ly);
int brr_sky_view_of_row(int gy);
void brr_sky_span(uint32_t *out, int fx, int fy, int count, int i);
uint32_t __cdecl brr_render_mode(uint32_t windowed);
void brr_hook_dd_setup(void);
/* Experimental draw distance: site render_view_entry (thunks.asm) draws each view with the
 * vision distance at 25 (the most the game's 64x64 visible-area grid holds; the game's own
 * limit is 22) and the texture distance at brr_draw_texture, then puts the player's own back */
extern uint32_t brr_render_view_continue;
extern uint32_t brr_draw_vision, brr_draw_texture;
#define DRAW_DISTANCE_GRID_MAX 25
#define DRAW_DISTANCE_FAR_MAX 60   /* keeps depths under 32,767 and the tables' sums in 32 bits */
#define BRR_RECORDS 8192           /* distance.c: the display list's landscape records and object nodes */
#define BRR_NODES 8192
extern uint8_t brr_records[], brr_nodes[];
extern uint32_t brr_record_limit;
extern uint32_t brr_edge_squares;            /* distance.c: squares added near the screen's sides */
extern int32_t brr_object_vision;           /* the squares objects are gathered from (site object_radius_vision) */
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
/* The 3D engine's reciprocal table (2^32 / n, 0x54A380) has 1,024 entries: spans up to 1,022
 * pixels, edges up to 1,023 rows. Ours has RECIPROCALS, and every reader is pointed at it (the
 * recip_* sites), so Max views can be up to RENDER_LIMIT_W x RENDER_LIMIT_H; a multiple of 16
 * wide (the engine's row stride and the 2D code's agree). */
#define RECIPROCALS 8192
#define RENDER_LIMIT_W 8176
#define RENDER_LIMIT_H 6132
extern uint32_t brr_reciprocals[RECIPROCALS];
extern int brr_view_max_w, brr_view_max_h;  /* the engine's buffers: 640x480, or Max's view */
extern int brr_widescreen;                  /* native widescreen decided (render.c) */
extern int32_t brr_proj_scale;
int __cdecl brr_landscape_width(int q);
void brr_hook_proj_scale(void);             /* thunks.asm: sites proj_scale_store, landscape_width */
void brr_hook_landscape_width(void);
void brr_backface(void);                   /* thunks.asm: [display] precise_3d, object_lighting */
void brr_project_x(void);
void brr_hook_lod(void);                   /* thunks.asm: site lod_select; render.c */
void brr_hook_fit_depth(void);              /* thunks.asm: site fit_depth */
int32_t __cdecl brr_lod_scale(int32_t depth);
extern int32_t brr_lod_depth;
extern uint32_t brr_close_ups_skipped;
const char *brr_model_name(const void *model);
void brr_hook_shading_threshold(void);
int __cdecl brr_shading_threshold(void);
extern uint32_t brr_count_unlit;
void brr_project_y(void);
extern uint32_t brr_count_project, brr_count_backface, brr_count_clip;
void brr_clip_t_x(void);
void brr_clip_t_y(void);
void brr_cull_margin(void);
void brr_shading_restore(void);
void brr_reciprocals_fill(void);
void brr_hook_sprite_rows(void);     /* thunks.asm: sites sprite_scale_rows and _columns */
void brr_hook_sprite_columns(void);
int __cdecl brr_render_before_present(void);  /* 1: it presented the frame itself */
void __cdecl brr_render_after_present(void);
uint32_t __cdecl brr_menu_mouse(void);
void brr_hook_menu_mouse(void);

/* music.c: the sound screen's CD volume, and thunks.asm's way in */
void __cdecl brr_sound_cd_draw(void);
void brr_sound_cd_action(void);
void brr_hook_sound_draw(void);
void __cdecl brr_call_sprite_draw(int frame, int x, int y);

/* controller.c */
extern uint32_t brr_orig_joystick_to_keys;
void brr_controller_start(void);
void brr_controller_stop(void);
void __cdecl brr_before_joystick(void);
void brr_hook_joystick(void);
int brr_controller_any_button(void);       /* a button on any pad or joystick, read now (splash.c) */

/* keys.c */
extern uint32_t brr_orig_menu_hotspots;
void __cdecl brr_before_menu_hotspots(void);
void __cdecl brr_after_menu_hotspots(void);
void brr_hook_menu_hotspots(void);

/* navwin.c, thunks.asm: the joystick menu navigation (the core: menunav.h) */
extern uint32_t brr_orig_hotspot_draw, brr_orig_sprite_draw, brr_orig_text_draw;
void brr_hook_hotspot_draw(void);              /* site hotspot_draw_call */
void brr_hook_label_draw(void);                /* site label_draw_call */
void brr_sound_sprite_down(void);              /* sites sound_button_*: frame base - step */
void brr_sound_sprite_up(void);                /* base + step */
void brr_sound_sprite_up2(void);               /* base + 2 x step */
void brr_knob_sprite(void);                    /* sites slider_knob_*: animation 1's frame */
void __cdecl brr_call_play_sample(const char *name);  /* the game's sound_play_sample */

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
