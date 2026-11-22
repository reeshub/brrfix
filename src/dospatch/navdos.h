/* Joystick menu navigation: RACING.EXE v2.24's addresses for the shared core
 * (../dropin/menunav.c), the twins of ../dropin/navwin.h's (GAME_SYMBOLS in
 * scripts/dos_build.py places the game_ symbols) */
#ifndef NAVDOS_H
#define NAVDOS_H
#include "brrdos.h"

extern volatile s16 game_hotspot_action;      /* fired this tick */
extern volatile u8 game_keyboard_last_key;    /* the last key typed (the name screen's) */
extern const u8 game_key_char_table[];
extern volatile s16 game_menu_idle_ticks;
extern volatile s16 game_text_x;              /* text_draw's cursor */
extern volatile u8 game_anims[];              /* 0x31 bytes each, +7 the frame */
extern u8 game_name_entry_text[];
extern volatile s16 game_name_entry_length;
extern volatile s16 game_name_entry_ask_new;
extern volatile u8 game_controls_player1_type;
extern volatile s16 game_menu_mouse_x, game_menu_mouse_y;
extern volatile s16 game_mouse_buttons_now;
extern volatile s16 game_mouse_move_x, game_mouse_move_y;  /* the mouse's movement this tick */
extern volatile s16 game_mouse_present;       /* a mouse driver answered */
extern volatile s16 game_game_mode;           /* 2 the attract demo */
extern volatile s32 game_credits_last_tick;   /* tick_count at the credits ticker's last line */
extern volatile s16 game_sound_options_anim;  /* the sound screen's buttons' step */
extern void game_sound_play_sample(const char *name);       /* SAMPLES\<name>.WAV, once */

#define NAV_HOTSPOTS  game_menu_hotspots
#define NAV_COUNT     game_menu_hotspot_count
#define NAV_ACTION    game_hotspot_action
#define NAV_SCREEN    game_screen_current
#define NAV_KEYS      ((volatile unsigned char *)game_key_table)
#define NAV_KEY_DOWN  game_keyboard_last_key
#define NAV_KEY_CHARS game_key_char_table
#define NAV_IDLE      game_menu_idle_ticks
#define NAV_TEXT_X    game_text_x
#define NAV_ANIMS     game_anims
#define NAV_NAME      game_name_entry_text
#define NAV_NAME_LEN  game_name_entry_length
#define NAV_NAME_ASK  game_name_entry_ask_new
#define NAV_PLAYER1   game_controls_player1_type
#define NAV_GAME_MODE game_game_mode
#define NAV_TICK_COUNT game_tick_count
#define NAV_CREDITS_TICK game_credits_last_tick
#define NAV_SOUND_STEP game_sound_options_anim
#define NAV_SOUND_CD  1               /* sound_options_draw draws the CD volume too */
#define NAV_ENABLED   (OPTION(OPT_MENU_PAD))
#define nav_pointer(shown)            /* no system pointer: the game's own is the only one */

/* DOS's own file calls, for the click and the last name (DOS/4GW passes them on) */
s32 dos_open_read(const char *name);
#pragma aux dos_open_read = "mov ax,3d00h" "int 21h" "sbb edx,edx" "movzx eax,ax" "or eax,edx" \
    parm [edx] value [eax] modify [edx];
s32 dos_read(s32 handle, void *data, u32 size);
#pragma aux dos_read = "mov ah,3fh" "int 21h" "sbb edx,edx" "movzx eax,ax" "or eax,edx" \
    parm [ebx] [edx] [ecx] value [eax] modify [edx];

void brr_dos_menu_mouse(void);

#endif
