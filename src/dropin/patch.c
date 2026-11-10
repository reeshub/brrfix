/* Verified in-memory patching. Nothing is written unless WINRACE.EXE on disk is the
 * exact v2.25 file (SHA-256) and every patch site and protected range in memory has
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
uint32_t brr_orig_camera_update = 0x404908;
uint32_t brr_rotation_continue = 0x446675, brr_game_atan = 0x412A20;
uint32_t brr_orig_present_window = 0x42CD2C;
/* shown on the main menu instead of the build date: "V2.25WR FIX0.1.0" (it fits where
 * "V2.24WR (09-05-96)" was) */
static const char version_text[] = "FIX" BRRFIX_VERSION;
static const char version_number[] = "V2.25";  /* the installed patch; the game still said V2.24 */
static char language_code[4];       /* the game's start-up language, from [display] language */
static char language_scratch[256];  /* where the config files' lang lines go instead */

static void hash_hex(const void *data, size_t size, char out[65])
{
    uint8_t digest[32];
    brr_sha256(data, size, digest);
    brr_sha256_hex(digest, out);
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
    if (!strcmp(fix, "controller"))
        return cfg.controller || cfg.debug_pad_test;
    if (!strcmp(fix, "display"))
        return cfg.display_mode != DISPLAY_ASK;
    if (!strcmp(fix, "vsync"))
        return cfg.vsync;
    if (!strcmp(fix, "fine_angles"))
        return cfg.fine_angles;
    if (!strcmp(fix, "camera")) return cfg.fix_camera;
    if (!strcmp(fix, "mode_switch")) return cfg.fix_mode_switch;
    if (!strcmp(fix, "language")) return cfg.language >= 1 && cfg.language <= 7;
    if (!strcmp(fix, "resolution")) return cfg.resolution_high;
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
    } else if (!strcmp(n, "display_choice") && s->length == 5) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_display_choice);
    } else if (!strcmp(n, "present_window_call") && s->length == 5) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_present_window);
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
    uint8_t bytes[32];
    int bad = 0;
    size_t count = sizeof SITES / sizeof SITES[0];
    if (!exe_matches()) {
        if (!cfg.debug_ignore_exe_checksum) {
            brr_log("result: no patches applied; running as a plain TIMER.DLL");
            return;
        }
        brr_log("debug: ignore_exe_checksum=1, so checking the patch sites anyway");
    }
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
        brr_log("result: %d check(s) failed; no patches applied; running as a plain TIMER.DLL", bad);
        return;
    }
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
