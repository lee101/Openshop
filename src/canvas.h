#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>

typedef struct {
    int width;
    int height;
    uint32_t *pixels; // ARGB8888
} Canvas;

int canvas_init(Canvas *c, int width, int height);
void canvas_free(Canvas *c);
void canvas_clear(Canvas *c, uint32_t color);
void canvas_set_pixel_raw(Canvas *c, int x, int y, uint32_t color);
void canvas_set_pixel(Canvas *c, int x, int y, uint32_t color);
uint32_t canvas_get_pixel(const Canvas *c, int x, int y);
void canvas_draw_circle(Canvas *c, int cx, int cy, int radius, uint32_t color);
void canvas_draw_line(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t color);
void canvas_draw_rect_outline(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t color);
void canvas_draw_rect_filled(Canvas *c, int x0, int y0, int x1, int y1, uint32_t color);
void canvas_draw_ellipse_outline(Canvas *c, int cx, int cy, int rx, int ry, int radius, uint32_t color);
void canvas_draw_ellipse_filled(Canvas *c, int cx, int cy, int rx, int ry, uint32_t color);
int canvas_flood_fill(Canvas *c, int x, int y, uint32_t new_color);
void canvas_flip_horizontal(Canvas *c);
void canvas_flip_vertical(Canvas *c);
void canvas_rotate_180(Canvas *c);
void canvas_invert_rgb(Canvas *c);
void canvas_translate(Canvas *c, int dx, int dy, uint32_t fill_color);
void canvas_grayscale(Canvas *c);

#endif
