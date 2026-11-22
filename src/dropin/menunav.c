/* Joystick menu navigation (the director, 2026-11-22), the shared core: PATCH.DLL's
 * [controller] menu_navigation and the DOS patch's menu_pad (both on by default) run this
 * same file on their own game's addresses (menunav.h).
 *
 * How it behaves, as if it had been in the 1996 game:
 * - The menus start as they always were: mouse and keyboard, the texts shimmering through
 *   the screen's fonts. Joystick 1's button 1 already chose (every screen with an OK lists
 *   it beside Return), button 2 goes back and the stick's left and right change the race,
 *   logo or car (the key twins added when a screen's script is read: keys.c, brrdos.c).
 * - Joystick 1 pushed up or down (or left or right on a screen that has no left and right of
 *   its own, such as car, view and race) gives the menus to the joystick: the top item is
 *   selected. The selected item stops shimmering (its texts keep their font of that moment,
 *   its animated buttons and borders their frame, and the sound screen's own buttons their
 *   step) and the mouse pointer is hidden (the game's, and Windows' own in a window); it
 *   still goes to the selection, 3/4 of the way along its text (or its box) and level with
 *   the middle of it, inside the box, for button 1's click. Up and down go through the items
 *   in reading order (a row's left to right, then the next row's), stopping at the ends, with
 *   a click (JOYCLICK, played as the game plays its BEEPs); left and right move to the
 *   nearest item that way in the same row. On the globe, logo and car screens, left and
 *   right are the screen's own. The globe has no items: button 1 is its OK, as Return, so the
 *   first press chooses the track the globe shows and the second goes on; the track screen
 *   after it starts on Race, which Return chooses there (the director, 2026-11-22).
 * - Button 1 is the left mouse button, at the selection: the game itself does the rest (a
 *   choice, a slider's - or +, a tick box), with its own sounds; the scripts' own button-1
 *   keys are hidden meanwhile, so they don't choose something else. Button 2 is Esc, as
 *   before, and also ends the attract demo and track select's flyover (View), as Esc does.
 * - Any input on the main menu (the joystick, a key, the mouse moved or clicked) starts its 15
 *   seconds before the attract demo again, as if the screen had just appeared (the 1996 game
 *   counts from the screen's start, whatever the mouse or keyboard do; the director: every
 *   input should hold it back).
 * - It carries on through every screen (the top item of each new one is selected), until a
 *   race starts, or the mouse is moved or clicked, or a key pressed: then the menus are the
 *   mouse's and keyboard's again.
 * - While it has the menus, player 1's controls are JS1 if they were Keys, until the player
 *   changes them by hand on the joysticks screen (then they're left alone).
 * - The name screen starts with the last name accepted (BRRNAME.TXT); button 1 takes it, and
 *   "CREATE NEW FILE?" takes button 1 as Y and button 2 as N. Not on the keys screen or the
 *   recording name screen (button 2 still goes back).
 *
 * Which items: the screen's mouse boxes (mbox lines), apart from the 2-pixel "back" boxes
 * in the corners, boxes under 16 pixels (the list cells of the recordings and the globe's
 * chosen tracks), a click anywhere, the sliders' bars (only their - and +), Windows' sound
 * screen's CD volume buttons (never drawn there), the arrows of the globe, logo and car
 * screens (the stick's left and right are theirs), and any whose texts and sprites weren't
 * drawn in the last half second (another mode's buttons). A text, sprite or animated sprite
 * belongs to the box its position is in (texts: the "@Xx,y" that starts them), the smallest
 * if several, or one whose corner is within 12 pixels. */
#include "menunav.h"

#define HOTSPOT_SIZE  0x24
#define MAX_HOTSPOTS  100
#define ANIM_SIZE     0x31
#define MAX_ANIMS     32
#define KEY_B1        0x73    /* joystick 1: buttons, then the stick */
#define KEY_B2        0x74
#define KEY_LEFT      0x77
#define KEY_RIGHT     0x78
#define KEY_UP        0x79
#define KEY_DOWN      0x7A
#define LAST_KEYBOARD 0x72    /* key-table indices up to here are the keyboard's */
#define SCREEN_GLOBE  0x00    /* the level select */
#define SCREEN_TRACK  0x01    /* the track: car, view, race */
#define ACTION_RACE   3       /* the track screen's Race (Return's choice there) */
#define SCREEN_MAIN   0x08
#define SCREEN_SOUND  0x0B
#define SCREEN_KEYS   0x0E
#define SCREEN_NAME   0x0F
#define SCREEN_VIEW   0x10    /* track select's View: the flyover */
#define SCREEN_RNAME  0x14    /* the recording's name */
#define SCREEN_STICKS 0x17
#define SCREEN_QUIT   0x7F
#define SCREEN_RACE   0x80
#define MODE_DEMO     2       /* game_mode: the attract demo */
#define ACTION_ESC    10      /* the race's and the flyover's Esc */
#define MOUSE_GRACE   20      /* ticks after a screen change when the mouse isn't watched */
#define DRAWN_TICKS   35      /* drawn within half a second: shown */
#define CLOSE         12      /* pixels: a box corner this close to a sprite or text */
#define MIN_ITEM      16

enum { B1, B2, LEFT = 4, RIGHT, UP, DOWN, JOY_KEYS };  /* 0x73 + n */
enum { SAVED_FONT = 1, SAVED_FRAME, SAVED_TYPE };

static int started, joy, screen, ticks_on_screen, selected, have_target, press_button, hid_b1;
static int pending_action, manual_type, full_name, owners_screen, owners_count, drawn_any, input_seen;
static int globe_again;
static int saved_index, saved_value, saved_what, still_set, still_step;
static unsigned char hid_value, held_over[2], hid_over[2];  /* buttons 1 and 2 */
static short target_x, target_y;
static unsigned long tick_serial;
static unsigned char joy_prev[JOY_KEYS], key_prev[LAST_KEYBOARD + 1];
static short owner[MAX_HOTSPOTS], anchor_x[MAX_HOTSPOTS], anchor_y[MAX_HOTSPOTS];
static short text_end[MAX_HOTSPOTS], frozen[MAX_HOTSPOTS];
static unsigned long drawn_at[MAX_HOTSPOTS];

/* A hotspot's fields, as shorts (the game changes them only between our calls: one thread):
 * +2 type (4 the pointer); mbox: +6 action, +8 x, +0xA y, +0xC width, +0xE height; stext: +4
 * the text, +8 font, +0xA letters shown (-1 hidden, 0x7FFF all); sprite: +6 x, +8 y; animated
 * sprite: +4 its animation, +6 its first frame, +0xA x, +0xC y (menu_hotspot_draw); key: +4
 * the key-table index */
#define HS(i) ((short *)(NAV_HOTSPOTS + (i) * HOTSPOT_SIZE))
enum { TYPE = 1, MB_ACTION = 3, MB_X = 4, MB_Y = 5, MB_W = 6, MB_H = 7, TX_FONT = 4, TX_SHOWN = 5,
       SP_X = 3, SP_Y = 4, AS_ANIM = 2, AS_X = 5, AS_Y = 6, KEY = 2 };

static int count(void)
{
    int n = NAV_COUNT;
    return n < 0 ? 0 : n > MAX_HOTSPOTS ? MAX_HOTSPOTS : n;
}

static void forget_screen(void)
{
    int i;
    for (i = 0; i < MAX_HOTSPOTS; i++) {
        owner[i] = -1;
        text_end[i] = -1;
        frozen[i] = -1;
        drawn_at[i] = 0;
    }
    selected = -1;
    have_target = press_button = drawn_any = full_name = 0;
    pending_action = -1;
    owners_screen = -1;
}

static void start(void)
{
    started = 1;
    screen = -1;
    saved_index = -1;
    forget_screen();
    nav_install_click();
}

/* the "@Xx,y" that starts a text: where text_draw puts its first letter */
static int text_anchor(const unsigned char *text, short *x, short *y)
{
    int i, n, v[2];
    if (!text)
        return 0;
    for (i = 0; i < 240 && text[i]; i++) {
        if (text[i] != '@' || text[i + 1] != 'X')
            continue;
        i += 2;
        for (n = 0; n < 2; n++) {
            if (text[i] < '0' || text[i] > '9')
                return 0;
            for (v[n] = 0; text[i] >= '0' && text[i] <= '9'; i++)
                v[n] = v[n] * 10 + text[i] - '0';
            if (n == 0 && text[i++] != ',')
                return 0;
        }
        *x = (short)v[0];
        *y = (short)v[1];
        return 1;
    }
    return 0;
}

static int distance(int a, int b)
{
    return a > b ? a - b : b - a;
}

/* Each text, sprite and animated sprite: its box (the smallest it's in, else one whose
 * corner is close). Kept while only hotspots are added (the main menu's credits ticker
 * takes the next one now and then). */
static void find_owners(void)
{
    int n = count(), d, m;
    for (d = 0; d < n; d++) {
        short *h = HS(d), x, y;
        int best = -1;
        long best_score = 0;
        owner[d] = -1;
        if (h[TYPE] == 1) {
            x = h[SP_X];
            y = h[SP_Y];
        } else if (h[TYPE] == 8) {
            x = h[AS_X];
            y = h[AS_Y];
        } else if (h[TYPE] != 2 || !text_anchor(*(const unsigned char **)(h + 2), &x, &y))
            continue;
        for (m = 0; m < n; m++) {
            short *b = HS(m);
            long score;
            if (b[TYPE] != 5 || b[MB_W] <= 2 || b[MB_H] <= 2)
                continue;
            if (x >= b[MB_X] && x < b[MB_X] + b[MB_W] && y >= b[MB_Y] && y < b[MB_Y] + b[MB_H])
                score = (long)b[MB_W] * b[MB_H];
            else {
                score = distance(x, b[MB_X]) + distance(y, b[MB_Y]);
                if (score > CLOSE)
                    continue;
                score += 0x40000000L;  /* only if it's in none */
            }
            if (best < 0 || score < best_score) {
                best = m;
                best_score = score;
            }
        }
        owner[d] = (short)best;
        anchor_x[d] = x;
        anchor_y[d] = y;
    }
    owners_screen = screen;
    owners_count = n;
}

/* A key hotspot for the stick's left or right with this action (any, with -1): the twins of
 * a screen's own arrow keys (globe, logo, car), so left and right are the screen's own there */
static int arrow_key(int action)
{
    int n = count(), i;
    for (i = 0; i < n; i++) {
        short *h = HS(i);
        if (h[TYPE] == 6 && (h[KEY] == KEY_LEFT || h[KEY] == KEY_RIGHT) && (action < 0 || h[MB_ACTION] == action))
            return 1;
    }
    return 0;
}

/* Boxes that aren't items: all of the globe's (its button 1 is its own OK key: see
 * nav_before_tick), the slider bars between the - and + buttons (and the sound screen's eight
 * steps each), the sound screen's CD volume - and + where the game doesn't draw them
 * (Windows: sound_options_draw draws only the effects volume), and the arrows where the
 * stick's left and right already do what they do */
static int left_out(int action)
{
    if (screen == SCREEN_GLOBE || arrow_key(action))
        return 1;
    if (screen == SCREEN_SOUND)
        return (action >= 10 && action <= 27) || (!NAV_SOUND_CD && action <= 1);
    if (screen == 0x16 || screen == 0x18)  /* landscape, game */
        return action == 1 || action == 4;
    return 0;
}

static int navigable(int m)
{
    int n = count(), d, owned = 0;
    short *b;
    if (m < 0 || m >= n)
        return 0;
    b = HS(m);
    if (b[TYPE] != 5 || b[MB_W] < MIN_ITEM || b[MB_H] < MIN_ITEM || (b[MB_W] >= 300 && b[MB_H] >= 190)
        || left_out(b[MB_ACTION]))
        return 0;
    for (d = 0; d < n; d++)
        if (owner[d] == m) {
            owned = 1;
            if (drawn_at[d] && tick_serial - drawn_at[d] <= DRAWN_TICKS && (HS(d)[TYPE] != 2 || HS(d)[TX_SHOWN] != -1))
                return 1;
        }
    return !owned;
}

/* One row: each box's middle is level with the other box (car, view and race, whose tops
 * differ by 10 pixels; a - and its +) */
static int same_row(const short *p, const short *q)
{
    int pm = p[MB_Y] + p[MB_H] / 2, qm = q[MB_Y] + q[MB_H] / 2;
    return pm >= q[MB_Y] && pm < q[MB_Y] + q[MB_H] && qm >= p[MB_Y] && qm < p[MB_Y] + p[MB_H];
}

/* reading order: row by row from the top, left to right in a row */
static int earlier(int a, int b)
{
    short *p = HS(a), *q = HS(b);
    if (same_row(p, q))
        return p[MB_X] < q[MB_X];
    return p[MB_Y] + p[MB_H] / 2 < q[MB_Y] + q[MB_H] / 2;
}

/* The item after (or before) one in reading order; with from -1, the first */
static int next_in_order(int from, int forward)
{
    int n = count(), m, best = -1;
    for (m = 0; m < n; m++) {
        if (m == from || !navigable(m))
            continue;
        if (from >= 0 && (forward ? !earlier(from, m) : !earlier(m, from)))
            continue;
        if (best < 0 || (forward ? earlier(m, best) : earlier(best, m)))
            best = m;
    }
    return best;
}

/* The item a screen starts with: the top one, or on the track screen Race, which Return
 * chooses there (the director: as Enter, unless the player moves left) */
static int first_item(void)
{
    int n = count(), m;
    if (screen == SCREEN_TRACK)
        for (m = 0; m < n; m++)
            if (HS(m)[MB_ACTION] == ACTION_RACE && navigable(m))
                return m;
    return next_in_order(-1, 1);
}

/* Left or right: the nearest item that way in the same row (the boxes level with each other),
 * wholly past the selection's middle; the least distance that way plus twice the distance
 * across */
static int next_item(int from, int way)
{
    int n = count(), m, best = -1;
    long best_score = 0;
    short *f = HS(from);
    int cx = f[MB_X] + f[MB_W] / 2, cy = f[MB_Y] + f[MB_H] / 2;
    for (m = 0; m < n; m++) {
        short *b = HS(m);
        int mx = b[MB_X] + b[MB_W] / 2;
        long score;
        if (m == from || !navigable(m) || b[MB_Y] >= f[MB_Y] + f[MB_H] || f[MB_Y] >= b[MB_Y] + b[MB_H])
            continue;
        if (way == LEFT && b[MB_X] + b[MB_W] <= cx)
            score = cx - mx;
        else if (way == RIGHT && b[MB_X] >= cx)
            score = mx - cx;
        else
            continue;
        score += 2L * distance(b[MB_Y] + b[MB_H] / 2, cy);
        if (best < 0 || score < best_score) {
            best = m;
            best_score = score;
        }
    }
    return best;
}

static void select_item(int m, int click)
{
    int n = count(), d;
    if (m == selected)
        return;
    for (d = 0; d < MAX_HOTSPOTS; d++)
        frozen[d] = -1;
    still_set = 0;
    selected = m;
    if (m < 0)
        return;
    for (d = 0; d < n; d++) {  /* what it shows now, it keeps */
        short *h = HS(d);
        if (owner[d] != m)
            continue;
        if (h[TYPE] == 2)
            frozen[d] = h[TX_FONT];
        else if (h[TYPE] == 8 && h[AS_ANIM] >= 0 && h[AS_ANIM] < MAX_ANIMS)
            frozen[d] = NAV_ANIMS[h[AS_ANIM] * ANIM_SIZE + 7];
    }
    NAV_IDLE = 0;
    if (click)
        nav_play_click();
}

/* The pointer: 3/4 along the item's texts (or its box), level with their middle, and always
 * inside the box, where button 1's click must land (a text can go on past its box: the damage
 * screen's "Damage:" ends with "(Fragile)" beyond it) */
static void aim(int m)
{
    int n = count(), d, x0 = 32767, x1 = -1, ty, tx;
    short *b = HS(m);
    tx = b[MB_X] + b[MB_W] * 3 / 4;
    ty = b[MB_Y] + b[MB_H] / 2;
    for (d = 0; d < n; d++)
        if (owner[d] == m && HS(d)[TYPE] == 2 && text_end[d] > anchor_x[d]) {
            if (anchor_x[d] < x0) {
                x0 = anchor_x[d];
                ty = anchor_y[d] + 8;  /* the menu fonts are 16 high */
            }
            if (text_end[d] > x1)
                x1 = text_end[d];
        }
    if (x1 > x0)
        tx = x0 + (x1 - x0) * 3 / 4;
    if (tx > b[MB_X] + b[MB_W] - 2)
        tx = b[MB_X] + b[MB_W] - 2;
    if (tx < b[MB_X] + 1)
        tx = b[MB_X] + 1;
    if (ty > b[MB_Y] + b[MB_H] - 2)
        ty = b[MB_Y] + b[MB_H] - 2;
    if (ty < b[MB_Y] + 1)
        ty = b[MB_Y] + 1;
    target_x = (short)(tx < 0 ? 0 : tx > 319 ? 319 : tx);
    target_y = (short)(ty < 0 ? 0 : ty > 199 ? 199 : ty);
    have_target = 1;
}

static void take(void)
{
    joy = 1;
    nav_pointer(0);
}

static void leave(void)
{
    int d;
    joy = 0;
    selected = -1;
    have_target = press_button = 0;
    for (d = 0; d < MAX_HOTSPOTS; d++)
        frozen[d] = -1;
    nav_pointer(1);
}

static void changed_screen(int s)
{
    if (screen == SCREEN_NAME && s != SCREEN_QUIT && NAV_NAME[0])
        nav_name_save((const char *)NAV_NAME);
    screen = s;
    ticks_on_screen = 0;
    forget_screen();
    if (s == SCREEN_NAME && !NAV_NAME[0]) {  /* the last name, ready to take */
        char name[16];
        int length = nav_name_load(name, sizeof name), i;
        for (i = 0; i < length; i++)
            if (!((name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= '0' && name[i] <= '9')))
                length = 0;
        if (length > 0 && length <= 8) {
            for (i = 0; i < length; i++)
                NAV_NAME[i] = (unsigned char)name[i];
            NAV_NAME[length] = 0;
            /* eight letters finish the screen at once (name_entry_tick): held at 7 until a key */
            NAV_NAME_LEN = (short)(length == 8 ? 7 : length);
            full_name = length == 8;
            nav_note("name screen: the last name (BRRNAME.TXT), letters", length);
        }
    }
}

static void name_screen(const unsigned char *pressed)
{
    if (full_name && NAV_KEY_DOWN) {
        int c = NAV_KEY_CHARS[NAV_KEY_DOWN];
        if (c == 0x81) {         /* delete: from the eighth letter */
            NAV_NAME_LEN = 8;
            full_name = 0;
        } else if (c != 0x83)    /* a full name takes no more letters (Return takes it) */
            NAV_KEY_DOWN = 0;
    }
    if (pressed[B1]) {           /* take the name (Y to "CREATE NEW FILE?") */
        pending_action = NAV_NAME_ASK ? 2 : 0;
        if (!joy) {
            take();
            nav_note("joystick on: the name screen's button 1", SCREEN_NAME);
        }
    } else if (pressed[B2] && NAV_NAME_ASK)
        pending_action = 3;      /* N (not Esc's quit) */
}

void nav_before_tick(void)
{
    volatile unsigned char *keys = NAV_KEYS;
    unsigned char now[JOY_KEYS], pressed[JOY_KEYS];
    int s, i, keyboard = 0, own, way = -1, chose = 0;
    hid_b1 = input_seen = 0;
    if (!NAV_ENABLED || !keys)
        return;
    if (!started)
        start();
    tick_serial++;
    s = NAV_SCREEN;
    for (i = 0; i < JOY_KEYS; i++) {
        now[i] = keys[KEY_B1 + i] != 0;
        pressed[i] = now[i] && !joy_prev[i];
        input_seen |= pressed[i];
        joy_prev[i] = now[i];
    }
    for (i = 1; i <= LAST_KEYBOARD; i++) {
        unsigned char down = keys[i] != 0;
        if (down && !key_prev[i])
            keyboard = input_seen = 1;
        key_prev[i] = down;
    }
    if (s != screen) {
        changed_screen(s);
        held_over[B1] = now[B1];  /* pressed for the screen before */
        held_over[B2] = now[B2];
    }
    ticks_on_screen++;
    /* A button still held from the screen before can't act here too (the new screen's first
     * tick would take it again: one press, two screens): hidden until it's let go. Keys.c does
     * the same for every key on Windows; the DOS menus had nothing. */
    for (i = B1; i <= B2; i++) {
        hid_over[i] = 0;
        if (!now[i])
            held_over[i] = 0;
        else if (held_over[i] && s != SCREEN_RACE) {
            hid_over[i] = keys[KEY_B1 + i];
            keys[KEY_B1 + i] = 0;
        }
    }
    /* Button 2 ends the attract demo and the flyover as Esc does (both quit on action 10 after
     * the hotspots: race_keys_tick, track_preview_tick) */
    if (pressed[B2] && ((s == SCREEN_RACE && NAV_GAME_MODE == MODE_DEMO) || s == SCREEN_VIEW))
        pending_action = ACTION_ESC;
    /* The globe: button 1 is its own OK key (the script's key 0,&73; the globe has no items
     * for the joystick, so it isn't hidden). OK with no race chosen chooses the track under
     * the globe's cursor, but there is none on arrival (globe_enter), so that first press only
     * picks one: OK is pressed again on the next tick, so one press chooses the track and the
     * next goes on (the director) */
    if (globe_again && s == SCREEN_GLOBE)
        pending_action = 0;
    globe_again = s == SCREEN_GLOBE && pressed[B1] && NAV_GLOBE_TRACK == -1 && NAV_GLOBE_RACES < 1;
    if (s == SCREEN_RACE) {
        if (joy) {
            leave();
            nav_note("joystick off: a race", s);
        }
        return;
    }
    if (keyboard && joy) {
        leave();
        nav_note("joystick off: a key", s);
    }
    if (s == SCREEN_NAME) {
        name_screen(pressed);
        return;
    }
    if (s == SCREEN_KEYS || s == SCREEN_RNAME || s == SCREEN_VIEW || s >= SCREEN_QUIT || count() == 0)
        return;
    own = arrow_key(-1);
    if (!joy && (pressed[UP] || pressed[DOWN] || (!own && (pressed[LEFT] || pressed[RIGHT])))) {
        take();
        nav_note("joystick on, screen", s);
        selected = -1;
        chose = 1;  /* the push that starts it selects the top item */
    }
    if (!joy)
        return;
    if (!manual_type && NAV_PLAYER1 == 0) {
        NAV_PLAYER1 = 1;
        nav_note("player 1 set to JS1, screen", s);
    }
    if (owners_screen != s || owners_count != count())
        find_owners();
    if (!navigable(selected)) {
        if (!drawn_any && ticks_on_screen < 10)  /* the screen's first picture first */
            selected = -1;
        else
            select_item(first_item(), chose);  /* the top item (or the track screen's Race) */
        chose = 1;
    }
    if (!chose && selected >= 0) {
        way = pressed[UP] ? UP : pressed[DOWN] ? DOWN : own ? -1 : pressed[LEFT] ? LEFT : pressed[RIGHT] ? RIGHT : -1;
        if (way >= 0) {
            int m = way >= UP ? next_in_order(selected, way == DOWN) : next_item(selected, way);
            if (m >= 0)
                select_item(m, 1);
        }
    }
    if (selected >= 0) {
        if (pressed[B1])
            press_button = 1;
        if (now[B1]) {  /* the selection's click instead of the scripts' button-1 keys */
            hid_value = keys[KEY_B1];
            keys[KEY_B1] = 0;
            hid_b1 = 1;
        }
        aim(selected);
    } else
        have_target = 0;
    if (!now[B1])
        press_button = 0;
}

void nav_after_tick(void)
{
    int i;
    if (!NAV_ENABLED || !started)
        return;
    /* Any input on the main menu (a joystick press, a key, or the mouse moved or clicked: the
     * game's own idle count, menu_idle_ticks, back at 0 after the hotspots' tick) starts the
     * attract demo's 15 seconds again (main_menu_tick counts from the screen's start, whatever
     * the input, in 1996); the credits ticker counts from the same clock */
    if (NAV_SCREEN == SCREEN_MAIN && (input_seen || NAV_IDLE == 0)) {
        NAV_CREDITS_TICK -= NAV_TICK_COUNT;
        NAV_TICK_COUNT = 0;
    }
    if (hid_b1) {  /* put back in the reverse order: a held-over button 1 was hidden first */
        NAV_KEYS[KEY_B1] = hid_value;
        hid_b1 = 0;
    }
    for (i = B1; i <= B2; i++)
        if (hid_over[i]) {
            NAV_KEYS[KEY_B1 + i] = hid_over[i];
            hid_over[i] = 0;
        }
    if (pending_action >= 0) {
        NAV_ACTION = (short)pending_action;
        pending_action = -1;
    }
    if (NAV_SCREEN == SCREEN_STICKS && NAV_ACTION == 3 && !manual_type) {
        manual_type = 1;
        nav_note("player 1's controls set by hand (left alone now), type", NAV_PLAYER1);
    }
}

void nav_mouse(int moved, int buttons, short *x, short *y, short *buttons_now)
{
    if (!NAV_ENABLED || !started || NAV_SCREEN == SCREEN_RACE)
        return;
    if (joy && (moved || buttons) && ticks_on_screen > MOUSE_GRACE) {
        leave();
        nav_note("joystick off: the mouse", NAV_SCREEN);
    }
    if (!joy || !have_target)
        return;
    *x = target_x;
    *y = target_y;
    /* the left button while button 1 is held (and the real buttons otherwise: without a DOS
     * mouse driver nothing else would let go of it) */
    *buttons_now = (short)(press_button ? 1 : buttons);
}

void nav_before_draw(int d)
{
    short *h;
    saved_index = -1;
    if (!NAV_ENABLED || !started || d < 0 || d >= MAX_HOTSPOTS)
        return;
    drawn_at[d] = tick_serial;
    drawn_any = 1;
    if (!joy)
        return;
    h = HS(d);
    saved_index = d;
    if (h[TYPE] == 4) {  /* the pointer isn't drawn while the joystick has the menus */
        saved_what = SAVED_TYPE;
        h[TYPE] = 0;
    } else if (frozen[d] < 0)
        saved_index = -1;
    else if (h[TYPE] == 2) {
        saved_what = SAVED_FONT;
        saved_value = h[TX_FONT];
        h[TX_FONT] = frozen[d];
    } else if (h[TYPE] == 8 && h[AS_ANIM] >= 0 && h[AS_ANIM] < MAX_ANIMS) {
        saved_what = SAVED_FRAME;
        saved_value = NAV_ANIMS[h[AS_ANIM] * ANIM_SIZE + 7];
        NAV_ANIMS[h[AS_ANIM] * ANIM_SIZE + 7] = (unsigned char)frozen[d];
    } else
        saved_index = -1;
}

void nav_after_draw(int d)
{
    short *h;
    if (!NAV_ENABLED || !started || d < 0 || d >= MAX_HOTSPOTS)
        return;
    h = HS(d);
    if (saved_index == d) {
        if (saved_what == SAVED_TYPE)
            h[TYPE] = 4;
        else if (saved_what == SAVED_FONT)
            h[TX_FONT] = (short)saved_value;
        else
            NAV_ANIMS[h[AS_ANIM] * ANIM_SIZE + 7] = (unsigned char)saved_value;
        saved_index = -1;
    }
    if (h[TYPE] == 2 && h[TX_SHOWN] == 0x7FFF)  /* all of it drawn: where it ends */
        text_end[d] = NAV_TEXT_X;
}

/* The sound screen's own buttons (sound_options_draw's sprite_draw calls, through thunks.asm:
 * the volume - and +, OK and the commentary tick box), whose frames follow the screen's own
 * step (sound_options_anim, 0-4) by per_step frames a step: in the selection they keep the
 * step they showed when first drawn after it was selected (and the tick box its new state
 * when toggled) */
int nav_sprite_frame(int frame, int x, int y, int per_step)
{
    short *b;
    if (!NAV_ENABLED || !joy || selected < 0)
        return frame;
    b = HS(selected);
    if (x < b[MB_X] || x >= b[MB_X] + b[MB_W] || y < b[MB_Y] || y >= b[MB_Y] + b[MB_H])
        return frame;
    if (!still_set) {
        still_set = 1;
        still_step = NAV_SOUND_STEP;
    }
    return frame + per_step * (still_step - NAV_SOUND_STEP);
}
