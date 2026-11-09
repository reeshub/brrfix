/* DPLAY.DLL: stands in for DirectX 3's DirectPlay, which WINRACE.EXE imports (ordinals 1
 * and 2) for network play only, and which Windows 10 and 11 don't have, so without it the
 * game can't start there. It hands both calls to a real DirectPlay when there is one (the
 * system's own dplay.dll, as on Windows 95 with DirectX 3, or else dplayx.dll); otherwise
 * network play is simply unavailable and single-player is unaffected. */
#include <windows.h>

typedef HRESULT (WINAPI *create_fn)(void *guid, void **dp, void *outer);
typedef HRESULT (WINAPI *enumerate_fn)(void *callback, void *context);

#define DPERR_UNAVAILABLE ((HRESULT)0x8877021CL)

HRESULT WINAPI DirectPlayCreate(void *guid, void **dp, void *outer);
HRESULT WINAPI DirectPlayEnumerate(void *callback, void *context);

static FARPROC real(int ordinal, const char *name)
{
    static HMODULE system_dplay, dplayx;
    static int tried;
    if (!tried) {
        char path[MAX_PATH];
        UINT n = GetSystemDirectoryA(path, MAX_PATH - 12);
        tried = 1;
        if (n && n < MAX_PATH - 12) {
            lstrcatA(path, "\\dplay.dll");
            system_dplay = LoadLibraryA(path);
        }
        if (!system_dplay)
            dplayx = LoadLibraryA("dplayx.dll");
    }
    if (system_dplay)
        return GetProcAddress(system_dplay, MAKEINTRESOURCEA(ordinal));
    return dplayx ? GetProcAddress(dplayx, name) : NULL;
}

HRESULT WINAPI DirectPlayCreate(void *guid, void **dp, void *outer)
{
    create_fn f = (create_fn)real(1, "DirectPlayCreate");
    if (f && f != DirectPlayCreate)  /* never ourselves, should the loader hand us back */
        return f(guid, dp, outer);
    if (dp)
        *dp = NULL;
    return DPERR_UNAVAILABLE;
}

HRESULT WINAPI DirectPlayEnumerate(void *callback, void *context)
{
    enumerate_fn f = (enumerate_fn)real(2, "DirectPlayEnumerateA");
    if (f && f != DirectPlayEnumerate)
        return f(callback, context);
    return S_OK;  /* no connection types to offer */
}
