/* The game's own windowed mode (round 2, item 4). WinMain (0x401070) checks the desktop's
 * colour depth (bits per pixel x planes, by GetDeviceCaps) and runs in a window only on a
 * 256-colour desktop; otherwise it asks "This program requires 256 colour mode to run
 * windowed - Continue full screen?". Its test (CMP EDI,8; JZ, at 0x4010E9) is replaced by
 * a call here, through brr_hook_display_choice, which continues WinMain at:
 *   0x40112A, the windowed path;
 *   0x401114, the full-screen path (as if OK was pressed, without the question);
 *   0x4010EE, the original question.
 * On a desktop that isn't 256 colours the drop-in draws the window itself (below). */
#include "brrfix.h"
#include <string.h>

uint32_t brr_display_windowed = 0x40112A, brr_display_fullscreen = 0x401114, brr_display_prompt = 0x4010EE;

/* 0 = windowed, 1 = full screen, 2 = ask (the original question) */
int __cdecl brr_display_choice(uint32_t depth)
{
    if (cfg.display_mode == DISPLAY_FULLSCREEN) {
        brr_log("display: full screen ([display] mode=fullscreen)");
        return 1;
    }
    if (cfg.display_mode == DISPLAY_WINDOWED) {
        brr_gdi_present = depth != 8;
        brr_log("display: the game's own window ([display] mode=windowed; %s)",
                depth == 8 ? "256-colour desktop: the game draws it itself"
                           : "drawn by the drop-in with GDI, converting the colours");
        return 0;
    }
    return depth == 8 ? 0 : 2;
}

/* ---- drawing the game's window on a true-colour desktop ----
 * The game's windowed present (0x42CE78 -> 0x42CAC0 copy -> 0x42CD2C Blt) assumes a
 * 256-colour desktop: its back surface takes the desktop's format, so on a 32-bit desktop
 * its 8-bit pixels land four to a pixel (a squeezed, discoloured picture, seen under
 * Wine). Instead, the present's entry jumps here (brr_hook_present) and, in a window on a
 * desktop that isn't 256 colours, the frame buffer is drawn with GDI's StretchDIBits as
 * an 8-bit picture with the game's own palette: Windows converts the colours exactly. */
#define GAME_HWND        (*(HWND *)0x451014)
#define GAME_DD_ACTIVE   (*(volatile uint8_t *)0x52D720)
#define GAME_FRAME       (*(uint8_t **)0x52D738)
#define GAME_FRAME_W     (*(volatile int32_t *)0x44BFB8)
#define GAME_FRAME_H     (*(volatile int32_t *)0x44BFBC)
#define GAME_CLEAR_FLAG  (*(volatile uint8_t *)0x52D754)  /* clear the screen this frame, then reset */
#define GAME_PALETTE     ((const PALETTEENTRY *)0x52D758)

uint32_t brr_present_continue = 0x42CE82;
int brr_gdi_present;  /* set when the game runs in a window on a desktop that isn't 256 colours */
static uint32_t gdi_frames;

static struct {
    BITMAPINFOHEADER header;
    RGBQUAD colours[256];
} picture;

/* The game answers WM_GETMINMAXINFO by capping its window at its own size, so the
 * window is subclassed: every message goes to the game's WndProc as before, and after
 * WM_GETMINMAXINFO the maximum is raised to fit the scaled window. */
static WNDPROC game_proc;

/* cnc-ddraw intercepts the window functions in every module's import table, to manage
 * full-screen games' windows: with it loaded, GetWindowRect reports the client size and
 * SetWindowPos leaves the game's window alone. The real ones, looked up directly: */
typedef BOOL (WINAPI *rect_fn)(HWND, RECT *);
typedef BOOL (WINAPI *setpos_fn)(HWND, HWND, int, int, int, int, UINT);
static rect_fn real_client_rect, real_window_rect;
static setpos_fn real_set_window_pos;

static void find_real_functions(void)
{
    HMODULE user32 = GetModuleHandleA("user32.dll");
    real_client_rect = (rect_fn)GetProcAddress(user32, "GetClientRect");
    real_window_rect = (rect_fn)GetProcAddress(user32, "GetWindowRect");
    real_set_window_pos = (setpos_fn)GetProcAddress(user32, "SetWindowPos");
}
static int max_w, max_h;
static int picture_w, picture_h, shown_w, shown_h;  /* the game's frame, and its size in the window */

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result;
    /* Mouse messages (0x200-0x209) carry a position in the window; in a window the game
     * uses it as a position in its picture (FUN_00420D88), so scale it back down. */
    if (message >= WM_MOUSEFIRST && message <= 0x209 && shown_w > 0 && shown_h > 0) {
        long x = (short)LOWORD(lparam), y = (short)HIWORD(lparam);
        x = x * picture_w / shown_w;
        y = y * picture_h / shown_h;
        lparam = MAKELPARAM((WORD)(short)x, (WORD)(short)y);
    }
    result = CallWindowProcA(game_proc, window, message, wparam, lparam);
    if (cfg.debug_log_mouse && message >= WM_MOUSEFIRST && message <= 0x209) {
        static int logged;
        if (logged++ < 40)
            brr_log("mouse: message 0x%03X at %d,%d in the picture; the game's position (0x44BBE4) %d,%d",
                    message, (short)LOWORD(lparam), (short)HIWORD(lparam), *(volatile short *)0x44BBE4,
                    *(volatile short *)0x44BBE6);
    }
    if (message == WM_GETMINMAXINFO && max_w > 0) {
        MINMAXINFO *info = (MINMAXINFO *)lparam;
        if (info->ptMaxTrackSize.x < max_w) info->ptMaxTrackSize.x = max_w;
        if (info->ptMaxTrackSize.y < max_h) info->ptMaxTrackSize.y = max_h;
        if (info->ptMaxSize.x < max_w) info->ptMaxSize.x = max_w;
        if (info->ptMaxSize.y < max_h) info->ptMaxSize.y = max_h;
    }
    return result;
}

/* The window's client area: the frame times window_scale (auto = the largest whole
 * number that fits in 85% of the desktop's work area), the height times 1.2 with
 * aspect=4:3. Re-applied whenever the client area differs, as the game sizes its
 * window itself when it changes screen. */
static void keep_window_size(HWND window, int w, int h)
{
    static int last_w, last_h, want_w, want_h, logged, centre;
    RECT client, frame, work;
    int scale = cfg.window_scale;
    if (w != last_w || h != last_h) {
        last_w = w;
        last_h = h;
        centre = 1;
        if (!scale) {
            SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
            for (scale = 8; scale > 1; scale--)
                if (w * scale <= (work.right - work.left) * 85 / 100
                    && h * scale * (cfg.window_aspect43 ? 6 : 5) / 5 <= (work.bottom - work.top) * 85 / 100)
                    break;
        }
        want_w = w * scale;
        want_h = cfg.window_aspect43 ? h * scale * 6 / 5 : h * scale;
        if (logged++ < 4)
            brr_log("display: window %dx%d (frame %dx%d, scale %d%s)", want_w, want_h, w, h, scale,
                    cfg.window_aspect43 ? ", 4:3" : "");
    }
    if (!real_set_window_pos)
        find_real_functions();
    if (!game_proc)
        game_proc = (WNDPROC)SetWindowLongA(window, GWL_WNDPROC, (LONG)window_proc);
    real_client_rect(window, &client);
    if (client.right == want_w && client.bottom == want_h)
        return;
    real_window_rect(window, &frame);
    /* grow the outer window by the difference, keeping its frame and menu bar */
    max_w = (frame.right - frame.left) + want_w - client.right;
    max_h = (frame.bottom - frame.top) + want_h - client.bottom;
    {
        static int tries;
        int x = 0, y = 0;
        UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
        BOOL ok;
        RECT after;
        /* centred on the desktop when the size is first set, not when the player has moved it */
        if (centre) {
            SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
            x = work.left + ((work.right - work.left) - max_w) / 2;
            y = work.top + ((work.bottom - work.top) - max_h) / 2;
            if (x < work.left) x = work.left;
            if (y < work.top) y = work.top;
            centre = 0;
        } else
            flags |= SWP_NOMOVE;
        ok = real_set_window_pos(window, NULL, x, y, max_w, max_h, flags);
        real_client_rect(window, &after);
        if (tries++ < 5)
            brr_log("display: resize to %dx%d outer%s: %s; client now %ldx%ld (was %ldx%ld)", max_w, max_h,
                    flags & SWP_NOMOVE ? "" : ", centred", ok ? "ok" : "failed", (long)after.right,
                    (long)after.bottom, (long)client.right, (long)client.bottom);
    }
}

/* Scale the 8-bit frame to the window ourselves, repeating pixels (nearest neighbour), so
 * GDI only converts colours: Wine's StretchDIBits blends at the edges of the blocks even
 * with COLORONCOLOR (a third of the pixels came out as in-between shades). */
static uint8_t *scaled;
static size_t scaled_size;

static int scale_to(const uint8_t *frame, int w, int h, int out_w, int out_h)
{
    int x, y, last_src = -1;
    size_t pitch = ((size_t)out_w + 3) & ~(size_t)3, need = pitch * (size_t)out_h;
    static int columns_w, columns_out;
    static int *columns;
    if (out_w <= 0 || out_h <= 0)
        return 0;
    if (need > scaled_size) {
        uint8_t *bigger = (uint8_t *)HeapAlloc(GetProcessHeap(), 0, need);
        if (!bigger)
            return 0;
        if (scaled)
            HeapFree(GetProcessHeap(), 0, scaled);
        scaled = bigger;
        scaled_size = need;
    }
    if (columns_w != w || columns_out != out_w) {
        if (columns)
            HeapFree(GetProcessHeap(), 0, columns);
        columns = (int *)HeapAlloc(GetProcessHeap(), 0, sizeof(int) * (size_t)out_w);
        if (!columns)
            return 0;
        for (x = 0; x < out_w; x++)
            columns[x] = (int)((long)x * w / out_w);
        columns_w = w;
        columns_out = out_w;
    }
    for (y = 0; y < out_h; y++) {
        int src = (int)((long)y * h / out_h);
        uint8_t *row = scaled + pitch * (size_t)y;
        if (src == last_src) {
            memcpy(row, row - pitch, (size_t)out_w);
            continue;
        }
        {
            const uint8_t *in = frame + (size_t)src * (size_t)w;
            for (x = 0; x < out_w; x++)
                row[x] = in[columns[x]];
        }
        last_src = src;
    }
    return 1;
}

int __cdecl brr_present_gdi(void)
{
    const uint8_t *frame = GAME_FRAME;
    int w = GAME_FRAME_W, h = GAME_FRAME_H, i;
    HWND window = GAME_HWND;
    RECT client;
    HDC dc;
    if (!GAME_DD_ACTIVE || !*GAME_WINDOWED || !frame || w <= 0 || h <= 0 || !window)
        return 0;
    picture.header.biSize = sizeof picture.header;
    picture.header.biWidth = w;
    picture.header.biHeight = -h;  /* top-down */
    picture.header.biPlanes = 1;
    picture.header.biBitCount = 8;
    picture.header.biCompression = BI_RGB;
    picture.header.biClrUsed = 256;
    for (i = 0; i < 256; i++) {
        picture.colours[i].rgbRed = GAME_PALETTE[i].peRed;
        picture.colours[i].rgbGreen = GAME_PALETTE[i].peGreen;
        picture.colours[i].rgbBlue = GAME_PALETTE[i].peBlue;
        picture.colours[i].rgbReserved = 0;
    }
    keep_window_size(window, w, h);
    real_client_rect(window, &client);
    picture_w = w;
    picture_h = h;
    shown_w = client.right;
    shown_h = client.bottom;
    brr_before_present_window();  /* vsync, if on */
    dc = GetDC(window);
    if (!dc)
        return 0;
    if (GAME_CLEAR_FLAG) {  /* as the game's copy (0x42CAC0) does: a black frame, once */
        PatBlt(dc, 0, 0, client.right, client.bottom, BLACKNESS);
        GAME_CLEAR_FLAG = 0;
        ReleaseDC(window, dc);
        return 1;
    }
    if (scale_to(frame, w, h, client.right, client.bottom)) {
        picture.header.biWidth = client.right;
        picture.header.biHeight = -client.bottom;
        SetDIBitsToDevice(dc, 0, 0, client.right, client.bottom, 0, 0, 0, client.bottom, scaled,
                          (BITMAPINFO *)&picture, DIB_RGB_COLORS);
    } else {
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchDIBits(dc, 0, 0, client.right, client.bottom, 0, 0, w, h, frame, (BITMAPINFO *)&picture,
                      DIB_RGB_COLORS, SRCCOPY);
    }
    ReleaseDC(window, dc);
    if (gdi_frames++ == 0)
        brr_log("display: drawing the game's window with GDI (%dx%d frame into a %ldx%ld window)", w, h,
                (long)client.right, (long)client.bottom);
    return 1;
}
