#ifndef GRADIENT_H
#define GRADIENT_H

#include "canvas.h"
#include <stdint.h>

typedef enum {
    GRADIENT_LINEAR = 0,
    GRADIENT_RADIAL,
    GRADIENT_TYPE_COUNT
} GradientType;

int gradient_type_valid(int type);
uint32_t gradient_lerp_argb(uint32_t start, uint32_t end, double t);
void canvas_gradient_fill(Canvas *c, int x0, int y0, int x1, int y1, uint32_t start, uint32_t end, GradientType type);

#endif
