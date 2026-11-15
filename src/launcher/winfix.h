/* WINRACEP.EXE: the launcher's copy of the game's WINRACE.EXE, beside the launcher, with
 * its section headers corrected ("P" for patched). The game's own files are left as they
 * are. The 1996 linker left every section's VirtualSize at 0 and gave the uninitialised
 * data section a file size the file doesn't have; Windows 95 and Wine accept that,
 * Windows 10 refuses to start it. The same correction as "brr.py win-headers". */
#ifndef BRR_WINFIX_H
#define BRR_WINFIX_H

enum { BRR_WIN_MISSING, BRR_WIN_ORIGINAL, BRR_WIN_CORRECTED, BRR_WIN_OTHER };

/* What the game's WINRACE.EXE in game_folder is: the v2.25 file as installed, the v2.25
 * file corrected in place by an earlier launcher (or WINRACE.EXE something else and
 * WINRACE.ORI, that launcher's backup, the original: BRR_WIN_ORIGINAL), or something else */
int brr_win_game_state(const char *game_folder);
/* Makes out (WINRACEP.EXE) from the game's WINRACE.EXE: corrected if it's the v2.25 file,
 * otherwise copied as it is. An out that's already right is left alone. Returns what out
 * now is (BRR_WIN_CORRECTED or BRR_WIN_OTHER), or -1 with a message if it failed. */
int brr_win_make_copy(const char *game_folder, const char *out, char *message, unsigned size);

#endif
