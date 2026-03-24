#ifndef GEOMETRY_HELPERS_H
#define GEOMETRY_HELPERS_H

#include "brush_state.h"

int brush_mask_contains(BrushShape shape, int x, int y, int radius);
void constrain_shape_end(Tool tool, int x0, int y0, int x1, int y1, int shift, int *out_x, int *out_y);

#endif
