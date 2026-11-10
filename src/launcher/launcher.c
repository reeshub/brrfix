/* Big Red Racing.exe: Big Red Launcher by ctrl-alt-rees (round 2, item 8). Our own program, placed next to
 * WINRACE.EXE, so Steam shows a proper name. It shows a settings window (which can be
 * skipped), checks the game's files, then starts WINRACE.EXE and waits for it to exit.
 *
 * Settings live in BRRFIX.INI (the drop-in's) and ddraw.ini (cnc-ddraw's). They're
 * changed with WritePrivateProfileStringA, value by value, so comments, layout and keys
 * the launcher doesn't know about are kept. Windows 95 compatible: ANSI functions that
 * Windows 95 has; anything newer is looked up at run time.
 *
 * Command line: -play starts the game straight away; -setup always shows the window;
 * holding Shift while starting also shows it. Anything else goes to WINRACE.EXE.
 *
 * The "Patch the DOS version" button patches (or restores) the DOS version's RACING.EXE
 * on disk, with the same code as BRRPATCH.EXE, for players on modern Windows. On modern
 * Windows it also corrects WINRACE.EXE's section headers (winfix.c), keeping WINRACE.ORI. */
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

static char folder[MAX_PATH], brrfix_ini[MAX_PATH], ddraw_ini[MAX_PATH];
static int windows_9x, under_wine;

/* made from HELP.md by dropin-build */
#define HELP_FILE "Big Red Launcher Help.txt"

/* ---- choices, in the order the combo boxes show them ---- */
struct choice { const char *label, *value; };

static const struct choice modes[] = {
    {"Full screen", "fullscreen"}, {"Windowed", "windowed"}, {0, 0}};
static const struct choice scales[] = {
    {"Largest that fits", "auto"}, {"1x (320x200)", "1"}, {"2x", "2"}, {"3x", "3"}, {"4x", "4"},
    {"5x", "5"}, {"6x", "6"}, {0, 0}};
static const struct choice presets[] = {
    {"Integer (sharp)", "integer"}, {"4:3 sharp", "43sharp"}, {"4:3 nearest", "43nearest"},
    {"Stretch", "stretch"}, {"Smooth", "smooth"}, {0, 0}};
static const struct choice smoothings[] = {
    {"Off", "0"}, {"Light", "25"}, {"Medium", "50"}, {"Strong", "100"}, {0, 0}};
static const struct choice frame_limits[] = {
    {"Display rate", "auto"}, {"Unlocked", "0"}, {"30 fps", "30"}, {"60 fps", "60"}, {"120 fps", "120"},
    {"144 fps", "144"}, {"240 fps", "240"}, {0, 0}};
static const struct choice volumes[] = {
    {"100%", "100"}, {"90%", "90"}, {"80%", "80"}, {"70%", "70"}, {"60%", "60"}, {"50%", "50"}, {"40%", "40"},
    {"30%", "30"}, {"20%", "20"}, {"10%", "10"}, {"0%", "0"}, {0, 0}};
static const struct choice deadzones[] = {
    {"0%", "0"}, {"5%", "5"}, {"10%", "10"}, {"15%", "15"}, {"20%", "20"}, {"25%", "25"}, {"30%", "30"}, {0, 0}};
static const struct choice sensitivities[] = {
    {"50%", "50"}, {"75%", "75"}, {"100%", "100"}, {"125%", "125"}, {"150%", "150"}, {"200%", "200"}, {0, 0}};
static const struct choice layouts[] = {{"Triggers", "triggers"}, {"Stick", "stick"}, {0, 0}};
static const struct choice rates[] = {{"70.0 (DOS original)", "70.0"}, {"71.43 (Windows original)", "71.43"}, {0, 0}};
static const struct choice caps[] = {{"8 ticks", "8"}, {"35 (original)", "35"}, {0, 0}};

/* the cnc-ddraw keys for each full-screen preset: boxing, maintas, aspect_ratio, shader, d3d9_filter */
static const char *const preset_keys[5][5] = {
    {"true", "true", "", "Shaders\\nearest-neighbor.glsl", "0"},
    {"false", "true", "4:3", "Shaders\\interpolation\\lanczos2-sharp.glsl", "0"},
    {"false", "true", "4:3", "Shaders\\nearest-neighbor.glsl", "0"},
    {"false", "false", "", "Shaders\\nearest-neighbor.glsl", "0"},
    {"false", "true", "", "Shaders\\interpolation\\catmull-rom-bilinear.glsl", "1"}};

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
static void fill(HWND dialog, int id, const struct choice *list, const char *value)
{
    HWND box = GetDlgItem(dialog, id);
    int i, chosen = 0;
    SendMessageA(box, CB_RESETCONTENT, 0, 0);
    for (i = 0; list[i].label; i++) {
        SendMessageA(box, CB_ADDSTRING, 0, (LPARAM)list[i].label);
        if (!lstrcmpiA(list[i].value, value))
            chosen = i;
    }
    SendMessageA(box, CB_SETCURSEL, (WPARAM)chosen, 0);  /* unreadable values fall back to the first */
}

static const char *chosen(HWND dialog, int id, const struct choice *list)
{
    LRESULT i = SendDlgItemMessageA(dialog, id, CB_GETCURSEL, 0, 0);
    return i >= 0 ? list[i].value : list[0].value;
}

static void check(HWND dialog, int id, int on) { CheckDlgButton(dialog, id, on ? BST_CHECKED : BST_UNCHECKED); }
static int checked(HWND dialog, int id) { return IsDlgButtonChecked(dialog, id) == BST_CHECKED; }
static void enable(HWND dialog, int id, int on) { EnableWindow(GetDlgItem(dialog, id), on); }

/* Grey out what the other choices make irrelevant */
static void update_enabled(HWND dialog)
{
    static const int music_controls[] = {IDC_VOLUME_LABEL, IDC_VOLUME, IDC_FOLDER, IDC_BROWSE, IDC_TRACKS};
    static const int pad_controls[] = {IDC_PAD_L1, IDC_DEADZONE, IDC_PAD_L2, IDC_SENSITIVITY, IDC_PAD_L3, IDC_LAYOUT};
    const char *mode = chosen(dialog, IDC_MODE, modes);
    int window = lstrcmpiA(mode, "fullscreen") != 0, full = lstrcmpiA(mode, "windowed") != 0;
    size_t i;
    enable(dialog, IDC_SCALE_LABEL, window);
    enable(dialog, IDC_SCALE, window);
    enable(dialog, IDC_ASPECT, window);
    enable(dialog, IDC_PRESET_LABEL, full);
    enable(dialog, IDC_PRESET, full);
    for (i = 0; i < sizeof music_controls / sizeof music_controls[0]; i++)
        enable(dialog, music_controls[i], checked(dialog, IDC_MUSIC));
    for (i = 0; i < sizeof pad_controls / sizeof pad_controls[0]; i++)
        enable(dialog, pad_controls[i], checked(dialog, IDC_PAD));
}

/* One line on each setting, shown when the mouse rests on it */
static const struct { int id; const char *text; } tips[] = {
    {IDC_MODE, "In its own window with the 1996 menu bar, or full screen."},
    {IDC_SCALE, "How big the window is, in whole multiples of the game's 320x200 picture."},
    {IDC_ASPECT, "Stretch the picture's height by 1.2, as a 1996 monitor did. Off: square, sharp pixels."},
    {IDC_PRESET, "How the picture fills the screen in full screen: sharp with borders, 4:3, stretched or smooth."},
    {IDC_INTERP, "Draws the vehicles and the camera between the game's 35 physics steps a second, for smooth motion."},
    {IDC_SMOOTHING, "Experimental: softens the camera's movement further. Not needed for the stutter fix."},
    {IDC_FRAMES, "The most pictures drawn a second. The game itself runs at the same speed whatever you choose."},
    {IDC_VSYNC, "Waits for the screen's refresh before each picture: no tearing, slightly more input lag."},
    {IDC_MUSIC, "Plays the CD music from ripped tracks (trackNN.flac, .ogg or .wav) instead of the CD."},
    {IDC_VOLUME, "Music volume, on top of the game's own CD volume option."},
    {IDC_FOLDER, "The folder with the ripped tracks: MUSIC next to the game, or a full path."},
    {IDC_BROWSE, "Choose the folder with the ripped tracks."},
    {IDC_PAD, "Use an Xbox-style pad as the game's joystick (choose the joystick in the game's controls menu)."},
    {IDC_DEADZONE, "How far the stick moves before steering starts."},
    {IDC_SENSITIVITY, "How much stick gives full lock: above 100% reaches it sooner."},
    {IDC_LAYOUT, "Triggers: right accelerates, left brakes. Stick: push up and down, as the original."},
    {IDC_RATE, "Game ticks a second: 70.0 as the DOS version (real time), or 71.43 as the 1996 Windows version ran (2% fast)."},
    {IDC_CAP, "After a pause, how much missed game time is caught up at once: 8 ticks, or 35 as the original (see Help)."},
    {IDC_LOG, "Writes BRRFIX.LOG next to the game: what was fixed, for bug reports."},
    {IDC_DEFAULTS, "Puts every setting back to its default (press Save or Play to keep them)."},
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

static void music_folder(const char *setting, char *out)
{
    if (setting[0] && (setting[1] == ':' || setting[0] == '\\'))
        lstrcpynA(out, setting, MAX_PATH);
    else
        wsprintfA(out, "%s\\%s", folder, setting[0] ? setting : "MUSIC");
}

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

/* Put the settings into the window: from the files, or the defaults */
static void load(HWND dialog, int defaults)
{
    char v[MAX_PATH];
    const char *f = defaults ? NULL : brrfix_ini, *d = defaults ? NULL : ddraw_ini;
#define GET(file, section, key, fallback) (ini_get(file, section, key, fallback, v, sizeof v), v)
    fill(dialog, IDC_MODE, modes, GET(f, "display", "mode", "fullscreen"));
    fill(dialog, IDC_SCALE, scales, GET(f, "display", "window_scale", "auto"));
    check(dialog, IDC_ASPECT, !lstrcmpiA(GET(f, "display", "aspect", "4:3"), "4:3"));
    {   /* the full-screen preset: as saved in BRRFIX.INI, or else whichever ddraw.ini matches,
         * by its boxing, maintas, aspect_ratio and shader */
        char boxing[16], aspect[16], shader[MAX_PATH], maintas[16];
        int i, found = 0;
        ini_get(f, "display", "fullscreen_scaling", "", v, sizeof v);
        for (i = 0; i < 5; i++)
            if (!lstrcmpiA(v, presets[i].value))
                break;
        if (i < 5)
            found = i;
        else {
            ini_get(d, "ddraw", "boxing", "true", boxing, sizeof boxing);
            ini_get(d, "ddraw", "maintas", "true", maintas, sizeof maintas);
            ini_get(d, "ddraw", "aspect_ratio", "", aspect, sizeof aspect);
            ini_get(d, "ddraw", "shader", preset_keys[0][3], shader, sizeof shader);
            for (i = 0; i < 5; i++)
                if (!lstrcmpiA(boxing, preset_keys[i][0]) && !lstrcmpiA(maintas, preset_keys[i][1])
                    && !lstrcmpiA(aspect, preset_keys[i][2]) && !lstrcmpiA(shader, preset_keys[i][3]))
                    found = i;
        }
        fill(dialog, IDC_PRESET, presets, presets[found].value);
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
    fill(dialog, IDC_VOLUME, volumes, GET(f, "music", "volume", "100"));
    SetDlgItemTextA(dialog, IDC_FOLDER, GET(f, "music", "folder", "MUSIC"));
    check(dialog, IDC_PAD, defaults ? 1 : ini_flag("controller", "enabled", 1));
    fill(dialog, IDC_DEADZONE, deadzones, GET(f, "controller", "deadzone", "15"));
    fill(dialog, IDC_SENSITIVITY, sensitivities, GET(f, "controller", "sensitivity", "100"));
    fill(dialog, IDC_LAYOUT, layouts, GET(f, "controller", "layout", "triggers"));
    fill(dialog, IDC_RATE, rates, GET(f, "clock", "tick_rate", "70.0"));
    fill(dialog, IDC_CAP, caps, GET(f, "clock", "catch_up_cap", "8"));
    check(dialog, IDC_LOG, defaults ? 0 : ini_flag("log", "log", 0));
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
    int preset = (int)SendDlgItemMessageA(dialog, IDC_PRESET, CB_GETCURSEL, 0, 0), i;
    static const char *const preset_names[5] = {"boxing", "maintas", "aspect_ratio", "shader", "d3d9_filter"};
    ini_set(brrfix_ini, "display", "mode", mode);
    if (preset < 0 || preset > 4)
        preset = 0;
    ini_set(brrfix_ini, "display", "fullscreen_scaling", presets[preset].value);
    ini_set(brrfix_ini, "display", "window_scale", chosen(dialog, IDC_SCALE, scales));
    ini_set(brrfix_ini, "display", "aspect", checked(dialog, IDC_ASPECT) ? "4:3" : "square");
    ini_set(brrfix_ini, "fixes", "interpolation", checked(dialog, IDC_INTERP) ? "1" : "0");
    ini_set(brrfix_ini, "camera", "smoothing", chosen(dialog, IDC_SMOOTHING, smoothings));
    ini_set(brrfix_ini, "clock", "frame_limit", chosen(dialog, IDC_FRAMES, frame_limits));
    ini_set(brrfix_ini, "clock", "tick_rate", chosen(dialog, IDC_RATE, rates));
    ini_set(brrfix_ini, "clock", "catch_up_cap", chosen(dialog, IDC_CAP, caps));
    ini_set(brrfix_ini, "music", "music", checked(dialog, IDC_MUSIC) ? "1" : "0");
    ini_set(brrfix_ini, "music", "volume", chosen(dialog, IDC_VOLUME, volumes));
    GetDlgItemTextA(dialog, IDC_FOLDER, text, sizeof text);
    ini_set(brrfix_ini, "music", "folder", text[0] ? text : "MUSIC");
    ini_set(brrfix_ini, "controller", "enabled", checked(dialog, IDC_PAD) ? "1" : "0");
    ini_set(brrfix_ini, "controller", "deadzone", chosen(dialog, IDC_DEADZONE, deadzones));
    ini_set(brrfix_ini, "controller", "sensitivity", chosen(dialog, IDC_SENSITIVITY, sensitivities));
    ini_set(brrfix_ini, "controller", "layout", chosen(dialog, IDC_LAYOUT, layouts));
    ini_set(brrfix_ini, "log", "log", checked(dialog, IDC_LOG) ? "1" : "0");
    ini_set(brrfix_ini, "launcher", "skip", checked(dialog, IDC_SKIP) ? "1" : "0");
    if (windows_9x)
        ini_set(brrfix_ini, "clock", "vsync", checked(dialog, IDC_VSYNC) ? "1" : "0");
    else {
        /* cnc-ddraw: the window mode follows ours, and the full-screen preset */
        ini_set(ddraw_ini, "ddraw", "windowed", lstrcmpiA(mode, "fullscreen") ? "true" : "false");
        /* In a window the drop-in draws and scales the picture and the mouse itself, so cnc-ddraw
         * mustn't lock the pointer to its 320x200 idea of the window, scale the mouse again, or
         * hook the window; full screen is cnc-ddraw's job, with its defaults (hook=4). */
        {
            int full = !lstrcmpiA(mode, "fullscreen");
            ini_set(ddraw_ini, "ddraw", "hook", full ? "4" : "0");
            ini_set(ddraw_ini, "ddraw", "adjmouse", full ? "true" : "false");
            ini_set(ddraw_ini, "ddraw", "devmode", full ? "false" : "true");
            /* at the desktop's own resolution: otherwise Windows switches to the game's (blurry) */
            ini_set(ddraw_ini, "ddraw", "fullscreen", full ? "true" : "false");
        }
        ini_set(ddraw_ini, "ddraw", "vsync", checked(dialog, IDC_VSYNC) ? "true" : "false");
        /* In a window, no letterbox: under Wine cnc-ddraw otherwise moves the mouse by the
         * borders it would have drawn (playtest 2026-11-10: 60 pixels at 960x720) */
        if (lstrcmpiA(mode, "fullscreen"))
            preset = 3;
        for (i = 0; i < 5; i++)
            ini_set(ddraw_ini, "ddraw", preset_names[i], preset_keys[preset][i]);
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
 * versions install into the same folder, so look here first. */
static void patch_dos(HWND dialog)
{
    static const char title[] = "Big Red Racing: the DOS version";
    char dos_folder[MAX_PATH], text[MAX_PATH + 400], message[512];
    int state, result;
    HCURSOR cursor;
    lstrcpynA(dos_folder, folder, MAX_PATH);
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
    }
    cursor = SetCursor(LoadCursorA(NULL, IDC_WAIT));
    result = state == BRR_DOS_PATCHED ? brr_dos_restore(dos_folder, message, sizeof message)
                                      : brr_dos_patch(dos_folder, message, sizeof message);
    SetCursor(cursor);
    if (result == 0 && state != BRR_DOS_PATCHED)
        lstrcatA(message, "\n\nStart the DOS version as usual, with RACING. To undo, press this button again.");
    MessageBoxA(dialog, message, title, MB_OK | (result < 0 ? MB_ICONERROR : MB_ICONINFORMATION));
}

static BOOL CALLBACK settings_proc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    if (message == WM_INITDIALOG) {
        static const int pad_controls[] = {IDC_PAD_GROUP, IDC_PAD, IDC_DEADZONE, IDC_SENSITIVITY, IDC_LAYOUT,
                                           IDC_PAD_L1, IDC_PAD_L2, IDC_PAD_L3};
        char title[96];
        size_t i;
        wsprintfA(title, "Big Red Launcher %s by ctrl-alt-rees", BRRFIX_VERSION);
        SetWindowTextA(dialog, title);
        SendMessageA(dialog, WM_SETICON, ICON_BIG, (LPARAM)LoadIconA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(IDI_APP)));
        load(dialog, 0);
        add_tips(dialog);
        if (windows_9x) {  /* no cnc-ddraw (DirectDraw does full screen itself) and no XInput */
            ShowWindow(GetDlgItem(dialog, IDC_PRESET), SW_HIDE);
            ShowWindow(GetDlgItem(dialog, IDC_PRESET_LABEL), SW_HIDE);
            for (i = 0; i < sizeof pad_controls / sizeof pad_controls[0]; i++)
                ShowWindow(GetDlgItem(dialog, pad_controls[i]), SW_HIDE);
        }
        return TRUE;
    }
    if (message == WM_HELP) {
        open_help(dialog);
        return TRUE;
    }
    if (message == WM_COMMAND) {
        switch (LOWORD(wparam)) {
        case IDC_MODE:
            if (HIWORD(wparam) == CBN_SELCHANGE)
                update_enabled(dialog);
            return TRUE;
        case IDC_MUSIC:
        case IDC_PAD:
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
        case IDC_DEFAULTS:
            load(dialog, 1);
            return TRUE;
        case IDC_BROWSE:
            browse(dialog);
            return TRUE;
        case IDC_DOSPATCH:
            patch_dos(dialog);
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

static int checks_pass(void)
{
    char path[MAX_PATH], setting[MAX_PATH], message[300];
    int state;
    wsprintfA(path, "%s\\WINRACE.EXE", folder);
    if (GetFileAttributesA(path) == 0xFFFFFFFFu) {
        MessageBoxA(NULL, "WINRACE.EXE isn't here. Put Big Red Racing.exe in the game's folder, next to "
                    "WINRACE.EXE.", "Big Red Racing", MB_OK | MB_ICONERROR);
        return 0;
    }
    state = brr_win_state(folder);
    /* also when an earlier launcher's correction is there and WINRACE.ORI is the original */
    if (state == BRR_WIN_ORIGINAL || (state == BRR_WIN_OTHER && brr_win_backup_is_original(folder))) {
        /* Windows 95-ME and Wine can also start the file as it is, so a "No" isn't fatal there */
        int optional = windows_9x || under_wine;
        if (MessageBoxA(NULL, "WINRACE.EXE needs a small correction: the 1996 linker wrote its file headers "
                        "carelessly, and Windows 10 and 11 refuse to start it.\n\nCorrect them now? Only the "
                        "headers change; the game is the same, and works on every version of "
                        "Windows. The original is kept as WINRACE.ORI.", "Big Red Racing",
                        MB_YESNO | MB_ICONQUESTION) != IDYES) {
            if (!optional) {
                MessageBoxA(NULL, "Without the correction, this version of Windows can't start the game.",
                            "Big Red Racing", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
        } else if (brr_win_correct(folder, message, sizeof message) < 0) {
            MessageBoxA(NULL, message, "Big Red Racing", MB_OK | MB_ICONERROR);
            if (!optional)
                return 0;
        }
    } else if (state != BRR_WIN_ORIGINAL && state != BRR_WIN_CORRECTED
               && !ask("WINRACE.EXE isn't the v2.25 version (from the official v2.25 patch). "
                       "Big Red Launcher will leave the game unchanged.\n\nStart anyway?"))
        return 0;
    wsprintfA(path, "%s\\TIMER.DLL", folder);
    if (!file_contains(path, "BRRFIX.INI") && !ask("The TIMER.DLL here isn't Big Red Launcher's (it may be the "
                                                   "game's own). The game will run without the fixes, and "
                                                   "may not run at all on modern Windows.\n\nStart anyway?"))
        return 0;
    wsprintfA(path, "%s\\ddraw.dll", folder);
    if (!windows_9x && GetFileAttributesA(path) == 0xFFFFFFFFu
        && !ask("cnc-ddraw (ddraw.dll) isn't here. On modern Windows and Proton the game needs it to fill "
                "the screen.\n\nStart anyway?"))
        return 0;
    if (ini_flag("music", "music", 1)) {
        ini_get(brrfix_ini, "music", "folder", "MUSIC", setting, sizeof setting);
        music_folder(setting, path);
        if (!brr_count_tracks(path) && !ask("There are no music tracks in the music folder, so there'll be no "
                                            "music (unless there's a real CD).\n\nStart anyway?"))
            return 0;
    }
    return 1;
}

static DWORD start_game(const char *arguments)
{
    char command[MAX_PATH * 2], exe[MAX_PATH];
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    DWORD code = 1;
    if (under_wine) {  /* cnc-ddraw instead of Wine's own DirectDraw, without launch options */
        char overrides[512];
        DWORD n = GetEnvironmentVariableA("WINEDLLOVERRIDES", overrides, sizeof overrides);
        if (n == 0 || n >= sizeof overrides)
            lstrcpyA(overrides, "ddraw=n,b");
        else if (!strstr(overrides, "ddraw"))
            lstrcatA(overrides, ";ddraw=n,b");
        SetEnvironmentVariableA("WINEDLLOVERRIDES", overrides);
    }
    wsprintfA(exe, "%s\\WINRACE.EXE", folder);
    wsprintfA(command, "\"%s\"%s%s", exe, arguments[0] ? " " : "", arguments);
    memset(&startup, 0, sizeof startup);
    startup.cb = sizeof startup;
    if (!CreateProcessA(exe, command, NULL, NULL, FALSE, 0, NULL, folder, &startup, &process)) {
        char text[300];
        DWORD error = GetLastError();
        int n = wsprintfA(text, "WINRACE.EXE couldn't be started.\n\nWindows error %lu: ", (unsigned long)error);
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
    windows_9x = (GetVersion() & 0x80000000u) != 0;
    under_wine = GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL;
    split_arguments(GetCommandLineA(), arguments, &play, &setup);
    InitCommonControls();
    show_window = !ini_flag("launcher", "skip", 0);
    if (setup || (GetAsyncKeyState(VK_SHIFT) & 0x8000))
        show_window = 1;
    else if (play)
        show_window = 0;
    if (show_window && DialogBoxParamA(instance, MAKEINTRESOURCEA(IDD_SETTINGS), NULL, settings_proc, 0) != IDOK)
        return 0;
    if (!checks_pass())
        return 1;
    return (int)start_game(arguments);
}
