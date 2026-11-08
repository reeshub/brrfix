/* Finding ripped CD tracks (tracks.c): shared by the drop-in and the launcher */
#ifndef BRR_TRACKS_H
#define BRR_TRACKS_H
/* path (MAX_PATH) gets the file for track (2..99) in folder; returns 1 if found */
int brr_find_track(const char *folder, int track, char *path);
int brr_count_tracks(const char *folder);
#endif
