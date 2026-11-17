/* Finding ripped CD tracks in a folder, by track number: track02.flac, Track02.ogg,
 * "Track 02.wav", 02.flac, "02 - Title.flac" and the like; FLAC, Ogg Vorbis or WAV. */
#include <windows.h>
#include "tracks.h"

static const char *const extensions[] = {"flac", "ogg", "wav"};

int brr_find_track(const char *folder, int track, char *path)
{
    static const char *const names[] = {"track%02d", "Track%02d", "Track %02d", "track %02d", "%02d"};
    int e, n;
    for (e = 0; e < 3; e++) {
        WIN32_FIND_DATAA found;
        HANDLE search;
        for (n = 0; n < 5; n++) {
            char name[64];
            wsprintfA(name, names[n], track);
            wsprintfA(path, "%s\\%s.%s", folder, name, extensions[e]);
            if (GetFileAttributesA(path) != 0xFFFFFFFFu)
                return 1;
        }
        wsprintfA(path, "%s\\%02d *.%s", folder, track, extensions[e]);
        search = FindFirstFileA(path, &found);
        if (search != INVALID_HANDLE_VALUE) {
            FindClose(search);
            wsprintfA(path, "%s\\%s", folder, found.cFileName);
            return 1;
        }
    }
    return 0;
}

int brr_count_tracks(const char *folder)
{
    char path[MAX_PATH];
    int track, count = 0;
    for (track = 2; track < 100; track++)
        count += brr_find_track(folder, track, path);
    return count;
}
