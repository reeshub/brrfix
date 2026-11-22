/* Joystick menu navigation: the shared core (menunav.c), compiled into PATCH.DLL (Windows)
 * and into the DOS patch (with BRR_DOS defined). Both games' menus are the same code (every
 * menu function has a DOS twin with the same instructions), so the core works on the
 * game's own data only: its hotspots, key table, menu mouse and name buffer, at each
 * game's addresses (navwin.h, ../dospatch/navdos.h). Each platform calls the core from
 * three places and gives it five small functions (below). */
#ifndef MENUNAV_H
#define MENUNAV_H

#ifdef BRR_DOS
#include "../dospatch/navdos.h"
#else
#include "navwin.h"
#endif

/* The core: called by the platform's hooks */
void nav_before_tick(void);   /* each tick, just before menu_hotspots_tick */
void nav_after_tick(void);    /* and just after it (the screen's own tick follows) */
/* In the menu's mouse update, with the position the game made from the real mouse, its real
 * buttons and whether it moved: sets the pointer (and the left button) while the joystick
 * has it */
void nav_mouse(int moved, int buttons, short *x, short *y, short *buttons_now);
void nav_before_draw(int index);  /* around menu_draw_group's menu_hotspot_draw(index) */
void nav_after_draw(int index);
/* sound_options_draw's sprite_draw(frame, x, y) of a button: the frame to draw instead */
int nav_sprite_frame(int frame, int x, int y, int per_step);

/* JOYCLICK.WAV, built by the core before nav_install_click (JOYCLICK_WAV_SIZE bytes) */
extern unsigned char nav_click_wav[];

/* The platform: */
void nav_install_click(void);               /* SAMPLES\JOYCLICK.WAV, if it isn't there */
void nav_play_click(void);                  /* the game's sound_play_sample("JOYCLICK") */
int nav_name_load(char *name, int size);    /* the last name accepted (BRRNAME.TXT), its length */
void nav_name_save(const char *name);
#ifndef nav_note
void nav_note(const char *what, int value); /* the log (Windows' BRRFIX.LOG; nothing on DOS) */
#endif
#ifndef nav_pointer
void nav_pointer(int shown);                /* the system's pointer (Windows, in a window) */
#endif

#define NAV_CLICK_NAME "JOYCLICK"
#define NAV_CLICK_FILE "SAMPLES\\JOYCLICK.WAV"
#define NAV_NAME_FILE  "BRRNAME.TXT"

#endif
