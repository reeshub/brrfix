/* Windows' own file functions for the launcher's file patchers: see winfile.h. */
#define BRR_WINFILE_IMPLEMENTATION
#include <windows.h>
#include "winfile.h"

#define MAX_OPEN 4

struct brr_file {
    HANDLE handle;
    int used, error;
};

static brr_file files[MAX_OPEN];

brr_file *brr_fopen(const char *path, const char *mode)
{
    int writing = mode[0] == 'w', updating = mode[1] == '+' || (mode[1] && mode[2] == '+');  /* "r+b" */
    HANDLE handle;
    int i;
    for (i = 0; i < MAX_OPEN && files[i].used; i++)
        ;
    if (i == MAX_OPEN)
        return NULL;
    handle = CreateFileA(path, updating ? GENERIC_READ | GENERIC_WRITE : writing ? GENERIC_WRITE : GENERIC_READ,
                         writing || updating ? 0 : FILE_SHARE_READ, NULL,
                         writing ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE)
        return NULL;
    files[i].handle = handle;
    files[i].used = 1;
    files[i].error = 0;
    return &files[i];
}

size_t brr_fread(void *data, size_t size, size_t count, brr_file *f)
{
    size_t want = size * count, done = 0;
    while (size && done < want) {
        DWORD got = 0;
        if (!ReadFile(f->handle, (char *)data + done, (DWORD)(want - done), &got, NULL)) {
            f->error = 1;
            break;
        }
        if (!got)
            break;  /* end of the file */
        done += got;
    }
    return size ? done / size : 0;
}

size_t brr_fwrite(const void *data, size_t size, size_t count, brr_file *f)
{
    size_t want = size * count, done = 0;
    while (size && done < want) {
        DWORD put = 0;
        if (!WriteFile(f->handle, (const char *)data + done, (DWORD)(want - done), &put, NULL) || !put) {
            f->error = 1;
            break;
        }
        done += put;
    }
    return size ? done / size : 0;
}

int brr_fseek(brr_file *f, long offset, int origin)
{
    DWORD method = origin == SEEK_CUR ? FILE_CURRENT : origin == SEEK_END ? FILE_END : FILE_BEGIN;
    SetLastError(NO_ERROR);
    if (SetFilePointer(f->handle, offset, NULL, method) == 0xFFFFFFFFu && GetLastError() != NO_ERROR)
        return -1;
    return 0;
}

long brr_ftell(brr_file *f)
{
    DWORD at;
    SetLastError(NO_ERROR);
    at = SetFilePointer(f->handle, 0, NULL, FILE_CURRENT);
    if (at == 0xFFFFFFFFu && GetLastError() != NO_ERROR)
        return -1L;
    return (long)at;
}

int brr_ferror(brr_file *f)
{
    return f->error;
}

int brr_fclose(brr_file *f)
{
    int ok = CloseHandle(f->handle) != 0;
    f->used = 0;
    return ok && !f->error ? 0 : EOF;
}

int brr_remove(const char *path)
{
    return DeleteFileA(path) ? 0 : -1;
}

/* As Open Watcom's own rename on Windows: fails if the new name already exists */
int brr_rename(const char *from, const char *to)
{
    return MoveFileA(from, to) ? 0 : -1;
}
