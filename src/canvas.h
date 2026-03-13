#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>

typedef struct {
    int width;
    int height;
    uint32_t *pixels; /* ARGB8888 */
} Canvas;

/* Lifecycle */
int      canvas_init(Canvas *c, int width, int height);
void     canvas_free(Canvas *c);
void     canvas_clear(Canvas *c, uint32_t color);

/* Pixel access */
void     canvas_set_pixel(Canvas *c, int x, int y, uint32_t color);
uint32_t canvas_get_pixel(const Canvas *c, int x, int y);

/* Hard-edge primitives (Bresenham / midpoint) */
void     canvas_draw_circle(Canvas *c, int cx, int cy, int radius, uint32_t color);
void     canvas_draw_line(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t color);
void     canvas_draw_rect_outline(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t color);
void     canvas_draw_ellipse_outline(Canvas *c, int cx, int cy, int rx, int ry, int radius, uint32_t color);

/* Filled shapes */
void     canvas_fill_rect(Canvas *c, int x0, int y0, int x1, int y1, uint32_t color);
void     canvas_fill_ellipse(Canvas *c, int cx, int cy, int rx, int ry, uint32_t color);

/*
 * Soft/SDF brush: gaussian falloff so edges fade smoothly.
 * The alpha of `color` sets the maximum opacity at the centre.
 */
void     canvas_draw_soft_circle(Canvas *c, int cx, int cy, int radius, uint32_t color);
void     canvas_draw_soft_line(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t color);

/*
 * Spray / airbrush: scatter `density` random dots inside the circle.
 * density ~ 20-80 gives a typical spray feel.
 */
void     canvas_draw_spray(Canvas *c, int cx, int cy, int radius, uint32_t color, int density);

/* Flood fill */
int      canvas_flood_fill(Canvas *c, int x, int y, uint32_t new_color);

#endif /* CANVAS_H */
