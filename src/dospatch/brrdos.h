/* Big Red Racing DOS patch (Phase 3b): shared declarations.
 *
 * Built with Open Watcom (wcc386: flat model, 486 instructions, and the register calling
 * convention of the game's own Watcom C) and linked at the end of the game's code object
 * by "python3 brr.py dos-build", which turns it into data/dos-patches.json. The game's
 * functions and variables are reached through the game_ symbols below, which the build
 * places at their addresses in RACING.EXE v2.24 (the addresses Ghidra shows). Our own
 * variables are placed after the game's data object. Integer arithmetic only: the game
 * runs on a 486SX, which has no maths coprocessor. */
#ifndef BRRDOS_H
#define BRRDOS_H

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef short s16;
typedef unsigned long u32;
typedef long s32;

/* ---- the game (addresses: GAME_SYMBOLS in scripts/dos_build.py) ---- */
extern void game_race_enter(void);           /* race screen: enter, tick, draw, leave */
extern void game_race_tick(void);
extern void game_race_draw(void);
extern void game_race_leave(void);
extern void game_camera_update(u8 *record);  /* record in EAX */
extern void game_copy_frame(void);           /* the game's buffer to video memory */
extern void game_copy_frame_split(void);     /* the same, split screen, first view */

extern volatile u16 game_timer_ticks;        /* +1 every 1/70 s (Miles timer interrupt) */
extern volatile u8 game_frame_ticks;         /* ticks run this frame (at most the cap) */
extern volatile s32 game_tick_count;         /* ticks since the screen started */
extern volatile u8 game_key_table[];         /* by scan code: non-zero while held */
extern u8 game_control_set[];                /* player 1: accel, brake, left, right, nitro, reset */
extern u8 game_camera_view1[];               /* camera records, 0x67 bytes each */
extern u8 game_camera_view2[];
extern u8 game_vehicles[];                   /* 7 records of 0x395 bytes */
extern s32 game_camera_pos[3];               /* the camera the renderer draws from */
extern s16 game_camera_angles[3];            /* pitch, yaw, roll */
extern u32 game_camera_heights[];            /* per vehicle type, 8 bytes each (camera mode 3) */
extern u32 game_rand_seed;                   /* Watcom rand() */
extern u8 *game_frame_buffer;
extern u16 game_frame_width;
extern u16 game_frame_height;
extern const s16 game_sine_table[];          /* 1,280 entries; cosine = 256 entries on */
extern volatile s32 game_rot_sin_a;          /* rotation_matrix's globals */
extern volatile u32 game_rot_a, game_rot_b, game_rot_c;

#define VEHICLE_SIZE  0x395u
#define VEHICLE_COUNT 7

/* ---- the options, written into brr_options by the patcher (OPTIONS in dos_build.py,
 * which checks this list matches) ---- */
enum {
    OPT_CAMERA,
    OPT_CAP,
    OPT_INTERPOLATION,
    OPT_FINE_ANGLES,
    OPT_ROLL_FIX,
    OPT_RETRACE,
    OPT_LOG,
    OPT_SEED_SET,
    OPT_SEED,
    OPT_CAPTURE_FRAMES,
    OPT_CAPTURE_TICK,
    OPT_PROVE,
    OPT_COUNT
};
extern const volatile u32 brr_options[OPT_COUNT];
#define OPTION(n) (brr_options[n])

struct input_span { u32 key; s32 first, last; };  /* key < 0x100: scan code; 0x100 + n: control n */
#define INPUT_MAX 8
extern const volatile struct input_span brr_input[INPUT_MAX];
extern const volatile u32 brr_input_count;
extern const char brr_tag[];      /* "FIX0.1.0 CIARV8", shown on the menu */

/* ---- thunks.asm ---- */
struct rot_args { u32 pitch, yaw, roll; s32 *m; u32 regs[4]; };
void brr_call_rotation_original(struct rot_args *a);

/* ---- DOS, DPMI and port access ---- */
s32 dos_create(const char *name);
#pragma aux dos_create = "mov ah,3ch" "xor ecx,ecx" "int 21h" "sbb edx,edx" "or eax,edx" \
    parm [edx] value [eax] modify [ecx edx];
s32 dos_write(s32 handle, const void *data, u32 size);
#pragma aux dos_write = "mov ah,40h" "int 21h" "sbb edx,edx" "or eax,edx" \
    parm [ebx] [edx] [ecx] value [eax] modify [edx];
void dos_commit(s32 handle);   /* write DOS's buffers for the file to disk */
#pragma aux dos_commit = "mov ah,68h" "int 21h" parm [ebx] modify [eax];
void dos_close(s32 handle);
#pragma aux dos_close = "mov ah,3eh" "int 21h" parm [ebx] modify [eax];
u32 dpmi_alloc(u32 size);   /* linear (= near) address, or 0 */
#pragma aux dpmi_alloc = "mov ecx,eax" "shr eax,16" "mov ebx,eax" "mov eax,501h" "int 31h" \
    "sbb eax,eax" "shl ebx,16" "mov bx,cx" "not eax" "and eax,ebx" \
    parm [eax] value [eax] modify [ebx ecx esi edi];
u8 port_in(u16 port);
#pragma aux port_in = "in al,dx" parm [dx] value [al];
void port_out(u16 port, u8 value);
#pragma aux port_out = "out dx,al" parm [dx] [al];

/* ---- brrdos.c ---- */
extern int brr_in_race_draw;
extern int brr_fine_active;
extern int brr_fast;          /* the machine draws a frame in 3 ticks or fewer, on average */
extern u32 brr_frames_drawn;
s32 sar(s32 x, int n);

/* ---- angles.c ---- */
void brr_rotation_fine(struct rot_args *a);
void brr_fine_camera_angles(const u8 *record, s16 *angles);

/* ---- log.c (the logging build) ---- */
void log_race_enter(void);
void log_before_tick(void);
void log_after_tick(void);
void log_before_draw(void);
void log_after_draw(int blended);
void log_race_leave(void);
void log_text(const char *text);
void log_number(s32 value);
void log_line(void);
void prove_fine_angles(void);

#endif
