/* WINRACE.EXE's section headers (see winfix.h). Standard C file functions only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "winfix.h"
#include "sha256.h"
#include "game.h"

#if defined(__NT__) || defined(_WIN32)
#define SEPARATOR "\\"
#else
#define SEPARATOR "/"
#endif
#define PATH_SIZE 300

static void join(char *out, const char *folder, const char *name)
{
    size_t n = strlen(folder);
    out[0] = 0;
    if (n + strlen(name) + 2 > PATH_SIZE)
        return;
    strcpy(out, folder);
    if (n && folder[n - 1] != '\\' && folder[n - 1] != '/')
        strcat(out, SEPARATOR);
    strcat(out, name);
}

static void say(char *message, unsigned size, const char *text)
{
    strncpy(message, text, size - 1);
    message[size - 1] = 0;
}

static int hash_is(const uint8_t *data, size_t size, const char *expected)
{
    uint8_t digest[32];
    char hex[65];
    brr_sha256(data, size, digest);
    brr_sha256_hex(digest, hex);
    return !strcmp(hex, expected);
}

/* The whole file, or NULL; *size set */
static uint8_t *load(const char *path, size_t *size)
{
    uint8_t *data;
    long length;
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (length = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET) != 0
        || !(data = malloc((size_t)length))) {
        fclose(f);
        return NULL;
    }
    if (fread(data, 1, (size_t)length, f) != (size_t)length) {
        free(data);
        data = NULL;
    }
    fclose(f);
    *size = (size_t)length;
    return data;
}

static int state_of(const char *path)
{
    size_t size = 0;
    uint8_t *data = load(path, &size);
    int state = BRR_WIN_OTHER;
    if (!data)
        return BRR_WIN_MISSING;
    if (size == GAME_EXE_SIZE)
        state = hash_is(data, size, GAME_EXE_SHA256) ? BRR_WIN_ORIGINAL
              : hash_is(data, size, GAME_EXE_CORRECTED_SHA256) ? BRR_WIN_CORRECTED : BRR_WIN_OTHER;
    free(data);
    return state;
}

int brr_win_state(const char *folder)
{
    char path[PATH_SIZE];
    join(path, folder, "WINRACE.EXE");
    return state_of(path);
}

static uint32_t get32(const uint8_t *p) { return p[0] | p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static void put32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8); p[2] = (uint8_t)(value >> 16); p[3] = (uint8_t)(value >> 24);
}

/* Each section's VirtualSize reaches the next section (Windows NT refuses gaps between
 * sections, and VirtualSize 0); uninitialised sections get no file data */
static void correct(uint8_t *data)
{
    uint32_t pe = get32(data + 0x3C), image_size = get32(data + pe + 24 + 56);
    unsigned count = data[pe + 6] | data[pe + 7] << 8, optional = data[pe + 20] | data[pe + 21] << 8, i;
    uint8_t *table = data + pe + 24 + optional;
    for (i = 0; i < count; i++) {
        uint8_t *s = table + 40 * i;
        uint32_t end = i + 1 < count ? get32(s + 40 + 12) : image_size;
        put32(s + 8, end - get32(s + 12));
        if ((get32(s + 36) & 0x80) && get32(s + 16)) {
            put32(s + 16, 0);
            put32(s + 20, 0);
        }
    }
}

int brr_win_backup_is_original(const char *folder)
{
    char path[PATH_SIZE];
    join(path, folder, "WINRACE.ORI");
    return state_of(path) == BRR_WIN_ORIGINAL;
}

int brr_win_correct(const char *folder, char *message, unsigned size)
{
    char exe[PATH_SIZE], backup[PATH_SIZE], fresh[PATH_SIZE];
    size_t length = 0;
    uint8_t *data;
    int kept, ok, redo = 0;
    FILE *f;
    join(exe, folder, "WINRACE.EXE");
    join(backup, folder, "WINRACE.ORI");
    join(fresh, folder, "WINRACE.NEW");
    kept = state_of(backup);
    switch (state_of(exe)) {
    case BRR_WIN_CORRECTED:
        say(message, size, "WINRACE.EXE is already corrected. Nothing was changed.");
        return 1;
    case BRR_WIN_ORIGINAL:
        break;
    default:
        /* for example corrected by an earlier version of the launcher: start again from the original */
        if (kept != BRR_WIN_ORIGINAL) {
            say(message, size, "WINRACE.EXE isn't the v2.25 file this correction is for. Nothing was changed.");
            return -1;
        }
        redo = 1;
    }
    if (kept != BRR_WIN_MISSING && kept != BRR_WIN_ORIGINAL) {
        say(message, size, "WINRACE.ORI is already there, and it isn't the original WINRACE.EXE. Move or "
            "rename it, then try again. Nothing was changed.");
        return -1;
    }
    data = load(redo ? backup : exe, &length);
    if (!data) {
        say(message, size, "Couldn't read WINRACE.EXE. Nothing was changed.");
        return -1;
    }
    correct(data);
    ok = hash_is(data, length, GAME_EXE_CORRECTED_SHA256);
    if (ok) {
        f = fopen(fresh, "wb");
        ok = f && fwrite(data, 1, length, f) == length;
        if (f && fclose(f) != 0)
            ok = 0;
    }
    free(data);
    if (!ok || state_of(fresh) != BRR_WIN_CORRECTED) {
        remove(fresh);
        say(message, size, "Couldn't write the corrected file (WINRACE.NEW). Is the disk full or the "
            "folder write-protected? Nothing was changed.");
        return -1;
    }
    if (kept == BRR_WIN_ORIGINAL ? remove(exe) != 0 : rename(exe, backup) != 0) {
        remove(fresh);
        say(message, size, "Couldn't replace WINRACE.EXE (is it read-only?). Nothing was changed.");
        return -1;
    }
    if (rename(fresh, exe) != 0) {
        if (!redo)
            rename(backup, exe);
        remove(fresh);
        say(message, size, redo ? "Couldn't rename WINRACE.NEW to WINRACE.EXE. The original is still "
            "WINRACE.ORI: rename it to WINRACE.EXE." : "Couldn't rename WINRACE.NEW to WINRACE.EXE. The "
            "original WINRACE.EXE is back in place.");
        return -1;
    }
    say(message, size, "WINRACE.EXE is corrected. The original is saved as WINRACE.ORI.");
    return 0;
}
