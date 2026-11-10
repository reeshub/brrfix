/* The clock. _TIMER_Count = whole ticks since TIMER_Init_, computed from
 * QueryPerformanceCounter in integer arithmetic (the game's DirectDraw code may put
 * the FPU in single precision). A helper thread keeps the counter current for every
 * reader; the clock_read patch recomputes it at the exact moment run_screen_frame
 * reads it, remembers the fraction of a tick for interpolation, and paces frames.
 * With [clock] source=original it instead copies the 1996 TIMER.DLL (a 14 ms
 * multimedia timer adding 1) and logs when each tick arrives. */
#include "brrfix.h"
#include <mmsystem.h>

static int64_t freq, load_qpc, start_qpc;
static volatile LONG running;
static HANDLE thread;
static UINT event_id;
static uint32_t frame_fraction_milli;  /* thousandths of a tick at the last clock read */

/* statistics, reported every 10 s and at shutdown */
static int64_t stats_since, last_frame_qpc, pace_next;
static uint32_t stats_frames, stats_ticks_run, stats_hist[5], frame_limit_hz;
static uint32_t stats_cap_hits, stats_clock_advanced, last_count;
/* where the last frame's time went (QPC counts), for diagnosing long frames */
int64_t brr_prof_draw, brr_prof_ticks;
static int64_t prof_wait, prof_read_exit;
static int64_t cb_last, cb_min = -1, cb_max, cb_sum;
static uint32_t cb_count, cb_hist[4];

int64_t brr_qpc_freq(void) { return freq; }

static int64_t qpc_raw(void)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

int64_t brr_qpc(void) { return qpc_raw() - load_qpc; }

void brr_clock_setup(void)
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    freq = f.QuadPart;
    load_qpc = start_qpc = qpc_raw();
}

static uint32_t ticks_at(int64_t raw, uint32_t *fraction_milli)
{
    int64_t elapsed = raw - start_qpc;
    if (elapsed < 0)
        elapsed = 0;
    int64_t scaled = elapsed * (int64_t)cfg.rate_milli;  /* ticks x freq x 1000 */
    int64_t unit = freq * 1000;
    if (fraction_milli)
        *fraction_milli = (uint32_t)((scaled % unit) * 1000 / unit);
    return (uint32_t)(scaled / unit);
}

uint32_t brr_clock_ticks_now(void) { return ticks_at(qpc_raw(), NULL); }

float brr_clock_frame_fraction(void) { return (float)frame_fraction_milli / 1000.0f; }

static DWORD WINAPI updater(LPVOID unused)
{
    (void)unused;
    while (running) {
        InterlockedExchange(&brr_timer_count, (LONG)brr_clock_ticks_now());
        Sleep(1);
    }
    return 0;
}

static void CALLBACK original_tick(UINT id, UINT msg, DWORD_PTR user, DWORD_PTR a, DWORD_PTR b)
{
    (void)id; (void)msg; (void)user; (void)a; (void)b;
    InterlockedIncrement(&brr_timer_count);
    int64_t now = qpc_raw();
    if (cb_last) {
        int64_t gap = now - cb_last;
        cb_sum += gap;
        cb_count++;
        if (cb_min < 0 || gap < cb_min) cb_min = gap;
        if (gap > cb_max) cb_max = gap;
        int64_t ms = gap * 1000 / freq;
        cb_hist[ms < 8 ? 0 : ms < 13 ? 1 : ms < 16 ? 2 : 3]++;
    }
    cb_last = now;
    if (cfg.log_clock && (now - load_qpc) - stats_since >= freq * 10)
        brr_clock_report(0);
}

void brr_clock_start(void)
{
    start_qpc = qpc_raw();
    stats_since = brr_qpc();
    running = 1;
    if (cfg.clock_original) {  /* exactly as the 1996 DLL: no timeBeginPeriod */
        event_id = timeSetEvent(14, 1000, original_tick, 0, TIME_PERIODIC);
        return;
    }
    timeBeginPeriod(1);
    thread = CreateThread(NULL, 0, updater, NULL, 0, NULL);
    if (thread)
        SetThreadPriority(thread, THREAD_PRIORITY_TIME_CRITICAL);
}

void brr_clock_stop(void)
{
    running = 0;
    if (event_id)
        timeKillEvent(event_id);
    event_id = 0;
    if (thread) {
        WaitForSingleObject(thread, 100);
        CloseHandle(thread);
        thread = NULL;
    }
    if (!cfg.clock_original)
        timeEndPeriod(1);
}

/* Sleep in 1 ms steps, then spin for the last 2 ms, so waits end within a few
 * microseconds (one long Sleep can overshoot badly under Wine). */
static void wait_until(int64_t target_raw)
{
    for (;;) {
        int64_t left = target_raw - qpc_raw();
        if (left <= 0)
            return;
        if (left * 1000 / freq > 2)
            Sleep(1);
        else
            Sleep(0);
    }
}

/* ---- frame pacing ---- */
int brr_cnc_ddraw, brr_vsync_active;
static void *direct_draw;  /* our own IDirectDraw, only for WaitForVerticalBlank */

static void *create_direct_draw(void)
{
    typedef HRESULT (WINAPI *create_fn)(GUID *, void **, void *);
    create_fn create = (create_fn)GetProcAddress(GetModuleHandleA("ddraw.dll"), "DirectDrawCreate");
    void *dd = NULL;
    if (!create || create(NULL, &dd, NULL) != 0)
        return NULL;
    return dd;
}

static uint32_t refresh_rate(void);

/* At the first frame (DirectDraw is loaded by then): who paces the frames. When the
 * loaded ddraw.dll is cnc-ddraw (it exports GameHandlesClose, which no real DirectDraw
 * does) and [clock] pacing=cnc-ddraw, it owns pacing (its maxgameticks and vsync).
 * By default the drop-in paces: frame_limit caps the frame rate (auto = the display's
 * refresh rate, or vsync's pace if vsync is on), and vsync waits for the vertical blank
 * before the windowed present (a full-screen Flip is synchronised by the hardware). With
 * cnc-ddraw, vsync is cnc-ddraw's own setting (vsync=true in ddraw.ini): it draws the
 * screen itself. Measured (2026-11-07): cnc-ddraw's game limiter is off by default, its
 * "refresh rate" chose 60 fps on a 144 Hz display, and at 144 it ran at 127-135 fps;
 * ours holds the rate exactly. */
static void choose_pacing(void)
{
    HMODULE ddraw = GetModuleHandleA("ddraw.dll");
    brr_cnc_ddraw = ddraw && GetProcAddress(ddraw, "GameHandlesClose") != NULL;
    if (brr_cnc_ddraw && !cfg.pacing_dropin) {
        frame_limit_hz = 0;
        brr_log("pacing: by cnc-ddraw ([clock] pacing=cnc-ddraw): its maxgameticks and vsync in ddraw.ini; "
                "the drop-in's frame limit and vsync are off");
        return;
    }
    if (cfg.vsync && brr_cnc_ddraw)
        brr_log("pacing: [clock] vsync is for DirectDraw without cnc-ddraw; with cnc-ddraw set vsync=true "
                "in ddraw.ini");
    else if (cfg.vsync) {
        direct_draw = create_direct_draw();
        brr_vsync_active = direct_draw != NULL;
        if (!brr_vsync_active)
            brr_log("pacing: vsync is on, but DirectDraw couldn't be opened to wait for the vertical blank");
    }
    frame_limit_hz = cfg.frame_limit == FRAME_LIMIT_AUTO ? refresh_rate()
                   : cfg.frame_limit > 0 ? (uint32_t)cfg.frame_limit : 0;
    brr_log("pacing: by the drop-in%s; frame limit %s%lu fps; vsync %s", brr_cnc_ddraw ? " (cnc-ddraw present)" : "",
            frame_limit_hz ? "" : "unlocked: ", (unsigned long)frame_limit_hz,
            brr_vsync_active ? "on (before the windowed present; then frame_limit=auto steps aside in a window)" : "off");
}

/* Before the game blits its frame into its window (0x42CD2C, from the present). */
void __cdecl brr_before_present_window(void)
{
    typedef HRESULT (WINAPI *wait_fn)(void *, DWORD, HANDLE);
    static uint32_t waits, instant;
    int64_t before;
    if (!brr_vsync_active)
        return;
    before = qpc_raw();
    /* IDirectDraw::WaitForVerticalBlank is entry 22 of its table; 1 = DDWAITVB_BLOCKBEGIN */
    ((wait_fn)(*(void ***)direct_draw)[22])(direct_draw, 1, NULL);
    /* Some DirectDraws (Wine's, for one) return at once: then the frame limit paces */
    if (waits < 60) {
        if ((qpc_raw() - before) * 10000 / freq < 3)  /* under 0.3 ms */
            instant++;
        if (++waits == 60 && instant > 50) {
            brr_vsync_active = 0;
            brr_log("pacing: WaitForVerticalBlank returns at once here (%lu of 60 waits), so vsync is off "
                    "and the frame limit paces", (unsigned long)instant);
        }
    }
}

static uint32_t refresh_rate(void)
{
    HDC screen = GetDC(NULL);
    int hz = screen ? GetDeviceCaps(screen, VREFRESH) : 0;
    if (screen)
        ReleaseDC(NULL, screen);
    return hz > 1 ? (uint32_t)hz : 60;
}

void brr_clock_report(int final)
{
    if (!cfg.log_clock)
        return;
    int64_t now = brr_qpc();
    int64_t span_us = (now - stats_since) * 1000000 / freq;
    if (stats_frames && span_us > 0) {
        /* ticks the game actually ran per second, in thousandths */
        int64_t tps_milli = (int64_t)stats_ticks_run * 1000000000 / span_us;
        int64_t fps_milli = (int64_t)stats_frames * 1000000000 / span_us;
        brr_log("clock: %ld.%03ld s, %lu frames (%ld.%03ld fps, limit %lu), game ran %lu ticks "
                "(%ld.%03ld per s; clock advanced %lu), ticks per frame 0:%lu 1:%lu 2:%lu 3:%lu 4+:%lu, cap hits %lu",
                (long)(span_us / 1000000), (long)(span_us / 1000 % 1000),
                (unsigned long)stats_frames, (long)(fps_milli / 1000), (long)(fps_milli % 1000),
                (unsigned long)frame_limit_hz, (unsigned long)stats_ticks_run,
                (long)(tps_milli / 1000), (long)(tps_milli % 1000), (unsigned long)stats_clock_advanced,
                (unsigned long)stats_hist[0], (unsigned long)stats_hist[1],
                (unsigned long)stats_hist[2], (unsigned long)stats_hist[3],
                (unsigned long)stats_hist[4], (unsigned long)stats_cap_hits);
    }
    if (cfg.clock_original && cb_count) {
        int64_t mean_us = cb_sum * 1000000 / freq / cb_count;
        brr_log("original timer: %lu callbacks, interval mean %ld us, min %ld us, max %ld us; "
                "<8 ms:%lu 8-13 ms:%lu 13-16 ms:%lu >=16 ms:%lu",
                (unsigned long)cb_count, (long)mean_us, (long)(cb_min * 1000000 / freq),
                (long)(cb_max * 1000000 / freq), (unsigned long)cb_hist[0],
                (unsigned long)cb_hist[1], (unsigned long)cb_hist[2], (unsigned long)cb_hist[3]);
        if (!final) {
            cb_count = 0; cb_sum = 0; cb_min = -1; cb_max = 0;
            cb_hist[0] = cb_hist[1] = cb_hist[2] = cb_hist[3] = 0;
        }
    }
    stats_since = now;
    stats_frames = stats_ticks_run = stats_cap_hits = stats_clock_advanced = 0;
    for (int i = 0; i < 5; i++)
        stats_hist[i] = 0;
}

/* Called by the clock_read patch once per pass of run_screen_frame; returns the
 * count in EAX (the game uses AX). Also counts the ticks the previous frame ran. */
uint32_t __cdecl brr_clock_read(void)
{
    static int started;
    brr_install_key_filter();
    if (!started) {
        started = 1;
        choose_pacing();
    } else {
        uint8_t ran = *GAME_FRAME_TICKS;
        stats_frames++;
        stats_ticks_run += ran;
        stats_hist[ran > 4 ? 4 : ran]++;
        if (ran >= (cfg.fix_cap ? cfg.cap : 35))
            stats_cap_hits++;
    }
    {   /* log screen changes (screen numbers as in WndProc's dispatch) */
        static int last_screen = -1;
        int screen = *(volatile uint16_t *)0x4B13C0;
        if (screen != last_screen && cfg.log_clock) {
            static int logged;
            if (logged++ < 200)
                brr_log("screen: 0x%02X (game mode %d)", screen, *(volatile int16_t *)0x49C5C0);
            last_screen = screen;
        }
        brr_display_screen(screen);
    }
    int64_t wait_start = qpc_raw();
    /* in a window with vsync on, the vertical-blank wait paces an automatic limit */
    int vsync_paces = brr_vsync_active && cfg.frame_limit == FRAME_LIMIT_AUTO && *GAME_WINDOWED;
    if (frame_limit_hz && !vsync_paces) {
        int64_t period = freq / frame_limit_hz;
        int64_t now = qpc_raw();
        if (pace_next && now < pace_next)
            wait_until(pace_next);
        now = qpc_raw();
        /* next deadline; if we fell more than a frame behind, start again from now */
        pace_next = (pace_next && now - pace_next < period) ? pace_next + period : now + period;
    }
    int64_t now = qpc_raw();
    int64_t this_wait = now - wait_start;
    uint32_t fraction;
    uint32_t count = ticks_at(now, &fraction);
    frame_fraction_milli = fraction;
    if (started && last_count) {
        stats_clock_advanced += count - last_count;
        if (count - last_count > (uint32_t)(cfg.fix_cap ? cfg.cap : 35))
            brr_log("clock: long frame (screen 0x%02X; usually a screen loading): %lu ticks since the last frame (cap %d); last frame: "
                    "pacing wait %ld us, race ticks %ld us, race draw %ld us, everything else %ld us",
                    *(volatile uint16_t *)0x4B13C0, (unsigned long)(count - last_count), cfg.fix_cap ? cfg.cap : 35,
                    (long)(prof_wait * 1000000 / freq), (long)(brr_prof_ticks * 1000000 / freq),
                    (long)(brr_prof_draw * 1000000 / freq),
                    (long)((now - prof_read_exit - prof_wait - brr_prof_ticks - brr_prof_draw) * 1000000 / freq));
    }
    last_count = count;
    InterlockedExchange(&brr_timer_count, (LONG)count);
    last_frame_qpc = now;
    prof_wait = this_wait;
    brr_prof_draw = brr_prof_ticks = 0;
    if (cfg.log_clock && (brr_qpc() - stats_since) >= freq * 10)
        brr_clock_report(0);
    prof_read_exit = qpc_raw();
    return count;
}

/* Replaces vga_wait_retrace: wait for the next 1/70 s, as the VGA card's 70 Hz
 * refresh did, so palette fades keep their original speed. */
void __cdecl brr_retrace(void)
{
    int64_t now = qpc_raw();
    int64_t period_scaled = freq * 1000;  /* one refresh = freq*1000/70000 counts */
    int64_t scaled = (now - load_qpc) * 70000;
    int64_t next = (scaled / period_scaled + 1) * period_scaled;
    wait_until(load_qpc + (next + 69999) / 70000);
}
