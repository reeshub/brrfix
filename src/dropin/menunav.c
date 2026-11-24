/* Joystick (and arrow-key) menu navigation (the director, 2026-11-22), the shared core: PATCH.DLL's
 * [controller] menu_navigation and the DOS patch's menu_pad (both on by default) run this
 * same file on their own game's addresses (menunav.h).
 *
 * How it behaves, as if it had been in the 1996 game:
 * - In 1996 the menus were the mouse's and keyboard's, the texts shimmering through the
 *   screen's fonts. Joystick 1's button 1 already chose (every screen with an OK lists it
 *   beside Return), button 2 goes back and the stick's left and right change the race, logo
 *   or car (the key twins added when a screen's script is read: keys.c, brrdos.c).
 * - Joystick 1 pushed up or down (or left or right on a screen that has no left and right of
 *   its own, such as car, view and race; on Windows also a pad's D-pad, controller.c) gives
 *   the menus to the joystick: the top item is selected. Its other inputs (buttons 1 and 2,
 *   left and right where they're the screen's own, and the keys below, Esc too) give them as
 *   well, after doing their usual job that once (the director, 2026-11-24). The selected
 *   item stops shimmering (its texts keep their font of that moment, its animated buttons
 *   and borders their frame, and the sound screen's own buttons their step) and the mouse
 *   pointer is hidden (the game's, and Windows' own in a window); it still goes to the
 *   middle of the selection's box, for button 1's click. Up and down go through the items
 *   in reading order (a row's left to right, then the next row's), stopping at the ends, with
 *   a click (JOYCLICK, played as the game plays its BEEPs); left and right move to the
 *   nearest item that way in the same row. On the globe, logo and car screens, left and
 *   right are the screen's own. The globe has no items: button 1 is its OK, as Return, so the
 *   first press chooses the track the globe shows and the second goes on; the track screen
 *   after it starts on Race, which Return chooses there (the director, 2026-11-22).
 * - Button 1 is the left mouse button, at the selection: the game itself does the rest (a
 *   choice, a tick box), with its own sounds; the scripts' own button-1 keys are hidden
 *   meanwhile, so they don't choose something else. Button 2 is Esc, as before, and also ends
 *   the attract demo and track select's flyover (View), as Esc does.
 * - A slider (display's vision and texture distances, the sound screen's CD and effects
 *   volumes, the damage screen's PL1 and PL2) is one item: its -, its +, its name, and its bar
 *   and knob or level (which animate or stay still with it, the director, 2026-11-24). Up and
 *   down go past it like any item; left and right are its - and + (the game's own action for
 *   each, one step a push), and button 1 does nothing there (the director, 2026-11-24; holding
 *   button 1 on a - had stepped it every tick, and display's two distances push each other).
 * - The keys screen gives every key to its blinking row, so the navigation only shows that row
 *   as the selection there (its name animates or stays still, as any selection).
 * - Any input on the main menu (the joystick, a key, the mouse moved or clicked) starts its 15
 *   seconds before the attract demo again, as if the screen had just appeared (the 1996 game
 *   counts from the screen's start, whatever the mouse or keyboard do; the director: every
 *   input should hold it back).
 * - The keyboard's arrow keys (and the keypad's) do what the stick does, and start it the same
 *   way; Return and keypad Enter click the selection, as button 1 (with nothing selected, as
 *   on the globe, they're the scripts' own, as in 1996). Esc stays the scripts' back.
 * - Menu animations (the director, 2026-11-22; [controller] menu_animations, DOS option
 *   menu_anim): original-ish, as above, the selection still; or inverted: every other item
 *   is held still (as it looked when it stopped being the selection, or when first drawn)
 *   and only the selection animates (the default since 2026-11-24). The screens' own pictures
 *   animate as ever; their headings too, unless still titles is on ([controller] still_titles,
 *   on by default with inverted; DOS: with inverted), which holds their texts as first drawn,
 *   except on the screens that are one choice (name, globe, vehicle, driver, logo, recordings).
 *   A new screen is held from its very first picture, drawn by its enter routine before its
 *   first tick (no flash of animation as it appears; the director, 2026-11-23). An item's
 *   labels go with it: a text in no box labels the row of items beside it, or just below it
 *   (the damage screen's "PL1:" and "PL2:", the sound screen's "FX", the landscape screen's
 *   slider names), and the joysticks screen's "Keys/Custom" or "JS1" take their line's font.
 * - While the joystick or the arrow keys have the menus, the buttons only the mouse needs
 *   aren't drawn: the globe's Select and car select's Next (the director, 2026-11-23).
 * - The menus start with it (the director, 2026-11-24): the main menu after the name screen has
 *   its top item selected and no pointer from its first picture. It carries on through every
 *   screen (the top item of each new one is selected), through races (the results come back to
 *   it), until the mouse is moved: then the menus are the mouse's again, with the pointer, and
 *   all animating, until the stick, a button or a navigation key is used again. Other keys are
 *   the scripts' own and leave it as it is.
 * - While the joystick has been used for it, player 1's controls are JS1 if they were Keys
 *   (not for the arrow keys alone), until the player changes them by hand on the joysticks
 *   screen (then they're left alone).
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
#define JOYCLICK_DATA
#include "joyclick.h"

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
#define SCREEN_CAR    0x03    /* car select */
#define ACTION_RACE   3       /* the track screen's Race (Return's choice there) */
#define SCREEN_MAIN   0x08
#define SCREEN_CHAR   0x0A    /* the driver */
#define SCREEN_SOUND  0x0B
#define SCREEN_LOGO   0x0D
#define SCREEN_KEYS   0x0E
#define SCREEN_NAME   0x0F
#define SCREEN_VIEW   0x10    /* track select's View: the flyover */
#define SCREEN_RNAME  0x14    /* the recording's name */
#define SCREEN_LAND   0x16    /* display: vision and texture distances */
#define SCREEN_STICKS 0x17
#define SCREEN_GAME   0x18    /* damage */
#define SCREEN_QUIT   0x7F
#define SCREEN_RACE   0x80
#define SCREEN_RECO   0x83    /* "Load Recording" */
#define MODE_DEMO     2       /* game_mode: the attract demo */
#define ACTION_ESC    10      /* the race's and the flyover's Esc */
#define MOUSE_GRACE   20      /* ticks after a screen change when the mouse isn't watched */
#define DRAWN_TICKS   35      /* drawn within half a second: shown */
#define CLOSE         12      /* pixels: a box corner this close to a sprite or text */
#define MIN_ITEM      16
#define TEXT_HIGH     16      /* the menu fonts */
#define LABEL_REACH   110     /* pixels: from a label to the first item of its row */
#define KEYS_REACH    200     /* the keys screen's: its names at x 10, their keys' boxes at 185 */
#define KEYS_FIRST    10      /* the keys screen's first row's box (action), then one a row */

enum { B1, B2, LEFT = 4, RIGHT, UP, DOWN, JOY_KEYS };  /* 0x73 + n */
enum { SAVED_FONT = 1, SAVED_FRAME, SAVED_TYPE };
#define KEY_ESC       0x01
#define KEY_RETURN    0x1C
#define KEY_PAD_ENTER 0x70

/* The keyboard's navigation keys (key-table indices, the same on Windows and DOS:
 * key_char_table): the arrows and the keypad's, as the stick's directions, then Return and
 * keypad Enter, which click the selection as button 1 does */
#define KEYBOARD_ARROWS 8
static const unsigned char keyboard_nav[] = { 0x4B, 0x6B, 0x4D, 0x6D, 0x48, 0x68, 0x50, 0x64,
                                              KEY_RETURN, KEY_PAD_ENTER };
#define KEYBOARD_NAV  (sizeof keyboard_nav)
/* The sliders: each is one item, its - and + with the label of their row; left and right are
 * its - and + (the director, 2026-11-24). The screen, then the - box's action and the +'s. */
static const unsigned char sliders[][3] = { { SCREEN_SOUND, 0, 1 }, { SCREEN_SOUND, 2, 3 },
                                            { SCREEN_LAND, 0, 2 }, { SCREEN_LAND, 3, 5 },
                                            { SCREEN_GAME, 0, 2 }, { SCREEN_GAME, 3, 5 } };
#define SLIDERS       (sizeof sliders / sizeof sliders[0])
/* Held from the screen before: hidden until let go (button 1, button 2, Return, keypad Enter) */
static const unsigned char held_keys[] = { KEY_B1, KEY_B2, KEY_RETURN, KEY_PAD_ENTER };
#define HELD_KEYS     (sizeof held_keys)

static int started, joy, screen, ticks_on_screen, selected, have_target, press_button, hid_clicks;
static int pending_action, manual_type, full_name, owners_screen, owners_count, drawn_any, input_seen;
static int globe_again, stick_used, arrived;
static int saved_index, saved_value, saved_what, still_set, still_step;
static unsigned char hid_click[3], held_over[HELD_KEYS], hid_over[HELD_KEYS];
static short target_x, target_y;
static unsigned long tick_serial;
static unsigned char joy_prev[JOY_KEYS], key_prev[LAST_KEYBOARD + 1];
static short owner[MAX_HOTSPOTS], frozen[MAX_HOTSPOTS];
static short row_of[MAX_HOTSPOTS];        /* a label's row: one of its items (find_owners) */
static unsigned char item[MAX_HOTSPOTS];  /* a box that can be an item (find_owners) */
static short plus_of[MAX_HOTSPOTS];       /* a slider's - box: its + box (find_owners) */
static short slider_of[MAX_HOTSPOTS];     /* a slider's + and bar boxes: its - box */
static unsigned long drawn_at[MAX_HOTSPOTS];

/* A hotspot's fields, as shorts (the game changes them only between our calls: one thread):
 * +2 type (4 the pointer); mbox: +6 action, +8 x, +0xA y, +0xC width, +0xE height; stext: +4
 * the text, +8 font, +0xA letters shown (-1 hidden, 0x7FFF all); sprite: +6 x, +8 y; animated
 * sprite: +4 its animation, +6 its first frame, +0xA x, +0xC y (menu_hotspot_draw); key: +4
 * the key-table index */
#define HS(i) ((short *)(NAV_HOTSPOTS + (i) * HOTSPOT_SIZE))
enum { TYPE = 1, MB_ACTION = 3, MB_X = 4, MB_Y = 5, MB_W = 6, MB_H = 7, TX_FONT = 4, TX_SHOWN = 5,
       SP_X = 3, SP_Y = 4, AS_ANIM = 2, AS_X = 5, AS_Y = 6, KEY = 2 };
/* The sliders' knobs: animation 1 (anim_frame(0, 1)), its step (+7) and its frames (+9 on) */
#define KNOB_STEP     (NAV_ANIMS[ANIM_SIZE + 7])
#define KNOB_FRAME(s) (NAV_ANIMS[ANIM_SIZE + 9 + (s)])

static int count(void)
{
    int n = NAV_COUNT;
    return n < 0 ? 0 : n > MAX_HOTSPOTS ? MAX_HOTSPOTS : n;
}

static void forget_screen(void)
{
    int i;
    for (i = 0; i < MAX_HOTSPOTS; i++) {
        owner[i] = row_of[i] = -1;
        frozen[i] = -1;
        drawn_at[i] = 0;
    }
    selected = -1;
    have_target = press_button = drawn_any = full_name = still_set = 0;
    pending_action = -1;
    owners_screen = -1;
}

unsigned char nav_click_wav[JOYCLICK_WAV_SIZE];

/* JOYCLICK.WAV from its header and half-rate samples (joyclick.h): each stored sample, then
 * the mean of it and the next one (sound-convert's rebuild) */
static void build_click(void)
{
    unsigned char *out = nav_click_wav + JOYCLICK_HEADER;
    int i;
    for (i = 0; i < JOYCLICK_HEADER; i++)
        nav_click_wav[i] = joyclick_header[i];
    for (i = 0; i < JOYCLICK_HALF; i++) {
        *out++ = joyclick_half[i];
        *out++ = i + 1 < JOYCLICK_HALF ? (unsigned char)((joyclick_half[i] + joyclick_half[i + 1] + 1) / 2)
                                       : joyclick_half[i];
    }
}

static void start(void)
{
    started = 1;
    build_click();
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

/* Each text, sprite and animated sprite: its box (the smallest it's in, a text by its middle
 * line, else one whose corner is close). A text in no box is a label, of the row of items
 * starting at most LABEL_REACH pixels to its right on its line, or just below it (a caption):
 * the damage screen's "PL1:" and "PL2:", the sound screen's "FX", the landscape screen's slider
 * names. Kept while only hotspots are added (the main menu's credits ticker takes the next one
 * now and then). */
static int left_out(int action);

static int box_with(int action)
{
    int n = count(), m;
    for (m = 0; m < n; m++)
        if (item[m] && HS(m)[MB_ACTION] == action)
            return m;
    return -1;
}

/* (x, y) in item m's box; for a slider anywhere along it, from its - to its +, and a little
 * above their tops (its bar, and the knob or level the screen's own code draws there) */
static int in_item(int m, int x, int y)
{
    short *b = HS(m), *r = plus_of[m] >= 0 ? HS(plus_of[m]) : b;
    return x >= b[MB_X] && x < r[MB_X] + r[MB_W] && y >= b[MB_Y] - (r != b ? CLOSE : 0) && y < b[MB_Y] + b[MB_H];
}

static void find_owners(void)
{
    int n = count(), d, m, reach = screen == SCREEN_KEYS ? KEYS_REACH : LABEL_REACH;
    unsigned j;
    for (m = 0; m < n; m++) {
        short *b = HS(m);
        item[m] = b[TYPE] == 5 && b[MB_W] >= MIN_ITEM && b[MB_H] >= MIN_ITEM && !(b[MB_W] >= 300 && b[MB_H] >= 190)
                  && !left_out(b[MB_ACTION]);
        plus_of[m] = slider_of[m] = -1;
    }
    for (j = 0; j < SLIDERS; j++) {
        int minus, plus;
        if (sliders[j][0] != screen || (minus = box_with(sliders[j][1])) < 0 || (plus = box_with(sliders[j][2])) < 0)
            continue;
        plus_of[minus] = (short)plus;
        for (m = 0; m < n; m++)  /* its +, and its bar's boxes (display's and damage's scaler, the volume steps) */
            if (m == plus || (!item[m] && HS(m)[TYPE] == 5 && in_item(minus, HS(m)[MB_X], HS(m)[MB_Y])))
                slider_of[m] = (short)minus;
    }
    for (d = 0; d < n; d++) {
        short *h = HS(d), x, y, middle;
        int best = -1;
        long best_score = 0;
        owner[d] = row_of[d] = -1;
        if (h[TYPE] == 1) {
            x = h[SP_X];
            y = h[SP_Y];
        } else if (h[TYPE] == 8) {
            x = h[AS_X];
            y = h[AS_Y];
        } else if (h[TYPE] != 2 || !text_anchor(*(const unsigned char **)(h + 2), &x, &y))
            continue;
        middle = h[TYPE] == 2 ? y + TEXT_HIGH / 2 : y;
        for (m = 0; m < n; m++) {
            short *b = HS(m);
            long score;
            if (b[TYPE] != 5 || b[MB_W] <= 2 || b[MB_H] <= 2)
                continue;
            if (x >= b[MB_X] && x < b[MB_X] + b[MB_W] && middle >= b[MB_Y] && middle < b[MB_Y] + b[MB_H])
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
        if (best >= 0 || h[TYPE] != 2)
            continue;
        for (m = 0; m < n; m++) {  /* a label: the nearest row, on its line first */
            short *b = HS(m);
            long score;
            if (!item[m] || b[MB_X] <= x || b[MB_X] - x > reach)
                continue;
            if (b[MB_Y] < y + TEXT_HIGH && b[MB_Y] + b[MB_H] > y)
                score = 0;
            else if (b[MB_Y] >= y + TEXT_HIGH && b[MB_Y] - (y + TEXT_HIGH) <= CLOSE)
                score = b[MB_Y] - (y + TEXT_HIGH);
            else
                continue;
            score = score * 1000 + b[MB_X] - x;
            if (best < 0 || score < best_score) {
                best = m;
                best_score = score;
            }
        }
        row_of[d] = (short)best;
    }
    owners_screen = screen;
    owners_count = n;
}

/* The owners, found again when the screen or its hotspots have changed */
static void owners(void)
{
    if (owners_screen != screen || owners_count != count())
        find_owners();
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
    if (m < 0 || m >= n || !item[m] || slider_of[m] >= 0)
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

/* What a text (its font) or an animated sprite (its animation's step) shows now; -1 for
 * anything else */
static int shown_now(int d)
{
    short *h = HS(d);
    if (h[TYPE] == 2)
        return h[TX_FONT];
    if (h[TYPE] == 8 && h[AS_ANIM] >= 0 && h[AS_ANIM] < MAX_ANIMS)
        return NAV_ANIMS[h[AS_ANIM] * ANIM_SIZE + 7];
    return -1;
}

/* Part of item m: its own texts and pictures (a slider's: its +'s and its bar's too), and the
 * labels of its row */
static int part_of(int d, int m)
{
    int o = owner[d];
    return m >= 0 && (o == m || (o >= 0 && slider_of[o] == m) || (row_of[d] >= 0 && same_row(HS(row_of[d]), HS(m))));
}

/* A heading, or another of the screen's own pictures: part of no item */
static int heading(int d)
{
    int o = owner[d];
    return row_of[d] < 0 && (o < 0 || (!item[o] && slider_of[o] < 0));
}

/* The screens that are one choice (the name, the globe, the vehicle, the driver, the logo, the
 * recordings), whose headings keep animating with still titles (the director, 2026-11-24) */
static int single_purpose(void)
{
    return screen == SCREEN_GLOBE || screen == SCREEN_CAR || screen == SCREEN_CHAR || screen == SCREEN_LOGO
        || screen == SCREEN_NAME || screen == SCREEN_RNAME || screen == SCREEN_RECO;
}

/* What d is held at as it's drawn now, -1 if it animates. Inverted: every item's texts and
 * pictures but the selection's, as they were when first drawn unselected (each held when it
 * stops being the selection, or as the screen appears). Original-ish: the selection's,
 * from select_item. Still titles: the headings' texts too, as first drawn. */
static int hold(int d)
{
    if (heading(d)) {
        if (!NAV_STILL_TITLES || HS(d)[TYPE] != 2 || single_purpose())
            frozen[d] = -1;
        else if (frozen[d] < 0)
            frozen[d] = (short)shown_now(d);
    } else if (NAV_INVERTED) {
        if (part_of(d, selected))
            frozen[d] = -1;
        else if (frozen[d] < 0)
            frozen[d] = (short)shown_now(d);
    }
    return frozen[d];
}

/* Buttons only the mouse needs, not drawn while the joystick or the arrow keys have the menus
 * (the director, 2026-11-23): the globe's Select (button 1 and Return choose the track there)
 * and car select's Next (left and right change the vehicle) */
static int hidden(int m)
{
    int a;
    if (m < 0)
        return 0;
    a = HS(m)[MB_ACTION];
    return (screen == SCREEN_GLOBE && a == 4) || (screen == SCREEN_CAR && a == 0);
}

/* Original-ish: the selection keeps what it shows as it's selected. Inverted: the others are
 * held as they're first drawn unselected (hold), and the sound screen's buttons keep the step
 * they first had */
static void select_item(int m, int click)
{
    int n = count(), d;
    if (m == selected)
        return;
    selected = m;
    if (!NAV_INVERTED) {
        still_set = 0;
        for (d = 0; d < n; d++)
            if (!heading(d))
                frozen[d] = part_of(d, m) ? (short)shown_now(d) : -1;
    }
    if (m < 0)
        return;
    NAV_IDLE = 0;
    if (click)
        nav_play_click();
}

/* The pointer, hidden: in the middle of the item's box, where button 1's click must land (it
 * was 3/4 along the item's text, level with its middle, while it was shown: the director had
 * it hidden, 2026-11-22) */
static void aim(int m)
{
    short *b = HS(m);
    target_x = (short)(b[MB_X] + b[MB_W] / 2);
    target_y = (short)(b[MB_Y] + b[MB_H] / 2);
    have_target = 1;
}

static void take(void)
{
    joy = 1;
    stick_used = 0;
    nav_pointer(0);
}

static void leave(void)
{
    int d;
    joy = 0;
    selected = -1;
    have_target = press_button = still_set = 0;
    for (d = 0; d < MAX_HOTSPOTS; d++)
        frozen[d] = -1;
    nav_pointer(1);
}

static void changed_screen(int s)
{
    if (screen == SCREEN_NAME && s != SCREEN_QUIT && NAV_NAME[0])
        nav_name_save((const char *)NAV_NAME);
    /* the menus start with the navigation (the director, 2026-11-24): the main menu after the
     * name has its top item selected and no pointer, from its first picture, until the mouse moves */
    if (screen == SCREEN_NAME && s == SCREEN_MAIN && !joy) {
        take();
        nav_note("on: the menus' start", s);
    }
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

/* A new screen, noticed by the first of the hooks to see it: its enter routine draws it (and
 * fades it in) before its first tick, so the draw hooks look too, and that first picture
 * already has only the selection animating, or only it still (the director, 2026-11-23: no
 * flash of animation as a screen appears). The tick does the rest (arrived). */
static void follow_screen(void)
{
    if (NAV_SCREEN != screen) {
        changed_screen(NAV_SCREEN);
        arrived = 1;
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
            stick_used = 1;
            nav_note("on: the name screen's button 1", SCREEN_NAME);
        }
    } else if (pressed[B2] && NAV_NAME_ASK)
        pending_action = 3;      /* N (not Esc's quit) */
}

void nav_before_tick(void)
{
    volatile unsigned char *keys = NAV_KEYS;
    unsigned char now[JOY_KEYS], pressed[JOY_KEYS], stick[JOY_KEYS];
    int s, i, own, way = -1, chose = 0, keyboard_nav_on, stick_pressed = 0, esc;
    unsigned j;
    hid_clicks = input_seen = 0;
    if (!NAV_ENABLED || !keys)
        return;
    if (!started)
        start();
    tick_serial++;
    s = NAV_SCREEN;
    for (i = 0; i < JOY_KEYS; i++) {
        now[i] = keys[KEY_B1 + i] != 0;
        stick[i] = pressed[i] = now[i] && !joy_prev[i];
        stick_pressed |= pressed[i];
        joy_prev[i] = now[i];
    }
    input_seen = stick_pressed;
    /* The keyboard's navigation keys join the stick's (not where keys are typed or given to
     * controls); other keys are the scripts' own, as ever, and leave the navigation on: only the
     * mouse moving hands the menus back (the director, 2026-11-24) */
    keyboard_nav_on = s != SCREEN_KEYS && s != SCREEN_NAME && s != SCREEN_RNAME;
    esc = keys[KEY_ESC] && !key_prev[KEY_ESC];
    for (i = 1; i <= LAST_KEYBOARD; i++) {
        unsigned char down = keys[i] != 0, fresh = down && !key_prev[i];
        key_prev[i] = down;
        input_seen |= fresh;
        for (j = 0; keyboard_nav_on && j < KEYBOARD_NAV; j++)
            if (keyboard_nav[j] == i) {
                int c = j < KEYBOARD_ARROWS ? LEFT + (int)(j / 2) : B1;
                now[c] |= down;
                pressed[c] |= fresh;
            }
    }
    follow_screen();
    if (arrived) {
        arrived = 0;
        for (j = 0; j < HELD_KEYS; j++)  /* pressed for the screen before (Return: as its click) */
            held_over[j] = keys[held_keys[j]] != 0 && (j < 2 || joy);
    }
    ticks_on_screen++;
    /* A button still held from the screen before can't act here too (the new screen's first
     * tick would take it again: one press, two screens): hidden until it's let go. Keys.c does
     * the same for every key on Windows; the DOS menus had nothing. */
    for (j = 0; j < HELD_KEYS; j++) {
        unsigned char k = held_keys[j];
        hid_over[j] = 0;
        if (!keys[k])
            held_over[j] = 0;
        else if (held_over[j] && s != SCREEN_RACE) {
            hid_over[j] = keys[k];
            keys[k] = 0;
        }
    }
    /* Button 2 ends the attract demo and the flyover as Esc does (both quit on action 10 after
     * the hotspots: race_keys_tick, track_preview_tick) */
    if (stick[B2] && ((s == SCREEN_RACE && NAV_GAME_MODE == MODE_DEMO) || s == SCREEN_VIEW))
        pending_action = ACTION_ESC;
    /* The globe: button 1 is its own OK key (the script's key 0,&73; the globe has no items
     * for the joystick, so it isn't hidden). OK with no race chosen chooses the track under
     * the globe's cursor, but there is none on arrival (globe_enter), so that first press only
     * picks one: OK is pressed again on the next tick, so one press chooses the track and the
     * next goes on (the director; Return keeps its 1996 way: two presses) */
    if (globe_again && s == SCREEN_GLOBE)
        pending_action = 0;
    globe_again = s == SCREEN_GLOBE && stick[B1] && NAV_GLOBE_TRACK == -1 && NAV_GLOBE_RACES < 1;
    if (s == SCREEN_RACE)  /* the navigation carries on after it (the director, 2026-11-24: the results) */
        return;
    if (s == SCREEN_NAME) {
        name_screen(stick);
        return;
    }
    if (s == SCREEN_KEYS && joy && count() > 0) {
        /* every key is the game's here: it gives the blinking row the next key pressed. The
         * navigation only shows which row that is, as its selection (no pointer, no click) */
        owners();
        select_item(box_with(KEYS_FIRST + NAV_KEY_SLOT), 0);
        have_target = 0;
        return;
    }
    if (s == SCREEN_KEYS || s == SCREEN_RNAME || s == SCREEN_VIEW || s >= SCREEN_QUIT || count() == 0)
        return;
    own = arrow_key(-1);
    if (!joy) {
        /* Any of the navigation's inputs gives it the menus (the director, 2026-11-24: car
         * select's left and right didn't): a push up or down, or left or right where the screen
         * has none of its own, selects the top item; a button, Esc, or the screen's own left
         * or right does its usual job too, this once */
        int push = pressed[UP] || pressed[DOWN] || (!own && (pressed[LEFT] || pressed[RIGHT]));
        if (!push && !pressed[B1] && !pressed[B2] && !pressed[LEFT] && !pressed[RIGHT] && !esc)
            return;
        take();
        nav_note("on, screen", s);
        selected = -1;
        if (!push)
            return;
        chose = 1;  /* the push that starts it selects the top item */
    }
    stick_used |= stick_pressed;
    if (stick_used && !manual_type && NAV_PLAYER1 == 0) {  /* the joystick's, not the arrow keys' */
        NAV_PLAYER1 = 1;
        nav_note("player 1 set to JS1, screen", s);
    }
    owners();
    if (!navigable(selected)) {
        if (!drawn_any && ticks_on_screen < 10)  /* the screen's first picture first */
            selected = -1;
        else
            select_item(first_item(), chose);  /* the top item (or the track screen's Race) */
        chose = 1;
    }
    if (!chose && selected >= 0) {
        way = pressed[UP] ? UP : pressed[DOWN] ? DOWN : own ? -1 : pressed[LEFT] ? LEFT : pressed[RIGHT] ? RIGHT : -1;
        if (way >= 0 && way < UP && plus_of[selected] >= 0)  /* a slider: its - or +, one step a push */
            pending_action = HS(way == LEFT ? selected : plus_of[selected])[MB_ACTION];
        else if (way >= 0) {
            int m = way >= UP ? next_in_order(selected, way == DOWN) : next_item(selected, way);
            if (m >= 0)
                select_item(m, 1);
        }
    }
    if (selected >= 0) {
        if (pressed[B1] && plus_of[selected] < 0)  /* a slider takes left and right, not button 1 */
            press_button = 1;
        if (now[B1]) {  /* the selection's click instead of the scripts' button-1 and Return keys */
            hid_click[0] = keys[KEY_B1];
            hid_click[1] = keys[KEY_RETURN];
            hid_click[2] = keys[KEY_PAD_ENTER];
            keys[KEY_B1] = keys[KEY_RETURN] = keys[KEY_PAD_ENTER] = 0;
            hid_clicks = 1;
        }
        if (plus_of[selected] < 0)
            aim(selected);
        else
            have_target = 0;
    } else
        have_target = 0;
    if (!now[B1])
        press_button = 0;
}

void nav_after_tick(void)
{
    unsigned j;
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
    if (hid_clicks) {  /* put back in the reverse order: a held-over key was hidden first */
        NAV_KEYS[KEY_B1] = hid_click[0];
        NAV_KEYS[KEY_RETURN] = hid_click[1];
        NAV_KEYS[KEY_PAD_ENTER] = hid_click[2];
        hid_clicks = 0;
    }
    for (j = 0; j < HELD_KEYS; j++)
        if (hid_over[j]) {
            NAV_KEYS[held_keys[j]] = hid_over[j];
            hid_over[j] = 0;
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
    if (joy && moved && ticks_on_screen > MOUSE_GRACE) {  /* moved, not just a button */
        leave();
        nav_note("off: the mouse", NAV_SCREEN);
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
    follow_screen();
    drawn_at[d] = tick_serial;
    drawn_any = 1;
    if (!joy)
        return;
    owners();
    h = HS(d);
    saved_index = d;
    if (h[TYPE] == 4 || hidden(owner[d])) {  /* the pointer, Select and Next: not drawn */
        saved_what = SAVED_TYPE;
        saved_value = h[TYPE];
        h[TYPE] = 0;
    } else if (hold(d) < 0)
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
            h[TYPE] = (short)saved_value;
        else if (saved_what == SAVED_FONT)
            h[TX_FONT] = (short)saved_value;
        else
            NAV_ANIMS[h[AS_ANIM] * ANIM_SIZE + 7] = (unsigned char)saved_value;
        saved_index = -1;
    }
}

/* A text the screen's own code draws (site label_draw_call: the joysticks screen's "Keys/Custom",
 * "JS1" and so on beside "Pl1:" and "Pl2:"), which takes whatever font the last script text
 * left, "Pl2:"'s: the font of the item it's in, as that item's own text shows this frame */
void nav_label_font(void)
{
    int n, d, m;
    short x = NAV_TEXT_X, y = NAV_TEXT_Y;
    if (!NAV_ENABLED || !started || !joy || NAV_SCREEN != screen || owners_screen != screen)
        return;
    n = count();
    for (m = 0; m < n; m++) {
        short *b = HS(m);
        if (!item[m] || x < b[MB_X] || x >= b[MB_X] + b[MB_W] || y < b[MB_Y] || y >= b[MB_Y] + b[MB_H])
            continue;
        for (d = 0; d < n; d++)
            if (HS(d)[TYPE] == 2 && part_of(d, m)) {
                int f = hold(d);
                NAV_TEXT_FONT = (short)(f >= 0 ? f : HS(d)[TX_FONT]);
                return;
            }
    }
}

/* The sound screen's own buttons and levels (sound_options_draw's sprite_draw calls, through
 * thunks.asm: the volumes' - and + and levels, OK and the commentary tick box), whose frames
 * follow the screen's own step (sound_options_anim, 0-4) by per_step frames a step.
 * Original-ish: the selection's keep the step they showed when first drawn after it was
 * selected; inverted: the others keep the step they first had. The tick box still shows its
 * new state when toggled. per_step 0: a slider's knob on the display and damage screens
 * (their draw routines' sprite_draw(anim_frame(animation 1) + 0x1E)), held at the step its
 * slider's - (an animated sprite on animation 1) is held at, so it stops and starts with its
 * slider (the director, 2026-11-24). */
int nav_sprite_frame(int frame, int x, int y, int per_step)
{
    int n = count(), d, inside;
    if (!NAV_ENABLED || !started)
        return frame;
    follow_screen();
    if (!joy)
        return frame;
    owners();
    if (!per_step) {
        for (d = 0; d < n; d++) {
            int o = owner[d];
            if (HS(d)[TYPE] == 8 && frozen[d] >= 0 && o >= 0 && plus_of[o] >= 0 && in_item(o, x, y))
                return frame + KNOB_FRAME(frozen[d]) - KNOB_FRAME(KNOB_STEP);
        }
        return frame;
    }
    inside = selected >= 0 ? in_item(selected, x, y) : 0;  /* in_item is 0 or 1 (no 16 bytes in a row as RACING.EXE's) */
    if (inside == NAV_INVERTED)
        return frame;
    if (!still_set) {
        still_set = 1;
        still_step = NAV_SOUND_STEP;
    }
    return frame + per_step * (still_step - NAV_SOUND_STEP);
}
