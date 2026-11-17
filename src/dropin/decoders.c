/* The two third-party audio decoders, compiled into the drop-in (round 2, item 6):
 *  - dr_flac (github.com/mackron/dr_libs, tag flac-0.13.4; public domain or MIT-0)
 *  - stb_vorbis (github.com/nothings/stb, commit 4e9b368; MIT or public domain)
 * Neither is in this repository: dropin-build downloads the pinned files into
 * tools/third-party/ and checks their SHA-256 (see THIRD-PARTY.txt). */
#include <stdlib.h>
#include <string.h>

/* Open Watcom's qsort wants its comparison function in Watcom's own register
 * convention; ours are C functions, so stb_vorbis gets this small sort instead. */
static void brr_qsort(void *base, size_t count, size_t size, int (*compare)(const void *, const void *))
{
    unsigned char *items = (unsigned char *)base, swap[64];
    size_t gap, i, j;
    if (size > sizeof swap)
        return;
    for (gap = count / 2; gap > 0; gap /= 2)  /* Shell sort: small and fast enough here */
        for (i = gap; i < count; i++) {
            memcpy(swap, items + i * size, size);
            for (j = i; j >= gap && compare(items + (j - gap) * size, swap) > 0; j -= gap)
                memcpy(items + j * size, items + (j - gap) * size, size);
            memcpy(items + j * size, swap, size);
        }
}

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_SIMD
#include "dr_flac.h"

#define qsort brr_qsort
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "stb_vorbis.c"
