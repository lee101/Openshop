#include "canvas.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------- */

static uint8_t blend_channel(uint8_t src, uint8_t dst, uint8_t src_alpha) {
    int inv = 255 - src_alpha;
    int value = src * src_alpha + dst * inv + 127;
    return (uint8_t)(value / 255);
}

/* ---------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------- */

int canvas_init(Canvas *c, int width, int height) {
    if (!c || width <= 0 || height <= 0) return 0;
    c->width  = width;
    c->height = height;
    c->pixels = (uint32_t *)calloc((size_t)width * (size_t)height, sizeof(uint32_t));
    return c->pixels ? 1 : 0;
}

void canvas_free(Canvas *c) {
    if (!c) return;
    free(c->pixels);
    c->pixels = NULL;
    c->width  = 0;
    c->height = 0;
}

void canvas_clear(Canvas *c, uint32_t color) {
    if (!c || !c->pixels) return;
    size_t n = (size_t)c->width * (size_t)c->height;
    /* fast path: memset for black or white */
    if (color == 0x00000000 || color == 0xFFFFFFFF) {
        uint8_t byte = (color == 0xFFFFFFFF) ? 0xFF : 0x00;
        memset(c->pixels, byte, n * sizeof(uint32_t));
    } else {
        for (size_t i = 0; i < n; i++) c->pixels[i] = color;
    }
}

/* ---------------------------------------------------------------
 * Pixel access
 * --------------------------------------------------------------- */

void canvas_set_pixel(Canvas *c, int x, int y, uint32_t color) {
    if (!c || !c->pixels) return;
    if (x < 0 || y < 0 || x >= c->width || y >= c->height) return;
    uint8_t sa = (uint8_t)((color >> 24) & 0xFF);
    if (sa == 0) return;
    size_t idx = (size_t)y * (size_t)c->width + (size_t)x;
    if (sa == 255) {
        c->pixels[idx] = color;
        return;
    }
    uint32_t dst = c->pixels[idx];
    uint8_t sr = (uint8_t)((color >> 16) & 0xFF);
    uint8_t sg = (uint8_t)((color >>  8) & 0xFF);
    uint8_t sb = (uint8_t)(color & 0xFF);
    uint8_t dr = (uint8_t)((dst >> 16) & 0xFF);
    uint8_t dg = (uint8_t)((dst >>  8) & 0xFF);
    uint8_t db = (uint8_t)(dst & 0xFF);
    uint8_t da = (uint8_t)((dst >> 24) & 0xFF);
    uint8_t out_a = (uint8_t)(sa + ((da * (255 - sa) + 127) / 255));
    c->pixels[idx] = ((uint32_t)out_a            << 24)
                   | ((uint32_t)blend_channel(sr, dr, sa) << 16)
                   | ((uint32_t)blend_channel(sg, dg, sa) <<  8)
                   |  (uint32_t)blend_channel(sb, db, sa);
}

uint32_t canvas_get_pixel(const Canvas *c, int x, int y) {
    if (!c || !c->pixels) return 0;
    if (x < 0 || y < 0 || x >= c->width || y >= c->height) return 0;
    return c->pixels[(size_t)y * (size_t)c->width + (size_t)x];
}

/* ---------------------------------------------------------------
 * Hard-edge circle (filled disc, midpoint rasterisation)
 * --------------------------------------------------------------- */

void canvas_draw_circle(Canvas *c, int cx, int cy, int radius, uint32_t color) {
    if (!c || !c->pixels || radius <= 0) return;
    int r2 = radius * radius;
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= r2) {
                canvas_set_pixel(c, cx + x, cy + y, color);
            }
        }
    }
}

/* ---------------------------------------------------------------
 * Bresenham line with circular brush head
 * --------------------------------------------------------------- */

void canvas_draw_line(Canvas *c, int x0, int y0, int x1, int y1,
                      int radius, uint32_t color) {
    if (!c || !c->pixels) return;
    int dx =  abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        canvas_draw_circle(c, x0, y0, radius, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ---------------------------------------------------------------
 * Rectangle outline
 * --------------------------------------------------------------- */

void canvas_draw_rect_outline(Canvas *c, int x0, int y0, int x1, int y1,
                               int radius, uint32_t color) {
    if (!c || !c->pixels) return;
    int left   = x0 < x1 ? x0 : x1;
    int right  = x0 < x1 ? x1 : x0;
    int top    = y0 < y1 ? y0 : y1;
    int bottom = y0 < y1 ? y1 : y0;
    canvas_draw_line(c, left,  top,    right, top,    radius, color);
    canvas_draw_line(c, right, top,    right, bottom, radius, color);
    canvas_draw_line(c, right, bottom, left,  bottom, radius, color);
    canvas_draw_line(c, left,  bottom, left,  top,    radius, color);
}

/* ---------------------------------------------------------------
 * Ellipse outline (parametric midpoint)
 * --------------------------------------------------------------- */

void canvas_draw_ellipse_outline(Canvas *c, int cx, int cy, int rx, int ry,
                                  int radius, uint32_t color) {
    if (!c || !c->pixels || rx <= 0 || ry <= 0) return;
    for (int y = -ry; y <= ry; y++) {
        double norm = 1.0 - ((double)(y * y) / (double)(ry * ry));
        if (norm < 0.0) continue;
        int x = (int)((double)rx * sqrt(norm) + 0.5);
        canvas_draw_circle(c, cx + x, cy + y, radius, color);
        canvas_draw_circle(c, cx - x, cy + y, radius, color);
    }
}

/* ---------------------------------------------------------------
 * Filled shapes
 * --------------------------------------------------------------- */

void canvas_fill_rect(Canvas *c, int x0, int y0, int x1, int y1,
                      uint32_t color) {
    if (!c || !c->pixels) return;
    int left   = x0 < x1 ? x0 : x1;
    int right  = x0 < x1 ? x1 : x0;
    int top    = y0 < y1 ? y0 : y1;
    int bottom = y0 < y1 ? y1 : y0;
    if (left   < 0)          left   = 0;
    if (top    < 0)          top    = 0;
    if (right  >= c->width)  right  = c->width  - 1;
    if (bottom >= c->height) bottom = c->height - 1;
    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            canvas_set_pixel(c, x, y, color);
        }
    }
}

void canvas_fill_ellipse(Canvas *c, int cx, int cy, int rx, int ry,
                          uint32_t color) {
    if (!c || !c->pixels || rx <= 0 || ry <= 0) return;
    for (int y = -ry; y <= ry; y++) {
        double norm = 1.0 - ((double)(y * y) / (double)(ry * ry));
        if (norm < 0.0) continue;
        int hw = (int)((double)rx * sqrt(norm) + 0.5);
        for (int x = -hw; x <= hw; x++) {
            canvas_set_pixel(c, cx + x, cy + y, color);
        }
    }
}

/* ---------------------------------------------------------------
 * Soft (SDF/Gaussian) brush — smooth anti-aliased falloff.
 *
 * Uses exp(-t^2 * 3) where t = dist/radius (0=centre, 1=edge).
 * The brush colour's alpha acts as maximum centre opacity so the
 * user's opacity slider still works as expected.
 * --------------------------------------------------------------- */

void canvas_draw_soft_circle(Canvas *c, int cx, int cy, int radius,
                              uint32_t color) {
    if (!c || !c->pixels || radius <= 0) return;
    uint8_t base_a = (uint8_t)((color >> 24) & 0xFF);
    uint8_t cr     = (uint8_t)((color >> 16) & 0xFF);
    uint8_t cg     = (uint8_t)((color >>  8) & 0xFF);
    uint8_t cb     = (uint8_t)(color & 0xFF);
    float   r2f    = (float)(radius * radius);

    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            float dist2 = (float)(x * x + y * y);
            if (dist2 > r2f) continue;
            float t = sqrtf(dist2) / (float)radius;
            /* gaussian falloff: full at t=0, ~5% at t=1 */
            float weight = expf(-t * t * 3.0f);
            uint8_t a = (uint8_t)((float)base_a * weight + 0.5f);
            if (a == 0) continue;
            uint32_t px = ((uint32_t)a  << 24)
                        | ((uint32_t)cr << 16)
                        | ((uint32_t)cg <<  8)
                        |  (uint32_t)cb;
            canvas_set_pixel(c, cx + x, cy + y, px);
        }
    }
}

void canvas_draw_soft_line(Canvas *c, int x0, int y0, int x1, int y1,
                            int radius, uint32_t color) {
    if (!c || !c->pixels) return;
    int dx =  abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        canvas_draw_soft_circle(c, x0, y0, radius, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ---------------------------------------------------------------
 * Spray / airbrush
 *
 * Scatters `density` random pixels inside the disc.
 * Uses a uniform-area distribution (sqrt of uniform radial sample)
 * so drops don't cluster at the centre.
 * --------------------------------------------------------------- */

/* Simple LCG — avoids seeding rand() globally */
static uint32_t lcg_state = 0xDEADBEEFu;
static inline uint32_t lcg_next(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

void canvas_draw_spray(Canvas *c, int cx, int cy, int radius,
                       uint32_t color, int density) {
    if (!c || !c->pixels || radius <= 0 || density <= 0) return;
    for (int i = 0; i < density; i++) {
        float angle = (float)(lcg_next() % 3600) * (3.14159265f / 1800.0f);
        float r     = sqrtf((float)(lcg_next() % 1000) / 1000.0f) * (float)radius;
        int sx = cx + (int)(r * cosf(angle));
        int sy = cy + (int)(r * sinf(angle));
        canvas_set_pixel(c, sx, sy, color);
    }
}

/* ---------------------------------------------------------------
 * Flood fill (stack-based BFS)
 * --------------------------------------------------------------- */

typedef struct { int x; int y; } FillPoint;

int canvas_flood_fill(Canvas *c, int x, int y, uint32_t new_color) {
    if (!c || !c->pixels) return 0;
    if (x < 0 || y < 0 || x >= c->width || y >= c->height) return 0;
    uint32_t target = canvas_get_pixel(c, x, y);
    if (target == new_color) return 1;

    size_t     capacity = 1024;
    size_t     count    = 0;
    FillPoint *stack    = (FillPoint *)malloc(capacity * sizeof(FillPoint));
    if (!stack) return 0;

    stack[count++] = (FillPoint){x, y};
    while (count > 0) {
        FillPoint p = stack[--count];
        if (p.x < 0 || p.y < 0 || p.x >= c->width || p.y >= c->height) continue;
        if (canvas_get_pixel(c, p.x, p.y) != target) continue;
        canvas_set_pixel(c, p.x, p.y, new_color);
        if (count + 4 >= capacity) {
            size_t     nc   = capacity * 2;
            FillPoint *next = (FillPoint *)realloc(stack, nc * sizeof(FillPoint));
            if (!next) { free(stack); return 0; }
            stack    = next;
            capacity = nc;
        }
        stack[count++] = (FillPoint){p.x + 1, p.y};
        stack[count++] = (FillPoint){p.x - 1, p.y};
        stack[count++] = (FillPoint){p.x,     p.y + 1};
        stack[count++] = (FillPoint){p.x,     p.y - 1};
    }
    free(stack);
    return 1;
}
