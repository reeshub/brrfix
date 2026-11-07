/* Verified in-memory patching. Nothing is written unless WINRACE.EXE on disk is the
 * exact v2.25 file (SHA-256) and every patch site and protected range in memory has
 * the expected SHA-256 (sites.h is generated from data/dropin-patches.json by
 * "python3 brr.py dropin-build"). Patches are all-or-nothing; the result is logged.
 * The game's code is identified by checksums only: no original bytes are stored. */
#include "brrfix.h"
#include <stdio.h>
#include <string.h>
#include "sites.h"

int brr_patched;
uint32_t brr_orig_race_tick = GAME_RACE_TICK, brr_orig_race_draw = GAME_RACE_DRAW;
uint32_t brr_orig_camera_update = 0x404908;

/* ---- SHA-256 (FIPS 180-4) ---- */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(uint32_t h[8], const uint8_t *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, k;
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; k = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = k + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        k = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += k;
}

void brr_sha256(const void *data, size_t size, uint8_t out[32])
{
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    const uint8_t *p = data;
    uint8_t tail[128] = {0};
    size_t full = size & ~(size_t)63, rest = size - full;
    for (size_t i = 0; i < full; i += 64)
        sha256_block(h, p + i);
    memcpy(tail, p + full, rest);
    tail[rest] = 0x80;
    size_t tail_size = rest < 56 ? 64 : 128;
    uint64_t bits = (uint64_t)size * 8;
    for (int i = 0; i < 8; i++)
        tail[tail_size - 1 - i] = (uint8_t)(bits >> (8 * i));
    sha256_block(h, tail);
    if (tail_size == 128)
        sha256_block(h, tail + 64);
    for (int i = 0; i < 8; i++) {
        out[4 * i] = (uint8_t)(h[i] >> 24); out[4 * i + 1] = (uint8_t)(h[i] >> 16);
        out[4 * i + 2] = (uint8_t)(h[i] >> 8); out[4 * i + 3] = (uint8_t)h[i];
    }
}

static void to_hex(const uint8_t digest[32], char out[65])
{
    static const char digits[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out[2 * i] = digits[digest[i] >> 4];
        out[2 * i + 1] = digits[digest[i] & 15];
    }
    out[64] = 0;
}

static void hash_hex(const void *data, size_t size, char out[65])
{
    uint8_t digest[32];
    brr_sha256(data, size, digest);
    to_hex(digest, out);
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
            || cfg.debug_capture_frames > 0 || cfg.debug_input[0];
    if (!strcmp(fix, "camera_view"))  /* experimental, off by default */
        return cfg.camera_roll_fix || cfg.camera_smoothing > 0;
    if (!strcmp(fix, "camera")) return cfg.fix_camera;
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
    } else if (!strcmp(n, "camera_update_call") && s->length == 5) {
        out[0] = 0xE8;
        put_rel32(out + 1, s->address + 5, (void *)brr_hook_camera_update);
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
    if (cfg.fix_vga)
        AddVectoredExceptionHandler(1, io_handler);
    brr_patched = 1;
    int changed = ranges_match("after patching");
    brr_log("result: %d of %u sites patched; %u protected game functions (vehicle_physics, "
            "bounce_pitch, bounce_roll and everything else the race tick calls) %s",
            applied, (unsigned)count, (unsigned)(sizeof PROTECTED / sizeof PROTECTED[0]),
            changed ? "CHANGED (this is a bug)" : "unchanged (SHA-256 identical before and after)");
}
