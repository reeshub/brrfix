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
 * number of times: the physics is unchanged. */
#include "brrfix.h"
#include <math.h>

int32_t brr_ds_volumes[128];

void brr_sound_setup(void)
{
    int v;
    brr_ds_volumes[0] = -10000;  /* DSBVOLUME_MIN: silent */
    for (v = 1; v < 128; v++)
        brr_ds_volumes[v] = (int32_t)floor(2000.0 * log10(v / 127.0) + 0.5);
    if (cfg.distance_volume)
        brr_log("sound: effects at their full volume range (1 = %ld, 32 = %ld, 64 = %ld hundredths of a dB; "
                "the game's table only reached -110), vehicles gathered nearest first",
                (long)brr_ds_volumes[1], (long)brr_ds_volumes[32], (long)brr_ds_volumes[64]);
}
