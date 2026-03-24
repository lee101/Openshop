#ifndef SHAPE_DRAW_H
#define SHAPE_DRAW_H

#include "brush_state.h"
#include "canvas.h"

void draw_shape(Canvas *c, Tool tool, int x0, int y0, int x1, int y1, int radius, uint32_t color);

#endif
