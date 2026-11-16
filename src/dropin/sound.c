/* Distant vehicles quieter ([sound] distance_volume; the director, 2026-11-16).
 *
 * The game already fades a vehicle's sounds with its distance from the camera: vehicle_sounds
 * (0x415EE0) gives them 256 x (8,192 - distance) / 7,392 of their volume (full within about
 * 1.6 squares, nothing beyond 16), and every sound is scaled by the effects volume (0x44B740)
 * to 0-127. But on Windows ds_set_volume (0x42EBE4) hands DirectSound the byte table at
 * 0x44C058, -110 at 0 up to 0 at 127: a proper loudness curve (half the volume -15, a quarter
 * -32, about 2.5 to a decibel), whereas IDirectSoundBuffer::SetVolume counts hundredths of a
 * decibel. So every sound plays within 1.1 dB of full: the fade, the effects volume option and
 * the quieter knocks can't be heard. Site ds_volume_curve reads brr_ds_volumes instead: the
 * same curve in hundredths of a decibel (20 log10 of volume / 127; 0 silent), as the DOS
 * version's Miles sound library gets a 0-127 volume.
 *
 * Only 3 vehicles' engines play at once, and sound_update_vehicles (0x416520) gathered the
 * other vehicles' sounds lowest vehicle number first, so a far truck could hold an engine's
 * place while a near one was silent. Site sound_nearest_first makes it nearest first
 * (thunks.asm, brr_hook_sound_nearest). Every vehicle is still gathered once, so the game's
 * random numbers (vehicle_sounds calls rand, which the physics shares) are drawn the same
 * number of times: the physics is unchanged.
 *
 * Stereo ([sound] stereo_pan): each vehicle's sounds are panned by the sine of its bearing from
 * the camera (sound_update_vehicles: -128 to 127, right positive), and ds_set_pan (0x42EC8C)
 * scaled that straight to DirectSound's -10,000 to 10,000, which is the far speaker's
 * attenuation in hundredths of a decibel: a truck 30 degrees to one side was 50 dB down on the
 * other speaker, all but hard-panned. Site ds_pan_curve gives DirectSound a balance instead:
 * the far speaker's level falls in proportion (pan 64, half: -6 dB; 127: silent), as the DOS
 * version's Miles pans.
 *
 * 16 channels ([sound] channels=16): the game mixes its effects on 8 DirectSound buffers and
 * gathers at most 8 requests an update; one busy truck (four loops and four events) could fill
 * them all. Its channel tables move to the arrays below, with the same layout for 16 channels
 * and 16 requests, and every loop, bound and limit follows (116 sites "snd@ADDRESS:RULE", from
 * a census checked against the executable's own relocation table: work/logs/sound-channels/,
 * KNOWLEDGE "Sound"); every vehicle's engine may play (the game allowed 3). They're patched as
 * the DLL loads, before the game starts DirectSound. */
#include "brrfix.h"
#include <math.h>

int32_t brr_ds_volumes[128];
/* 16 channels: the game's tables (0x452960 keep, 0x452980 data, 0x4529A0 busy, 0x4543F0
 * requests, 0x4544C8 channels, 0x52F240 DirectSound buffers), twice as long */
int32_t brr_snd_keep[16], brr_snd_data[16], brr_snd_busy[16];
uint8_t brr_snd_requests[16 * 0x1B], brr_snd_channels[16 * 0x1B];
void *brr_ds_buffers[16];
int32_t brr_ds_pans[255];          /* pan -127..127 at [pan + 127]: DirectSound's pan */
int brr_sound_debug_on;            /* [debug] sound_log: thunks.asm calls brr_sound_debug */

void brr_sound_setup(void)
{
    int v;
    brr_ds_volumes[0] = -10000;  /* DSBVOLUME_MIN: silent */
    for (v = 1; v < 128; v++)
        brr_ds_volumes[v] = (int32_t)floor(2000.0 * log10(v / 127.0) + 0.5);
    for (v = 0; v < 255; v++) {
        int pan = v - 127, a = 127 - (pan < 0 ? -pan : pan);
        int32_t hundredths = a <= 0 ? 10000 : (int32_t)floor(-2000.0 * log10(a / 127.0) + 0.5);
        if (hundredths > 10000)
            hundredths = 10000;
        brr_ds_pans[v] = pan < 0 ? -hundredths : hundredths;
    }
    brr_sound_debug_on = cfg.debug_sound_log;
    if (cfg.stereo_pan)
        brr_log("sound: stereo as a balance (pan 32 = %ld, 64 = %ld, 96 = %ld hundredths of a dB on the far speaker; "
                "the game's own: 2520, 5039, 7559)", (long)brr_ds_pans[127 + 32], (long)brr_ds_pans[127 + 64],
                (long)brr_ds_pans[127 + 96]);
    if (cfg.distance_volume)
        brr_log("sound: effects at their full volume range (1 = %ld, 32 = %ld, 64 = %ld hundredths of a dB; "
                "the game's table only reached -110), vehicles gathered nearest first",
                (long)brr_ds_volumes[1], (long)brr_ds_volumes[32], (long)brr_ds_volumes[64]);
}

/* [debug] sound_log: from sound_update_vehicles (site sound_nearest_first), its frame: the
 * vehicles' pans at +0 and distances at +0x1C (7 each). Every 2 seconds, each active vehicle's
 * pan and distance beside where it is from the camera (the engine's last view: right, forward),
 * to check that a vehicle on the right has a positive pan. */
void __cdecl brr_sound_debug(const int32_t *frame)
{
    static int64_t next;
    const volatile int32_t *m = (const volatile int32_t *)0x54A2B0, *c = (const volatile int32_t *)0x54A2D4;
    int64_t now = brr_qpc();
    int i;
    if (now < next)
        return;
    next = now + 2 * brr_qpc_freq();
    {
        const uint8_t *channels = cfg.sound_channels == 16 ? (const uint8_t *)brr_snd_channels : (const uint8_t *)0x4544C8;
        void *const *buffers = cfg.sound_channels == 16 ? (void *const *)brr_ds_buffers : (void *const *)0x52F240;
        int busy = 0, made = 0, n = cfg.sound_channels;
        char kinds[40];
        for (i = 0; i < n; i++) {
            busy += channels[i * 0x1B] != 0xFF;
            made += buffers[i] != NULL;
            kinds[i * 2] = channels[i * 0x1B] == 0xFF ? '-' : (char)('0' + channels[i * 0x1B] % 10);
            kinds[i * 2 + 1] = channels[i * 0x1B] == 0xFF ? '.' : (char)('0' + channels[i * 0x1B + 1] % 10);
        }
        kinds[n * 2] = 0;
        brr_log("sound: %d of %d channels playing, %d DirectSound buffers made; owner and kind per channel: %s", busy,
                n, made, kinds);
    }
    for (i = 0; i < 7; i++) {
        const int32_t *p = (const int32_t *)(0x452A90 + i * 0x395);
        double dx = p[0] - c[0], dy = p[1] - c[1], dz = p[2] - c[2];
        double right = (m[0] * dx + m[1] * dy + m[2] * dz) / 32768.0;
        double ahead = (m[6] * dx + m[7] * dy + m[8] * dz) / 32768.0;
        if (*(const int16_t *)((const uint8_t *)p + 0x26) < 0)
            continue;
        brr_log("sound: vehicle %d pan %ld distance %ld; from the camera %ld right, %ld ahead", i, (long)frame[i],
                (long)frame[7 + i], (long)right, (long)ahead);
    }
}
