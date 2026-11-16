/* Verified in-memory patching. Nothing is written unless the running program on disk is
 * the exact v2.25 WINRACE.EXE, or WINRACEP.EXE made from it by the launcher (SHA-256), and every patch site and protected range in memory has
 * the expected SHA-256 (sites.h is generated from data/dropin-patches.json by
 * "python3 brr.py dropin-build"). Patches are all-or-nothing; the result is logged.
 * The game's code is identified by checksums only: no original bytes are stored. */
#include "brrfix.h"
#include <stdio.h>
#include <string.h>
#include "sites.h"
#include "sha256.h"

int brr_patched;
uint32_t brr_orig_race_tick = GAME_RACE_TICK, brr_orig_race_draw = GAME_RACE_DRAW;
uint32_t brr_orig_playback_tick = 0x413400, brr_orig_playback_draw = 0x4131DC;
uint32_t brr_orig_preview_tick = 0x423CCC, brr_orig_preview_draw = 0x423D24;
uint32_t brr_orig_camera_update = 0x404908;
uint32_t brr_rotation_continue = 0x446675, brr_game_atan = 0x412A20;
uint32_t brr_orig_present_window = 0x42CD2C;
uint32_t brr_orig_music_check = 0x409F00;
uint32_t brr_render_view_continue = 0x4176CA;  /* render_view after its six PUSHes */
uint32_t brr_draw_vision = DRAW_DISTANCE_GRID_MAX, brr_draw_texture = DRAW_DISTANCE_GRID_MAX;
int32_t brr_object_vision = DRAW_DISTANCE_GRID_MAX;
/* shown on the main menu instead of the build date: "V2.25WR FIX0.1.0" (it fits where
 * "V2.24WR (09-05-96)" was) */
static const char version_text[] = "FIX" BRRFIX_VERSION;
static const char version_number[] = "V2.25";  /* the installed patch; the game still said V2.24 */
/* The main menu's credits ticker (0x4234D8): every 128 ticks it shows CRED_<n> from the
 * language's text file (TOURN.<language> in TOURN.WAP), n counting 0 to 27 at 0x44BCE0. Ours
 * is step 2, after the publisher's and the developer's lines: the count goes to 29 (site
 * credit_count) and the lookup (site credit_lookup_call) gives our line there and CRED_<n-1>
 * after it. Written like the file's own entries (position and colour, the line, a blank
 * line); the game's font draws it in capitals, as it does theirs. */
#define CREDIT_INDEX ((volatile int32_t *)0x44BCE0)
#define CREDIT_STEP 2
uint32_t brr_orig_credit_lookup = 0x42818C;
static const char credit_text[] = "@X50,28 @C5\r\nCamera / Modern Windows fix by ctrl-alt-rees\r\n\r\n";
static char language_code[4];       /* the game's start-up language, from [display] language */
static char language_scratch[256];  /* where the config files' lang lines go instead */

static void hash_hex(const void *data, size_t size, char out[65])
{
    uint8_t digest[32];
    brr_sha256(data, size, digest);
    brr_sha256_hex(digest, out);
}

/* The credits lookup (site credit_lookup_call): our line, or NULL for the game's own lookup of
 * the name, "CRED_<n>" in the game's 20-byte buffer, renumbered after our step. */
const char *__cdecl brr_credit_text(char *name)
{
    int32_t n = *CREDIT_INDEX;
    if (n == CREDIT_STEP)
        return credit_text;
    if (n > CREDIT_STEP)
        wsprintfA(name, "CRED_%ld", (long)(n - 1));
    return NULL;
}

/* The DirectDraw the game loaded: cnc-ddraw by its own name (WINRACEP.EXE imports it as
 * CNC_DDRAW_DLL when the launcher found it), or ddraw.dll (Windows' own, Wine's, or cnc-ddraw
 * under that name, as before 2026-11-15). Static imports are loaded before our code runs. */
HMODULE brr_ddraw_module(void)
{
    HMODULE ddraw = GetModuleHandleA(CNC_DDRAW_DLL);
    return ddraw ? ddraw : GetModuleHandleA("ddraw.dll");
}

/* cnc-ddraw exports GameHandlesClose, which no real DirectDraw does */
int brr_is_cnc_ddraw(HMODULE ddraw)
{
    return ddraw && GetProcAddress(ddraw, "GameHandlesClose") != NULL;
}

/* ---- checks ---- */
static int exe_matches(void)
{
    char path[MAX_PATH], hex[65];
    GetModuleFileNameA(NULL, path, sizeof path);
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        brr_log("check: can't open %s to check it", path);
        return 0;
    }
    DWORD size = GetFileSize(file, NULL), got = 0;
    uint8_t *buffer = size == EXPECTED_EXE_SIZE ? HeapAlloc(GetProcessHeap(), 0, size) : NULL;
    int ok = buffer && ReadFile(file, buffer, size, &got, NULL) && got == size;
    CloseHandle(file);
    if (!ok) {
        brr_log("check: %s is %lu bytes, not the v2.25 WINRACE.EXE (%lu bytes)", path,
                (unsigned long)size, (unsigned long)EXPECTED_EXE_SIZE);
        if (buffer)
            HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    hash_hex(buffer, size, hex);
    HeapFree(GetProcessHeap(), 0, buffer);
    if (strcmp(hex, EXPECTED_WINRACEP_SHA256) == 0 || strcmp(hex, EXPECTED_WINRACEP_PLAIN_SHA256) == 0) {
        brr_log("check: %s is WINRACEP.EXE, made from the v2.25 WINRACE.EXE, %s (SHA-256 matches)", path,
                strcmp(hex, EXPECTED_WINRACEP_SHA256) == 0 ? "with cnc-ddraw (" CNC_DDRAW_DLL ")"
                                                           : "with Windows' own DirectDraw");
        return 1;
    }
    if (strcmp(hex, EXPECTED_EXE_CORRECTED_SHA256) == 0) {
        brr_log("check: %s is the v2.25 WINRACE.EXE with corrected section headers (SHA-256 matches)", path);
        return 1;
    }
    if (strcmp(hex, EXPECTED_EXE_SHA256) != 0) {
        brr_log("check: %s has SHA-256 %s, not the v2.25 WINRACE.EXE (%s)", path, hex,
                EXPECTED_EXE_SHA256);
        return 0;
    }
    brr_log("check: %s is the v2.25 WINRACE.EXE (SHA-256 matches)", path);
    return 1;
}

static int readable(uint32_t address, uint32_t size)
{
    MEMORY_BASIC_INFORMATION info;
    if (!VirtualQuery((void *)address, &info, sizeof info) || info.State != MEM_COMMIT)
        return 0;
    return address + size <= (uint32_t)info.BaseAddress + info.RegionSize;
}

static int ranges_match(const char *when)
{
    char hex[65];
    int bad = 0;
    for (size_t i = 0; i < sizeof PROTECTED / sizeof PROTECTED[0]; i++) {
        const struct range_info *r = &PROTECTED[i];
        if (!readable(r->address, r->size)) {
            brr_log("%s: protected %s at 0x%06lX is not readable", when, r->name, (unsigned long)r->address);
            bad++;
            continue;
        }
        hash_hex((const void *)r->address, r->size, hex);
        if (strcmp(hex, r->sha256) != 0) {
            brr_log("%s: protected %s at 0x%06lX differs from v2.25", when, r->name, (unsigned long)r->address);
            bad++;
        }
    }
    return bad;
}

/* ---- patches ---- */
static int enabled(const char *fix)
{
    if (!strcmp(fix, "vga")) return cfg.fix_vga;
    if (!strcmp(fix, "clock")) return cfg.fix_clock;
    if (!strcmp(fix, "cap")) return cfg.fix_cap;
    if (!strcmp(fix, "interpolation"))  /* the race hooks also carry the logs and the camera restore */
        return cfg.fix_interpolation || cfg.log_state || cfg.log_camera || cfg.check_restore
            || cfg.camera_roll_fix || cfg.camera_smoothing > 0
            || cfg.debug_capture_frames > 0 || cfg.debug_input[0] || cfg.debug_writeset_frames > 0
            || cfg.debug_dump_count > 0
            || cfg.debug_hold_pitch || cfg.debug_hold_roll || cfg.fine_angles || cfg.debug_prove_fine_angles;
    if (!strcmp(fix, "camera_view"))  /* experimental, off by default */
        return cfg.camera_roll_fix || cfg.camera_smoothing > 0 || cfg.debug_hold_pitch || cfg.debug_hold_roll
            || cfg.fine_angles;
    if (!strcmp(fix, "version"))
        return cfg.show_version;
    if (!strcmp(fix, "controller"))  /* also centres absent joysticks for the menu navigation */
        return cfg.controller || cfg.debug_pad_test || cfg.menu_navigation;
    if (!strcmp(fix, "keys"))
        return cfg.fix_menu_keys || cfg.debug_log_keys;
    if (!strcmp(fix, "display"))
        return cfg.display_mode != DISPLAY_ASK;
    if (!strcmp(fix, "present"))  /* our window drawing, and the new renderer */
        return cfg.display_mode != DISPLAY_ASK || brr_renderer_active;
    if (!strcmp(fix, "renderer"))
        return brr_renderer_active;
    if (!strcmp(fix, "renderer_resolution"))  /* the Modern renderer chooses the size itself */
        return brr_renderer_active;
    if (!strcmp(fix, "vsync"))
        return cfg.vsync;
    if (!strcmp(fix, "fine_angles"))
        return cfg.fine_angles;
    if (!strcmp(fix, "music")) return cfg.music;
    if (!strcmp(fix, "camera")) return cfg.fix_camera;
    if (!strcmp(fix, "mode_switch")) return cfg.fix_mode_switch;
    if (!strcmp(fix, "language")) return cfg.language >= 1 && cfg.language <= 7;
    if (!strcmp(fix, "resolution")) return cfg.resolution_high;
    if (!strcmp(fix, "credits")) return 1;
    if (!strcmp(fix, "render_max")) return cfg.render_max && brr_renderer_active;  /* experimental */
    if (!strcmp(fix, "full_colour")) return brr_full_colour;
    if (!strcmp(fix, "sky")) return brr_sky_active || brr_shade_active;  /* the 3D view's start, for both */
    if (!strcmp(fix, "shading")) return brr_shade_active;
    if (!strcmp(fix, "hud")) return brr_hud_active;
    if (!strcmp(fix, "precise_3d")) return cfg.precise_3d && brr_renderer_active;
    if (!strcmp(fix, "object_lighting")) return cfg.object_lighting && brr_renderer_active;
    if (!strcmp(fix, "sort_footprint")) return cfg.sort_footprint && brr_renderer_active;
    if (!strcmp(fix, "ground_perspective")) return cfg.ground_perspective && brr_renderer_active;
    if (!strcmp(fix, "close_gaps")) return cfg.close_gaps && brr_renderer_active;
    if (!strcmp(fix, "object_shading")) return cfg.object_shading && brr_renderer_active;
    if (!strcmp(fix, "object_detail")) return (cfg.object_detail != 0 || cfg.debug_lod_log) && brr_renderer_active;
    if (!strcmp(fix, "widescreen")) return brr_widescreen && brr_renderer_active && cfg.render_max;
    if (!strcmp(fix, "wide_menus"))
        return brr_widescreen && brr_renderer_active && cfg.render_max && cfg.widescreen_menus;
    if (!strcmp(fix, "menu_pad")) return cfg.menu_navigation;
    if (!strcmp(fix, "screenshot")) return (cfg.render_max && brr_renderer_active) || brr_sky_active;
    if (!strcmp(fix, "draw_distance")) return cfg.max_draw_distance > 0;  /* experimental, off by default */
    /* beyond the near pass's grid; also at 25 in widescreen, where the grid can't hold the
     * wider area (16:9 wants 24 squares either side at 25, it holds 18) and these fill the sides */
    if (!strcmp(fix, "draw_distance_far"))
        return cfg.max_draw_distance > DRAW_DISTANCE_GRID_MAX
            || (cfg.max_draw_distance == DRAW_DISTANCE_GRID_MAX && brr_widescreen);
    if (!strcmp(fix, "big_lists")) return brr_renderer_active;
    return 0;
}

static void put_rel32(uint8_t *out, uint32_t from_next, void *target)
{
    uint32_t rel = (uint32_t)target - from_next;
    memcpy(out, &rel, 4);
}

static void put_abs32(uint8_t *out, void *target)
{
    uint32_t value = (uint32_t)target;
    memcpy(out, &value, 4);
}

/* The new bytes for a site; returns 0 if the name is unknown or the length is wrong. */
static int replacement(const struct site_info *s, uint8_t *out)
{
    const char *n = s->name;
    memset(out, 0x90, s->length);
    if (!strcmp(n, "vga_wait_retrace") && s->length == 5) {
        out[0] = 0xE9;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_retrace);
    } else if (!strcmp(n, "clock_read") && s->length == 8) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_clock_read);
    } else if ((!strcmp(n, "cap_compare") || !strcmp(n, "cap_store")) && s->length == 1) {
        out[0] = (uint8_t)cfg.cap;
    } else if ((!strcmp(n, "race_tick_callback") || !strcmp(n, "race_tick_compare")) && s->length == 4) {
        put_abs32(out, (void *)brr_hook_race_tick);
    } else if (!strcmp(n, "race_draw_callback") && s->length == 4) {
        put_abs32(out, (void *)brr_hook_race_draw);
    } else if (!strcmp(n, "playback_tick_callback") && s->length == 4) {
        put_abs32(out, (void *)brr_hook_playback_tick);
    } else if (!strcmp(n, "playback_draw_callback") && s->length == 4) {
        put_abs32(out, (void *)brr_hook_playback_draw);
    } else if (!strcmp(n, "preview_tick_callback") && s->length == 4) {
        put_abs32(out, (void *)brr_hook_preview_tick);
    } else if (!strcmp(n, "preview_draw_callback") && s->length == 4) {
        put_abs32(out, (void *)brr_hook_preview_draw);
    } else if (!strcmp(n, "camera_update_call") && s->length == 5) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_camera_update);
    } else if (!strcmp(n, "present_entry") && s->length == 10) {
        out[0] = 0xE9;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_present);
        memset(out + 5, 0x90, 5);  /* NOPs after the jump (never reached) */
    } else if (!strcmp(n, "version_date") && s->length == 4) {
        put_abs32(out, (void *)version_text);
    } else if (!strcmp(n, "version_number") && s->length == 4) {
        put_abs32(out, (void *)version_number);
    } else if ((!strcmp(n, "window_command_66") || !strcmp(n, "window_command_67")) && s->length == 4) {
        put_rel32(out, s->address + 4, (void *)0x402619);  /* WndProc's default: DefWindowProcA */
    } else if (!strcmp(n, "scn_commit_call") && s->length == 5) {
        out[0] = 0xE8;  /* CALL brr_hook_scn_commit: the commit, then the joystick twins noted */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_scn_commit);
    } else if (!strcmp(n, "scn_end_call") && s->length == 5) {
        out[0] = 0xE8;  /* CALL brr_hook_scn_end: the twins added, then menu_background_save */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_scn_end);
    } else if (!strcmp(n, "class_menu") && s->length == 1) {
        out[0] = 0x74;  /* MOV [ESP+0x24],ESI: the window class has no menu */
    } else if (!strcmp(n, "window_command_resolution") && s->length == 7) {
        out[0] = 0xE9;  /* commands 0x64 and 0x65 (keypad - and +, the Modes menu) -> DefWindowProcA */
        put_rel32(out + 1, s->address + 5, (void *)0x402619);
    } else if (!strcmp(n, "set_menu_call") && s->length == 7) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_set_menu);
    } else if (!strcmp(n, "language_start") && s->length == 4) {
        static const char codes[7][4] = {"ENG", "USA", "FRE", "GER", "ITA", "SPA", "JAP"};
        memcpy(language_code, codes[cfg.language - 1], 4);
        put_abs32(out, (void *)language_code);
    } else if (!strcmp(n, "language_config") && s->length == 4) {
        put_abs32(out, (void *)language_scratch);
    } else if (!strcmp(n, "resolution_display") && s->length == 8) {
        static const uint32_t size[2] = {640, 480};
        memcpy(out, size, 8);
    } else if (!strcmp(n, "resolution_frame") && s->length == 12) {
        static const uint32_t size[3] = {640, 480, 640 * 480};
        memcpy(out, size, 12);
    } else if (!strcmp(n, "joystick_call") && s->length == 5) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_joystick);
    } else if (!strcmp(n, "set_view_entry") && s->length == 5) {
        out[0] = 0xE9;  /* JMP brr_hook_set_view, which runs the 4 instructions replaced here */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_set_view);
    } else if (!strcmp(n, "screenshot_race_call") && s->length == 5) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_screenshot_race);
    } else if (!strcmp(n, "screenshot_tape_call") && s->length == 5) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_screenshot_tape);
    } else if (!strcmp(n, "view_max") && s->length == 4) {
        uint16_t size[2];
        size[0] = (uint16_t)brr_view_max_w;
        size[1] = (uint16_t)brr_view_max_h;
        memcpy(out, size, 4);
    } else if (!strcmp(n, "scenery_snap") && s->length == 8) {
        out[0] = 0xE8;  /* CALL brr_hook_scenery_pos: the position's loads, snapped; then 3 NOPs */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_scenery_pos);
        out[5] = out[6] = out[7] = 0x90;
    } else if (!strncmp(n, "edge_slope_", 11) && s->length == 31) {
        /* the edge's x step from the full reciprocal: (dx x 2^31/dy) >> 15 in 64 bits, not dx x
         * floor(2^16/dy); the start x and the row count as before (EAX the same after; EDX is
         * cleared before its next use) */
        static const uint8_t code[31] = {
            0x8B, 0x45, 0x10,              /* MOV EAX,[EBP+0x10]   the far end's x */
            0x2B, 0x46, 0x10,              /* SUB EAX,[ESI+0x10]   dx */
            0xF7, 0xEB,                    /* IMUL EBX             EDX:EAX = dx x 2^31/dy */
            0x0F, 0xAC, 0xD0, 0x0F,        /* SHRD EAX,EDX,15      the 16.16 step */
            0x89, 0x07,                    /* MOV [EDI],EAX */
            0x8B, 0x46, 0x10,              /* MOV EAX,[ESI+0x10] */
            0xC1, 0xE0, 0x10,              /* SHL EAX,16 */
            0x89, 0x47, 0x18,              /* MOV [EDI+0x18],EAX   the start x */
            0x8B, 0xC1,                    /* MOV EAX,ECX */
            0xC1, 0xE0, 0x10,              /* SHL EAX,16           the row count's word, as before */
            0x89, 0x47, 0x30};             /* MOV [EDI+0x30],EAX */
        memcpy(out, code, 31);
    } else if (!strcmp(n, "ground_ring_routine") && s->length == 4) {
        put_abs32(out, (void *)0x41C9CCu);  /* landscape_draw_cell_near, for every textured ring */
    } else if (!strcmp(n, "ground_near_range") && s->length == 4) {
        put_abs32(out, (void *)0x46BB9Cu);  /* landscape_texture_range: perspective as far as textured */
    } else if (!strcmp(n, "persp_setup_hook") && s->length == 4) {
        put_abs32(out, (void *)brr_hook_persp_setup);
    } else if (!strncmp(n, "sort_radius_", 12) && s->length == 1) {
        out[0] = 0x0C;  /* MOV reg,[shape + 0xC]: SAR 16 then gives the footprint radius (+0xE), not +0xC's */
    } else if (!strcmp(n, "car_select_zoom") && s->length == 5) {
        /* MOV EBX, 176 (was 200): the car about 12% smaller, so it no longer reaches the menu's
         * left edge, which the widened background now shows (the director, 2026-11-16) */
        static const uint8_t mov[5] = {0xBB, 176, 0, 0, 0};
        memcpy(out, mov, 5);
    } else if (!strcmp(n, "raw_name_call") && s->length == 5) {
        out[0] = 0xE8;  /* CALL brr_hook_raw_name: which picture the screen loaded, then file_open */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_raw_name);
    } else if ((!strcmp(n, "hud_draw_call") || !strcmp(n, "hud_icon_call")) && s->length == 5) {
        out[0] = 0xE8;  /* CALL brr_hook_hud_draw / _icon: the HUD drawn into our layers (hud.c) */
        put_rel32(out + 1, s->address + 5,
                  !strcmp(n, "hud_draw_call") ? (void *)brr_hook_hud_draw : (void *)brr_hook_hud_icon);
    } else if ((!strcmp(n, "project_x") || !strcmp(n, "project_y")) && s->length == 8) {
        out[0] = 0xE8;  /* CALL brr_project_x / _y: the product in EDX:EAX (64 bits), for the IDIV */
        put_rel32(out + 1, s->address + 5, !strcmp(n, "project_x") ? (void *)brr_project_x : (void *)brr_project_y);
    } else if ((!strcmp(n, "clip_x_lerp") || !strcmp(n, "clip_y_lerp")) && s->length == 6) {
        /* IMUL EAX,ECX; SAR EAX,16 -> IMUL ECX; SHRD EAX,EDX,16 (64 bits; EDX is reloaded after) */
        static const uint8_t lerp64[6] = {0xF7, 0xE9, 0x0F, 0xAC, 0xD0, 0x10};
        memcpy(out, lerp64, 6);
    } else if ((!strcmp(n, "backface_model") || !strcmp(n, "backface_polygon")) && s->length == 15) {
        out[0] = 0xE8;  /* CALL brr_backface: the cross product in 64 bits, SF for the game's JS */
        put_rel32(out + 1, s->address + 5, (void *)brr_backface);
    } else if ((!strcmp(n, "clip_x_t") || !strcmp(n, "clip_y_t")) && s->length == 15) {
        out[0] = 0xE8;  /* CALL brr_clip_t_x / _y: the edge's fraction with a 64-bit dividend */
        put_rel32(out + 1, s->address + 5, !strcmp(n, "clip_x_t") ? (void *)brr_clip_t_x : (void *)brr_clip_t_y);
    } else if (!strcmp(n, "cull_margin") && s->length == 7) {
        out[0] = 0xE8;  /* CALL brr_cull_margin: radius x the projection scale, not x 128 */
        put_rel32(out + 1, s->address + 5, (void *)brr_cull_margin);
    } else if ((!strcmp(n, "unlit_restore_6") || !strcmp(n, "unlit_restore_9")) && s->length == 13) {
        out[0] = 0xE8;  /* CALL brr_shading_restore: mode 2 near, 1 far (the game left 0 far) */
        put_rel32(out + 1, s->address + 5, (void *)brr_shading_restore);
    } else if (!strncmp(n, "record_", 7) && s->length == 4) {
        /* the landscape records: our bigger buffer (distance.c) */
        uint8_t *records = brr_records;
        brr_record_limit = (uint32_t)(records + (BRR_RECORDS - 64) * 0xBD);
        if (!strcmp(n, "record_limit"))
            put_abs32(out, (void *)brr_record_limit);
        else if (!strcmp(n, "record_init_end") || !strcmp(n, "record_init_loop"))
            put_abs32(out, records + BRR_RECORDS * 0xBD);
        else
            put_abs32(out, records);
    } else if (!strncmp(n, "node_", 5) && s->length == 4) {
        /* the object nodes: our bigger pool (distance.c); node_first, node_hint, node_limit_* */
        put_abs32(out, strncmp(n, "node_limit", 10) ? (void *)brr_nodes : (void *)(brr_nodes + BRR_NODES * 17));
    } else if (!strcmp(n, "object_radius_vision") && s->length == 4) {
        brr_object_vision = cfg.max_draw_distance;  /* objects as far as the ground */
        put_abs32(out, (void *)&brr_object_vision);
    } else if (!strcmp(n, "lod_select") && s->length == 7) {
        out[0] = 0xE8;  /* CALL brr_hook_lod: brr_lod_depth for the loop, then the game's CMP */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_lod);
    } else if ((!strcmp(n, "lod_compare") || !strcmp(n, "if_distance_depth")) && s->length == 4) {
        put_abs32(out, (void *)&brr_lod_depth);  /* [0x54B488] -> [brr_lod_depth] */
    } else if (!strcmp(n, "fit_depth") && s->length == 6) {
        out[0] = 0xE8;  /* CALL brr_hook_fit_depth: the matched depth, then XOR EAX,EAX; MOV AX,[ECX+22h] */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_fit_depth);
    } else if (!strcmp(n, "shading_threshold") && s->length == 31) {
        out[0] = 0xE8;  /* CALL brr_hook_shading_threshold (EBP), then the XOR EDI,EDI it replaced */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_shading_threshold);
        out[5] = 0x31;
        out[6] = 0xFF;
    } else if (!strcmp(n, "proj_scale_store") && s->length == 5) {
        out[0] = 0xE8;  /* CALL brr_hook_proj_scale: our widescreen view's scale from its height */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_proj_scale);
    } else if (!strcmp(n, "landscape_width") && s->length == 12) {
        out[0] = 0xE8;  /* CALL brr_hook_landscape_width: the game's division, widened; NOPs after */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_landscape_width);
    } else if (!strcmp(n, "sprite_scaled_entry") && s->length == 6) {
        out[0] = 0xE9;  /* JMP brr_hook_sprite_scaled, which runs the 4 instructions replaced here */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_sprite_scaled);
    } else if (!strcmp(n, "sprite_scale_rows") && s->length == 11) {
        out[0] = 0xE8;  /* CALL brr_hook_sprite_rows: EAX = the rows' error start; NOPs after */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_sprite_rows);
    } else if (!strcmp(n, "sprite_scale_columns") && s->length == 6) {
        out[0] = 0xE8;  /* CALL brr_hook_sprite_columns: EAX = the columns' error start; a NOP after */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_sprite_columns);
    } else if (!strncmp(n, "recip_", 6) && s->length == 4) {
        /* the address in [reg*4 + 0x54A380] (edges, lines: entry n) or [reg*4 + 0x54A384]
         * (spans: entry length + 1) -> the same entry of ours */
        put_abs32(out, (uint8_t *)brr_reciprocals + (strncmp(n, "recip_span_", 11) ? 0 : 4));
    } else if (!strcmp(n, "credit_lookup_call") && s->length == 5) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_credit_lookup);
    } else if (!strcmp(n, "credit_count") && s->length == 1) {
        out[0] = 29;  /* MOV EBX,28 -> MOV EBX,29: the game's 28 lines and ours */
    } else if (!strcmp(n, "menu_hotspots_call") && s->length == 5) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_menu_hotspots);
    } else if (!strcmp(n, "display_choice") && s->length == 5) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_display_choice);
    } else if (!strcmp(n, "music_check_call") && s->length == 5) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_music_check);
    } else if (!strcmp(n, "present_window_call") && s->length == 5) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_present_window);
    } else if (!strcmp(n, "dd_setup_entry") && s->length == 9) {
        out[0] = 0xE9;  /* JMP brr_hook_dd_setup, which runs the 4 instructions replaced here */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_dd_setup);
    } else if (!strcmp(n, "render_view_entry") && s->length == 6) {
        /* textures as far as the ground is drawn (the game's own limit is 14), a distance of
         * their own, or the player's slider (0: the hook leaves it) */
        brr_draw_texture = cfg.texture_distance < 0 ? (uint32_t)cfg.max_draw_distance
                         : cfg.texture_distance > cfg.max_draw_distance ? (uint32_t)cfg.max_draw_distance
                         : (uint32_t)cfg.texture_distance;
        out[0] = 0xE9;  /* JMP brr_hook_render_view, which runs the 6 PUSHes replaced here */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_render_view);
    } else if (!strcmp(n, "landscape_walk_call") && s->length == 5) {
        brr_far_distance = cfg.max_draw_distance;
        out[0] = 0xE8;  /* CALL brr_hook_landscape_walk: the game's walk, then the far passes */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_landscape_walk);
    } else if (!strcmp(n, "dd_mode_check") && s->length == 20) {
        out[0] = 0xEB;  /* JMP to the full-screen cooperative level (0x11), whatever the mode's size */
        out[1] = 0x35;
    } else if (!strcmp(n, "dd_mode_bpp") && s->length == 1) {
        out[0] = 32;  /* PUSH 0x20: SetDisplayMode's bits per pixel (full colour; render.c presents) */
    } else if (!strncmp(n, "fill_", 5) && s->length % 4 == 0) {
        /* fill descriptors (shade.c): the smooth lighting and Gouraud spans -> our copies */
        uint32_t k;
        memcpy(out, (const void *)s->address, s->length);
        for (k = 0; k < s->length; k += 4) {
            uint32_t *slot = (uint32_t *)(out + k);
            if (*slot == 0x446D70u)
                *slot = (uint32_t)brr_poly_light_smooth;
            else if (*slot == 0x4438D0u)
                *slot = (uint32_t)brr_span_gouraud_z;
            else if (*slot == 0x443710u)
                *slot = (uint32_t)brr_span_gouraud;
        }
    } else if (!strcmp(n, "sky_draw_call") && s->length == 5) {
        out[0] = 0xE8;  /* CALL brr_hook_sky_draw: the sky's geometry recorded, its bands in the marker colour */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_sky_draw);
    } else if (!strcmp(n, "menu_double_call") && s->length == 5) {
        /* NOPs: the menus' doubling isn't called; the present scales them (render.c) */
    } else if (!strcmp(n, "menu_mouse") && s->length == 56) {
        out[0] = 0xE8;  /* CALL brr_hook_menu_mouse: CX, BX = the menu's position */
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_menu_mouse);
        out[5] = 0xEB;  /* JMP to the stores after the original mapping */
        out[6] = (uint8_t)(s->length - 7);
    } else if (!strcmp(n, "rotation_builder") && s->length == 5) {
        out[0] = 0xE9;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_rotation);
    } else if (!strcmp(n, "approach_clamped") && s->length == 5) {
        out[0] = 0xB0;  /* MOV AL,1 */
        out[1] = 0x01;
    } else if (!strcmp(n, "camera_mode4_previous") && s->length == 14) {
        static const uint8_t clear24[6] = {0x66, 0xC7, 0x46, 0x24, 0x00, 0x00};
        memcpy(out, clear24, 6);
        out[6] = 0xE8;
        put_rel32(out + 7, s->address + 11, (void *)brr_hook_cam4);
    } else {
        return 0;
    }
    return 1;
}

static void write_code(uint32_t address, const uint8_t *bytes, uint32_t size)
{
    DWORD old;
    VirtualProtect((void *)address, size, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void *)address, bytes, size);
    VirtualProtect((void *)address, size, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)address, size);
}

/* ---- the safety net: skip any IN/OUT the patches missed (e.g. -TIMER's border
 * flashing), instead of the "privileged instruction" crash. IN from 0x3DA
 * alternates retrace bits so wait loops end. ---- */
static LONG CALLBACK io_handler(EXCEPTION_POINTERS *info)
{
    static uint32_t logged[32];
    static int logged_count, toggle;
    EXCEPTION_RECORD *rec = info->ExceptionRecord;
    CONTEXT *ctx = info->ContextRecord;
    if (rec->ExceptionCode != EXCEPTION_PRIV_INSTRUCTION)
        return EXCEPTION_CONTINUE_SEARCH;
    const uint8_t *ip = (const uint8_t *)ctx->Eip;
    int length = 0, prefix = ip[0] == 0x66;
    uint8_t op = ip[prefix];
    switch (op) {
    case 0xEC: case 0xED:  /* IN AL/AX/EAX,DX */
        length = 1;
        if (op == 0xEC)
            ctx->Eax = (ctx->Eax & ~0xFFu) | (((ctx->Edx & 0xFFFF) == 0x3DA) ? ((toggle ^= 1) ? 0x09 : 0x00) : 0xFF);
        else
            ctx->Eax = prefix ? (ctx->Eax & ~0xFFFFu) : 0;
        break;
    case 0xEE: case 0xEF: length = 1; break;  /* OUT DX,AL/AX/EAX */
    case 0xE4: case 0xE5: length = 2; ctx->Eax &= ~0xFFu; break;  /* IN AL,imm8 */
    case 0xE6: case 0xE7: length = 2; break;  /* OUT imm8,AL */
    default: return EXCEPTION_CONTINUE_SEARCH;
    }
    uint32_t at = (uint32_t)ctx->Eip;
    int seen = 0;
    for (int i = 0; i < logged_count; i++)
        seen |= logged[i] == at;
    if (!seen && logged_count < 32) {
        logged[logged_count++] = at;
        brr_log("safety net: skipped %s at 0x%06lX (port 0x%04lX)", (op == 0xEC || op == 0xED || op == 0xE4 || op == 0xE5) ? "IN" : "OUT",
                (unsigned long)at, (unsigned long)(ctx->Edx & 0xFFFF));
    }
    ctx->Eip += (DWORD)(length + prefix);
    return EXCEPTION_CONTINUE_EXECUTION;
}

void brr_patch_all(void)
{
    char hex[65];
    uint8_t bytes[64];
    int bad = 0;
    size_t count = sizeof SITES / sizeof SITES[0];
    if (!exe_matches()) {
        if (!cfg.debug_ignore_exe_checksum) {
            brr_log("result: no patches applied; only the game's timer (as its own TIMER.DLL)");
            return;
        }
        brr_log("debug: ignore_exe_checksum=1, so checking the patch sites anyway");
    }
    brr_reciprocals_fill();
    for (size_t i = 0; i < count; i++) {
        const struct site_info *s = &SITES[i];
        if (!readable(s->address, s->length)) {
            brr_log("check: site %s at 0x%06lX is not readable", s->name, (unsigned long)s->address);
            bad++;
            continue;
        }
        hash_hex((const void *)s->address, s->length, hex);
        if (strcmp(hex, s->sha256) != 0) {
            brr_log("check: site %s at 0x%06lX differs from v2.25", s->name, (unsigned long)s->address);
            bad++;
        }
        if (s->length > sizeof bytes || !replacement(s, bytes)) {
            brr_log("check: no replacement for site %s", s->name);
            bad++;
        }
    }
    bad += ranges_match("check");
    if (cfg.music)
        bad += brr_music_check();
    if (bad) {
        brr_log("result: %d check(s) failed; no patches applied; only the game's timer (as its own TIMER.DLL)", bad);
        return;
    }
    brr_renderer_active = brr_renderer_decide(brr_is_cnc_ddraw(brr_ddraw_module()));
    if (!brr_renderer_active) {
        /* The original renderer is the 1996 game's picture (the director's renderer split,
         * 2026-11-15): its window at the game's own size and shape, and its own draw distance */
        cfg.max_draw_distance = 0;
        cfg.window_scale = 1;
        cfg.window_aspect43 = 0;
        cfg.render_max = 0;
        brr_log("render: the original renderer draws as in 1996: a window at the game's own size, "
                "the game's draw distance sliders");
    }
    brr_modern_window = brr_renderer_active && cfg.display_mode == DISPLAY_WINDOWED;
    cfg.window_aspect43 = 0;  /* the 4:3 shape is cnc-ddraw's scaling preset's now (Modern), or square (Original) */
    brr_full_colour = brr_renderer_active && cfg.full_colour;
    brr_sky_active = brr_full_colour && cfg.smooth_sky;
    brr_shade_active = brr_full_colour && cfg.smooth_shading;
    brr_shade_setup();  /* may turn it off (memory) */
    brr_hud_setup();
    if (brr_renderer_active)
        brr_log("render: %s%s%s", brr_full_colour ? "full colour (32-bit pictures from the game's palette)"
                                                   : "256 colours, as the game ([display] full_colour=0)",
                brr_sky_active ? "; the sky as a smooth gradient" : "",
                brr_shade_active ? "; lit polygons shaded smoothly" : "");
    int applied = 0;
    for (size_t i = 0; i < count; i++) {
        const struct site_info *s = &SITES[i];
        if (!enabled(s->fix))
            continue;
        replacement(s, bytes);
        write_code(s->address, bytes, s->length);
        applied++;
        brr_log("patched %s at 0x%06lX (%s, %lu bytes)", s->name, (unsigned long)s->address,
                s->fix, (unsigned long)s->length);
    }
    if (cfg.fix_vga) {
        /* Windows XP onwards, so looked up at run time: on Windows 95/98/ME the game's
         * port reads work natively, and the safety net isn't needed */
        typedef PVOID (WINAPI *add_handler_fn)(ULONG, LONG (CALLBACK *)(EXCEPTION_POINTERS *));
        add_handler_fn add_handler = (add_handler_fn)GetProcAddress(GetModuleHandleA("kernel32.dll"),
                                                                    "AddVectoredExceptionHandler");
        if (add_handler)
            add_handler(1, io_handler);
        else
            brr_log("port safety net: not available on this Windows (not needed on Windows 95/98/ME)");
    }
    brr_patched = 1;
    brr_music_start();
    brr_controller_start();
    int changed = ranges_match("after patching");
    brr_log("result: %d of %u sites patched; %u protected game functions (vehicle_physics, "
            "bounce_pitch, bounce_roll and everything else the race tick calls) %s",
            applied, (unsigned)count, (unsigned)(sizeof PROTECTED / sizeof PROTECTED[0]),
            changed ? "CHANGED (this is a bug)" : "unchanged (SHA-256 identical before and after)");
}
