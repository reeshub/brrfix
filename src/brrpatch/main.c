/* BRRPATCH.EXE: the DOS patch for Big Red Racing, for players. A 16-bit DOS program (no
 * extender), so it runs on the DOS PC itself, in a Windows 9x DOS box and in DOSBox.
 *   BRRPATCH [folder]      ask about each fix (Y or N), patch RACING.EXE, keeping the
 *                          original as RACING.ORI, then start the game (the director, 2026-11-22)
 *   BRRPATCH /Y [folder]   every fix, without asking, and the game isn't started
 *   BRRPATCH /N [folder]   ask, patch, but don't start the game
 *   BRRPATCH /U [folder]   put the original back */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#ifdef __DOS__
#include <conio.h>
#include <direct.h>
#include <dos.h>
#include <process.h>
#endif
#include "brrpatch.h"

/* print text wrapped at 78 columns, its lines after the first indented */
static void wrapped_from(const char *text, int indent)
{
    int width = 78;
    while (*text) {
        size_t n = strlen(text), cut = n;
        if (n > (size_t)width) {
            cut = (size_t)width;
            while (cut > 0 && text[cut] != ' ')
                cut--;
            if (cut == 0)
                cut = (size_t)width;
        }
        printf("%.*s\n", (int)cut, text);
        text += cut;
        while (*text == ' ')
            text++;
        if (*text) {
            printf("%*s", indent, "");
            width = 78 - indent;
        }
    }
}

static void wrapped(const char *text)
{
    wrapped_from(text, 0);
}

static void usage(void)
{
    printf("Fixes the camera stutter and smooths the picture on fast PCs; the physics is\n"
           "unchanged. Needs RACING.EXE v2.24, from the official v2.25 update.\n\n"
           "  BRRPATCH            ask about each fix, patch RACING.EXE, then start the game\n"
           "  BRRPATCH C:\\BIGRED  the same for the game in that folder\n"
           "  BRRPATCH /Y         every fix, without asking; the game isn't started\n"
           "  BRRPATCH /N         ask and patch, but don't start the game\n"
           "  BRRPATCH /U         undo: put the original RACING.EXE back\n\n"
           "The original is kept as RACING.ORI. Run BRRPATCH again to change the fixes.\n"
           "Start the game as usual, with RACING.\n");
}

/* One key: Y, N (or the letters given), Return for the first, Esc to stop. On DOS straight
 * from the keyboard; elsewhere (the tests) one line of the input each, its first letter. */
static int key(const char *letters)
{
    int c;
    for (;;) {
#ifdef __DOS__
        c = getch();
        if (c == 0 || c == 0xE0) {  /* an extended key's second code */
            getch();
            continue;
        }
#else
        char line[80];
        if (!fgets(line, sizeof line, stdin))
            return 27;
        c = (unsigned char)line[0];
#endif
        if (c == '\r' || c == '\n')
            return letters[0];
        if (c == 27)
            return 27;
        c = toupper(c);
        if (strchr(letters, c))
            return c;
    }
}

/* question: "Name: one sentence"; 1 yes, 0 no, -1 Esc */
static int ask(const char *question)
{
    int c;
    wrapped_from(question, 3);
    printf("   Apply it? [Y/N] ");
    fflush(stdout);
    c = key("YN");
    printf("%s\n", c == 27 ? "(stopped)" : c == 'Y' ? "Yes" : "No");
    return c == 27 ? -1 : c == 'Y';
}

/* Every question; 0 when answered, -1 when stopped with Esc */
static int questions(struct brr_dos_choices *c)
{
    printf("Choose the fixes (Y or N for each; Return for Y, Esc to stop):\n\n");
    if ((c->stutter = ask("1. Camera stutter: finer camera angles and a steady camera lean, so the "
                          "camera no longer judders.")) < 0
        || (c->camera = ask("2. Camera timing: two camera calculations that depended on the frame "
                            "rate no longer do.")) < 0
        || (c->interpolation = ask("3. Smooth movement: on fast PCs the vehicles are drawn between "
                                   "physics steps, 70 pictures a second instead of 35.")) < 0
        || (c->retrace = ask("4. No tearing: on fast PCs each picture is shown at the screen's "
                             "refresh.")) < 0
        || (c->short_catch_up = ask("5. Short catch-up: on fast PCs, after a pause, at most 8 ticks of "
                                    "game time are caught up at once instead of 35.")) < 0
        || (c->menu_navigation = ask("6. Menu navigation: the joystick and the arrow keys step through "
                                     "the menus' items, and button 1 or Return chooses.")) < 0)
        return -1;
    c->menu_inverted = 0;
    if (c->menu_navigation) {
        int k;
        wrapped_from("   Menu animations: O for original-ish (the selected item stops animating) or I "
                     "for inverted (only the selected item animates).", 3);
        printf("   [O/I] ");
        fflush(stdout);
        k = key("OI");
        printf("%s\n", k == 27 ? "(stopped)" : k == 'I' ? "Inverted" : "Original-ish");
        if (k == 27)
            return -1;
        c->menu_inverted = k == 'I';
    }
    printf("\nThe physics is unchanged, and a 486 runs it exactly as before.\n\n");
    return 0;
}

/* RACING in the game's folder, with BRRPATCH waiting (the director's choice: it keeps about
 * 60 KB while the game runs); 0 if it ran */
static int start_game(const char *folder)
{
#ifdef __DOS__
    char before[80];
    unsigned drive = 0, drives;
    int ran;
    if (!getcwd(before, sizeof before))
        before[0] = 0;
    _dos_getdrive(&drive);
    if (folder[0]) {
        int moved = 0;
        if (folder[1] == ':') {
            _dos_setdrive((unsigned)(toupper((unsigned char)folder[0]) - 'A' + 1), &drives);
            moved = folder[2] ? chdir(folder) : 0;
        } else
            moved = chdir(folder);
        if (moved != 0) {
            _dos_setdrive(drive, &drives);
            return -1;
        }
    }
    printf("\nStarting Big Red Racing...\n");
    fflush(stdout);
    ran = spawnl(P_WAIT, "RACING.EXE", "RACING.EXE", NULL) != -1;
    _dos_setdrive(drive, &drives);
    if (before[0])
        chdir(before);
    return ran ? 0 : -1;
#else
    (void)folder;
    return -1;
#endif
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
    struct brr_dos_choices choices;
    int undo = 0, all = 0, play = 1, result, i, state;
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
            if (letter == 'Y') {
                all = 1;
                play = 0;
                continue;
            }
            if (letter == 'N') {
                play = 0;
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
    brr_dos_progress = dot;
    if (!undo && !all) {  /* the questions only for a RACING.EXE it can patch */
        printf("Checking RACING.EXE");
        fflush(stdout);
        state = brr_dos_state(folder);
        printf("\n\n");
        if (state == BRR_DOS_ORIGINAL || state == BRR_DOS_PATCHED) {
            if (state == BRR_DOS_PATCHED)
                printf("RACING.EXE is already patched: your answers change its fixes.\n");
            if (questions(&choices) < 0) {
                printf("Stopped. Nothing was changed.\n");
                return 1;
            }
        } else
            all = 1;  /* brr_dos_patch says what's wrong */
    }
    if (all && !undo)
        printf("This patch makes these changes to RACING.EXE:\n"
               " - finer camera angles and a steady camera lean: no more camera stutter\n"
               " - two camera calculations no longer depend on the frame rate\n"
               " - on fast PCs, vehicles drawn between physics steps (races and the track\n"
               "   flyover): 70 pictures a second\n"
               " - on fast PCs, each picture shown at the screen's refresh: no tearing\n"
               " - on fast PCs, a shorter catch-up after a pause\n"
               " - menu navigation with the joystick and the arrow keys\n"
               "The physics is unchanged, and a 486 runs it exactly as before.\n\n");
    printf(undo ? "Putting the original back" : "Patching RACING.EXE");
    fflush(stdout);
    result = undo ? brr_dos_restore(folder, message, sizeof message)
                  : brr_dos_patch(folder, all ? NULL : &choices, message, sizeof message);
    printf("\n");
    wrapped(message);
    if (result != 0 || undo)
        return result < 0 ? 1 : 0;
    if (play && start_game(folder) == 0)
        return 0;
    printf("Start the game as usual, with RACING. BRRPATCH /U puts the original back.\n");
    return 0;
}
