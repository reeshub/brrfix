/* WINRACEP.EXE, the launcher's copy of WINRACE.EXE (see winfix.h). Standard C file functions only. */
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

static int state_of_data(const uint8_t *data, size_t size)
{
    if (size != GAME_EXE_SIZE)
        return BRR_WIN_OTHER;
    return hash_is(data, size, GAME_EXE_SHA256) ? BRR_WIN_ORIGINAL
         : hash_is(data, size, GAME_EXE_CORRECTED_SHA256) ? BRR_WIN_CORRECTED : BRR_WIN_OTHER;
}

/* The game's WINRACE.EXE, or WINRACE.ORI if an earlier launcher (which corrected
 * WINRACE.EXE in place) kept the original there and WINRACE.EXE isn't v2.25; NULL if
 * there's no WINRACE.EXE. *state is what the returned file is. */
static uint8_t *load_game(const char *game_folder, size_t *size, int *state)
{
    char path[PATH_SIZE];
    uint8_t *data, *backup;
    size_t backup_size = 0;
    join(path, game_folder, "WINRACE.EXE");
    data = load(path, size);
    if (!data) {
        *state = BRR_WIN_MISSING;
        return NULL;
    }
    *state = state_of_data(data, *size);
    if (*state != BRR_WIN_OTHER)
        return data;
    join(path, game_folder, "WINRACE.ORI");
    backup = load(path, &backup_size);
    if (backup && state_of_data(backup, backup_size) == BRR_WIN_ORIGINAL) {
        free(data);
        *size = backup_size;
        *state = BRR_WIN_ORIGINAL;
        return backup;
    }
    free(backup);
    return data;
}

int brr_win_game_state(const char *game_folder)
{
    size_t size = 0;
    int state;
    free(load_game(game_folder, &size, &state));
    return state;
}

static uint32_t get32(const uint8_t *p) { return p[0] | p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static void put32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8); p[2] = (uint8_t)(value >> 16); p[3] = (uint8_t)(value >> 24);
}

/* Each section's VirtualSize reaches the next section (Windows NT refuses gaps between
 * sections, and VirtualSize 0); uninitialised sections get no file data. Correcting a
 * corrected file changes nothing. */
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

static int same_file(const char *path, const uint8_t *data, size_t size)
{
    size_t found_size = 0;
    uint8_t *found = load(path, &found_size);
    int same = found && found_size == size && !memcmp(found, data, size);
    free(found);
    return same;
}

int brr_win_make_copy(const char *game_folder, const char *out, int cnc_ddraw, char *message, unsigned size)
{
    char fresh[PATH_SIZE];
    size_t length = 0;
    uint8_t *data;
    int state, ok;
    FILE *f;
    data = load_game(game_folder, &length, &state);
    if (!data) {
        say(message, size, "Couldn't read WINRACE.EXE in the game's folder.");
        return -1;
    }
    if (state != BRR_WIN_OTHER) {
        correct(data);
        /* the game imports our PATCH.DLL instead of its own TIMER.DLL, and cnc-ddraw by a name
         * of its own, so no other DirectDraw can take its place: each the same length */
        if (!memcmp(data + WINRACEP_TIMER_OFFSET, "timer.dll", 10))
            memcpy(data + WINRACEP_TIMER_OFFSET, "PATCH.DLL", 9);
        if (cnc_ddraw && !memcmp(data + WINRACEP_DDRAW_OFFSET, "DDRAW.dll", 10))
            memcpy(data + WINRACEP_DDRAW_OFFSET, CNC_DDRAW_DLL, 9);
        if (!hash_is(data, length, cnc_ddraw ? WINRACEP_SHA256 : WINRACEP_PLAIN_SHA256)) {  /* can't happen */
            free(data);
            say(message, size, "Couldn't make WINRACEP.EXE from WINRACE.EXE. Nothing was changed.");
            return -1;
        }
        state = BRR_WIN_CORRECTED;
    }
    if (same_file(out, data, length)) {
        free(data);
        say(message, size, "WINRACEP.EXE is up to date. Nothing was changed.");
        return state;
    }
    if (strlen(out) + 5 > PATH_SIZE) {
        free(data);
        say(message, size, "The game's path is too long. Nothing was changed.");
        return -1;
    }
    strcpy(fresh, out);
    strcat(fresh, ".new");
    f = fopen(fresh, "wb");
    ok = f && fwrite(data, 1, length, f) == length;
    if (f && fclose(f) != 0)
        ok = 0;
    ok = ok && same_file(fresh, data, length);
    free(data);
    if (!ok) {
        remove(fresh);
        say(message, size, "Couldn't write WINRACEP.EXE next to Big Red Racing.exe. Is the disk full or the "
            "folder write-protected? Nothing was changed.");
        return -1;
    }
    /* rename doesn't replace a file on Windows */
    remove(out);
    if (rename(fresh, out) != 0) {
        remove(fresh);
        say(message, size, "Couldn't replace WINRACEP.EXE next to Big Red Racing.exe (is the game still "
            "running, or the file read-only?).");
        return -1;
    }
    say(message, size, state != BRR_WIN_CORRECTED
        ? "WINRACEP.EXE is made: a copy of WINRACE.EXE as it is (it isn't v2.25)."
        : cnc_ddraw ? "WINRACEP.EXE is made: WINRACE.EXE with its headers corrected, using PATCH.DLL and cnc-ddraw."
        : "WINRACEP.EXE is made: WINRACE.EXE with its headers corrected, using PATCH.DLL.");
    return state;
}
