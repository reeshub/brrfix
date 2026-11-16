/* BRRPATCH.EXE: the DOS patch for Big Red Racing, for players. A 16-bit DOS program (no
 * extender), so it runs on the DOS PC itself, in a Windows 9x DOS box and in DOSBox.
 *   BRRPATCH [folder]      patch RACING.EXE, keeping the original as RACING.ORI
 *   BRRPATCH /U [folder]   put the original back */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "brrpatch.h"

/* print text wrapped at 78 columns */
static void wrapped(const char *text)
{
    while (*text) {
        size_t n = strlen(text), cut = n;
        if (n > 78) {
            cut = 78;
            while (cut > 0 && text[cut] != ' ')
                cut--;
            if (cut == 0)
                cut = 78;
        }
        printf("%.*s\n", (int)cut, text);
        text += cut;
        while (*text == ' ')
            text++;
    }
}

static void usage(void)
{
    printf("Fixes the camera stutter and smooths the picture on fast PCs; the physics is\n"
           "unchanged. Needs RACING.EXE v2.24, from the official v2.25 update.\n\n"
           "  BRRPATCH            patch RACING.EXE in this folder\n"
           "  BRRPATCH C:\\BIGRED  patch the game in that folder\n"
           "  BRRPATCH /U         undo: put the original RACING.EXE back\n\n"
           "The original is kept as RACING.ORI. Start the game as usual, with RACING.\n");
}

static void dot(void)
{
    putchar('.');
    fflush(stdout);
}

int main(int argc, char **argv)
{
    const char *folder = "";
    char message[512];
    int undo = 0, result, i;
    printf("Big Red Racing DOS patch %s\n", brr_dos_version());
    printf("For support please visit ctrl-alt-rees.com\n\n");
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if ((arg[0] == '/' || arg[0] == '-') && arg[1] && !arg[2]) {
            int letter = toupper((unsigned char)arg[1]);
            if (letter == 'U') {
                undo = 1;
                continue;
            }
            if (letter == '?' || letter == 'H') {
                usage();
                return 0;
            }
            printf("Unknown option %s. BRRPATCH /? lists the options.\n", arg);
            return 2;
        }
        folder = arg;
    }
    if (!undo)
        printf("This patch makes these changes to RACING.EXE:\n"
               " - finer camera angles and a steady camera lean: no more camera stutter\n"
               " - two camera calculations no longer depend on the frame rate\n"
               " - on fast PCs, vehicles drawn between physics steps (races and the track\n"
               "   flyover): 70 pictures a second\n"
               " - on fast PCs, each picture shown at the screen's refresh: no tearing\n"
               " - on fast PCs, a shorter catch-up after a pause\n"
               " - joystick menu navigation (partial)\n"
               "The physics is unchanged, and a 486 runs it exactly as before.\n\n");
    printf(undo ? "Putting the original back" : "Patching RACING.EXE");
    fflush(stdout);
    brr_dos_progress = dot;
    result = undo ? brr_dos_restore(folder, message, sizeof message)
                  : brr_dos_patch(folder, message, sizeof message);
    printf("\n");
    wrapped(message);
    if (result == 0 && !undo)
        printf("Start the game as usual, with RACING. BRRPATCH /U puts the original back.\n");
    return result < 0 ? 1 : 0;
}
