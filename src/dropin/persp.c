/* Perspective-correct ground at any distance (fix ground_perspective, the Modern renderer;
 * [display] ground_perspective, the director 2026-11-16: banks' textures twitched close up).
 *
 * The game drew a ground square two ways: within 4 squares (landscape_near_range 0x46BBB0)
 * perspective-correct in camera space (landscape_draw_cell_near 0x41C9CC), beyond it as two
 * affine screen-space triangles; the texture jumped by up to 1.7 texels where they met. Sites
 * ground_ring_routine and ground_near_range draw every textured square the first way; this
 * keeps the far ones precise.
 *
 * The engine draws a perspective-textured polygon (engine_draw_polygon type 4, 0x442020; the
 * landscape's nearest squares, fill 0x204 from landscape_draw_cell_near 0x41C9CC) by storing
 * poly_setup_perspective (0x446FF0) in poly_setup_hook (0x5430D4, site persp_setup_hook at
 * 0x442057). poly_clip_and_fill (0x442B04) and poly_output_unlit (0x442C42) call the hook once
 * per polygon, after near-plane clipping and before the screen-edge clippers, edge set-up and
 * span_persp (0x444870).
 *
 * The clip vertices of the polygon form a ring (0x40 bytes each, in the pool 0x543410 or the
 * vertex array *0x54A1F8):
 *   +0x08  z, 16.16 view depth: (z << 16) + 1 from engine_draw_polygon's view-space path
 *          (0x441DD8-0x441DDC), 0x400000 (64.0) on a vertex made by clip_edge_near (0x443444);
 *          at least 0x400000 once near clipping has run
 *   +0x14  w = 1/z: (2^53 - 1) / [+0x08] (IDIV at 0x441E18, the same in model_op_vertices
 *          0x4413E2), 0x7FFFFFFF from clip_edge_near (0x44343D): 2^37 / z for whole z
 *   +0x18  u, then u/z: on entry the texture column, 16.16 (0-255 within the 256x256 page; the
 *          landscape writes the tile's (t % 48 & 7) x 0x200000 and + 0x1F0000, so fraction 0),
 *          copied from the polygon record's UV list at 0x441FAC-0x441FB3 / 0x44209C-0x4420A3
 *   +0x1C  v, then v/z: the texture row, likewise
 *   +0x28  the next vertex of the ring; poly_ring (0x542B70) is the first
 *
 * What the game's set-up does (0x446FF0-0x447025, for every vertex of the ring):
 *   word [+0x1A] = (u << 13) / [+0x08]   (64-bit IDIV; the low 16 bits of the quotient)
 *   word [+0x1E] = (v << 13) / [+0x08]
 * so the dword at +0x18 is (u x 2^13 / z) << 16 with u's own fraction left in the low word,
 * and +0x14 stays 2^37 / z. Each pixel, span_persp interpolates the three dwords (from the
 * edges, which take +0x14, +0x18, +0x1C as their +0x1C, +0x20, +0x24) and computes
 *   texel = exp[log(u/z >> 16) - log(w >> 16)]
 * with 4,096-per-octave tables at 0x54B740 (log of 0-32,766, read at index + 1; exp built so
 * that the texel is 256 x (u/z) / w, rounded). Only the whole parts are used: u/z is a whole
 * number at each vertex, and w >> 16 is 2^21 / z, about 4,096 / squares. At 4 squares that is
 * 1,024 (a texel to within a quarter); at 25 squares 164 and at 60 squares 68, so at the
 * distances the far draw passes reach the game's set-up is out by 2.5 to 5 texels (simulated:
 * work/logs/texture-twitch/sim.txt).
 *
 * What this does instead, for polygons drawn without a depth buffer (render_flags 0x542008 bit
 * 0x10, how the landscape is drawn; the thunk runs the game's set-up for everything else):
 *   - w is scaled by 2^s, the same s for the whole polygon, the largest that keeps every w
 *     at most W_LIMIT; w is first limited to W_LIMIT (only clip_edge_near's 0x7FFFFFFF is
 *     over it, 0.003%), as index 0x7FFF would read past the log table into the exp table;
 *   - u/z = u x w' / 2^24 as a full 16.16 value (64-bit product). With s = 0 its whole part is
 *     the game's word (u x 2^29 / [+0x08] = (u x 2^13 / z) x 2^16), and the fraction is kept;
 *     likewise v/z.
 * u/z, v/z and w are all multiplied by 2^s, so each pixel's ratio, and so the texel, is the
 * game's, but the whole parts the tables see are up to 2^s larger: about 0.5 texel (the
 * rounding) at any distance. Without a depth buffer nothing else reads w: the clippers and
 * edges interpolate it (64-bit products) and only span_persp uses it.
 *
 * Returns 1 when done, 0 to have the thunk run the game's own set-up instead (a ring this
 * wasn't written for: a non-positive w, a texture coordinate of 256 or more, or no end). */
#include "brrfix.h"

#define POLY_RING   (*(uint8_t *volatile *)0x542B70)  /* the polygon's first clip vertex */

#define V_W(v)      (*(int32_t *)((v) + 0x14))
#define V_U(v)      (*(int32_t *)((v) + 0x18))
#define V_V(v)      (*(int32_t *)((v) + 0x1C))
#define V_NEXT(v)   (*(uint8_t **)((v) + 0x28))

/* w's whole part at most 0x7FFE: span_persp reads the log table at (w >> 16) + 1, and its
 * 32,767 entries end at index 0x7FFF; edge and span steps can't overshoot by a whole unit */
#define W_LIMIT     0x7FFEFFFFL
/* a texture coordinate, 16.16, under 256: the game's quotient then fits its word too */
#define UV_LIMIT    0x01000000L
/* a triangle clipped by the near plane and four screen edges has 8 corners; far more is a
 * broken ring */
#define RING_MAX    64

uint32_t brr_persp_setup_original = 0x446FF0;  /* poly_setup_perspective, for the thunk */
uint32_t brr_count_persp;                      /* polygons set up here (for a log line) */

int __cdecl brr_persp_setup(void)
{
    uint8_t *first = POLY_RING, *v;
    uint32_t top = 0;
    int count = 0, shift = 0;

    if (!first)
        return 1;  /* no polygon: nothing to set up */

    /* pass 1: check the ring and find the largest w, changing nothing */
    v = first;
    do {
        int32_t w = V_W(v), u = V_U(v), t = V_V(v);
        if (w <= 0 || u >= UV_LIMIT || u <= -UV_LIMIT || t >= UV_LIMIT || t <= -UV_LIMIT
            || ++count > RING_MAX)
            return 0;
        if ((uint32_t)w > top)
            top = (uint32_t)w;
        v = V_NEXT(v);
        if (!v)
            return 0;
    } while (v != first);

    if (top > (uint32_t)W_LIMIT)
        top = (uint32_t)W_LIMIT;
    while ((top << 1) <= (uint32_t)W_LIMIT) {  /* top > 0 and under 2^31: ends by s = 30 */
        top <<= 1;
        shift++;
    }

    /* pass 2: the same vertices, now known to be good */
    v = first;
    do {
        int32_t w = V_W(v);
        int64_t scaled;
        if (w > W_LIMIT)
            w = W_LIMIT;
        scaled = (int64_t)w << shift;  /* at most W_LIMIT */
        V_W(v) = (int32_t)scaled;
        /* |u| < 2^24 and w' < 2^31: the product fits 55 bits, the result 31 */
        V_U(v) = (int32_t)(((int64_t)V_U(v) * scaled) >> 24);
        V_V(v) = (int32_t)(((int64_t)V_V(v) * scaled) >> 24);
        v = V_NEXT(v);
    } while (v != first);

    brr_count_persp++;
    return 1;
}
