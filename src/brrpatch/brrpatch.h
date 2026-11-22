/* BRRPATCH: patches the DOS game's RACING.EXE in place, keeping the original as
 * RACING.ORI. Used by BRRPATCH.EXE (DOS) and by the launcher's "Patch the DOS version"
 * button (Windows). Standard C file functions only, so it builds for 16-bit DOS too. */
#ifndef BRRPATCH_H
#define BRRPATCH_H

enum { BRR_DOS_MISSING, BRR_DOS_ORIGINAL, BRR_DOS_PATCHED, BRR_DOS_OTHER, BRR_DOS_UNREADABLE };

/* The fixes BRRPATCH asks about (1 on, 0 off). They're the patch's options: every fix's
 * code is always written, and each one checks its option as the game runs. */
struct brr_dos_choices {
    int stutter;          /* finer camera angles and a steady camera lean (fine_angles, roll_fix) */
    int camera;           /* two camera calculations no longer tied to the frame rate (camera) */
    int interpolation;    /* fast PCs: vehicles drawn between physics steps */
    int retrace;          /* fast PCs: each picture at the screen's refresh */
    int short_catch_up;   /* fast PCs: a catch-up of 8 ticks at most, not 35 (cap) */
    int menu_navigation;  /* the joystick and the arrow keys in the menus (menu_pad) */
    int menu_inverted;    /* the menus' animations: 0 original-ish, 1 inverted (menu_anim) */
};

/* What RACING.EXE in folder ("" for the current folder) is (a patched one, whatever its
 * choices) */
int brr_dos_state(const char *folder);
/* 0 done, 1 nothing to do, -1 failed (nothing changed unless the message says so).
 * choices: NULL for every fix (the defaults; an already patched RACING.EXE is left as it
 * is), else those (an already patched RACING.EXE gets them). message: one or two sentences
 * for the player. */
int brr_dos_patch(const char *folder, const struct brr_dos_choices *choices, char *message, unsigned size);
int brr_dos_restore(const char *folder, char *message, unsigned size);
const char *brr_dos_version(void);
const char *brr_dos_tag(void);
/* Called every 64 KB read or written, if set (BRRPATCH.EXE prints a dot) */
extern void (*brr_dos_progress)(void);

#endif
