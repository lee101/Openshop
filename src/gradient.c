#include "gradient.h"

#include <math.h>

int gradient_type_valid(int type) {
    return type >= 0 && type < GRADIENT_TYPE_COUNT;
}

uint32_t gradient_lerp_argb(uint32_t start, uint32_t end, double t) {
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    {
        uint8_t a = (uint8_t)(((start >> 24) & 0xFF) + (((int)((end >> 24) & 0xFF) - (int)((start >> 24) & 0xFF)) * t) + 0.5);
        uint8_t r = (uint8_t)(((start >> 16) & 0xFF) + (((int)((end >> 16) & 0xFF) - (int)((start >> 16) & 0xFF)) * t) + 0.5);
        uint8_t g = (uint8_t)(((start >> 8) & 0xFF) + (((int)((end >> 8) & 0xFF) - (int)((start >> 8) & 0xFF)) * t) + 0.5);
        uint8_t b = (uint8_t)((start & 0xFF) + (((int)(end & 0xFF) - (int)(start & 0xFF)) * t) + 0.5);
        return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
}

void canvas_gradient_fill(Canvas *c, int x0, int y0, int x1, int y1, uint32_t start, uint32_t end, GradientType type) {
    double dx;
    double dy;
    double len2;

    if (!c || !c->pixels) {
        return;
    }
    dx = (double)(x1 - x0);
    dy = (double)(y1 - y0);
    len2 = dx * dx + dy * dy;
    if (len2 <= 0.0) {
        canvas_clear(c, end);
        return;
    }

    for (int y = 0; y < c->height; y++) {
        for (int x = 0; x < c->width; x++) {
            double t;
            if (type == GRADIENT_RADIAL) {
                double px = (double)(x - x0);
                double py = (double)(y - y0);
                t = sqrt(px * px + py * py) / sqrt(len2);
            } else {
                t = ((x - x0) * dx + (y - y0) * dy) / len2;
            }
            canvas_set_pixel(c, x, y, gradient_lerp_argb(start, end, t));
        }
    }
}
