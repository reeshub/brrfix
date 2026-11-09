/* Corrects WINRACE.EXE's section headers on disk, keeping the original as WINRACE.ORI.
 * The 1996 linker left every section's VirtualSize at 0 and gave the uninitialised data
 * section a file size the file doesn't have; Windows 95 and Wine accept that, Windows 10
 * refuses to start it. The same correction as "brr.py win-headers". */
#ifndef BRR_WINFIX_H
#define BRR_WINFIX_H

enum { BRR_WIN_MISSING, BRR_WIN_ORIGINAL, BRR_WIN_CORRECTED, BRR_WIN_OTHER };

/* What WINRACE.EXE in folder is: the v2.25 file as installed, or header-corrected */
int brr_win_state(const char *folder);
/* WINRACE.ORI is the original v2.25 file */
int brr_win_backup_is_original(const char *folder);
/* 0 done, 1 nothing to do, -1 failed (nothing changed unless the message says so) */
int brr_win_correct(const char *folder, char *message, unsigned size);

#endif
