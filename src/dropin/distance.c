/* Experimental: the landscape drawn further than the game's 64x64 visible-area grid allows
 * (Phase 3e; KNOWLEDGE "How far the draw distance can go").
 *
 * The game draws the ground near the camera as before (render_view's own steps, with the
 * vision distance at 25, the most its grid holds: site render_view_entry). Then, at its call
 * to the landscape walk (site landscape_walk_call), we add the squares further out, up to
 * [display] max_draw_distance, in extra passes that reuse the game's own code:
 *   - which squares: those in front of the camera, within its field of view and the far
 *     distance, that the near pass didn't draw (from the game's camera matrix);
 *   - for each 61x61 window over them, nearest first: the grid (0x44BA14) filled with those
 *     squares, the camera-space tables rebuilt by the game's FUN_0041D24C with the camera's
 *     square shifted to the window's centre, and the game's walk loop (from 0x41D618, which
 *     skips its first step: that would start the landscape's records again).
 * Corners shared with the near pass come out identical (the shift is whole squares of 512
 * units, and 512 / 8 divides exactly), so there are no cracks between the passes. The game's
 * own check that its list of polygon records isn't full (0x523980) still stops the drawing.
 */
#include "brrfix.h"
#include <math.h>
#include <string.h>

#define GRID_PTR      (*(uint8_t *volatile *)0x44BA14)  /* 64x64 bytes: 0x80 + ring = drawn */
#define DETAIL_LEVEL  (*(volatile int32_t *)0x44B044)  /* the vision distance */
#define LIST_WRITE    (*(volatile uint32_t *)0x52D6F4)  /* next polygon record */
#define LIST_LIMIT    brr_record_limit                  /* the walk's own limit (site record_limit) */
#define BOUND_X_MIN   (*(volatile int32_t *)0x46BBC0)   /* the walk's grid bounds */
#define BOUND_X_MAX   (*(volatile int32_t *)0x46BBC4)
#define BOUND_Y_MIN   (*(volatile int32_t *)0x46BBBC)
#define BOUND_Y_MAX   (*(volatile int32_t *)0x46BBC8)
#define MAP_X_FIRST   (*(volatile int16_t *)0x46BBDC)   /* map square of BOUND_X_MIN */
#define MAP_X_LAST    (*(volatile int16_t *)0x46BBD8)
#define MAP_Y_FIRST   (*(volatile int16_t *)0x46BBD2)   /* map square of BOUND_Y_MIN */
#define MAP_Y_LAST    (*(volatile int16_t *)0x46BBD4)
#define CAMERA_X      (*(volatile int32_t *)(GAME_CAMERA_POS + 0))
#define CAMERA_Z      (*(volatile int32_t *)(GAME_CAMERA_POS + 8))
#define ENGINE_CAMERA ((volatile int32_t *)0x54A2D4)    /* x, y, z */
#define ENGINE_MATRIX ((volatile int32_t *)0x54A2B0)    /* rows: camera x, y, z */
#define FOCAL_X       (*(volatile int32_t *)0x542000)
#define HALF_WIDTH    (*(volatile int32_t *)0x54201C)

#define REACH 94                    /* squares either side of the camera considered (60 x 1.5 + 4) */
#define SIDE (2 * REACH + 1)
#define WINDOW 61                   /* squares per window side: grid 1..61, corners up to 62 */

/* The display list's two buffers, bigger ([display] big_lists, the Modern renderer; sites
 * record_* and node_*): the game's 2,000 landscape records of 0xBD bytes (0x4CA430; its walk
 * stops 64 short, at 1,936) and its 999 object nodes of 17 bytes (0x5268D1-0x52AB28). At 44
 * squares in 16:9 the records were 93% full; at 60 they would run out. The list's sentinel
 * (0x5268C0) and the 100 shadow polygons (0x52AB28) stay where they are. */
uint8_t brr_records[BRR_RECORDS * 0xBD];
uint8_t brr_nodes[BRR_NODES * 17];
uint32_t brr_record_limit = 0x523980u;  /* the game's until the sites are patched */

uint32_t brr_landscape_walk_fn = 0x41D5E4, brr_landscape_walk_loop = 0x41D618;
uint32_t brr_landscape_tables_fn = 0x41D24C;
int brr_far_distance;               /* squares; 0 = no far passes */

static uint8_t near_drawn[SIDE][SIDE], wanted[SIDE][SIDE];  /* [dz][dx], + REACH; wanted: its ring, or 0 */
uint32_t brr_edge_squares;          /* squares added within 8 squares (the near area's sides), for the log */

static void far_passes(int ring)
{
    uint8_t *grid = GRID_PTR;
    int camera_x = CAMERA_X, camera_z = CAMERA_Z;
    int square_x = camera_x >> 9, square_z = camera_z >> 9;
    int x0 = BOUND_X_MIN, x1 = BOUND_X_MAX, y0 = BOUND_Y_MIN, y1 = BOUND_Y_MAX;
    int gx, gy, dx, dz, min_x = SIDE, max_x = -1, min_z = SIDE, max_z = -1, count = 0;
    double row_x, row_z, ratio, nearest, furthest;
    static int logged;

    if (!grid || FOCAL_X <= 0 || HALF_WIDTH <= 0 || ring < 7 || ring > 31)
        return;
    /* the squares the near pass drew: grid x 31 and y 32 are the camera's square */
    memset(near_drawn, 0, sizeof near_drawn);
    for (gy = y0 < 0 ? 0 : y0; gy <= y1 && gy < 64; gy++)
        for (gx = x0 < 0 ? 0 : x0; gx <= x1 && gx < 64; gx++)
            if (grid[gx + gy * 64] >= 0x80)
                near_drawn[32 - gy + REACH][gx - 31 + REACH] = 1;

    /* the squares wanted: centre in front, within the far distance and the field of view
     * (with a square and a half to spare at the sides), from the game's camera matrix */
    row_x = sqrt((double)ENGINE_MATRIX[0] * ENGINE_MATRIX[0] + (double)ENGINE_MATRIX[1] * ENGINE_MATRIX[1]
                 + (double)ENGINE_MATRIX[2] * ENGINE_MATRIX[2]);
    row_z = sqrt((double)ENGINE_MATRIX[6] * ENGINE_MATRIX[6] + (double)ENGINE_MATRIX[7] * ENGINE_MATRIX[7]
                 + (double)ENGINE_MATRIX[8] * ENGINE_MATRIX[8]);
    if (row_x <= 0 || row_z <= 0)
        return;
    ratio = (double)HALF_WIDTH / FOCAL_X;
    /* from 2 squares out, so that wider views get the sides the near pass's 4:3-shaped area
     * leaves out, even turning (it was 8, and before that ring - 6, 19); each square drawn by
     * its own distance's routine (the near ones with near-plane clipping), with 2 squares to
     * spare at the field of view's sides */
    nearest = 2 * 512.0;
    furthest = brr_far_distance * 512.0 + 256;
    for (dz = -REACH; dz <= REACH; dz++) {
        double rel_z = (double)(square_z + dz) * 512 + 256 - ENGINE_CAMERA[2];
        double rel_y = -(double)ENGINE_CAMERA[1];
        for (dx = -REACH; dx <= REACH; dx++) {
            double rel_x = (double)(square_x + dx) * 512 + 256 - ENGINE_CAMERA[0];
            double depth = (ENGINE_MATRIX[6] * rel_x + ENGINE_MATRIX[7] * rel_y + ENGINE_MATRIX[8] * rel_z) / row_z;
            double side = (ENGINE_MATRIX[0] * rel_x + ENGINE_MATRIX[1] * rel_y + ENGINE_MATRIX[2] * rel_z) / row_x;
            int want = depth >= nearest && depth <= furthest && fabs(side) <= depth * ratio + 1024
                       && !near_drawn[dz + REACH][dx + REACH];
            int square_ring = (int)(sqrt((double)dx * dx + (double)dz * dz) + 0.5);
            if (square_ring > ring)
                square_ring = ring;
            wanted[dz + REACH][dx + REACH] = want ? (uint8_t)(0x80 | square_ring) : 0;
            if (want) {
                count++;
                if (depth < 8 * 512.0)
                    brr_edge_squares++;
                if (dx + REACH < min_x) min_x = dx + REACH;
                if (dx + REACH > max_x) max_x = dx + REACH;
                if (dz + REACH < min_z) min_z = dz + REACH;
                if (dz + REACH > max_z) max_z = dz + REACH;
            }
        }
    }
    if (!logged++)
        brr_log("draw distance: first frame: %d more squares beyond the near pass, out to %d", count,
                brr_far_distance);
    if (!count)
        return;

    /* the windows, nearest first, so that if the list of records fills, the furthest go */
    {
        struct window { int x0, z0, distance; } windows[16], swap;
        int n = 0, i, j, wx, wz;
        for (wz = min_z; wz <= max_z && n < 16; wz += WINDOW)
            for (wx = min_x; wx <= max_x && n < 16; wx += WINDOW) {
                int cx = wx - REACH + WINDOW / 2, cz = wz - REACH + WINDOW / 2;
                windows[n].x0 = wx;
                windows[n].z0 = wz;
                windows[n].distance = cx * cx + cz * cz;
                n++;
            }
        for (i = 1; i < n; i++)
            for (j = i; j > 0 && windows[j].distance < windows[j - 1].distance; j--) {
                swap = windows[j];
                windows[j] = windows[j - 1];
                windows[j - 1] = swap;
            }
        for (i = 0; i < n; i++) {
            /* window squares dx = x0..x0+60, dz = z0..z0+60 (relative to the camera's square);
             * shifted camera square (sx, sz) = the window's centre: grid x = dx - sx + 31 and
             * grid y = sz - dz + 32 run from 1 to 61 */
            int ax = windows[i].x0 - REACH, az = windows[i].z0 - REACH;
            int sx = ax + WINDOW / 2, sz = az + WINDOW / 2 - 1;
            int gx_min = 64, gx_max = -1, gy_min = 64, gy_max = -1;
            if (LIST_WRITE >= LIST_LIMIT)
                break;
            memset(grid, 0, 64 * 64);
            for (dz = az; dz < az + WINDOW && dz <= REACH; dz++)
                for (dx = ax; dx < ax + WINDOW && dx <= REACH; dx++) {
                    if (!wanted[dz + REACH][dx + REACH])
                        continue;
                    gx = dx - sx + 31;
                    gy = sz - dz + 32;
                    grid[gx + gy * 64] = wanted[dz + REACH][dx + REACH];
                    if (gx < gx_min) gx_min = gx;
                    if (gx > gx_max) gx_max = gx;
                    if (gy < gy_min) gy_min = gy;
                    if (gy > gy_max) gy_max = gy;
                }
            if (gx_max < 0)
                continue;
            BOUND_X_MIN = gx_min;
            BOUND_X_MAX = gx_max;
            BOUND_Y_MIN = gy_min;
            BOUND_Y_MAX = gy_max;
            /* as FUN_0041CC80 sets them, for the shifted square */
            MAP_X_FIRST = (int16_t)((gx_min - 0x1F + square_x + sx) & 0xFF);
            MAP_X_LAST = (int16_t)((gx_max - 0x1F + square_x + sx) & 0xFF);
            MAP_Y_FIRST = (int16_t)((0x20 - gy_min + square_z + sz) & 0xFF);
            MAP_Y_LAST = (int16_t)((0x20 - gy_max + square_z + sz) & 0xFF);
            /* the camera-space tables, by the game's own code, with the camera's square moved */
            CAMERA_X = camera_x + sx * 512;
            CAMERA_Z = camera_z + sz * 512;
            brr_call_landscape_tables();
            CAMERA_X = camera_x;
            CAMERA_Z = camera_z;
            brr_call_landscape_far_walk();
        }
    }
}

/* Site landscape_walk_call: render_view's call to the landscape walk (0x41D5E4). The near
 * pass ran its set-up at vision 25 (site render_view_entry); the walks, the flat squares'
 * fade (FUN_0041C4C4) and the display list's fade (FUN_0042B8B4) use the far distance.
 * render_view_entry puts the player's own values back when the view is drawn. */
uint32_t __cdecl brr_landscape_walk(void)
{
    int ring = DETAIL_LEVEL;
    uint32_t result;
    DETAIL_LEVEL = brr_far_distance;
    result = brr_call_landscape_walk();
    far_passes(ring);
    return result;
}
