/* Horizon culling ([display] horizon_culling; the director, 2026-11-16; Modern renderer, with
 * close_gaps). The engine paints its display list far to near (display_list_draw 0x42B8B4),
 * so whatever nearer ground covers was filled first and painted over: behind a hill, every
 * square and object is drawn for nothing.
 *
 * Site display_list_call (render_view 0x417808, CALL display_list_draw) runs brr_horizon_cull
 * first. It walks the list backwards (near to far: the reverse of the painting order) keeping,
 * per screen column, the row h[x] from which every row down to the view's bottom is surely
 * covered by ground already seen. Ground seen earlier in this walk is painted later, so a node
 * whose every possibly painted pixel lies in that covered area would be painted over anyway:
 * it's unlinked. The picture is the same, whatever the game's sorting. Only ground counts as
 * cover: opaque (flat, or a texture square with no see-through colour 0), front-facing
 * triangles, worked out conservatively (a pixel counts only if the triangle surely fills it,
 * with the exact edges of close_gaps), and joined to the covered area below. Nodes are bounded
 * conservatively too (objects by their model's radius plus margins), and anything uncertain
 * (near the camera, effects, decals) is kept.
 *
 * The display list (work/logs/draw-order/horizon-spec.md): a sentinel node at 0x5268C0; nodes
 * of 17 bytes: +0 key, +4 next (nearer), +8 prev, +0xC type, +0xD payload. Type 7, a landscape
 * record (0xBD bytes), holds two triangles inline at +0xD and +0x65: +0 polygon type (low
 * byte: 0 flat, 3 affine texture, 4 perspective texture; 0x200: camera-space corners, else
 * screen pixels), +4 colour or texture page, +8 count (3), +0xC three corners (x, y, z dwords),
 * +0x34 three (u, v) in 16.16. Types 0, 3, 4, 5, 8: an engine object (+0 position, +0x14
 * parent, +0x18 model, whose first dword is its radius); 3, 5 and 8 are fitted to the ground
 * when drawn. Type 2: a shadow, a polygon copy with three world corners at +0xC. Type 6
 * (effects) and 10 (a car's decal, which writes a shared texture) are never culled. */
#include "brrfix.h"
#include <math.h>
#include <string.h>

#define LIST_SENTINEL   ((uint8_t *)0x5268C0)
#define LIST_LAST       (*(uint8_t *volatile *)0x5268C8)
#define VIEW_W          (*(volatile int32_t *)0x542014)
#define VIEW_H          (*(volatile int32_t *)0x542024)
#define SCALE_X         (*(volatile int32_t *)0x542000)
#define SCALE_Y         (*(volatile int32_t *)0x542004)
#define CENTRE_X        (*(volatile int32_t *)0x54201C)
#define CENTRE_Y        (*(volatile int32_t *)0x542028)
#define CAMERA_MATRIX   ((volatile int32_t *)0x54A2B0)  /* rows: right, down, forward; 1.15 */
#define CAMERA_POS      ((volatile int32_t *)0x54A2D4)
#define LAND_HEIGHTS    ((const uint8_t *)0x487D50)      /* 256 x 256, [z >> 9][x >> 9] */
#define LAND_PAGES      ((uint8_t *const volatile *)0x44B9FC)
#define LEVEL_NAME      ((const char *)0x44AF5A)

#define NEAR_Z      64.0
#define MAX_COLUMNS 8192
#define JOIN        0.01     /* rows: two triangles' shared edge, worked out from either side */
#define FAR_AWAY    1.0e6    /* a screen coordinate this big isn't one */

struct pt { double x, y; };
struct ipt { int32_t x, y; };
struct rect { int x0, y0, x1, y1; };

static int horizon[MAX_COLUMNS];   /* per column: from this row down, surely painted over */
static int32_t edge[MAX_COLUMNS];  /* per column: the covering ground's top, 16.16, exactly */
static int32_t col_top[MAX_COLUMNS + 2], col_bottom[MAX_COLUMNS + 2];  /* one polygon's, 16.16 */
static int logging;  /* [debug] horizon_log: frames still to log */
static int view_w, view_h;
static double cam_m[9], cam_p[3], scale_x, scale_y, centre_x, centre_y;
uint32_t brr_horizon_records, brr_horizon_records_culled, brr_horizon_objects, brr_horizon_objects_culled;
int64_t brr_horizon_time, brr_horizon_frames;  /* the pass's own time (QueryPerformanceCounter units) */

/* The nearest whole number, without the library's slow conversion (|v| < 2^31) */
static int32_t round_int(double v)
{
    union { double d; int32_t i[2]; } u;
    u.d = v + 6755399441055744.0;
    return u.i[0];
}

/* ---- see-through texture squares: a 48-bit mask per landscape page ---- */
static const uint8_t *page_seen[4];
static uint32_t page_mask_lo[4], page_mask_hi[4];
static char page_level[16];

static void pages_check_level(void)  /* once a frame: a new level has new pages */
{
    if (strncmp(page_level, LEVEL_NAME, sizeof page_level - 1) != 0) {
        memset((void *)page_seen, 0, sizeof page_seen);
        strncpy(page_level, LEVEL_NAME, sizeof page_level - 1);
    }
}

static int square_opaque(const uint8_t *page, int square)
{
    int p, i, x, y;
    if (square < 0 || square >= 48)
        return 0;
    for (p = 0; p < 4; p++)
        if (page && LAND_PAGES[p] == page)
            break;
    if (p == 4)
        return 0;  /* not a landscape page: don't count it */
    if (page_seen[p] != page) {
        page_seen[p] = page;
        page_mask_lo[p] = page_mask_hi[p] = 0;
        for (i = 0; i < 48; i++) {
            const uint8_t *s = page + (i / 8) * 32 * 256 + (i % 8) * 32;
            int zero = 0;
            for (y = 0; y < 32 && !zero; y++)
                for (x = 0; x < 32; x++)
                    if (!s[y * 256 + x]) {
                        zero = 1;
                        break;
                    }
            if (zero) {
                if (i < 32) page_mask_lo[p] |= 1u << i;
                else page_mask_hi[p] |= 1u << (i - 32);
            }
        }
    }
    return square < 32 ? !(page_mask_lo[p] & (1u << square)) : !(page_mask_hi[p] & (1u << (square - 32)));
}

/* ---- a triangle as a screen polygon ---- */
static void view_space(const double w[3], double out[3])
{
    double d0 = w[0] - cam_p[0], d1 = w[1] - cam_p[1], d2 = w[2] - cam_p[2];
    out[0] = (cam_m[0] * d0 + cam_m[1] * d1 + cam_m[2] * d2) / 32768.0;
    out[1] = (cam_m[3] * d0 + cam_m[4] * d1 + cam_m[5] * d2) / 32768.0;
    out[2] = (cam_m[6] * d0 + cam_m[7] * d1 + cam_m[8] * d2) / 32768.0;
}

static void project(const double v[3], struct pt *out)
{
    double inverse = 1.0 / v[2];
    out->x = v[0] * scale_x * inverse + centre_x;
    out->y = v[1] * scale_y * inverse + centre_y;
}

/* The triangle at tri (a record's) as a screen polygon: 3 corners, or up to 4 once a
 * camera-space one is cut at the near plane (as clip_edge_near 0x4433F0 does). 0: nothing
 * in front of the camera, or numbers that aren't screen positions. */
static int triangle_polygon(const uint8_t *tri, struct pt *out)
{
    uint32_t type = *(const uint32_t *)tri;
    const int32_t *v = (const int32_t *)(tri + 0xC);
    int i, n = 0;
    if (!(type & 0x300)) {
        for (i = 0; i < 3; i++) {
            out[i].x = v[3 * i];
            out[i].y = v[3 * i + 1];
            if (fabs(out[i].x) > FAR_AWAY || fabs(out[i].y) > FAR_AWAY)
                return 0;
        }
        return 3;
    }
    if (!(type & 0x200))
        return 0;
    for (i = 0; i < 3; i++) {
        const int32_t *a = v + 3 * i, *b = v + 3 * ((i + 1) % 3);
        int in_a = a[2] >= NEAR_Z, in_b = b[2] >= NEAR_Z;
        double p[3];
        if (in_a) {
            p[0] = a[0], p[1] = a[1], p[2] = a[2];
            project(p, &out[n++]);
        }
        if (in_a != in_b) {
            double t = (NEAR_Z - a[2]) / (double)(b[2] - a[2]);
            p[0] = a[0] + (b[0] - a[0]) * t;
            p[1] = a[1] + (b[1] - a[1]) * t;
            p[2] = NEAR_Z;
            project(p, &out[n++]);
        }
    }
    return n >= 3 ? n : 0;
}

/* Every corner a screen position, or in front of the near plane */
static int in_front(const uint8_t *tri)
{
    const int32_t *v = (const int32_t *)(tri + 0xC);
    if (!(*(const uint32_t *)tri & 0x300))
        return 1;
    return v[2] >= NEAR_Z && v[5] >= NEAR_Z && v[8] >= NEAR_Z;
}

static double signed_area(const struct pt *p, int n)
{
    double a = 0;
    int i;
    for (i = 0; i < n; i++)
        a += p[i].x * p[(i + 1) % n].y - p[(i + 1) % n].x * p[i].y;
    return a;  /* > 0: facing the camera (y down), as poly_cull_visible 0x445E20 keeps it */
}

/* One side of the clipping: where the segment a-b crosses x (or y) = c, worked out the same
 * whichever way round the segment is given, so two triangles sharing it get the same point */
static struct pt cross(struct pt a, struct pt b, int vertical, double c)
{
    struct pt r;
    if (vertical ? (a.x > b.x || (a.x == b.x && a.y > b.y)) : (a.y > b.y || (a.y == b.y && a.x > b.x))) {
        struct pt s = a;
        a = b;
        b = s;
    }
    if (vertical) {
        r.x = c;
        r.y = a.y + (c - a.x) * (b.y - a.y) / (b.x - a.x);
    } else {
        r.y = c;
        r.x = a.x + (c - a.y) * (b.x - a.x) / (b.y - a.y);
    }
    return r;
}

/* Keep the part of the polygon on one side of x (vertical) or y = c (keep_above: >= c, else <= c) */
static int clip(const struct pt *in, int n, struct pt *out, int vertical, double c, int keep_above)
{
    int i, m = 0;
    for (i = 0; i < n; i++) {
        struct pt a = in[i], b = in[(i + 1) % n];
        double va = vertical ? a.x : a.y, vb = vertical ? b.x : b.y;
        int ia = keep_above ? va >= c : va <= c, ib = keep_above ? vb >= c : vb <= c;
        if (ia)
            out[m++] = a;
        if (ia != ib)
            out[m++] = cross(a, b, vertical, c);
    }
    return m;
}

/* The polygon on the screen (with a pixel round it), its corners on whole pixels */
static int screen_polygon(const struct pt *p, int n, struct ipt *out)
{
    struct pt a[12], b[12];
    int i, inside = 1;
    for (i = 0; i < n; i++)
        if (p[i].x < 0 || p[i].x > view_w + 1 || p[i].y < -1 || p[i].y > view_h + 1)
            inside = 0;
    if (inside)
        memcpy(a, p, n * sizeof *p);
    else {
        n = clip(p, n, b, 1, 0, 1);
        n = clip(b, n, a, 1, view_w + 1, 0);
        n = clip(a, n, b, 0, -1, 1);
        n = clip(b, n, a, 0, view_h + 1, 0);
    }
    for (i = 0; i < n; i++) {
        out[i].x = round_int(a[i].x);
        out[i].y = round_int(a[i].y);
    }
    return n;
}

/* Ground that will be painted. The spans fill a row from floor(left) to floor(right), so pixel
 * x of a row is filled when left < x + 1 <= right: the triangle is sampled at X = x + 1. With
 * close_gaps' exact edges, neighbouring squares (which share their corners) leave no gaps, and
 * here each edge is followed from its left end by whole steps (exact, in 1/65536 of a row), so a
 * shared edge gives both triangles the same numbers. Per column, a triangle whose bottom reaches
 * the covering ground's top (edge[x]) raises it to its own top; anything missing (a hole, a
 * see-through or turned-away triangle, the sky) breaks the chain. The rows surely painted start
 * two rows below the top (horizon[x]): the corners here are rounded, the engine's truncated.
 * Returns 1 if it raised any column. */
static int cover(const struct ipt *p, int n)
{
    int i, X, lo = view_w + 1, hi = 0, grew = 0;
    for (i = 0; i < n; i++) {
        if (p[i].x < lo) lo = p[i].x;
        if (p[i].x > hi) hi = p[i].x;
    }
    if (lo < 1) lo = 1;
    if (hi > view_w) hi = view_w;
    if (lo > hi)
        return 0;
    for (X = lo; X <= hi; X++) {
        col_top[X] = 0x7FFFFFFF;
        col_bottom[X] = -0x7FFFFFFF;
    }
    for (i = 0; i < n; i++) {
        const struct ipt *a = &p[i], *b = &p[(i + 1) % n];
        int32_t dx, numerator, q, r, v, acc, X1;
        if (a->x == b->x)
            continue;
        if (a->x > b->x) {
            const struct ipt *s = a;
            a = b;
            b = s;
        }
        dx = b->x - a->x;
        numerator = (b->y - a->y) * 65536;  /* rows are under 2^14: fits */
        q = numerator / dx;
        r = numerator - q * dx;
        if (r < 0) {
            q--;
            r += dx;
        }
        v = a->y * 65536;
        acc = 0;
        X1 = b->x < hi ? b->x : hi;
        for (X = a->x; X <= X1; X++) {
            if (X >= lo) {
                if (v < col_top[X]) col_top[X] = v;
                if (v > col_bottom[X]) col_bottom[X] = v;
            }
            v += q;
            acc += r;
            if (acc >= dx) {
                v++;
                acc -= dx;
            }
        }
    }
    for (X = lo; X <= hi; X++) {
        int32_t top = col_top[X];
        int x = X - 1;
        if (top < col_bottom[X] && col_bottom[X] >= edge[x] && top < edge[x]) {
            int row = ((top + 65535) >> 16) + 2;
            edge[x] = top;
            horizon[x] = row < 0 ? 0 : row;
            grew = 1;
        }
    }
    return grew;
}

/* Everything the rectangle could paint is already covered (or it's off the view) */
static int covered(const struct rect *r)
{
    int x;
    if (r->x1 < 0 || r->x0 >= view_w || r->y1 < 0 || r->y0 >= view_h)
        return 1;
    for (x = r->x0 - 2 < 0 ? 0 : r->x0 - 2; x <= r->x1 + 2 && x < view_w; x++)
        if (r->y0 < horizon[x])
            return 0;
    return 1;
}

static void unlink_node(uint8_t *node)
{
    uint8_t *next = *(uint8_t **)(node + 4), *prev = *(uint8_t **)(node + 8);
    *(uint8_t **)(prev + 4) = next;
    *(uint8_t **)(next + 8) = prev;
}

/* ---- a landscape record: cull it if covered, else let it cover ---- */
static void ground(uint8_t *record)
{
    static const int tri_at[2] = {0xD, 0x65};
    struct pt poly[2][4];
    int n[2], k, i;
    brr_horizon_records++;
    for (k = 0; k < 2; k++)
        n[k] = triangle_polygon(record + tri_at[k], poly[k]);
    /* squares the near plane cuts are never culled */
    if (n[0] == 3 && n[1] == 3 && in_front(record + tri_at[0]) && in_front(record + tri_at[1])) {
        struct rect r;
        double minx = poly[0][0].x, maxx = minx, miny = poly[0][0].y, maxy = miny;
        for (k = 0; k < 2; k++)
            for (i = 0; i < 3; i++) {
                if (poly[k][i].x < minx) minx = poly[k][i].x;
                if (poly[k][i].x > maxx) maxx = poly[k][i].x;
                if (poly[k][i].y < miny) miny = poly[k][i].y;
                if (poly[k][i].y > maxy) maxy = poly[k][i].y;
            }
        if (minx < -FAR_AWAY || maxx > FAR_AWAY || miny < -FAR_AWAY || maxy > FAR_AWAY)
            goto keep;
        r.x0 = round_int(minx) - 2;  /* the engine's corners are truncated, ours rounded */
        r.x1 = round_int(maxx) + 2;
        r.y0 = round_int(miny) - 2;
        r.y1 = round_int(maxy) + 2;
        if (covered(&r)) {
            unlink_node(record);
            brr_horizon_records_culled++;
            return;
        }
    }
keep:
    {
        int covers[2] = {0, 0}, pass, m[2];
        struct ipt screen[2][12];
        for (k = 0; k < 2; k++) {
            const uint8_t *tri = record + tri_at[k];
            uint32_t fill = *(const uint32_t *)tri & 0xFF;
            /* turned away, or so thin that rounding could turn it: it doesn't cover */
            if (n[k] < 3 || signed_area(poly[k], n[k]) <= 64.0)
                continue;
            if (fill == 3 || fill == 4) {
                const int32_t *uv = (const int32_t *)(tri + 0x34);
                int32_t u = uv[0], v = uv[1];
                for (i = 1; i < 3; i++) {
                    if (uv[2 * i] < u) u = uv[2 * i];
                    if (uv[2 * i + 1] < v) v = uv[2 * i + 1];
                }
                if (u < 0 || v < 0
                    || !square_opaque(*(const uint8_t *const *)(tri + 4), ((v >> 16) / 32) * 8 + (u >> 16) / 32))
                    continue;
            } else if (fill != 0)
                continue;
            m[k] = screen_polygon(poly[k], n[k], screen[k]);
            covers[k] = m[k] >= 3;
        }
        /* the square's two triangles in either order: the farther one at a column joins on
         * only once the nearer one has */
        for (pass = 0; pass < 2; pass++) {
            int grew = 0;
            for (k = 0; k < 2; k++)
                if (covers[k])
                    grew |= cover(screen[k], m[k]);
            if (!grew || !(covers[0] && covers[1]))
                break;
        }
    }
}

/* ---- objects ---- */
/* The screen rectangle round a sphere (world centre c, radius r): 0 if the near plane cuts it */
static int sphere_rect(const double c[3], double radius, struct rect *out)
{
    double v[3], lo, hi, a, b;
    view_space(c, v);
    if (v[2] - radius < NEAR_Z)
        return 0;
    a = v[0] + radius;
    b = v[0] - radius;
    hi = centre_x + scale_x * a / (a > 0 ? v[2] - radius : v[2] + radius);
    lo = centre_x + scale_x * b / (b < 0 ? v[2] - radius : v[2] + radius);
    if (fabs(lo) > FAR_AWAY || fabs(hi) > FAR_AWAY)
        return 0;
    out->x0 = round_int(lo < hi ? lo : hi) - 3;
    out->x1 = round_int(lo < hi ? hi : lo) + 3;
    a = v[1] + radius;
    b = v[1] - radius;
    hi = centre_y + scale_y * a / (a > 0 ? v[2] - radius : v[2] + radius);
    lo = centre_y + scale_y * b / (b < 0 ? v[2] - radius : v[2] + radius);
    if (fabs(lo) > FAR_AWAY || fabs(hi) > FAR_AWAY)
        return 0;
    out->y0 = round_int(lo < hi ? lo : hi) - 3;
    out->y1 = round_int(lo < hi ? hi : lo) + 3;
    return 1;
}

/* How far a ground-fitted model's corners can move: the ground's height round it, from its
 * own height (scenery_fit_mesh_to_ground 0x42B600 sets each to the ground beneath) */
static double ground_range(const double c[3], double radius)
{
    int x0 = ((int)(c[0] - radius) >> 9) - 1, x1 = ((int)(c[0] + radius) >> 9) + 1;
    int z0 = ((int)(c[2] - radius) >> 9) - 1, z1 = ((int)(c[2] + radius) >> 9) + 1;
    double most = 0;
    int x, z;
    if (x1 - x0 > 16 || z1 - z0 > 16)
        return -1;
    for (z = z0; z <= z1; z++)
        for (x = x0; x <= x1; x++) {
            double d = -16.0 * LAND_HEIGHTS[(z & 255) * 256 + (x & 255)] - c[1];
            if (d < 0) d = -d;
            if (d > most) most = d;
        }
    return most;
}

static int object_rect(const uint8_t *object, int fitted, struct rect *out)
{
    const uint8_t *model = *(const uint8_t *const *)(object + 0x18);
    const uint8_t *parent = *(const uint8_t *const *)(object + 0x14);
    const int32_t *pos = (const int32_t *)object;
    double c[3], radius;
    if (!model)
        return 0;
    radius = *(const int32_t *)model;
    if (radius < 0 || radius > 65536)
        return 0;
    if (parent) {  /* a vehicle's part: round its vehicle, its offset counted twice over */
        const int32_t *pp = (const int32_t *)parent;
        if (*(const uint8_t *const *)(parent + 0x14))
            return 0;
        radius += 2.0 * sqrt((double)pos[0] * pos[0] + (double)pos[1] * pos[1] + (double)pos[2] * pos[2]) + 16;
        c[0] = pp[0], c[1] = pp[1], c[2] = pp[2];
    } else
        c[0] = pos[0], c[1] = pos[1], c[2] = pos[2];
    radius += radius / 64 + 8 + 256;  /* the model's own overshoot; sprites beyond its radius */
    if (fitted) {
        double range = ground_range(c, radius);
        if (range < 0)
            return 0;
        radius += range;
    }
    return sphere_rect(c, radius, out);
}

/* An engine object in a vehicle record or among its parts (vehicles 0x452A90, 7 x 0x395) */
static int in_vehicle(const uint8_t *object)
{
    const uint8_t *parent = *(const uint8_t *const *)(object + 0x14);
    return ((uint32_t)object >= 0x452A90u && (uint32_t)object < 0x452A90u + 7 * 0x395u) || parent != NULL;
}

static int shadow_rect(const uint8_t *polygon, struct rect *out)
{
    const int32_t *w = (const int32_t *)(polygon + 0xC);
    double lo_x = 0, hi_x = 0, lo_y = 0, hi_y = 0;
    int i;
    for (i = 0; i < 3; i++) {
        double c[3], v[3];
        struct pt s;
        c[0] = w[3 * i], c[1] = w[3 * i + 1], c[2] = w[3 * i + 2];
        view_space(c, v);
        if (v[2] < NEAR_Z)
            return 0;
        project(v, &s);
        if (!i || s.x < lo_x) lo_x = s.x;
        if (!i || s.x > hi_x) hi_x = s.x;
        if (!i || s.y < lo_y) lo_y = s.y;
        if (!i || s.y > hi_y) hi_y = s.y;
    }
    if (fabs(lo_x) > FAR_AWAY || fabs(hi_x) > FAR_AWAY || fabs(lo_y) > FAR_AWAY || fabs(hi_y) > FAR_AWAY)
        return 0;
    out->x0 = round_int(lo_x) - 3;
    out->x1 = round_int(hi_x) + 3;
    out->y0 = round_int(lo_y) - 3;
    out->y1 = round_int(hi_y) + 3;
    return 1;
}

/* Site display_list_call: before display_list_draw */
static void cull(void);

static int64_t draw_start;
int64_t brr_horizon_draw_time;  /* display_list_draw's own time, for the log */

void __cdecl brr_horizon_cull(void)
{
    int64_t start = brr_qpc();
    cull();
    draw_start = brr_qpc();
    brr_horizon_time += draw_start - start;
    brr_horizon_frames++;
}

/* After display_list_draw */
void __cdecl brr_horizon_drawn(void)
{
    brr_horizon_draw_time += brr_qpc() - draw_start;
}

static void cull(void)
{
    uint8_t *node;
    int i;
    if (cfg.debug_horizon_log == 999)
        return;  /* tests: the hook and the list's drawing time, without culling */
    view_w = VIEW_W;
    view_h = VIEW_H;
    if (view_w <= 0 || view_h <= 0 || view_w > MAX_COLUMNS || !LIST_LAST)
        return;
    for (i = 0; i < 9; i++)
        cam_m[i] = CAMERA_MATRIX[i];
    for (i = 0; i < 3; i++)
        cam_p[i] = CAMERA_POS[i];
    scale_x = SCALE_X;
    scale_y = SCALE_Y;
    centre_x = CENTRE_X;
    centre_y = CENTRE_Y;
    if (scale_x <= 0 || scale_y <= 0)
        return;
    pages_check_level();
    for (i = 0; i < view_w; i++) {
        horizon[i] = view_h;
        edge[i] = view_h * 65536;  /* the covered area starts at the picture's bottom edge */
    }
    {
        static int started;
        if (cfg.debug_horizon_log > 0 && cfg.debug_horizon_log < 999 && !started && *GAME_TICK_COUNT >= 300) {
            started = 1;
            logging = cfg.debug_horizon_log;
        }
    }
    for (node = LIST_LAST; node && node != LIST_SENTINEL;) {
        uint8_t *prev = *(uint8_t **)(node + 8);
        uint8_t type = node[0xC];
        struct rect r;
        if (type == 7)
            ground(node);
        else if (type == 0 || type == 2 || type == 3 || type == 4 || type == 5 || type == 8) {
            const uint8_t *payload = *(const uint8_t *const *)(node + 0xD);
            int ok;
            brr_horizon_objects++;
            if (!payload)
                ok = 0;
            else if (type != 2 && in_vehicle(payload))
                ok = 0;  /* vehicles and their parts: kept (their wheels and parts move from the model) */
            else if (type == 2)
                ok = shadow_rect(payload, &r);
            else
                ok = object_rect(payload, type == 3 || type == 5 || type == 8, &r);
            if (ok && covered(&r)) {
                unlink_node(node);
                brr_horizon_objects_culled++;
            }
        }
        node = prev;
    }
    if (logging) {
        char text[400];
        int n = 0, x;
        for (x = 0; x < view_w && n < 380; x += view_w / 24)
            n += wsprintfA(text + n, " %d", horizon[x]);
        brr_log("horizon: view %dx%d, scale %g,%g centre %g,%g, camera %g,%g,%g; h every %d columns:%s", view_w, view_h,
                scale_x, scale_y, centre_x, centre_y, cam_p[0], cam_p[1], cam_p[2], view_w / 24, text);
        logging--;
    }
}
