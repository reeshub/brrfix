/* CD music from the player's own ripped tracks (round 2, item 6).
 *
 * The game plays music with the MCI "cdaudio" device only (cd_audio_1/2/3, 0x404F50,
 * 0x405044, 0x4050B4): open, time format TMSF, play from track N to N+1 with a
 * notification, stop, close; MM_MCINOTIFY (MCI_NOTIFY_SUCCESSFUL) tells it a track has
 * finished, and cd_audio_poll then starts it again. At start-up it tries to play track 2:
 * only if that works does it turn music on and offer the CD volume option.
 *
 * Its mciSendCommandA import (slot 0x56C160, found and checked in the import table in
 * memory) goes to brr_mci. A cdaudio device is emulated when the music folder ([music]
 * folder, MUSIC next to the game by default) has tracks: FLAC, Ogg Vorbis or WAV, named
 * by track number. Everything else, or with no tracks, goes to the real mciSendCommandA,
 * so a real CD still works (on Windows 95, say).
 *
 * The game's own volume never reached Windows: cd_audio_fade_tick lowers a level at
 * 0x44AE84 (from 0x44AE88 = CD volume 0-7 x 255 / 7) for fades. Here it's the gain, times
 * [music] volume. Played through waveOut on a thread of our own (started with the
 * runtime's _beginthreadex, as the decoders use malloc). */
#include "brrfix.h"
#include <mmsystem.h>
#include <process.h>
#include <string.h>
#include <stdlib.h>
#include "dr_flac.h"

/* stb_vorbis's interface, as compiled in decoders.c */
typedef struct stb_vorbis stb_vorbis;
typedef struct { unsigned int sample_rate; int channels; unsigned int setup_memory_required,
                 setup_temp_memory_required, temp_memory_required; int max_frame_size; } stb_vorbis_info;
stb_vorbis *stb_vorbis_open_memory(const unsigned char *data, int len, int *error, void *alloc);
stb_vorbis_info stb_vorbis_get_info(stb_vorbis *f);
int stb_vorbis_get_samples_short_interleaved(stb_vorbis *f, int channels, short *buffer, int num_shorts);
unsigned int stb_vorbis_stream_length_in_samples(stb_vorbis *f);
void stb_vorbis_close(stb_vorbis *f);

#define MCI_SLOT      ((uint32_t *)0x56C160)   /* WINRACE.EXE's import of mciSendCommandA */
#define GAME_LEVEL    ((volatile int32_t *)0x44AE84)
#define DEVICE_ID     0x4252u                  /* "BR": our emulated cdaudio */
#define BUFFERS       4
#define BUFFER_FRAMES 4096

typedef MCIERROR (WINAPI *mci_fn)(MCIDEVICEID, UINT, DWORD_PTR, DWORD_PTR);
static mci_fn real_mci;
static char folder[MAX_PATH];
static int tracks_found, device_open;

/* ---- finding tracks ---- */
static const char *const extensions[] = {"flac", "ogg", "wav"};

static int find_track(int track, char *path)
{
    static const char *const names[] = {"track%02d", "Track%02d", "Track %02d", "track %02d", "%02d"};
    size_t e, n;
    for (e = 0; e < sizeof extensions / sizeof extensions[0]; e++) {
        for (n = 0; n < sizeof names / sizeof names[0]; n++) {
            char name[64];
            wsprintfA(name, names[n], track);
            wsprintfA(path, "%s\\%s.%s", folder, name, extensions[e]);
            if (GetFileAttributesA(path) != 0xFFFFFFFFu)
                return 1;
        }
        {   /* "02 - Title.flac" and the like */
            WIN32_FIND_DATAA found;
            HANDLE search;
            wsprintfA(path, "%s\\%02d *.%s", folder, track, extensions[e]);
            search = FindFirstFileA(path, &found);
            if (search != INVALID_HANDLE_VALUE) {
                FindClose(search);
                wsprintfA(path, "%s\\%s", folder, found.cFileName);
                return 1;
            }
        }
    }
    return 0;
}

/* ---- decoding ---- */
struct decoder {
    int kind;  /* 0 none, 1 FLAC, 2 Ogg Vorbis, 3 WAV */
    uint8_t *file;
    DWORD size;
    drflac *flac;
    stb_vorbis *vorbis;
    const uint8_t *wav;
    DWORD wav_left;
    int channels, rate;
};

static void decoder_close(struct decoder *d)
{
    if (d->flac) drflac_close(d->flac);
    if (d->vorbis) stb_vorbis_close(d->vorbis);
    if (d->file) HeapFree(GetProcessHeap(), 0, d->file);
    memset(d, 0, sizeof *d);
}

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static int decoder_open(struct decoder *d, const char *path)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD got = 0;
    memset(d, 0, sizeof *d);
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    d->size = GetFileSize(f, NULL);
    d->file = d->size != 0xFFFFFFFFu ? (uint8_t *)HeapAlloc(GetProcessHeap(), 0, d->size) : NULL;
    if (d->file && (!ReadFile(f, d->file, d->size, &got, NULL) || got != d->size)) {
        HeapFree(GetProcessHeap(), 0, d->file);
        d->file = NULL;
    }
    CloseHandle(f);
    if (!d->file)
        return 0;
    if (d->size > 4 && !memcmp(d->file, "fLaC", 4)) {
        d->flac = drflac_open_memory(d->file, d->size, NULL);
        if (d->flac) {
            d->kind = 1;
            d->channels = d->flac->channels;
            d->rate = (int)d->flac->sampleRate;
        }
    } else if (d->size > 4 && !memcmp(d->file, "OggS", 4)) {
        int error = 0;
        d->vorbis = stb_vorbis_open_memory(d->file, (int)d->size, &error, NULL);
        if (d->vorbis) {
            stb_vorbis_info info = stb_vorbis_get_info(d->vorbis);
            d->kind = 2;
            d->channels = info.channels;
            d->rate = (int)info.sample_rate;
        }
    } else if (d->size > 44 && !memcmp(d->file, "RIFF", 4) && !memcmp(d->file + 8, "WAVE", 4)) {
        DWORD at = 12;
        while (at + 8 <= d->size) {  /* walk the chunks: fmt (16-bit PCM only), then data */
            DWORD length = le32(d->file + at + 4);
            if (!memcmp(d->file + at, "fmt ", 4) && length >= 16) {
                const uint8_t *fmt = d->file + at + 8;
                if ((fmt[0] | fmt[1] << 8) != 1 || (fmt[14] | fmt[15] << 8) != 16)
                    break;
                d->channels = fmt[2] | fmt[3] << 8;
                d->rate = (int)le32(fmt + 4);
            } else if (!memcmp(d->file + at, "data", 4) && d->channels) {
                d->wav = d->file + at + 8;
                d->wav_left = length < d->size - at - 8 ? length : d->size - at - 8;
                d->kind = 3;
                break;
            }
            at += 8 + length + (length & 1);
        }
    }
    if (!d->kind || d->channels < 1 || d->channels > 2 || d->rate < 8000) {
        decoder_close(d);
        return 0;
    }
    return 1;
}

/* up to `frames` frames of 16-bit samples; returns frames decoded (0 = the end) */
static int decoder_read(struct decoder *d, short *out, int frames)
{
    if (d->kind == 1)
        return (int)drflac_read_pcm_frames_s16(d->flac, (drflac_uint64)frames, out);
    if (d->kind == 2)
        return stb_vorbis_get_samples_short_interleaved(d->vorbis, d->channels, out, frames * d->channels);
    if (d->kind == 3) {
        DWORD bytes = (DWORD)frames * (DWORD)d->channels * 2;
        if (bytes > d->wav_left)
            bytes = d->wav_left - d->wav_left % ((DWORD)d->channels * 2);
        memcpy(out, d->wav, bytes);
        d->wav += bytes;
        d->wav_left -= bytes;
        return (int)(bytes / ((DWORD)d->channels * 2));
    }
    return 0;
}

/* ---- the player thread ---- */
static CRITICAL_SECTION lock;
static HANDLE wake;
static int command_play, command_stop, play_track;
static HWND notify_window;
static volatile int playing;

static void notify(HWND window, WPARAM how)
{
    if (window)
        PostMessageA(window, MM_MCINOTIFY, how, DEVICE_ID);
}

static void apply_gain(short *samples, int count)
{
    int32_t level = cfg.music_follow_game ? *GAME_LEVEL : 255, gain, i;
    if (level < 0) level = 0;
    if (level > 255) level = 255;
    gain = level * cfg.music_volume;  /* 0..25500 */
    if (gain >= 25500)
        return;
    for (i = 0; i < count; i++)
        samples[i] = (short)((int32_t)samples[i] * gain / 25500);
}

static unsigned __stdcall player(void *unused)
{
    static WAVEHDR headers[BUFFERS];
    static short buffers[BUFFERS][BUFFER_FRAMES * 2];
    struct decoder d;
    HWAVEOUT out = NULL;
    HWND window = NULL;
    int out_rate = 0, out_channels = 0, ending = 0, i;
    HANDLE done = CreateEventA(NULL, FALSE, FALSE, NULL);
    (void)unused;
    memset(&d, 0, sizeof d);
    for (;;) {
        HANDLE events[2];
        int play, stop, track;
        HWND requester;
        events[0] = wake;
        events[1] = done;
        WaitForMultipleObjects(2, events, FALSE, 50);
        EnterCriticalSection(&lock);
        play = command_play; stop = command_stop; track = play_track; requester = notify_window;
        command_play = command_stop = 0;
        LeaveCriticalSection(&lock);
        if (stop || play) {
            if (out) {
                waveOutReset(out);
                for (i = 0; i < BUFFERS; i++)
                    if (headers[i].dwFlags & WHDR_PREPARED)
                        waveOutUnprepareHeader(out, &headers[i], sizeof headers[i]);
            }
            if (d.kind && window)
                notify(window, MCI_NOTIFY_ABORTED);
            decoder_close(&d);
            window = NULL;
            ending = 0;
            playing = 0;
        }
        if (play) {
            char path[MAX_PATH];
            if (find_track(track, path) && decoder_open(&d, path)) {
                if (!out || out_rate != d.rate || out_channels != d.channels) {
                    WAVEFORMATEX format;
                    if (out)
                        waveOutClose(out);
                    out = NULL;
                    memset(&format, 0, sizeof format);
                    format.wFormatTag = WAVE_FORMAT_PCM;
                    format.nChannels = (WORD)d.channels;
                    format.nSamplesPerSec = (DWORD)d.rate;
                    format.wBitsPerSample = 16;
                    format.nBlockAlign = (WORD)(d.channels * 2);
                    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
                    if (waveOutOpen(&out, WAVE_MAPPER, &format, (DWORD_PTR)done, 0, CALLBACK_EVENT) != 0) {
                        brr_log("music: couldn't open the sound output for track %d", track);
                        out = NULL;
                        decoder_close(&d);
                    }
                    out_rate = d.rate;
                    out_channels = d.channels;
                }
                if (d.kind) {
                    static int logged;
                    if (logged++ < 30)
                        brr_log("music: playing %s (%d Hz, %d channels; the game's CD volume level %ld of 255)",
                                path, d.rate, d.channels, (long)*GAME_LEVEL);
                    window = requester;
                    playing = 1;
                    for (i = 0; i < BUFFERS; i++)
                        headers[i].dwFlags = WHDR_DONE;  /* all free */
                }
            } else
                brr_log("music: track %d: no playable file in %s", track, folder);
        }
        if (!d.kind || !out)
            continue;
        /* refill every buffer the sound card has finished with */
        for (i = 0; i < BUFFERS; i++) {
            WAVEHDR *h = &headers[i];
            int frames;
            if (!(h->dwFlags & WHDR_DONE))
                continue;
            if (h->dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(out, h, sizeof *h);
            if (ending)
                continue;
            frames = decoder_read(&d, buffers[i], BUFFER_FRAMES);
            if (frames <= 0) {
                ending = 1;
                continue;
            }
            apply_gain(buffers[i], frames * d.channels);
            memset(h, 0, sizeof *h);
            h->lpData = (LPSTR)buffers[i];
            h->dwBufferLength = (DWORD)(frames * d.channels * 2);
            waveOutPrepareHeader(out, h, sizeof *h);
            waveOutWrite(out, h, sizeof *h);
        }
        if (ending) {
            int busy = 0;
            for (i = 0; i < BUFFERS; i++)
                busy |= !(headers[i].dwFlags & WHDR_DONE);
            if (!busy) {  /* the track has finished, as a CD track does */
                decoder_close(&d);
                playing = 0;
                if (cfg.debug_log_mouse || cfg.log_clock)
                    brr_log("music: track finished; telling the game");
                notify(window, MCI_NOTIFY_SUCCESSFUL);
                window = NULL;
                ending = 0;
            }
        }
    }
}

/* ---- the MCI front end ---- */
static MCIERROR WINAPI brr_mci(MCIDEVICEID device, UINT message, DWORD_PTR flags, DWORD_PTR param)
{
    if (message == MCI_OPEN && param && (flags & MCI_OPEN_TYPE) && !(flags & MCI_OPEN_TYPE_ID)) {
        MCI_OPEN_PARMSA *open = (MCI_OPEN_PARMSA *)param;
        if (tracks_found && open->lpstrDeviceType && !lstrcmpiA(open->lpstrDeviceType, "cdaudio")) {
            open->wDeviceID = DEVICE_ID;
            device_open = 1;
            return 0;
        }
    }
    if (device != DEVICE_ID || !tracks_found)
        return real_mci(device, message, flags, param);
    switch (message) {
    case MCI_SET:
        return 0;
    case MCI_PLAY: {
        MCI_PLAY_PARMS *play = (MCI_PLAY_PARMS *)param;
        int track = play && (flags & MCI_FROM) ? (int)(play->dwFrom & 0xFF) : 1;
        char path[MAX_PATH];
        static int logged;
        if (!find_track(track, path))
            return MCIERR_OUTOFRANGE;
        if (logged++ < 30)
            brr_log("music: the game plays track %d%s", track, flags & MCI_NOTIFY ? " (and asks to be told when it ends)" : "");
        EnterCriticalSection(&lock);
        command_play = 1;
        play_track = track;
        notify_window = play && (flags & MCI_NOTIFY) ? (HWND)play->dwCallback : NULL;
        LeaveCriticalSection(&lock);
        SetEvent(wake);
        return 0;
    }
    case MCI_STOP:
    case MCI_PAUSE:
    case MCI_CLOSE:
        EnterCriticalSection(&lock);
        command_stop = 1;
        LeaveCriticalSection(&lock);
        SetEvent(wake);
        if (message == MCI_CLOSE)
            device_open = 0;
        return 0;
    case MCI_STATUS: {
        MCI_STATUS_PARMS *status = (MCI_STATUS_PARMS *)param;
        if (status && (flags & MCI_STATUS_ITEM)) {
            if (status->dwItem == MCI_STATUS_NUMBER_OF_TRACKS)
                status->dwReturn = (DWORD)tracks_found + 1;
            else if (status->dwItem == MCI_STATUS_MODE)
                status->dwReturn = playing ? MCI_MODE_PLAY : MCI_MODE_STOP;
            else if (status->dwItem == MCI_STATUS_MEDIA_PRESENT)
                status->dwReturn = TRUE;
            else
                status->dwReturn = 0;
        }
        return 0;
    }
    default:
        return 0;
    }
}

/* At patch time: is the import where we expect it, and does it name mciSendCommandA? */
int brr_music_check(void)
{
    const uint8_t *base = (const uint8_t *)0x400000;
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(base + ((const IMAGE_DOS_HEADER *)base)->e_lfanew);
    const IMAGE_IMPORT_DESCRIPTOR *d = (const IMAGE_IMPORT_DESCRIPTOR *)
        (base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; d->Name; d++) {
        const uint32_t *names = (const uint32_t *)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        uint32_t *slots = (uint32_t *)(base + d->FirstThunk);
        int i;
        if (lstrcmpiA((const char *)base + d->Name, "WINMM.dll"))
            continue;
        for (i = 0; names[i]; i++)
            if (!(names[i] & 0x80000000u) && !strcmp((const char *)base + names[i] + 2, "mciSendCommandA")) {
                if (&slots[i] == MCI_SLOT)
                    return 0;
                brr_log("check: mciSendCommandA's import is at 0x%06lX, not 0x56C160", (unsigned long)&slots[i]);
                return 1;
            }
    }
    brr_log("check: no mciSendCommandA import from WINMM.dll");
    return 1;
}

void brr_music_start(void)
{
    char base[MAX_PATH], *slash;
    DWORD old;
    int track;
    if (!cfg.music)
        return;
    if (cfg.music_folder[0] && (cfg.music_folder[1] == ':' || cfg.music_folder[0] == '\\'))
        lstrcpynA(folder, cfg.music_folder, sizeof folder);
    else {
        GetModuleFileNameA(NULL, base, sizeof base);  /* WINRACE.EXE's folder */
        slash = strrchr(base, '\\');
        if (slash)
            *slash = 0;
        wsprintfA(folder, "%s\\%s", base, cfg.music_folder[0] ? cfg.music_folder : "MUSIC");
    }
    for (track = 2; track < 100; track++) {
        char path[MAX_PATH];
        if (find_track(track, path))
            tracks_found++;
    }
    real_mci = (mci_fn)*MCI_SLOT;
    InitializeCriticalSection(&lock);
    wake = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (tracks_found && _beginthreadex(NULL, 65536, player, NULL, 0, NULL) == 0) {
        brr_log("music: couldn't start the player thread; CD audio goes to Windows");
        tracks_found = 0;
    }
    VirtualProtect(MCI_SLOT, 4, PAGE_READWRITE, &old);
    *MCI_SLOT = (uint32_t)brr_mci;
    VirtualProtect(MCI_SLOT, 4, old, &old);
    if (tracks_found)
        brr_log("music: %d tracks in %s; the game's CD audio plays them (volume %d%%%s)", tracks_found, folder,
                cfg.music_volume, cfg.music_follow_game ? " x the game's CD volume" : "");
    else
        brr_log("music: no tracks in %s (trackNN.flac, .ogg or .wav); CD audio goes to Windows (a real CD)", folder);
}
