/* Big Red Racing.exe: Big Red Launcher by ctrl-alt-rees (round 2, item 8). Our own program, so Steam
 * shows a proper name. It shows a settings window (which can be skipped), checks the
 * game's files, then starts the game and waits for it to exit.
 *
 * The layout: the launcher and our files at the top, the installed game in Game (BRRFIX.INI
 * [launcher] game: another folder name or a full path) and the ripped CD tracks in Music.
 * The game's files are left as they are: the launcher makes WINRACEP.EXE beside itself,
 * WINRACE.EXE with its section headers corrected, our PATCH.DLL imported instead of the
 * game's TIMER.DLL and, if it's here (and not on Windows 95, 98 or ME, which can't load it),
 * cnc-ddraw (CNCDD.DLL) instead of DirectDraw (winfix.c), and starts that in the game's
 * folder. Windows looks for DLLs beside the program first, so the game gets ours (and
 * DPLAY.DLL), and its own KEY.DLL from the game's folder (the current folder, and added to
 * PATH), which is why WINRACEP.EXE started by itself can't find KEY.DLL. Under Wine and
 * Proton cnc-ddraw's own name means no DLL override is needed.
 *
 * Settings live in BRRFIX.INI (the drop-in's) and ddraw.ini (cnc-ddraw's). They're
 * changed with WritePrivateProfileStringA, value by value, so comments, layout and keys
 * the launcher doesn't know about are kept. Windows 95 compatible: ANSI functions that
 * Windows 95 has; anything newer is looked up at run time.
 *
 * Command line: -play starts the game straight away; -setup always shows the window;
 * holding Shift while starting also shows it. Anything else goes to the game. When the
 * window was shown, it comes back when the game ends (on the same tab); Quit leaves.
 *
 * The window's settings are on six tabs (General, Display, Renderer, Audio, Controller,
 * Advanced), with the buttons below them; General has the Period and Modern defaults. Choices
 * that are the 1996 game's own are marked "(1996)" (the director, 2026-11-16).
 *
 * The "Patch the DOS version" button patches (or restores) the DOS version's RACING.EXE
 * on disk, with the same code as BRRPATCH.EXE, for players on modern Windows. */
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stddef.h>
#include <string.h>
#include "resource.h"
#include "version.h"
#include "game.h"
#include "tracks.h"
#include "brrpatch.h"
#include "winfix.h"

static char folder[MAX_PATH], game_folder[MAX_PATH], brrfix_ini[MAX_PATH], ddraw_ini[MAX_PATH];
static int windows_9x;

/* made from HELP.md by dropin-build */
#define HELP_FILE "Big Red Launcher Help.txt"

/* ---- choices, in the order the combo boxes show them ---- */
struct choice { const char *label, *value; };

static const struct choice modes[] = {
    {"Full screen", "fullscreen"}, {"Windowed", "windowed"}, {0, 0}};
/* the Original renderer shows the first two; Max is the Modern renderer's (resolution=high
 * with render_resolution=max in BRRFIX.INI) */
static const struct choice resolutions[] = {
    {"Low (320x200) (1996)", "low"}, {"High (640x480) (1996)", "high"}, {"Max", "max"}, {0, 0}};
#define ORIGINAL_RESOLUTIONS 2
static const struct choice renderers[] = {{"Modern", "new"}, {"Original", "original"}, {0, 0}};
static const struct choice distances[] = {{"In-game sliders (1996)", "0"}, {"Engine max (25)", "25"},
                                          {"Far (44)", "44"}, {"Max out (60)", "60"}, {0, 0}};
static const struct choice texture_distances[] = {{"As draw distance", "draw"}, {"Engine max (25)", "25"},
                                                  {"In-game slider (1996)", "slider"}, {0, 0}};
static const struct choice lightings[] = {{"Flat", "flat"}, {"Smooth", "smooth"}, {"Original (1996)", "game"}, {0, 0}};
static const struct choice details[] = {
    {"Highest", "highest"}, {"Matched", "matched"}, {"Original (1996)", "original"}, {0, 0}};
static const struct choice hud_sizes[] = {{"Small", "small"}, {"Large (1996)", "large"}, {0, 0}};
static const struct choice colour_depths[] = {{"256 colours (1996)", "0"}, {"Full colour", "1"}, {0, 0}};
static const struct choice languages[] = {
    {"English", "ENG"}, {"USA", "USA"}, {"French", "FRE"},
    {"German", "GER"}, {"Italian", "ITA"}, {"Spanish", "SPA"}, {"Japanese", "JAP"}, {0, 0}};
static const struct choice scales[] = {  /* the Modern renderer's window: multiples of 640x480 */
    {"Largest that fits", "auto"}, {"1x (640x480)", "1"}, {"2x (1280x960)", "2"}, {"3x (1920x1440)", "3"},
    {"4x (2560x1920)", "4"}, {"5x", "5"}, {"6x", "6"}, {0, 0}};
/* cnc-ddraw's scaling presets, each under its Scaling aspect (the director, 2026-11-16: the same
 * names in each), with its ddraw.ini keys: boxing, maintas, aspect_ratio, shader, d3d9_filter.
 * Integer is square only: cnc-ddraw's whole-number scaling keeps the picture's own shape. */
#define SHADER_SHARP   "Shaders\\interpolation\\lanczos2-sharp.glsl"
#define SHADER_NEAREST "Shaders\\nearest-neighbor.glsl"
#define SHADER_SMOOTH  "Shaders\\interpolation\\catmull-rom-bilinear.glsl"
static const struct preset {
    int square;
    const char *label, *value, *keys[5];
} presets[] = {
    {0, "Sharp", "43sharp", {"false", "true", "4:3", SHADER_SHARP, "0"}},
    {0, "Nearest", "43nearest", {"false", "true", "4:3", SHADER_NEAREST, "0"}},
    {0, "Smooth", "43smooth", {"false", "true", "4:3", SHADER_SMOOTH, "1"}},
    {0, "Stretch", "43stretch", {"false", "false", "", SHADER_NEAREST, "0"}},
    {1, "Sharp", "squaresharp", {"false", "true", "", SHADER_SHARP, "0"}},
    {1, "Nearest", "squarenearest", {"false", "true", "", SHADER_NEAREST, "0"}},
    {1, "Smooth", "smooth", {"false", "true", "", SHADER_SMOOTH, "1"}},
    {1, "Integer", "integer", {"true", "true", "", SHADER_NEAREST, "0"}},
    {1, "Stretch", "stretch", {"false", "false", "", SHADER_NEAREST, "0"}}};
#define PRESET_COUNT ((int)(sizeof presets / sizeof presets[0]))
#define PRESET_ORIGINAL "squaresharp" /* the Original renderer's default (director, 2026-11-17) */
#define PRESET_MODERN "squaresharp"   /* the Modern renderer's (director, 2026-11-16) */
#define PRESET_WINDOW "stretch"       /* the Original renderer's own window: no letterbox */
static const struct choice aspects[] = {{"4:3", "4:3"}, {"Square", "square"}, {0, 0}};
/* Max's limit (Modern renderer): that part of the screen's (or window's) height (director, 2026-11-16) */
static const struct choice max_limits[] = {
    {"1/5 screen height", "1/5"}, {"1/4 screen height", "1/4"}, {"1/3 screen height", "1/3"},
    {"1/2 screen height", "1/2"}, {"Screen height", "screen"}, {0, 0}};
static const struct choice channel_counts[] = {{"8 (1996)", "8"}, {"16", "16"}, {"32", "32"}, {0, 0}};
static const struct choice smoothings[] = {
    {"Off", "0"}, {"Light", "25"}, {"Medium", "50"}, {"Strong", "100"}, {0, 0}};
static const struct choice frame_limits[] = {
    {"Display rate", "auto"}, {"Unlocked", "0"}, {"30 fps", "30"}, {"60 fps", "60"}, {"120 fps", "120"},
    {"144 fps", "144"}, {"240 fps", "240"}, {0, 0}};
static const struct choice intensities[] = {
    {"100%", "100"}, {"80%", "80"}, {"60%", "60"}, {"40%", "40"}, {"20%", "20"}, {0, 0}};
static const struct choice volumes[] = {
    {"100%", "100"}, {"90%", "90"}, {"80%", "80"}, {"70%", "70"}, {"60%", "60"}, {"50%", "50"}, {"40%", "40"},
    {"30%", "30"}, {"20%", "20"}, {"10%", "10"}, {"0%", "0"}, {0, 0}};
static const struct choice deadzones[] = {
    {"0%", "0"}, {"5%", "5"}, {"10%", "10"}, {"15%", "15"}, {"20%", "20"}, {"25%", "25"}, {"30%", "30"}, {0, 0}};
static const struct choice sensitivities[] = {
    {"50%", "50"}, {"75%", "75"}, {"100%", "100"}, {"125%", "125"}, {"150%", "150"}, {"200%", "200"}, {0, 0}};
static const struct choice layouts[] = {{"Triggers", "triggers"}, {"Stick", "stick"}, {0, 0}};
static const struct choice menu_animations[] = {{"Original-ish", "original"}, {"Inverted", "inverted"}, {0, 0}};
static const struct choice rates[] = {{"70.0 (DOS original)", "70.0"}, {"71.43 (Windows original)", "71.43"}, {0, 0}};
static const struct choice caps[] = {{"8 ticks", "8"}, {"35 (original)", "35"}, {0, 0}};

/* the scaling preset each renderer uses (index into presets): 0 the Original's
 * (fullscreen_scaling), 1 the Modern one's (modern_scaling); the combo box shows the chosen
 * renderer's, shown_fresh says whose it is showing */
static int preset_of[2];
static int shown_fresh = -1;

/* ---- the INI files ---- */
/* file NULL: just the fallback (the defaults) */
static void ini_get(const char *file, const char *section, const char *key, const char *fallback, char *out, int size)
{
    if (file)
        GetPrivateProfileStringA(section, key, fallback, out, size, file);
    else
        lstrcpynA(out, fallback, size);
}

static int ini_flag(const char *section, const char *key, int fallback)
{
    return (int)GetPrivateProfileIntA(section, key, fallback, brrfix_ini) != 0;
}

static void ini_set(const char *file, const char *section, const char *key, const char *value)
{
    WritePrivateProfileStringA(section, key, value, file);
}

/* ---- combo boxes ---- */
/* the first count choices of list (all with -1) */
static void fill_first(HWND dialog, int id, const struct choice *list, const char *value, int count)
{
    HWND box = GetDlgItem(dialog, id);
    int i, chosen = 0;
    SendMessageA(box, CB_RESETCONTENT, 0, 0);
    for (i = 0; list[i].label && (count < 0 || i < count); i++) {
        SendMessageA(box, CB_ADDSTRING, 0, (LPARAM)list[i].label);
        if (!lstrcmpiA(list[i].value, value))
            chosen = i;
    }
    SendMessageA(box, CB_SETCURSEL, (WPARAM)chosen, 0);  /* unreadable values fall back to the first */
}

static void fill(HWND dialog, int id, const struct choice *list, const char *value)
{
    fill_first(dialog, id, list, value, -1);
}

static const char *chosen(HWND dialog, int id, const struct choice *list)
{
    LRESULT i = SendDlgItemMessageA(dialog, id, CB_GETCURSEL, 0, 0);
    return i >= 0 ? list[i].value : list[0].value;
}

/* value if the list has it, else fallback (a value from an older launcher) */
static const char *listed(const struct choice *list, const char *value, const char *fallback)
{
    int i;
    for (i = 0; list[i].label; i++)
        if (!lstrcmpiA(list[i].value, value))
            return value;
    return fallback;
}

static int preset_index(const char *value, const char *fallback)
{
    int i;
    for (i = 0; i < PRESET_COUNT; i++)
        if (!lstrcmpiA(presets[i].value, value))
            return i;
    return fallback ? preset_index(fallback, NULL) : 0;
}

/* The Scaling box: the presets of one Scaling aspect, want chosen, or its namesake in this
 * aspect (Sharp for Sharp), or the first */
static void fill_presets(HWND dialog, int square, int want)
{
    HWND box = GetDlgItem(dialog, IDC_PRESET);
    int i, chosen_item = 0;
    SendMessageA(box, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < PRESET_COUNT; i++) {
        LRESULT item;
        if (presets[i].square != square)
            continue;
        item = SendMessageA(box, CB_ADDSTRING, 0, (LPARAM)presets[i].label);
        SendMessageA(box, CB_SETITEMDATA, (WPARAM)item, (LPARAM)i);
        if (i == want || (presets[want].square != square && !lstrcmpA(presets[i].label, presets[want].label)))
            chosen_item = (int)item;
    }
    SendMessageA(box, CB_SETCURSEL, (WPARAM)chosen_item, 0);
}

/* the preset the Scaling box shows (index into presets), or -1 */
static int shown_preset(HWND dialog)
{
    LRESULT item = SendDlgItemMessageA(dialog, IDC_PRESET, CB_GETCURSEL, 0, 0);
    return item >= 0 ? (int)SendDlgItemMessageA(dialog, IDC_PRESET, CB_GETITEMDATA, (WPARAM)item, 0) : -1;
}

/* both boxes showing a preset */
static void show_preset(HWND dialog, int preset)
{
    fill(dialog, IDC_ASPECT, aspects, presets[preset].square ? "square" : "4:3");
    fill_presets(dialog, presets[preset].square, preset);
}

static void check(HWND dialog, int id, int on) { CheckDlgButton(dialog, id, on ? BST_CHECKED : BST_UNCHECKED); }
static int checked(HWND dialog, int id) { return IsDlgButtonChecked(dialog, id) == BST_CHECKED; }
static void enable(HWND dialog, int id, int on) { EnableWindow(GetDlgItem(dialog, id), on); }

/* ---- the tabs ---- */
/* All the settings are in one dialog; a tab shows its own controls and hides the rest.
 * resource.h numbers each tab's controls in its own hundred, from 1100. */
static const char *const tab_names[] = {"General", "Display", "Renderer", "Audio", "Controller", "Advanced",
                                        "Updates"};
#define TAB_COUNT ((int)(sizeof tab_names / sizeof tab_names[0]))

/* controls that this PC never shows (Windows 9x, Proton), whichever tab is chosen */
static int never_shown[32], never_shown_count;
static HFONT title_font;
static int current_page;  /* the tab shown, kept when the window comes back after the game */
static int returning;     /* the window is back after the game */

static int page_of(int id)
{
    return id >= IDC_PAGE_GENERAL && id < IDC_PAGE_GENERAL + TAB_COUNT * 100 ? (id - IDC_PAGE_GENERAL) / 100 : -1;
}

static void hide_for_good(HWND dialog, int id)
{
    if (never_shown_count < (int)(sizeof never_shown / sizeof never_shown[0]))
        never_shown[never_shown_count++] = id;
    ShowWindow(GetDlgItem(dialog, id), SW_HIDE);
}

static int hidden_by_choice(HWND dialog, int id);

static void show_page(HWND dialog, int page)
{
    HWND child;
    /* not before the window first appears: turning redrawing back on would show it early */
    int visible = IsWindowVisible(dialog);
    if (visible)
        SendMessageA(dialog, WM_SETREDRAW, FALSE, 0);
    for (child = GetWindow(dialog, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        int id = GetDlgCtrlID(child), i, show = page_of(id) == page;
        if (page_of(id) < 0)
            continue;
        for (i = 0; i < never_shown_count; i++)
            if (never_shown[i] == id)
                show = 0;
        if (show && hidden_by_choice(dialog, id))
            show = 0;
        ShowWindow(child, show ? SW_SHOWNA : SW_HIDE);
    }
    if (visible) {
        SendMessageA(dialog, WM_SETREDRAW, TRUE, 0);
        RedrawWindow(dialog, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
}

static void add_tabs(HWND dialog)
{
    HWND tabs = GetDlgItem(dialog, IDC_TABS);
    TCITEMA item;
    int i;
    for (i = 0; i < TAB_COUNT; i++) {
        memset(&item, 0, sizeof item);
        item.mask = TCIF_TEXT;
        item.pszText = (LPSTR)tab_names[i];
        SendMessageA(tabs, TCM_INSERTITEMA, (WPARAM)i, (LPARAM)&item);
    }
    SendMessageA(tabs, TCM_SETCURSEL, (WPARAM)current_page, 0);
    show_page(dialog, current_page);
}

/* the title on the General tab, in bold */
#define RELEASES_URL "https://github.com/reeshub/brrfix/releases"

/* The Updates tab's version line: the version and the day this launcher was built (the
 * compiler's __DATE__, "Nov 17 2026") */
static void show_version(HWND dialog)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    static const char *const names[] = {"January", "February", "March", "April", "May", "June", "July",
                                        "August", "September", "October", "November", "December"};
    const char *date = __DATE__;
    char text[128];
    int month;
    for (month = 0; month < 12; month++)
        if (!memcmp(months + month * 3, date, 3))
            break;
    if (month < 12)
        wsprintfA(text, "Big Red Launcher %s, built %d %s %s", BRRFIX_VERSION,
                  (date[4] == ' ' ? 0 : date[4] - '0') * 10 + date[5] - '0', names[month], date + 7);
    else
        wsprintfA(text, "Big Red Launcher %s, built %s", BRRFIX_VERSION, date);
    SetDlgItemTextA(dialog, IDC_VERSION, text);
}

static void set_title(HWND dialog)
{
    char text[96];
    LOGFONTA font;
    HFONT normal = (HFONT)SendMessageA(dialog, WM_GETFONT, 0, 0);
    wsprintfA(text, "Big Red Launcher %s by ctrl-alt-rees", BRRFIX_VERSION);
    SetDlgItemTextA(dialog, IDC_TITLE, text);
    if (normal && GetObjectA(normal, sizeof font, &font)) {
        font.lfWeight = FW_BOLD;
        title_font = CreateFontIndirectA(&font);
        if (title_font)
            SendDlgItemMessageA(dialog, IDC_TITLE, WM_SETFONT, (WPARAM)title_font, FALSE);
    }
}

/* Whether the Modern renderer is chosen (Windows 95, 98 and ME have only the original) */
static int new_renderer(HWND dialog)
{
    return !windows_9x && !lstrcmpiA(chosen(dialog, IDC_RENDERER, renderers), "new");
}

static int full_colour(HWND dialog)
{
    return new_renderer(dialog) && !lstrcmpiA(chosen(dialog, IDC_COLOUR, colour_depths), "1");
}

/* The director's renderer split (2026-11-15): the Original renderer offers only what the 1996
 * game and cnc-ddraw did (mode, resolution, the full-screen scaling, smoothness); the Modern
 * one its own window size and shape, colour depth (and with full colour the smoother
 * gradients) and draw distance. The colour animation is for both. */
static int hidden_by_choice(HWND dialog, int id)
{
    int fresh = new_renderer(dialog);
    switch (id) {
    case IDC_SCALE:
    case IDC_SCALE_LABEL:
    case IDC_MAXLIMIT:
    case IDC_MAXLIMIT_LABEL:
    case IDC_WIDESCREEN:
    case IDC_HUD43:
    case IDC_COLOUR:
    case IDC_COLOUR_LABEL:
    case IDC_DRAWDIST:
    case IDC_DRAWDIST_LABEL:
    case IDC_WORLD_GROUP:
    case IDC_TEXTURES:
    case IDC_TEXTURES_LABEL:
    case IDC_LIGHTING:
    case IDC_LIGHTING_LABEL:
    case IDC_DETAIL:
    case IDC_DETAIL_LABEL:
    case IDC_HUDSIZE:
    case IDC_HUDSIZE_LABEL:
    case IDC_WIDEMENUS:
    case IDC_GRADIENT_GROUP:
    case IDC_SMOOTH_SKY:
    case IDC_SMOOTH_SHADING:
    case IDC_FIXES_GROUP:
    case IDC_GROUND_PERSP:
    case IDC_SORT_FOOT:
    case IDC_CLOSE_GAPS:
    case IDC_HORIZON:
        return !fresh;
    }
    return 0;
}

/* a control's place, in dialog units */
static void place(HWND dialog, int id, int x, int y, int w, int h)
{
    RECT r;
    r.left = x;
    r.top = y;
    r.right = x + w;
    r.bottom = y + h;
    MapDialogRect(dialog, &r);
    SetWindowPos(GetDlgItem(dialog, id), NULL, r.left, r.top, r.right - r.left, r.bottom - r.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

/* The Display tab: the rows each renderer shows (on Windows 95, 98 and ME no Renderer row, nor
 * cnc-ddraw's scaling), 14 units apart, and the group as tall as them (launcher.rc's rules):
 * there just Mode and Resolution, as tall as Frame rate beside it */
static void layout_display(HWND dialog)
{
    int fresh = new_renderer(dialog), y = 36;
#define ROW(label, box, list) (place(dialog, label, 18, y + 2, 54, 8), place(dialog, box, 74, y, 88, list), y += 14)
    if (!windows_9x)
        ROW(IDC_RENDERER_LABEL, IDC_RENDERER, 60);
    ROW(IDC_MODE_LABEL, IDC_MODE, 60);
    ROW(IDC_RESOLUTION_LABEL, IDC_RESOLUTION, 60);
    if (fresh) {
        ROW(IDC_MAXLIMIT_LABEL, IDC_MAXLIMIT, 80);
        place(dialog, IDC_WIDESCREEN, 18, y, 144, 10);
        y += 14;
    }
    if (!windows_9x) {
        ROW(IDC_ASPECT_LABEL, IDC_ASPECT, 60);
        ROW(IDC_PRESET_LABEL, IDC_PRESET, 80);
    }
    if (fresh) {
        ROW(IDC_SCALE_LABEL, IDC_SCALE, 90);
        ROW(IDC_COLOUR_LABEL, IDC_COLOUR, 60);
    }
#undef ROW
    /* down to the last row's 12 units and 6 below them */
    place(dialog, IDC_DISPLAY_GROUP, 12, 24, 156, y - 14 + 12 + 6 - 24);
}

/* The Renderer tab: Smoothness and Full colour side by side, then Race world, Miscellaneous
 * and Engine fixes across (launcher.rc); with the Original renderer just Smoothness, and
 * Miscellaneous under it as wide */
static void layout_renderer(HWND dialog)
{
    if (new_renderer(dialog)) {
        place(dialog, IDC_MISC_GROUP, 12, 120, 300, 44);
        place(dialog, IDC_COLOUR_ANIM, 18, 132, 144, 10);
    } else {
        place(dialog, IDC_MISC_GROUP, 12, 72, 156, 30);
        place(dialog, IDC_COLOUR_ANIM, 18, 84, 144, 10);
    }
}

/* The tabs as tall as the tallest page this PC can show (its lowest control, 6 units above the
 * tabs' bottom, as at their sides), the controls under them and the window moved up to match:
 * on Windows 95, 98 and ME, whose pages are shorter, the window is too. With either renderer
 * possible, all of both renderers' controls count, so changing it never resizes the window.
 * Measured with GetWindowRect only (the game imports it, so Windows 95 has it): the window's
 * inside starts where the tabs' screen position less their place in launcher.rc says. */
#define TABS_X 6
#define TABS_Y 4
static void fit_window(HWND dialog)
{
    static const int below[] = {IDC_SKIP, IDC_DOSPATCH, IDC_HELPFILE, IDOK, IDC_SAVE, IDCANCEL};
    HWND tabs = GetDlgItem(dialog, IDC_TABS), child;
    RECT r, units = {TABS_X, TABS_Y, 0, 6};
    int lowest = 0, shift, i, origin_x, origin_y;
    for (child = GetWindow(dialog, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        int id = GetDlgCtrlID(child), skip = page_of(id) < 0 || (windows_9x && hidden_by_choice(dialog, id));
        for (i = 0; i < never_shown_count && !skip; i++)
            skip = never_shown[i] == id;
        if (skip)
            continue;
        GetWindowRect(child, &r);
        if (!lowest || r.bottom > lowest)
            lowest = r.bottom;
    }
    MapDialogRect(dialog, &units);
    GetWindowRect(tabs, &r);
    shift = r.bottom - (lowest + units.bottom);
    if (!lowest || shift <= 0)
        return;
    origin_x = r.left - units.left;
    origin_y = r.top - units.top;
    SetWindowPos(tabs, NULL, 0, 0, r.right - r.left, r.bottom - r.top - shift, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
    for (i = 0; i < (int)(sizeof below / sizeof below[0]); i++) {
        GetWindowRect(GetDlgItem(dialog, below[i]), &r);
        SetWindowPos(GetDlgItem(dialog, below[i]), NULL, r.left - origin_x, r.top - origin_y - shift, 0, 0,
                     SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    GetWindowRect(dialog, &r);  /* still centred (DS_CENTER placed it) */
    SetWindowPos(dialog, NULL, r.left, r.top + shift / 2, r.right - r.left, r.bottom - r.top - shift,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

/* The resolutions each renderer offers, keeping the choice where it can (Max -> High) */
static void fill_resolutions(HWND dialog, const char *value)
{
    int fresh = new_renderer(dialog);
    if (!fresh && !lstrcmpiA(value, "max"))
        value = "high";
    fill_first(dialog, IDC_RESOLUTION, resolutions, value, fresh ? -1 : ORIGINAL_RESOLUTIONS);
}

/* Native widescreen in effect: Max in full screen with Native widescreen ticked, on a screen
 * wider than 4:3. The pictures then have the screen's shape, so cnc-ddraw keeps it (save). */
static int native_widescreen(HWND dialog)
{
    return new_renderer(dialog) && !lstrcmpiA(chosen(dialog, IDC_MODE, modes), "fullscreen")
        && checked(dialog, IDC_WIDESCREEN) && !lstrcmpiA(chosen(dialog, IDC_RESOLUTION, resolutions), "max")
        && GetSystemMetrics(SM_CXSCREEN) * 3 > GetSystemMetrics(SM_CYSCREEN) * 4;
}

/* Grey out what the other choices make irrelevant */
static void update_enabled(HWND dialog)
{
    static const int music_controls[] = {IDC_VOLUME_LABEL, IDC_VOLUME, IDC_FOLDER_LABEL, IDC_FOLDER, IDC_BROWSE,
                                         IDC_TRACKS};
    static const int pad_controls[] = {IDC_PAD_L1, IDC_DEADZONE, IDC_PAD_L2, IDC_SENSITIVITY, IDC_PAD_L3, IDC_LAYOUT,
                                       IDC_VIBRATION, IDC_NITRO_RUMBLE};
    const char *mode = chosen(dialog, IDC_MODE, modes);
    int window = lstrcmpiA(mode, "fullscreen") != 0, full = lstrcmpiA(mode, "windowed") != 0;
    int fresh = new_renderer(dialog), offered = (int)SendDlgItemMessageA(dialog, IDC_RESOLUTION, CB_GETCOUNT, 0, 0);
    size_t i;
    if (offered != (fresh ? 3 : ORIGINAL_RESOLUTIONS))  /* the renderer has changed */
        fill_resolutions(dialog, chosen(dialog, IDC_RESOLUTION, resolutions));
    /* the texture distance, with a draw distance of the launcher's */
    {
        int own = lstrcmpiA(chosen(dialog, IDC_DRAWDIST, distances), "0") != 0;
        enable(dialog, IDC_TEXTURES_LABEL, own);
        enable(dialog, IDC_TEXTURES, own);
    }
    /* Max's limit and widescreen, for Max only */
    {
        int max = !lstrcmpiA(chosen(dialog, IDC_RESOLUTION, resolutions), "max");
        enable(dialog, IDC_MAXLIMIT_LABEL, max);
        enable(dialog, IDC_MAXLIMIT, max);
        enable(dialog, IDC_WIDESCREEN, max);
        /* the menus' backgrounds across the screen: for native widescreen only */
        enable(dialog, IDC_WIDEMENUS, max && full && checked(dialog, IDC_WIDESCREEN));
    }
    /* horizon culling counts on close gaps' exact edges */
    enable(dialog, IDC_HORIZON, checked(dialog, IDC_CLOSE_GAPS));
    /* the Modern renderer's window (cnc-ddraw's) */
    enable(dialog, IDC_SCALE_LABEL, window);
    enable(dialog, IDC_SCALE, window);
    /* cnc-ddraw's scaling presets: the Original renderer's full screen; the Modern renderer's
     * pictures in full screen and in its window. The boxes show the chosen renderer's own. */
    if (shown_fresh != fresh) {
        int shown = shown_preset(dialog);
        if (shown_fresh >= 0 && shown >= 0)
            preset_of[shown_fresh] = shown;
        show_preset(dialog, preset_of[fresh]);
        shown_fresh = fresh;
    }
    enable(dialog, IDC_PRESET_LABEL, full || fresh);
    enable(dialog, IDC_PRESET, full || fresh);
    /* native widescreen's pictures have the screen's shape already (the menus 4:3 inside) */
    enable(dialog, IDC_ASPECT_LABEL, (full || fresh) && !native_widescreen(dialog));
    enable(dialog, IDC_ASPECT, (full || fresh) && !native_widescreen(dialog));
    /* the gradients need full colour (Display tab) */
    enable(dialog, IDC_SMOOTH_SKY, full_colour(dialog));
    enable(dialog, IDC_SMOOTH_SHADING, full_colour(dialog));
    layout_display(dialog);
    layout_renderer(dialog);
    show_page(dialog, current_page);
    for (i = 0; i < sizeof music_controls / sizeof music_controls[0]; i++)
        enable(dialog, music_controls[i], checked(dialog, IDC_MUSIC));
    for (i = 0; i < sizeof pad_controls / sizeof pad_controls[0]; i++)
        enable(dialog, pad_controls[i], checked(dialog, IDC_PAD));
    /* the intensity of whichever vibration is on */
    {
        int shaking = checked(dialog, IDC_PAD) && (checked(dialog, IDC_VIBRATION) || checked(dialog, IDC_NITRO_RUMBLE));
        enable(dialog, IDC_INTENSITY_LABEL, shaking);
        enable(dialog, IDC_INTENSITY, shaking);
    }
    enable(dialog, IDC_MENU_ANIM_LABEL, checked(dialog, IDC_MENU_NAV));
    enable(dialog, IDC_MENU_ANIM, checked(dialog, IDC_MENU_NAV));
}

/* One line on each setting, shown when the mouse rests on it */
static const struct { int id; const char *text; } tips[] = {
    {IDC_RENDERER, "Modern: the picture made once at your screen's or window's size, sharp, in full colour if you "
                   "like. Original: the 1996 game's own drawing, as it was."},
    {IDC_MODE, "Full screen, or a window: Original, the game's own with its 1996 menu bar; Modern, Alt+Enter for "
               "full screen."},
    {IDC_RESOLUTION, "Low: 320x200, as the game starts. High: 640x480, its sharper mode. Max (Modern): races with "
                     "as many pixels as Max limit allows. Original: keypad + and - switch in the game."},
    {IDC_MAXLIMIT, "How many rows Max draws races with: a part of your screen's (or window's) height, enlarged "
                   "to fill it, or all of it. The game draws in software, so more rows cost frame rate."},
    {IDC_WIDESCREEN, "With Max, in full screen on a wide screen: races at the screen's own shape, seeing more at the "
                     "sides; the menus 4:3 in the middle (their backgrounds across the screen, as below)."},
    {IDC_WIDEMENUS, "With native widescreen: each menu's background picture across the whole screen, sharp, "
                    "its top (or top and bottom) cropped; off: the menus 4:3 with black bars."},
    {IDC_GROUND_PERSP, "Ground textures drawn the same way at every distance: the game switched method 4 squares away, "
                       "so banks beside the road twitched as you came near."},
    {IDC_SORT_FOOT, "Objects put in drawing order by their footprint, not their height, so trees and buildings stop "
                    "showing through the hills in front of them."},
    {IDC_CLOSE_GAPS, "Pieces the level designers placed a hair apart (Italy's tunnels, the start's billboards) drawn "
                     "together, and polygon edges worked out exactly: fewer slits of sky."},
    {IDC_HORIZON, "Experimental: skips drawing ground and objects that nearer hills will cover anyway. The same "
                  "picture; so far it saves about as much time as it takes. Needs Close gaps."},
    {IDC_HUD43, "The race dashboard's pixels 1.2 times taller than wide, as the 320x200 art looked on a 1996 4:3 "
                "monitor; off: square."},
    {IDC_SCALE, "How big the Modern renderer's window is, in whole multiples of 640x480; the picture is "
                "scaled into it as Scaling says."},
    {IDC_COLOUR, "256 colours, as the game drew them, or full colour, for the smoother gradients (Renderer tab)."},
    {IDC_SMOOTH_SKY, "The sky as a smooth gradient instead of the game's 16 flat bands (with full colour: Display "
                     "tab, Colour depth)."},
    {IDC_SMOOTH_SHADING, "Lit surfaces (buildings, trees, vehicles, tunnels) shaded smoothly, not a palette shade at "
                         "a time (with full colour: Display tab, Colour depth)."},
    {IDC_COLOUR_ANIM, "The colour cycling of four levels (CHOPPER3, CITY4 Italy, MOON2, RIG4) that the DOS version "
                      "had and the Windows version lost."},
    {IDC_ASPECT, "The shape cnc-ddraw shows the picture at: 4:3, as a 1996 monitor showed the game's 320x200, "
                 "or square pixels. Native widescreen pictures keep the screen's shape either way."},
    {IDC_PRESET, "How cnc-ddraw enlarges the picture to fill the screen's (or window's) height: Sharp, Nearest "
                 "(plain pixels), Smooth, Integer (whole numbers, black borders) or Stretch (the whole screen)."},
    {IDC_INTERP, "Draws the vehicles and the camera between the game's 35 physics steps a second, for smooth motion."},
    {IDC_SMOOTHING, "Experimental: softens the camera's movement further. Not needed for the stutter fix."},
    {IDC_FRAMES, "The most pictures drawn a second. The game itself runs at the same speed whatever you choose."},
    {IDC_VSYNC, "Waits for the screen's refresh before each picture: no tearing, slightly more input lag."},
    {IDC_DISTANCE_VOL, "Other vehicles' sounds fade with distance, as the game meant (on Windows every sound "
                       "played almost at full volume); also gives the game's effects volume its full range."},
    {IDC_STEREO_PAN, "Vehicles to the right sound more from the right speaker, smoothly; the game's own panning sent "
                     "anything a little off centre almost entirely to one side."},
    {IDC_CHANNELS, "How many sounds play at once: the game's 8, or 16 or 32, with every truck's engine (the game "
                   "allowed 3), so more of the field is heard at once. 8 on Windows 95, 98 and ME, where each "
                   "costs a 1990s PC some speed."},
    {IDC_MUSIC, "Plays the CD music from ripped tracks (trackNN.flac, .ogg or .wav) instead of the CD."},
    {IDC_VOLUME, "Music volume, on top of the game's own CD volume option."},
    {IDC_FOLDER, "The folder with the ripped tracks: Music next to the launcher, or a full path."},
    {IDC_BROWSE, "Choose the folder with the ripped tracks."},
    {IDC_PAD, "Use an Xbox-style pad as the game's joystick (choose the joystick in the game's controls menu)."},
    {IDC_JOYCPL, "Opens Windows' game controller settings, to set up and calibrate a joystick or pad."},
    {IDC_LANGUAGE, "The game's language, as in its window's Language menu."},
    {IDC_DEADZONE, "How far the stick moves before steering starts."},
    {IDC_SENSITIVITY, "How much stick gives full lock: above 100% reaches it sooner."},
    {IDC_LAYOUT, "Triggers: right accelerates, left brakes. Stick: push up and down, as the original."},
    {IDC_VIBRATION, "A brief buzz from the pad when player one's vehicle is damaged (with the game's damage option on)."},
    {IDC_NITRO_RUMBLE, "A steady rumble from the pad while player one's nitro flames show."},
    {IDC_INTENSITY, "How strong the damage buzz is; the nitro rumble is half as strong."},
    {IDC_MENU_NAV, "Run the menus with a joystick, a pad or the arrow keys: up and down step through the items (the "
                   "pointer hides), button 1 (A) or Return chooses, button 2 (B) goes back. The joystick also sets player one to JS1."},
    {IDC_MENU_ANIM, "Original-ish: the selected item stops animating. Inverted: everything else stops, and the selected item animates."},
    {IDC_RATE, "Game ticks a second: 70.0 as the DOS version (real time), or 71.43 as the 1996 Windows version ran (2% fast)."},
    {IDC_CAP, "After a pause, how much missed game time is caught up at once: 8 ticks, or 35 as the original (see Help)."},
    {IDC_LOG, "Writes BRRFIX.LOG next to the launcher: what was fixed, for bug reports."},
    {IDC_RELEASES, "Opens github.com/reeshub/brrfix/releases in your web browser, to see if there's a newer version."},
    {IDC_DRAWDIST, "How far the ground, scenery and other vehicles are drawn: the options menu's sliders, or 25, "
                   "44 or 60 squares (the most the engine's numbers allow). Further costs frame rate."},
    {IDC_TEXTURES, "How far the ground is textured with a draw distance chosen here: as far as it's drawn, 25 "
                   "squares, or the options menu's slider. Beyond, it's plain shaded colour, which is quicker."},
    {IDC_LIGHTING, "How trees, buildings and vehicles are lit. Smooth: from their corners, everywhere. Original: "
                   "smooth near, flat beyond 8 squares (they change as they pass). Flat: by face, everywhere."},
    {IDC_DETAIL, "Which of a model's versions is drawn. Highest: always the most detailed. Matched: picked by its size "
                 "on screen, so fences and barriers don't change shape close up. Original: by distance, for the 1996 "
                 "picture."},
    {IDC_HUDSIZE, "The race dashboard's size: Small (the default, about half) or Large, as big as the 1996 "
                  "low-resolution picture showed it."},
    {IDC_PERIOD, "Every setting for a 1990s PC: full screen at 320x200, the original renderer (press Save or Play "
                 "to keep them)."},
    {IDC_MODERN, "Every setting for a modern PC: full screen, races at the screen's resolution (Max), drawn as far as the "
                 "engine allows, the Modern renderer in full colour, modern pads (press Save or Play to keep them)."},
    {IDC_SKIP, "Start the game straight away next time. Hold Shift while starting to see this window again."},
    {IDC_DOSPATCH, "Fixes the DOS version's RACING.EXE for DOS PCs and DOSBox, keeping the original as RACING.ORI."},
    {IDC_HELPFILE, "Opens the help file, which explains every setting (F1)."},
    {IDOK, "Saves the settings and starts the game."},
    {IDC_SAVE, "Saves the settings without starting the game."},
    {IDCANCEL, "Closes this window without saving."}};

#ifndef TTTOOLINFOA_V1_SIZE
#define TTTOOLINFOA_V1_SIZE offsetof(TOOLINFOA, lParam)
#endif

static void add_tips(HWND dialog)
{
    HWND tip = CreateWindowExA(0, TOOLTIPS_CLASSA, NULL, WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
                               CW_USEDEFAULT, CW_USEDEFAULT, dialog, NULL, GetModuleHandleA(NULL), NULL);
    TOOLINFOA info;
    size_t i;
    if (!tip)
        return;
    SendMessageA(tip, TTM_SETMAXTIPWIDTH, 0, 360);
    for (i = 0; i < sizeof tips / sizeof tips[0]; i++) {
        memset(&info, 0, sizeof info);
        info.cbSize = TTTOOLINFOA_V1_SIZE;  /* the size Windows 95's first common controls accept */
        info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        info.hwnd = dialog;
        info.uId = (UINT)(DWORD)GetDlgItem(dialog, tips[i].id);
        info.lpszText = (LPSTR)tips[i].text;
        SendMessageA(tip, TTM_ADDTOOLA, 0, (LPARAM)&info);
    }
}

static void open_help(HWND dialog)
{
    char path[MAX_PATH];
    wsprintfA(path, "%s\\%s", folder, HELP_FILE);
    if ((DWORD)ShellExecuteA(dialog, "open", path, NULL, folder, SW_SHOWNORMAL) <= 32)
        MessageBoxA(dialog, "The help file (" HELP_FILE ") isn't next to the launcher.", "Big Red Launcher",
                    MB_OK | MB_ICONINFORMATION);
}

/* a folder setting: a full path, or a folder next to the launcher */
static void folder_from(const char *setting, const char *fallback, char *out)
{
    if (setting[0] && (setting[1] == ':' || setting[0] == '\\'))
        lstrcpynA(out, setting, MAX_PATH);
    else
        wsprintfA(out, "%s\\%s", folder, setting[0] ? setting : fallback);
}

static void music_folder(const char *setting, char *out) { folder_from(setting, "Music", out); }

static void show_track_count(HWND dialog)
{
    char setting[MAX_PATH], path[MAX_PATH], text[96];
    int count;
    GetDlgItemTextA(dialog, IDC_FOLDER, setting, sizeof setting);
    music_folder(setting, path);
    count = brr_count_tracks(path);
    if (count)
        wsprintfA(text, "%d tracks found.", count);
    else
        lstrcpyA(text, "No tracks found (trackNN.flac, .ogg or .wav).");
    SetDlgItemTextA(dialog, IDC_TRACKS, text);
}

/* Put the settings into the window: from the files, or one of the two sets of defaults (SPEC:
 * period for 1990s PCs, which can't draw the 640x480 3D quickly, and modern for the rest) */
enum { FROM_FILES, MODERN_DEFAULTS, PERIOD_DEFAULTS };

static void load(HWND dialog, int defaults)
{
    char v[MAX_PATH];
    const char *f = defaults ? NULL : brrfix_ini, *d = defaults ? NULL : ddraw_ini;
    int period = defaults == PERIOD_DEFAULTS;
#define GET(file, section, key, fallback) (ini_get(file, section, key, fallback, v, sizeof v), v)
    fill(dialog, IDC_MODE, modes, GET(f, "display", "mode", "fullscreen"));
    fill(dialog, IDC_RENDERER, renderers, GET(f, "display", "renderer", period ? "original" : "new"));
    {
        char resolution[16];
        ini_get(f, "display", "resolution", period ? "low" : "high", resolution, sizeof resolution);
        /* Max is the Modern renderer's default (director, 2026-11-15) */
        if (!lstrcmpiA(resolution, "high")
            && !lstrcmpiA(GET(f, "display", "render_resolution", period ? "original" : "max"), "max"))
            lstrcpyA(resolution, "max");
        fill_resolutions(dialog, resolution);
    }
    fill(dialog, IDC_LANGUAGE, languages, GET(f, "display", "language", "ENG"));
    fill(dialog, IDC_SCALE, scales, GET(f, "display", "window_scale", "auto"));
    fill(dialog, IDC_MAXLIMIT, max_limits, listed(max_limits, GET(f, "display", "render_max_height", "1/2"), "1/2"));
    {   /* the full-screen preset: as saved in BRRFIX.INI, or else whichever ddraw.ini matches,
         * by its boxing, maintas, aspect_ratio and shader */
        char boxing[16], aspect[16], shader[MAX_PATH], maintas[16];
        int i, found = 0;
        ini_get(f, "display", "fullscreen_scaling", "", v, sizeof v);
        found = preset_index(v, PRESET_ORIGINAL);
        if (!v[0] && d) {  /* a BRRFIX.INI from before the setting: ddraw.ini's keys say which */
            ini_get(d, "ddraw", "boxing", "true", boxing, sizeof boxing);
            ini_get(d, "ddraw", "maintas", "true", maintas, sizeof maintas);
            ini_get(d, "ddraw", "aspect_ratio", "", aspect, sizeof aspect);
            ini_get(d, "ddraw", "shader", SHADER_NEAREST, shader, sizeof shader);
            for (i = 0; i < PRESET_COUNT; i++)
                if (!lstrcmpiA(boxing, presets[i].keys[0]) && !lstrcmpiA(maintas, presets[i].keys[1])
                    && !lstrcmpiA(aspect, presets[i].keys[2]) && !lstrcmpiA(shader, presets[i].keys[3]))
                    found = i;
        }
        preset_of[0] = found;
        /* Square, Sharp: the Modern renderer's default (director, 2026-11-16) */
        preset_of[1] = preset_index(GET(f, "display", "modern_scaling", PRESET_MODERN), PRESET_MODERN);
        shown_fresh = -1;  /* update_enabled fills the boxes with the chosen renderer's */
    }
    check(dialog, IDC_INTERP, defaults ? 1 : ini_flag("fixes", "interpolation", 1));
    fill(dialog, IDC_SMOOTHING, smoothings, GET(f, "camera", "smoothing", "0"));
    fill(dialog, IDC_FRAMES, frame_limits, GET(f, "clock", "frame_limit", "auto"));
    if (windows_9x || defaults)
        check(dialog, IDC_VSYNC, defaults ? 0 : ini_flag("clock", "vsync", 0));
    else {  /* with cnc-ddraw, vsync is its setting */
        GET(d, "ddraw", "vsync", "false");
        check(dialog, IDC_VSYNC, !lstrcmpiA(v, "true"));
    }
    check(dialog, IDC_MUSIC, defaults ? 1 : ini_flag("music", "music", 1));
    check(dialog, IDC_DISTANCE_VOL, defaults ? 1 : ini_flag("sound", "distance_volume", 1));
    check(dialog, IDC_STEREO_PAN, defaults ? 1 : ini_flag("sound", "stereo_pan", 1));
    /* auto (BRRFIX.INI's default): the game's 8 on Windows 95, 98 and ME, where each sound playing
     * costs a 1990s processor time (the director, 2026-11-22), and with Period defaults; else 32 */
    fill(dialog, IDC_CHANNELS, channel_counts,
         listed(channel_counts, GET(f, "sound", "channels", "auto"), period || (windows_9x && !defaults) ? "8" : "32"));
    fill(dialog, IDC_VOLUME, volumes, GET(f, "music", "volume", "100"));
    SetDlgItemTextA(dialog, IDC_FOLDER, GET(f, "music", "folder", "Music"));
    check(dialog, IDC_PAD, defaults ? 1 : ini_flag("controller", "enabled", 1));
    fill(dialog, IDC_DEADZONE, deadzones, GET(f, "controller", "deadzone", "15"));
    fill(dialog, IDC_SENSITIVITY, sensitivities, GET(f, "controller", "sensitivity", "100"));
    fill(dialog, IDC_LAYOUT, layouts, GET(f, "controller", "layout", "triggers"));
    check(dialog, IDC_VIBRATION, defaults ? 1 : ini_flag("controller", "vibration", 1));
    check(dialog, IDC_NITRO_RUMBLE, defaults ? 1 : ini_flag("controller", "nitro_rumble", 1));
    check(dialog, IDC_MENU_NAV, defaults ? 1 : ini_flag("controller", "menu_navigation", 1));
    fill(dialog, IDC_MENU_ANIM, menu_animations, GET(f, "controller", "menu_animations", "original"));
    fill(dialog, IDC_INTENSITY, intensities, GET(f, "controller", "vibration_intensity", "80"));
    fill(dialog, IDC_RATE, rates, GET(f, "clock", "tick_rate", "70.0"));
    fill(dialog, IDC_CAP, caps, GET(f, "clock", "catch_up_cap", "8"));
    check(dialog, IDC_LOG, defaults ? 0 : ini_flag("log", "log", 0));
    GET(f, "display", "max_draw_distance", period ? "0" : "60");  /* Max out: the Modern default (director, 2026-11-16) */
    fill(dialog, IDC_DRAWDIST, distances, !lstrcmpiA(v, "1") ? "25" : v);  /* 1: the first experiment's on */
    fill(dialog, IDC_TEXTURES, texture_distances, GET(f, "display", "texture_distance", "draw"));
    fill(dialog, IDC_LIGHTING, lightings, GET(f, "display", "object_shading", "flat"));  /* the director, 2026-11-16 */
    fill(dialog, IDC_DETAIL, details, GET(f, "display", "object_detail", "highest"));  /* the director, 2026-11-16 */
    fill(dialog, IDC_HUDSIZE, hud_sizes, GET(f, "display", "hud_size", "small"));
    fill(dialog, IDC_COLOUR, colour_depths, defaults ? "1" : (ini_flag("display", "full_colour", 1) ? "1" : "0"));
    check(dialog, IDC_SMOOTH_SKY, defaults ? 1 : ini_flag("display", "smooth_sky", 1));
    check(dialog, IDC_SMOOTH_SHADING, defaults ? 1 : ini_flag("display", "smooth_shading", 1));
    check(dialog, IDC_COLOUR_ANIM, defaults ? 1 : ini_flag("display", "colour_animation", 1));
    check(dialog, IDC_WIDESCREEN, defaults ? 1 : ini_flag("display", "widescreen", 1));
    check(dialog, IDC_HUD43, defaults ? 0 : ini_flag("display", "hud_aspect43", 0));
    check(dialog, IDC_WIDEMENUS, defaults ? 1 : ini_flag("display", "widescreen_menus", 1));
    check(dialog, IDC_GROUND_PERSP, defaults ? 1 : ini_flag("display", "ground_perspective", 1));
    check(dialog, IDC_SORT_FOOT, defaults ? 1 : ini_flag("display", "sort_footprint", 1));
    check(dialog, IDC_CLOSE_GAPS, defaults ? 1 : ini_flag("display", "close_gaps", 1));
    check(dialog, IDC_HORIZON, defaults ? 0 : ini_flag("display", "horizon_culling", 0));
    if (!defaults)
        check(dialog, IDC_SKIP, ini_flag("launcher", "skip", 0));
#undef GET
    show_track_count(dialog);
    update_enabled(dialog);
}

static void save(HWND dialog)
{
    char text[MAX_PATH];
    const char *mode = chosen(dialog, IDC_MODE, modes);
    int preset = shown_preset(dialog), i, fresh_now = new_renderer(dialog);
    static const char *const preset_names[5] = {"boxing", "maintas", "aspect_ratio", "shader", "d3d9_filter"};
    ini_set(brrfix_ini, "display", "mode", mode);
    if (!windows_9x)  /* there the DLL keeps the original renderer whatever the file says */
        ini_set(brrfix_ini, "display", "renderer", chosen(dialog, IDC_RENDERER, renderers));
    {
        const char *resolution = chosen(dialog, IDC_RESOLUTION, resolutions);
        int max = !lstrcmpiA(resolution, "max");
        ini_set(brrfix_ini, "display", "resolution", max ? "high" : resolution);
        if (!windows_9x)
            ini_set(brrfix_ini, "display", "render_resolution", max ? "max" : "original");
    }
    ini_set(brrfix_ini, "display", "language", chosen(dialog, IDC_LANGUAGE, languages));
    if (preset >= 0 && preset < PRESET_COUNT)
        preset_of[fresh_now] = preset;
    preset = preset_of[fresh_now];
    ini_set(brrfix_ini, "display", "fullscreen_scaling", presets[preset_of[0]].value);
    if (!windows_9x)
        ini_set(brrfix_ini, "display", "modern_scaling", presets[preset_of[1]].value);
    ini_set(brrfix_ini, "display", "window_scale", chosen(dialog, IDC_SCALE, scales));
    ini_set(brrfix_ini, "display", "render_max_height", chosen(dialog, IDC_MAXLIMIT, max_limits));
    ini_set(brrfix_ini, "fixes", "interpolation", checked(dialog, IDC_INTERP) ? "1" : "0");
    ini_set(brrfix_ini, "camera", "smoothing", chosen(dialog, IDC_SMOOTHING, smoothings));
    ini_set(brrfix_ini, "clock", "frame_limit", chosen(dialog, IDC_FRAMES, frame_limits));
    ini_set(brrfix_ini, "clock", "tick_rate", chosen(dialog, IDC_RATE, rates));
    ini_set(brrfix_ini, "clock", "catch_up_cap", chosen(dialog, IDC_CAP, caps));
    ini_set(brrfix_ini, "music", "music", checked(dialog, IDC_MUSIC) ? "1" : "0");
    ini_set(brrfix_ini, "music", "volume", chosen(dialog, IDC_VOLUME, volumes));
    GetDlgItemTextA(dialog, IDC_FOLDER, text, sizeof text);
    ini_set(brrfix_ini, "music", "folder", text[0] ? text : "Music");
    ini_set(brrfix_ini, "controller", "enabled", checked(dialog, IDC_PAD) ? "1" : "0");
    ini_set(brrfix_ini, "controller", "deadzone", chosen(dialog, IDC_DEADZONE, deadzones));
    ini_set(brrfix_ini, "controller", "sensitivity", chosen(dialog, IDC_SENSITIVITY, sensitivities));
    ini_set(brrfix_ini, "controller", "layout", chosen(dialog, IDC_LAYOUT, layouts));
    ini_set(brrfix_ini, "controller", "vibration", checked(dialog, IDC_VIBRATION) ? "1" : "0");
    ini_set(brrfix_ini, "controller", "nitro_rumble", checked(dialog, IDC_NITRO_RUMBLE) ? "1" : "0");
    ini_set(brrfix_ini, "controller", "menu_navigation", checked(dialog, IDC_MENU_NAV) ? "1" : "0");
    ini_set(brrfix_ini, "controller", "menu_animations", chosen(dialog, IDC_MENU_ANIM, menu_animations));
    ini_set(brrfix_ini, "controller", "vibration_intensity", chosen(dialog, IDC_INTENSITY, intensities));
    ini_set(brrfix_ini, "log", "log", checked(dialog, IDC_LOG) ? "1" : "0");
    ini_set(brrfix_ini, "display", "max_draw_distance", chosen(dialog, IDC_DRAWDIST, distances));
    ini_set(brrfix_ini, "display", "texture_distance", chosen(dialog, IDC_TEXTURES, texture_distances));
    ini_set(brrfix_ini, "display", "object_shading", chosen(dialog, IDC_LIGHTING, lightings));
    ini_set(brrfix_ini, "display", "object_detail", chosen(dialog, IDC_DETAIL, details));
    ini_set(brrfix_ini, "display", "hud_size", chosen(dialog, IDC_HUDSIZE, hud_sizes));
    ini_set(brrfix_ini, "display", "full_colour", chosen(dialog, IDC_COLOUR, colour_depths));
    ini_set(brrfix_ini, "display", "smooth_sky", checked(dialog, IDC_SMOOTH_SKY) ? "1" : "0");
    ini_set(brrfix_ini, "display", "smooth_shading", checked(dialog, IDC_SMOOTH_SHADING) ? "1" : "0");
    ini_set(brrfix_ini, "display", "colour_animation", checked(dialog, IDC_COLOUR_ANIM) ? "1" : "0");
    ini_set(brrfix_ini, "display", "widescreen", checked(dialog, IDC_WIDESCREEN) ? "1" : "0");
    ini_set(brrfix_ini, "display", "hud_aspect43", checked(dialog, IDC_HUD43) ? "1" : "0");
    ini_set(brrfix_ini, "display", "widescreen_menus", checked(dialog, IDC_WIDEMENUS) ? "1" : "0");
    ini_set(brrfix_ini, "display", "ground_perspective", checked(dialog, IDC_GROUND_PERSP) ? "1" : "0");
    ini_set(brrfix_ini, "display", "sort_footprint", checked(dialog, IDC_SORT_FOOT) ? "1" : "0");
    ini_set(brrfix_ini, "display", "close_gaps", checked(dialog, IDC_CLOSE_GAPS) ? "1" : "0");
    ini_set(brrfix_ini, "display", "horizon_culling", checked(dialog, IDC_HORIZON) ? "1" : "0");
    ini_set(brrfix_ini, "sound", "distance_volume", checked(dialog, IDC_DISTANCE_VOL) ? "1" : "0");
    ini_set(brrfix_ini, "sound", "stereo_pan", checked(dialog, IDC_STEREO_PAN) ? "1" : "0");
    ini_set(brrfix_ini, "sound", "channels", chosen(dialog, IDC_CHANNELS, channel_counts));
    ini_set(brrfix_ini, "launcher", "skip", checked(dialog, IDC_SKIP) ? "1" : "0");
    if (windows_9x)
        ini_set(brrfix_ini, "clock", "vsync", checked(dialog, IDC_VSYNC) ? "1" : "0");
    else {
        /* cnc-ddraw: the window mode follows ours, and the full-screen preset */
        int full = !lstrcmpiA(mode, "fullscreen"), fresh = new_renderer(dialog);
        ini_set(ddraw_ini, "ddraw", "windowed", full ? "false" : "true");
        /* The Original renderer's window is the game's own: the drop-in draws and scales the
         * picture and the mouse itself, so cnc-ddraw mustn't lock the pointer to its 320x200
         * idea of the window, scale the mouse again, or hook the window. Full screen, and the
         * Modern renderer's window (the game runs full screen; cnc-ddraw makes the window,
         * director 2026-11-15), are cnc-ddraw's job, with hook=3: what it chose itself from its
         * default hook=4 while WINRACEP.EXE imported it as ddraw.dll. */
        ini_set(ddraw_ini, "ddraw", "hook", full || fresh ? "3" : "0");
        ini_set(ddraw_ini, "ddraw", "adjmouse", full || fresh ? "true" : "false");
        ini_set(ddraw_ini, "ddraw", "devmode", full || fresh ? "false" : "true");
        /* at the desktop's own resolution: otherwise Windows switches to the game's (blurry) */
        ini_set(ddraw_ini, "ddraw", "fullscreen", full ? "true" : "false");
        /* Alt+Enter: the Modern renderer's window <-> full screen upscaled at the desktop's own
         * resolution, not a change of the screen's mode */
        ini_set(ddraw_ini, "ddraw", "toggle_upscaled", fresh ? "true" : "false");
        ini_set(ddraw_ini, "ddraw", "vsync", checked(dialog, IDC_VSYNC) ? "true" : "false");
        /* The Modern renderer hands cnc-ddraw its pictures at their own size (the menus' 320x200,
         * races' own), which the chosen preset enlarges, in full screen and in the window (whose
         * size, a multiple of 640x480, is width and height; 0 in full screen). The Original
         * renderer's window: no letterbox, as under Wine cnc-ddraw otherwise moves the mouse by
         * the borders it would have drawn (playtest 2026-11-10: 60 pixels at 960x720). */
        {
            char number[16];
            int scale = 0, w = 0, h = 0;
            if (fresh && !full) {
                const char *chosen_scale = chosen(dialog, IDC_SCALE, scales);
                scale = lstrcmpiA(chosen_scale, "auto") ? chosen_scale[0] - '0' : 0;
                if (scale <= 0) {  /* the largest that fits 85% of the desktop's work area */
                    RECT work;
                    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
                    for (scale = 8; scale > 1; scale--)
                        if (640 * scale <= (work.right - work.left) * 85 / 100
                            && 480 * scale <= (work.bottom - work.top) * 85 / 100)
                            break;
                }
                w = 640 * scale;
                h = 480 * scale;
            }
            wsprintfA(number, "%d", w);
            ini_set(ddraw_ini, "ddraw", "width", number);
            wsprintfA(number, "%d", h);
            ini_set(ddraw_ini, "ddraw", "height", number);
        }
        if (!fresh && !full)
            preset = preset_index(PRESET_WINDOW, NULL);
        for (i = 0; i < 5; i++)
            ini_set(ddraw_ini, "ddraw", preset_names[i], presets[preset].keys[i]);
        /* Native widescreen (Max, full screen, a screen wider than 4:3): the pictures already
         * have the screen's shape, the menus 4:3 inside them, so cnc-ddraw keeps their shape */
        if (native_widescreen(dialog))
            ini_set(ddraw_ini, "ddraw", "aspect_ratio", "");
    }
}

static void browse(HWND dialog)
{
    char path[MAX_PATH];
    BROWSEINFOA info;
    LPITEMIDLIST chosen_item;
    memset(&info, 0, sizeof info);
    info.hwndOwner = dialog;
    info.pszDisplayName = path;
    info.lpszTitle = "The folder with the ripped CD tracks (trackNN.flac, .ogg or .wav):";
    info.ulFlags = BIF_RETURNONLYFSDIRS;
    chosen_item = SHBrowseForFolderA(&info);
    if (chosen_item && SHGetPathFromIDListA(chosen_item, path)) {
        SetDlgItemTextA(dialog, IDC_FOLDER, path);
        show_track_count(dialog);
    }
}

/* Patch the DOS version's RACING.EXE, or put the original back. The DOS and Windows
 * versions install into the same folder, so look in the game's folder first. */
static void patch_dos(HWND dialog)
{
    static const char title[] = "Big Red Racing: the DOS version";
    char dos_folder[MAX_PATH], text[MAX_PATH + 400], message[512];
    int state, result;
    HCURSOR cursor;
    lstrcpynA(dos_folder, game_folder, MAX_PATH);
    state = brr_dos_state(dos_folder);
    if (state == BRR_DOS_MISSING) {
        BROWSEINFOA info;
        LPITEMIDLIST item;
        char name[MAX_PATH];
        memset(&info, 0, sizeof info);
        info.hwndOwner = dialog;
        info.pszDisplayName = name;
        info.lpszTitle = "The DOS version's folder, with RACING.EXE in it (for example C:\\BIGRED):";
        info.ulFlags = BIF_RETURNONLYFSDIRS;
        item = SHBrowseForFolderA(&info);
        if (!item || !SHGetPathFromIDListA(item, dos_folder))
            return;
        state = brr_dos_state(dos_folder);
    }
    if (state == BRR_DOS_ORIGINAL) {
        wsprintfA(text, "Patch the DOS version's RACING.EXE in %s?\n\nThis fixes the camera stutter and smooths "
                  "the picture on fast PCs (patch %s); the physics is unchanged. The original is kept as "
                  "RACING.ORI, and this button can put it back.", dos_folder, brr_dos_version());
        if (MessageBoxA(dialog, text, title, MB_YESNO | MB_ICONQUESTION) != IDYES)
            return;
    } else if (state == BRR_DOS_PATCHED) {
        wsprintfA(text, "The DOS version's RACING.EXE in %s is already patched.\n\nPut the original back "
                  "(from RACING.ORI)?", dos_folder);
        if (MessageBoxA(dialog, text, title, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
            return;
    } else if (state == BRR_DOS_EARLIER) {
        wsprintfA(text, "The DOS version's RACING.EXE in %s has an earlier version of this patch.\n\n"
                  "Update it to patch %s? The original stays in RACING.ORI, and this button can put "
                  "it back.", dos_folder, brr_dos_version());
        if (MessageBoxA(dialog, text, title, MB_YESNO | MB_ICONQUESTION) != IDYES)
            return;
    }
    cursor = SetCursor(LoadCursorA(NULL, IDC_WAIT));
    result = state == BRR_DOS_PATCHED ? brr_dos_restore(dos_folder, message, sizeof message)
                                      : brr_dos_patch(dos_folder, NULL, message, sizeof message);
    SetCursor(cursor);
    if (result == 0 && state != BRR_DOS_PATCHED)
        lstrcatA(message, "\n\nStart the DOS version as usual, with RACING. To undo, press this button again.");
    MessageBoxA(dialog, message, title, MB_OK | (result < 0 ? MB_ICONERROR : MB_ICONINFORMATION));
}

static BOOL CALLBACK settings_proc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    if (message == WM_INITDIALOG) {
        static const int pad_controls[] = {IDC_PAD, IDC_DEADZONE, IDC_SENSITIVITY, IDC_LAYOUT,
                                           IDC_PAD_L1, IDC_PAD_L2, IDC_PAD_L3, IDC_VIBRATION, IDC_NITRO_RUMBLE,
                                           IDC_VIB_GROUP, IDC_INTENSITY_LABEL, IDC_INTENSITY};
        char title[96], steam[8];
        size_t i;
        never_shown_count = 0;  /* a new window each time (it comes back after the game) */
        wsprintfA(title, "Big Red Launcher %s by ctrl-alt-rees", BRRFIX_VERSION);
        SetWindowTextA(dialog, title);
        SendMessageA(dialog, WM_SETICON, ICON_BIG, (LPARAM)LoadIconA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(IDI_APP)));
        set_title(dialog);
        show_version(dialog);
        load(dialog, FROM_FILES);
        add_tips(dialog);
        if (windows_9x) {  /* no cnc-ddraw (DirectDraw does full screen itself), no XInput, no new renderer */
            hide_for_good(dialog, IDC_RENDERER);
            hide_for_good(dialog, IDC_RENDERER_LABEL);
            hide_for_good(dialog, IDC_PRESET);
            hide_for_good(dialog, IDC_PRESET_LABEL);
            hide_for_good(dialog, IDC_ASPECT);
            hide_for_good(dialog, IDC_ASPECT_LABEL);
            for (i = 0; i < sizeof pad_controls / sizeof pad_controls[0]; i++)
                hide_for_good(dialog, pad_controls[i]);
            /* Modern defaults are for what this Windows can't run (the director, 2026-11-22) */
            hide_for_good(dialog, IDC_MODERN);
            hide_for_good(dialog, IDC_MODERN_TEXT);
            SetDlgItemTextA(dialog, IDC_DEFAULTS_NOTE, "It sets every setting; press Save or Play to keep them.");
            place(dialog, IDC_DEFAULTS_NOTE, 18, 128, 288, 8);
            place(dialog, IDC_DEFAULTS_GROUP, 12, 94, 300, 48);
            /* the note in a group of two rows; menu navigation works with a 1996 joystick too,
             * so its group goes under it */
            place(dialog, IDC_PAD_GROUP, 12, 24, 300, 44);
            place(dialog, IDC_MENU_GROUP, 12, 72, 300, 44);
            place(dialog, IDC_MENU_NAV, 18, 84, 200, 10);
            place(dialog, IDC_MENU_ANIM_LABEL, 18, 100, 60, 8);
            place(dialog, IDC_MENU_ANIM, 80, 98, 70, 60);
        } else
            hide_for_good(dialog, IDC_PAD_NOTE);
        /* under Proton, Steam Input sets up controllers */
        if (GetEnvironmentVariableA("STEAM_COMPAT_DATA_PATH", steam, sizeof steam))
            hide_for_good(dialog, IDC_JOYCPL);
        fit_window(dialog);
        add_tabs(dialog);
        /* the tabs have the focus, so the arrow keys change tab and Return still plays */
        SetFocus(GetDlgItem(dialog, IDC_TABS));
        if (returning)  /* in front again once the game has gone */
            SetForegroundWindow(dialog);
        return FALSE;
    }
    if (message == WM_NOTIFY) {
        const NMHDR *header = (const NMHDR *)lparam;
        if (header->idFrom == IDC_TABS && header->code == TCN_SELCHANGE) {
            current_page = (int)SendDlgItemMessageA(dialog, IDC_TABS, TCM_GETCURSEL, 0, 0);
            show_page(dialog, current_page);
            return TRUE;
        }
        return FALSE;
    }
    if (message == WM_DESTROY) {
        if (title_font)
            DeleteObject(title_font);
        title_font = NULL;
        return FALSE;
    }
    if (message == WM_HELP) {
        open_help(dialog);
        return TRUE;
    }
    if (message == WM_COMMAND) {
        switch (LOWORD(wparam)) {
        case IDC_MODE:
        case IDC_RENDERER:
        case IDC_RESOLUTION:
        case IDC_COLOUR:
        case IDC_DRAWDIST:
            if (HIWORD(wparam) == CBN_SELCHANGE)
                update_enabled(dialog);
            return TRUE;
        case IDC_ASPECT:  /* the same preset in the other aspect: Sharp stays Sharp */
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                int shown = shown_preset(dialog);
                fill_presets(dialog, !lstrcmpiA(chosen(dialog, IDC_ASPECT, aspects), "square"),
                             shown >= 0 ? shown : 0);
            }
            return TRUE;
        case IDC_MUSIC:
        case IDC_PAD:
        case IDC_WIDESCREEN:
        case IDC_CLOSE_GAPS:
        case IDC_VIBRATION:
        case IDC_NITRO_RUMBLE:
        case IDC_MENU_NAV:
            update_enabled(dialog);
            return TRUE;
        case IDC_HELPFILE:
            open_help(dialog);
            return TRUE;
        case IDOK:
            save(dialog);
            EndDialog(dialog, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        case IDC_SAVE:
            save(dialog);
            return TRUE;
        case IDC_PERIOD:
            load(dialog, PERIOD_DEFAULTS);
            return TRUE;
        case IDC_MODERN:
            load(dialog, MODERN_DEFAULTS);
            return TRUE;
        case IDC_BROWSE:
            browse(dialog);
            return TRUE;
        case IDC_DOSPATCH:
            patch_dos(dialog);
            return TRUE;
        case IDC_RELEASES:
            if ((DWORD)ShellExecuteA(dialog, "open", RELEASES_URL, NULL, NULL, SW_SHOWNORMAL) <= 32)
                MessageBoxA(dialog, "No web browser could be opened. The releases are at:\n" RELEASES_URL,
                            "Big Red Launcher", MB_OK | MB_ICONINFORMATION);
            return TRUE;
        case IDC_JOYCPL:
            if ((DWORD)ShellExecuteA(dialog, "open", "control.exe", "joy.cpl", NULL, SW_SHOWNORMAL) <= 32)
                MessageBoxA(dialog, "Windows' game controller settings (joy.cpl) aren't available here.",
                            "Big Red Launcher", MB_OK | MB_ICONINFORMATION);
            return TRUE;
        case IDC_FOLDER:
            if (HIWORD(wparam) == EN_KILLFOCUS)
                show_track_count(dialog);
            return TRUE;
        }
    }
    return FALSE;
}

/* ---- checks before starting ---- */
static int ask(const char *text)
{
    return MessageBoxA(NULL, text, "Big Red Racing", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON1) == IDYES;
}

static int file_contains(const char *path, const char *needle)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD size, got = 0, i, n = (DWORD)lstrlenA(needle);
    char *data;
    int found = 0;
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    size = GetFileSize(f, NULL);
    data = (char *)HeapAlloc(GetProcessHeap(), 0, size ? size : 1);
    if (data && ReadFile(f, data, size, &got, NULL) && got == size)
        for (i = 0; i + n <= size && !found; i++)
            found = !memcmp(data + i, needle, n);
    if (data)
        HeapFree(GetProcessHeap(), 0, data);
    CloseHandle(f);
    return found;
}

/* Wine (and Proton), whichever Windows version it reports: ntdll exports wine_get_version.
 * Real Windows 95, 98 and ME have no ntdll of that kind. */
static int under_wine(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version") != NULL;
}

static int checks_pass(void)
{
    char path[MAX_PATH], setting[MAX_PATH], message[MAX_PATH + 300];
    int state = brr_win_game_state(game_folder), cnc_ddraw;
    if (state == BRR_WIN_MISSING) {
        wsprintfA(path, "%s\\WINRACE.EXE", folder);
        if (GetFileAttributesA(path) != 0xFFFFFFFFu)  /* dropped into the game's folder, as earlier versions were */
            wsprintfA(message, "Big Red Launcher now goes in the folder above the game's files: make a folder "
                      "called Game next to Big Red Racing.exe and move the game's files (WINRACE.EXE and the "
                      "rest) into it. README.txt explains.");
        else
            wsprintfA(message, "WINRACE.EXE isn't in %s.\n\nPut the game's files there, installed and updated "
                      "to v2.25.", game_folder);
        MessageBoxA(NULL, message, "Big Red Racing", MB_OK | MB_ICONERROR);
        return 0;
    }
    if (state == BRR_WIN_OTHER && !ask("WINRACE.EXE isn't the v2.25 version (from the official v2.25 patch). "
                                       "Big Red Launcher will leave the game unchanged.\n\nStart anyway?"))
        return 0;
    /* the copy the game runs as: the game's own files stay as they are */
    /* cnc-ddraw: used when it's here, but never on Windows 95, 98 or ME, which can't load it:
     * its header asks for Windows XP (5.1), so WINRACEP.EXE wouldn't start ("CNCDD.DLL expects
     * a newer version of Windows", the director's Windows ME, 2026-11-17); their own DirectDraw
     * does full screen. Wine reporting Windows 95-ME loads it all the same, and needs it, as
     * Wine's own DirectDraw can't change the display mode. */
    wsprintfA(path, "%s\\" CNC_DDRAW_DLL, folder);
    cnc_ddraw = GetFileAttributesA(path) != 0xFFFFFFFFu && (!windows_9x || under_wine());
    if (!cnc_ddraw && !windows_9x
        && !ask("cnc-ddraw (" CNC_DDRAW_DLL ") isn't next to Big Red Racing.exe. On modern Windows and Proton "
                "the game needs it to fill the screen.\n\nStart anyway?"))
        return 0;
    wsprintfA(path, "%s\\WINRACEP.EXE", folder);
    if (brr_win_make_copy(game_folder, path, cnc_ddraw, message, sizeof message) < 0) {
        MessageBoxA(NULL, message, "Big Red Racing", MB_OK | MB_ICONERROR);
        return 0;
    }
    wsprintfA(path, "%s\\PATCH.DLL", folder);
    if (!file_contains(path, "BRRFIX.INI")) {
        MessageBoxA(NULL, "PATCH.DLL, Big Red Launcher's fix, isn't next to Big Red Racing.exe, so the game "
                    "can't start. Copy it there again from the Big Red Launcher zip.", "Big Red Racing",
                    MB_OK | MB_ICONERROR);
        return 0;
    }
    if (ini_flag("music", "music", 1)) {
        ini_get(brrfix_ini, "music", "folder", "Music", setting, sizeof setting);
        music_folder(setting, path);
        if (!brr_count_tracks(path) && !ask("There are no music tracks in the music folder, so there'll be no "
                                            "music (unless there's a real CD).\n\nStart anyway?"))
            return 0;
    }
    return 1;
}

static DWORD start_game(const char *arguments)
{
    char command[MAX_PATH * 2], exe[MAX_PATH], *path;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    DWORD code = 1, size;
    /* the game's KEY.DLL is found in the current folder; PATH as well, in case a PC leaves that out */
    size = GetEnvironmentVariableA("PATH", NULL, 0);
    path = (char *)HeapAlloc(GetProcessHeap(), 0, size + MAX_PATH + 2);
    if (path) {
        if (!size || GetEnvironmentVariableA("PATH", path, size) >= size)
            path[0] = 0;
        if (path[0])
            lstrcatA(path, ";");
        lstrcatA(path, game_folder);
        SetEnvironmentVariableA("PATH", path);
        HeapFree(GetProcessHeap(), 0, path);
    }
    wsprintfA(exe, "%s\\WINRACEP.EXE", folder);
    wsprintfA(command, "\"%s\"%s%s", exe, arguments[0] ? " " : "", arguments);
    memset(&startup, 0, sizeof startup);
    startup.cb = sizeof startup;
    if (!CreateProcessA(exe, command, NULL, NULL, FALSE, 0, NULL, game_folder, &startup, &process)) {
        char text[300];
        DWORD error = GetLastError();
        int n = wsprintfA(text, "WINRACEP.EXE couldn't be started.\n\nWindows error %lu: ", (unsigned long)error);
        if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0,
                            text + n, (DWORD)(sizeof text - n), NULL))
            text[n - 2] = 0;
        MessageBoxA(NULL, text, "Big Red Racing", MB_OK | MB_ICONERROR);
        return 1;
    }
    CloseHandle(process.hThread);
    /* stay running until the game exits, so Steam counts play time and Proton waits */
    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    return code;
}

/* the command line after the program's own name, minus -play and -setup */
static void split_arguments(const char *line, char *rest, int *play, int *setup)
{
    const char *p = line;
    rest[0] = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"')
            p++;
        if (*p)
            p++;
    } else
        while (*p && *p != ' ' && *p != '\t')
            p++;
    while (*p) {
        const char *start;
        int length;
        while (*p == ' ' || *p == '\t')
            p++;
        start = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        length = (int)(p - start);
        if (length == 5 && !strncmp(start, "-play", 5))
            *play = 1;
        else if (length == 6 && !strncmp(start, "-setup", 6))
            *setup = 1;
        else if (length > 0 && lstrlenA(rest) + length + 2 < MAX_PATH) {
            if (rest[0])
                lstrcatA(rest, " ");
            strncat(rest, start, (size_t)length);
        }
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show)
{
    char arguments[MAX_PATH], *slash;
    int play = 0, setup = 0, show_window;
    (void)previous; (void)command_line; (void)show;
    GetModuleFileNameA(NULL, folder, sizeof folder);
    slash = strrchr(folder, '\\');
    if (slash)
        *slash = 0;
    wsprintfA(brrfix_ini, "%s\\BRRFIX.INI", folder);
    wsprintfA(ddraw_ini, "%s\\ddraw.ini", folder);
    {
        char setting[MAX_PATH];
        ini_get(brrfix_ini, "launcher", "game", "Game", setting, sizeof setting);
        folder_from(setting, "Game", game_folder);
    }
    windows_9x = (GetVersion() & 0x80000000u) != 0;
    split_arguments(GetCommandLineA(), arguments, &play, &setup);
    InitCommonControls();
    show_window = !ini_flag("launcher", "skip", 0);
    if (setup || (GetAsyncKeyState(VK_SHIFT) & 0x8000))
        show_window = 1;
    else if (play)
        show_window = 0;
    /* With the window: back to it when the game ends (Quit leaves). Without it (-play, or
     * "Skip this window"): just the game, so Steam goes back to its library afterwards. */
    for (;;) {
        int code;
        if (show_window && DialogBoxParamA(instance, MAKEINTRESOURCEA(IDD_SETTINGS), NULL, settings_proc, 0) != IDOK)
            return 0;
        if (!checks_pass()) {
            if (!show_window)
                return 1;
            returning = 1;
            continue;
        }
        code = (int)start_game(arguments);
        if (!show_window)
            return code;
        returning = 1;
    }
}
