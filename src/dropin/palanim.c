/* The levels' colour animation, restored (2026-11-15, the director's request; [display]
 * colour_animation, both renderers).
 *
 * CHOPPER3, CITY4, MOON2 and RIG4 have a "palanim RATE,FIRST,LAST" line, which level_load
 * reads into 0x44BAE0, 0x44BAE4 and 0x44BAE8; the race code turns the animation on (0x44BADC)
 * when the race starts or carries on after a pause, if the rate isn't 0, and off when it
 * pauses or ends (race_enter, race_tick, race_tick_paused, race_keys_tick, race_leave). In
 * DOS the 70 Hz timer interrupt (timer_handler 0x2C390) then counted ticks (0x44BAD8 here)
 * and, every RATE of them, rotated palette entries FIRST..LAST by one (LAST's colour to
 * FIRST) and sent that range to the VGA. The Windows build keeps that code (0x428AA0) but
 * nothing calls it: TIMER.DLL replaced the Miles timer. Here our clock runs it, one step per
 * game tick, on the game's own palette (palette_current 0x44A008, 6-bit) through palette_set
 * (0x42D1E8), so DirectDraw, the GDI window and the full-colour picture all follow. */
#include "brrfix.h"
#include <string.h>

#define ANIM_ON       (*(volatile int32_t *)0x44BADC)
#define ANIM_COUNTER  (*(volatile int32_t *)0x44BAD8)
#define ANIM_RATE     (*(volatile int32_t *)0x44BAE0)
#define ANIM_FIRST    (*(volatile int32_t *)0x44BAE4)
#define ANIM_LAST     (*(volatile int32_t *)0x44BAE8)
#define PALETTE       ((uint8_t *)0x44A008)  /* 256 x RGB, 6 bits each */

/* palette_set (0x42D1E8): EAX the entries, EDX the first, EBX the count (thunks.asm) */
void __cdecl brr_call_palette_set(const uint8_t *rgb, int first, int count);

/* Called by the clock once a frame with the game ticks that frame ran */
void brr_colour_anim(uint32_t ticks)
{
    static int32_t logged_first = -1, logged_last = -1, logged_rate = -1;
    int32_t first = ANIM_FIRST, last = ANIM_LAST, rate = ANIM_RATE;
    int steps = 0;
    uint8_t keep[3];
    if (!cfg.colour_animation || !ANIM_ON || rate <= 0 || first < 0 || last > 255 || first >= last)
        return;
    if (ticks > 35)
        ticks = 35;  /* after a long frame, as the cap does: the cycling doesn't race to catch up */
    while (ticks--) {
        if (++ANIM_COUNTER >= rate) {
            ANIM_COUNTER = 0;
            memcpy(keep, PALETTE + last * 3, 3);
            memmove(PALETTE + (first + 1) * 3, PALETTE + first * 3, (size_t)(last - first) * 3);
            memcpy(PALETTE + first * 3, keep, 3);
            steps++;
        }
    }
    if (!steps)
        return;
    brr_call_palette_set(PALETTE + first * 3, first, last - first + 1);
    if (first != logged_first || last != logged_last || rate != logged_rate) {
        logged_first = first;
        logged_last = last;
        logged_rate = rate;
        brr_log("colour animation: palette entries %ld-%ld turn one step every %ld ticks (the level's palanim)",
                (long)first, (long)last, (long)rate);
    }
}
