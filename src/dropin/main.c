/* DLL start-up, configuration (BRRFIX.INI), logging (BRRFIX.LOG) and the game's
 * TIMER.DLL interface, which WINRACEP.EXE imports from PATCH.DLL: TIMER_Init_, TIMER_Shutdown_, _TIMER_Count
 * and _TIMER_Handler@20 (thin register-preserving thunks in thunks.asm call these). */
#include "brrfix.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

struct brr_config cfg;
volatile LONG brr_timer_count;

static HINSTANCE self;
static FILE *log_file;
static CRITICAL_SECTION log_lock;
static int timer_running;

void brr_path_beside_dll(char *out, DWORD size, const char *name)
{
    DWORD n = GetModuleFileNameA(self, out, size);
    while (n > 0 && out[n - 1] != '\\' && out[n - 1] != '/')
        n--;
    out[n] = 0;
    lstrcpynA(out + n, name, (int)(size - n));
}

void brr_log(const char *format, ...)
{
    va_list args;
    if (!log_file)
        return;
    EnterCriticalSection(&log_lock);
    int64_t ms = brr_qpc_freq() ? brr_qpc() * 1000 / brr_qpc_freq() : 0;
    fprintf(log_file, "%7ld.%03ld ", (long)(ms / 1000 % 10000000), (long)(ms % 1000));
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fputc('\n', log_file);
    fflush(log_file);
    LeaveCriticalSection(&log_lock);
}

static int ini_int(const char *path, const char *section, const char *key, int fallback)
{
    return (int)GetPrivateProfileIntA(section, key, fallback, path);
}

static int ini_milli(const char *path, const char *section, const char *key, int fallback)
{
    char text[64];
    GetPrivateProfileStringA(section, key, "", text, sizeof text, path);
    if (!text[0])
        return fallback;
    /* Parse "70.0" without floating point: whole part, then up to three decimals. */
    int value = 0, decimals = -1, negative = 0;
    const char *p = text;
    if (*p == '-') { negative = 1; p++; }
    for (; *p; p++) {
        if (*p == '.' && decimals < 0) { decimals = 0; continue; }
        if (*p < '0' || *p > '9') break;
        if (decimals >= 3) continue;
        value = value * 10 + (*p - '0');
        if (decimals >= 0) decimals++;
    }
    for (int i = decimals < 0 ? 0 : decimals; i < 3; i++)
        value *= 10;
    return negative ? -value : value;
}

static void load_config(void)
{
    char path[MAX_PATH];
    char text[32];
    brr_path_beside_dll(path, sizeof path, "BRRFIX.INI");
    cfg.fix_vga = ini_int(path, "fixes", "vga", 1);
    cfg.fix_clock = ini_int(path, "fixes", "clock", 1);
    cfg.fix_cap = ini_int(path, "fixes", "cap", 1);
    cfg.fix_interpolation = ini_int(path, "fixes", "interpolation", 1);
    cfg.fix_camera = ini_int(path, "fixes", "camera", 1);
    cfg.fix_menu_keys = ini_int(path, "fixes", "menu_keys", 1);
    cfg.fix_mode_switch = ini_int(path, "fixes", "mode_switch", 1);
    GetPrivateProfileStringA("display", "resolution", "high", text, sizeof text, path);
    cfg.resolution_high = !lstrcmpiA(text, "high");
    GetPrivateProfileStringA("display", "renderer", "new", text, sizeof text, path);
    cfg.renderer_new = lstrcmpiA(text, "original") != 0;
    GetPrivateProfileStringA("display", "render_resolution", "max", text, sizeof text, path);
    cfg.render_max = !lstrcmpiA(text, "max");
    GetPrivateProfileStringA("display", "render_max_height", "1080", text, sizeof text, path);
    cfg.render_max_rows = !lstrcmpiA(text, "screen") ? 0 : atoi(text);
    if (cfg.render_max_rows != 0 && cfg.render_max_rows < 480)
        cfg.render_max_rows = 1080;
    GetPrivateProfileStringA("display", "language", "ENG", text, sizeof text, path);
    {
        static const char *const codes[] = {"ENG", "USA", "FRE", "GER", "ITA", "SPA", "JAP"};
        int i;
        cfg.language = 1;
        for (i = 0; i < 7; i++)
            if (!lstrcmpiA(text, codes[i]))
                cfg.language = i + 1;
    }
    GetPrivateProfileStringA("clock", "source", "qpc", text, sizeof text, path);
    cfg.clock_original = lstrcmpiA(text, "original") == 0;
    cfg.rate_milli = (uint32_t)ini_milli(path, "clock", "tick_rate", 70000);
    if (cfg.rate_milli < 1000 || cfg.rate_milli > 1000000)
        cfg.rate_milli = 70000;
    cfg.cap = ini_int(path, "clock", "catch_up_cap", 8);
    if (cfg.cap < 1 || cfg.cap > 255)
        cfg.cap = 8;
    GetPrivateProfileStringA("clock", "frame_limit", "auto", text, sizeof text, path);
    if (!text[0] || !lstrcmpiA(text, "auto"))
        cfg.frame_limit = FRAME_LIMIT_AUTO;
    else {
        cfg.frame_limit = ini_int(path, "clock", "frame_limit", 0);
        if (cfg.frame_limit < 0)  /* -1 meant unlimited in earlier versions */
            cfg.frame_limit = 0;
    }
    cfg.vsync = ini_int(path, "clock", "vsync", 0);
    GetPrivateProfileStringA("display", "window_scale", "auto", text, sizeof text, path);
    cfg.window_scale = !lstrcmpiA(text, "auto") ? 0 : ini_int(path, "display", "window_scale", 0);
    if (cfg.window_scale < 0 || cfg.window_scale > 8)
        cfg.window_scale = 0;
    GetPrivateProfileStringA("display", "aspect", "4:3", text, sizeof text, path);
    cfg.window_aspect43 = !lstrcmpiA(text, "4:3");
    cfg.max_draw_distance = ini_int(path, "display", "max_draw_distance", 0);
    if (cfg.max_draw_distance < 0)
        cfg.max_draw_distance = 0;
    else if (cfg.max_draw_distance > 0 && cfg.max_draw_distance < DRAW_DISTANCE_GRID_MAX)
        cfg.max_draw_distance = DRAW_DISTANCE_GRID_MAX;  /* 1 (the first experiment) means the most */
    else if (cfg.max_draw_distance > DRAW_DISTANCE_FAR_MAX)
        cfg.max_draw_distance = DRAW_DISTANCE_FAR_MAX;
    cfg.debug_log_mouse = ini_int(path, "debug", "log_mouse", 0);
    cfg.full_colour = ini_int(path, "display", "full_colour", 1);
    cfg.smooth_sky = ini_int(path, "display", "smooth_sky", 1);
    cfg.smooth_shading = ini_int(path, "display", "smooth_shading", 1);
    cfg.colour_animation = ini_int(path, "display", "colour_animation", 1);
    cfg.menu_navigation = ini_int(path, "controller", "menu_navigation", 1);
    cfg.show_version = ini_int(path, "display", "show_version", 1);
    cfg.controller = ini_int(path, "controller", "enabled", 1);
    cfg.pad_deadzone = ini_int(path, "controller", "deadzone", 15);
    if (cfg.pad_deadzone < 0 || cfg.pad_deadzone > 60) cfg.pad_deadzone = 15;
    cfg.pad_sensitivity = ini_int(path, "controller", "sensitivity", 100);
    if (cfg.pad_sensitivity < 10 || cfg.pad_sensitivity > 300) cfg.pad_sensitivity = 100;
    cfg.pad_curve = ini_int(path, "controller", "curve", 150);
    if (cfg.pad_curve < 100 || cfg.pad_curve > 300) cfg.pad_curve = 150;
    GetPrivateProfileStringA("controller", "layout", "triggers", text, sizeof text, path);
    cfg.pad_triggers = lstrcmpiA(text, "stick") != 0;
    cfg.pad_analogue = ini_int(path, "controller", "analogue", 1);
    cfg.pad_vibration = ini_int(path, "controller", "vibration", 1);
    cfg.pad_nitro_rumble = ini_int(path, "controller", "nitro_rumble", 1);
    cfg.pad_damage_strength = ini_int(path, "controller", "vibration_intensity", 80);
    if (cfg.pad_damage_strength < 0 || cfg.pad_damage_strength > 100)
        cfg.pad_damage_strength = 80;
    cfg.pad_nitro_strength = cfg.pad_damage_strength / 2;
    cfg.debug_pad_test = ini_int(path, "debug", "pad_test", 0);
    cfg.debug_player_type[0] = ini_int(path, "debug", "player1_type", -1);
    cfg.debug_player_type[1] = ini_int(path, "debug", "player2_type", -1);
    cfg.debug_log_joysticks = ini_int(path, "debug", "log_joysticks", 0);
    cfg.music = ini_int(path, "music", "music", 1);
    cfg.music_volume = ini_int(path, "music", "volume", 100);
    if (cfg.music_volume < 0 || cfg.music_volume > 100)
        cfg.music_volume = 100;
    cfg.music_follow_game = ini_int(path, "music", "follow_game_volume", 1);
    GetPrivateProfileStringA("music", "folder", "Music", cfg.music_folder, sizeof cfg.music_folder, path);
    GetPrivateProfileStringA("display", "mode", "fullscreen", text, sizeof text, path);
    cfg.display_mode = !lstrcmpiA(text, "fullscreen") ? DISPLAY_FULLSCREEN
                     : !lstrcmpiA(text, "ask") ? DISPLAY_ASK : DISPLAY_WINDOWED;
    GetPrivateProfileStringA("clock", "pacing", "dropin", text, sizeof text, path);
    cfg.pacing_dropin = lstrcmpiA(text, "cnc-ddraw") != 0;
    cfg.interp_delay_milli = ini_milli(path, "interpolation", "delay", 2000);
    if (cfg.interp_delay_milli < 0 || cfg.interp_delay_milli > 8000)
        cfg.interp_delay_milli = 2000;
    cfg.camera_smoothing = ini_int(path, "camera", "smoothing", 0);
    if (cfg.camera_smoothing < 0 || cfg.camera_smoothing > 1000)
        cfg.camera_smoothing = 0;
    cfg.camera_roll_fix = ini_int(path, "camera", "roll_fix", 1);
    cfg.fine_angles = ini_int(path, "camera", "fine_angles", 1);
    cfg.log = ini_int(path, "log", "log", 0);
    cfg.log_clock = ini_int(path, "log", "clock", 1);
    cfg.log_state = ini_int(path, "log", "state", 0);
    cfg.log_camera = ini_int(path, "log", "camera", 0);
    cfg.check_restore = ini_int(path, "log", "check_restore", 0);
    cfg.debug_ignore_exe_checksum = ini_int(path, "debug", "ignore_exe_checksum", 0);
    cfg.debug_camera_mode = ini_int(path, "debug", "camera_mode", -1);
    cfg.debug_capture_frames = ini_int(path, "debug", "capture_frames", 0);
    cfg.debug_capture_from_tick = ini_int(path, "debug", "capture_from_tick", 0);
    GetPrivateProfileStringA("debug", "input", "", cfg.debug_input, sizeof cfg.debug_input, path);
    cfg.debug_writeset_frames = ini_int(path, "debug", "writeset_frames", 0);
    cfg.debug_writeset_from_tick = ini_int(path, "debug", "writeset_from_tick", 0);
    cfg.debug_dump_tick = ini_int(path, "debug", "dump_records_tick", 0);
    cfg.debug_dump_count = ini_int(path, "debug", "dump_records_count", 0);
    cfg.debug_hold_pitch = ini_int(path, "debug", "hold_pitch", 0);
    cfg.debug_hold_roll = ini_int(path, "debug", "hold_roll", 0);
    cfg.debug_prove_fine_angles = ini_int(path, "debug", "prove_fine_angles", 0);
    cfg.debug_log_keys = ini_int(path, "debug", "log_keys", 0);
    cfg.debug_span_census = ini_int(path, "debug", "span_census", 0);
    GetPrivateProfileStringA("debug", "level", "", cfg.debug_level, sizeof cfg.debug_level, path);
    cfg.debug_shade_split = ini_int(path, "debug", "shade_split", 0);
    cfg.debug_shot_tick = ini_int(path, "debug", "shot_tick", 0);
    cfg.debug_shade_no_corners = ini_int(path, "debug", "shade_no_corners", 0);
    if (cfg.clock_original)
        cfg.fix_clock = 0;
    if (!cfg.fix_clock && cfg.fix_interpolation) {
        cfg.fix_interpolation = 0;  /* needs the fractional clock */
    }
}

static void open_log(void)
{
    char path[MAX_PATH];
    InitializeCriticalSection(&log_lock);
    if (!cfg.log)
        return;
    brr_path_beside_dll(path, sizeof path, "BRRFIX.LOG");
    log_file = fopen(path, "w");
}

uint32_t __cdecl brr_timer_start(uint32_t instance)
{
    (void)instance;
    brr_timer_count = 0;
    if (!timer_running) {
        timer_running = 1;
        brr_clock_start();
        brr_log("TIMER_Init_: clock started (%s, %u.%03u Hz)",
                cfg.clock_original ? "original 14 ms callback" : "QueryPerformanceCounter",
                cfg.rate_milli / 1000, cfg.rate_milli % 1000);
    }
    return 1;
}

uint32_t __cdecl brr_timer_stop(void)
{
    if (timer_running) {
        timer_running = 0;
        brr_clock_stop();
        brr_controller_stop();
        brr_clock_report(1);
        brr_log("TIMER_Shutdown_: count %ld", (long)brr_timer_count);
    }
    return (uint32_t)brr_timer_count;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        self = instance;
        DisableThreadLibraryCalls(instance);
        brr_clock_setup();
        load_config();
        open_log();
        brr_log("Big Red Launcher %s (PATCH.DLL)", BRRFIX_VERSION);
        brr_log("config: vga=%d clock=%d cap=%d interpolation=%d camera=%d menu_keys=%d source=%s "
                "tick_rate=%u.%03u catch_up_cap=%d frame_limit=%d vsync=%d pacing=%s delay=%d.%03u camera_smoothing=%d fine_angles=%d roll_fix=%d "
                "resolution=%s renderer=%s render_resolution=%s render_max_height=%d aspect=%s max_draw_distance=%d",
                cfg.fix_vga, cfg.fix_clock, cfg.fix_cap, cfg.fix_interpolation, cfg.fix_camera, cfg.fix_menu_keys,
                cfg.clock_original ? "original" : "qpc", cfg.rate_milli / 1000,
                cfg.rate_milli % 1000, cfg.cap, cfg.frame_limit, cfg.vsync, cfg.pacing_dropin ? "dropin" : "cnc-ddraw",
                cfg.interp_delay_milli / 1000, cfg.interp_delay_milli % 1000, cfg.camera_smoothing, cfg.fine_angles, cfg.camera_roll_fix,
                cfg.resolution_high ? "high" : "low", cfg.renderer_new ? "new" : "original",
                cfg.render_max ? "max" : "original", cfg.render_max_rows, cfg.window_aspect43 ? "4:3" : "square", cfg.max_draw_distance);
        brr_patch_all();
        brr_interp_setup();
        brr_debug_setup();
        brr_shade_census_setup();
    } else if (reason == DLL_PROCESS_DETACH) {
        brr_interp_close();
        if (log_file) {
            brr_log("unloading");
            fclose(log_file);
            log_file = NULL;
        }
    }
    return TRUE;
}
