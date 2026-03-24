#include "brush_render.h"

#include "geometry_helpers.h"

#include <stdlib.h>

void stamp_brush(Canvas *c, int cx, int cy, int radius, uint32_t color, BrushShape shape) {
    int dx;
    int dy;

    if (!c || !c->pixels || radius <= 0) {
        return;
    }
    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            if (!brush_mask_contains(shape, dx, dy, radius)) {
                continue;
            }
            canvas_set_pixel(c, cx + dx, cy + dy, color);
        }
    }
}

void erase_stamp(Canvas *c, int cx, int cy, int radius, uint32_t clear_color, BrushShape shape) {
    int dx;
    int dy;

    if (!c || !c->pixels || radius <= 0) {
        return;
    }
    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            if (!brush_mask_contains(shape, dx, dy, radius)) {
                continue;
            }
            canvas_set_pixel_raw(c, cx + dx, cy + dy, clear_color);
        }
    }
}

void erase_line(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t clear_color, BrushShape shape) {
    int dx;
    int sx;
    int dy;
    int sy;
    int err;

    if (!c || !c->pixels) {
        return;
    }
    dx = abs(x1 - x0);
    sx = x0 < x1 ? 1 : -1;
    dy = -abs(y1 - y0);
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;

    while (1) {
        erase_stamp(c, x0, y0, radius, clear_color, shape);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        {
            int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}

void draw_brush_line(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t color, BrushShape shape) {
    int dx;
    int sx;
    int dy;
    int sy;
    int err;

    if (!c || !c->pixels) {
        return;
    }
    dx = abs(x1 - x0);
    sx = x0 < x1 ? 1 : -1;
    dy = -abs(y1 - y0);
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;

    while (1) {
        stamp_brush(c, x0, y0, radius, color, shape);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        {
            int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}
