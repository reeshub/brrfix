/* BRRPATCH: patches the DOS game's RACING.EXE in place, keeping the original as
 * RACING.ORI. Used by BRRPATCH.EXE (DOS) and by the launcher's "Patch the DOS version"
 * button (Windows). Standard C file functions only, so it builds for 16-bit DOS too. */
#ifndef BRRPATCH_H
#define BRRPATCH_H

enum { BRR_DOS_MISSING, BRR_DOS_ORIGINAL, BRR_DOS_PATCHED, BRR_DOS_OTHER, BRR_DOS_UNREADABLE };

/* What RACING.EXE in folder ("" for the current folder) is */
int brr_dos_state(const char *folder);
/* 0 done, 1 nothing to do, -1 failed (nothing changed unless the message says so).
 * message: one or two sentences for the player. */
int brr_dos_patch(const char *folder, char *message, unsigned size);
int brr_dos_restore(const char *folder, char *message, unsigned size);
const char *brr_dos_version(void);
const char *brr_dos_tag(void);
/* Called every 64 KB read or written, if set (BRRPATCH.EXE prints a dot) */
extern void (*brr_dos_progress)(void);

#endif
