/* Windows' own file functions for the launcher's file patchers.
 *
 * The DOS patcher (src/brrpatch/brrpatch.c) and the header corrector (winfix.c) use the
 * standard C file functions, so the same code builds as the 16-bit DOS program
 * BRRPATCH.EXE and with any machine's compiler, for tests. In a Windows program, though,
 * Open Watcom's fread brings its console-input code with it, which imports
 * GetConsoleMode, SetConsoleMode, ReadConsoleInputA and WriteConsoleA, none of them on
 * Windows 95. So dropin-build compiles both for the launcher with this header forced in
 * (wcc386 -fi), sending those calls to winfile.c, which uses CreateFileA, ReadFile and
 * the rest. Only what they use: whole files opened "rb" or "wb", seeks, the position,
 * at most a few files open at once. */
#ifndef BRR_WINFILE_H
#define BRR_WINFILE_H

#include <stdio.h>
#include <stddef.h>

typedef struct brr_file brr_file;
brr_file *brr_fopen(const char *path, const char *mode);
size_t brr_fread(void *data, size_t size, size_t count, brr_file *f);
size_t brr_fwrite(const void *data, size_t size, size_t count, brr_file *f);
int brr_fseek(brr_file *f, long offset, int origin);
long brr_ftell(brr_file *f);
int brr_ferror(brr_file *f);
int brr_fclose(brr_file *f);
int brr_remove(const char *path);
int brr_rename(const char *from, const char *to);

#ifndef BRR_WINFILE_IMPLEMENTATION
#undef FILE
#undef fopen
#undef fread
#undef fwrite
#undef fseek
#undef ftell
#undef ferror
#undef fclose
#undef remove
#undef rename
#define FILE brr_file
#define fopen brr_fopen
#define fread brr_fread
#define fwrite brr_fwrite
#define fseek brr_fseek
#define ftell brr_ftell
#define ferror brr_ferror
#define fclose brr_fclose
#define remove brr_remove
#define rename brr_rename
#endif

#endif
