/* Joystick menu navigation: WINRACE.EXE v2.25's addresses for the shared core (menunav.c).
 * Each has a DOS twin in ../dospatch/navdos.h. */
#ifndef NAVWIN_H
#define NAVWIN_H
#include "brrfix.h"

#define NAV_HOTSPOTS     ((unsigned char *)0x4B03B0)               /* menu_hotspots: 0x24 bytes each */
#define NAV_COUNT        (*(volatile short *)0x44BF38)             /* menu_hotspot_count */
#define NAV_ACTION       (*(volatile short *)0x44BF2C)             /* hotspot_action: fired this tick */
#define NAV_SCREEN       (*(volatile unsigned short *)0x4B13C0)    /* screen_current */
#define NAV_KEYS         (*(volatile unsigned char **)0x56C648)    /* KEY.DLL's _KEY_pressed */
#define NAV_KEY_DOWN     (**(volatile unsigned char **)0x56C644)   /* KEY.DLL's _KEY_down: the last key typed */
#define NAV_KEY_CHARS    ((const unsigned char *)0x44ADCE)         /* key_char_table */
#define NAV_IDLE         (*(volatile short *)0x4B13B4)             /* menu_idle_ticks: 15 s on the main menu, the demo */
#define NAV_TEXT_X       (*(volatile short *)0x452652)             /* text_x: text_draw's cursor */
#define NAV_TEXT_Y       (*(volatile short *)0x452650)             /* text_y */
#define NAV_TEXT_FONT    (*(volatile short *)0x44B34C)             /* text_font: text_draw's font */
#define NAV_ANIMS        ((volatile unsigned char *)0x4B11C4)      /* anims: 0x31 bytes each, +7 the frame */
#define NAV_NAME         ((unsigned char *)0x44AE68)               /* name_entry_text: up to 8 + NUL */
#define NAV_NAME_LEN     (*(volatile short *)0x44AE76)             /* name_entry_length */
#define NAV_NAME_ASK     (*(volatile short *)0x44AE7C)             /* name_entry_ask_new: "CREATE NEW FILE?" */
#define NAV_PLAYER1      (*(volatile unsigned char *)0x44B010)     /* controls_player1_type: 0 keys, 1 JS1 */
#define NAV_GAME_MODE    (*(volatile short *)0x44AF58)             /* game_mode: 2 the attract demo */
#define NAV_TICK_COUNT   (*GAME_TICK_COUNT)                        /* tick_count: ticks since the screen started */
#define NAV_CREDITS_TICK (*(volatile int *)0x44BCE4)               /* credits_last_tick: tick_count at the ticker's last line */
#define NAV_SOUND_STEP   (*(volatile short *)0x44AEC0)             /* sound_options_anim: the sound screen's buttons' step */
#define NAV_SOUND_CD     0                                         /* sound_options_draw draws no CD volume */
#define NAV_GLOBE_TRACK  (*(volatile short *)0x44A984)             /* globe_cursor's track: -1 until one is picked */
#define NAV_GLOBE_RACES  (*(volatile int *)0x4524CC)               /* series_races: the races chosen on the globe */
#define NAV_ENABLED      (cfg.menu_navigation)
#define NAV_INVERTED     (cfg.menu_animations == 1)                /* [controller] menu_animations=inverted */

#endif
