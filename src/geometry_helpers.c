#include "geometry_helpers.h"

#include <stdlib.h>

int brush_mask_contains(BrushShape shape, int x, int y, int radius) {
    switch (shape) {
    case BRUSH_SHAPE_SQUARE:
        return abs(x) <= radius && abs(y) <= radius;
    case BRUSH_SHAPE_DIAMOND:
        return abs(x) + abs(y) <= radius;
    case BRUSH_SHAPE_ROUND:
    default:
        return x * x + y * y <= radius * radius;
    }
}

void constrain_shape_end(Tool tool, int x0, int y0, int x1, int y1, int shift, int *out_x, int *out_y) {
    int dx;
    int dy;
    int adx;
    int ady;

    if (!out_x || !out_y) {
        return;
    }
    *out_x = x1;
    *out_y = y1;
    if (!shift) {
        return;
    }

    dx = x1 - x0;
    dy = y1 - y0;
    adx = abs(dx);
    ady = abs(dy);

    if (tool == TOOL_LINE) {
        if (adx > ady * 2) {
            *out_x = x0 + (dx >= 0 ? adx : -adx);
            *out_y = y0;
        } else if (ady > adx * 2) {
            *out_x = x0;
            *out_y = y0 + (dy >= 0 ? ady : -ady);
        } else {
            int len = adx > ady ? adx : ady;
            *out_x = x0 + (dx >= 0 ? len : -len);
            *out_y = y0 + (dy >= 0 ? len : -len);
        }
    } else if (tool == TOOL_RECT || tool == TOOL_FILLED_RECT || tool == TOOL_ELLIPSE || tool == TOOL_FILLED_ELLIPSE) {
        int len = adx > ady ? adx : ady;
        *out_x = x0 + (dx >= 0 ? len : -len);
        *out_y = y0 + (dy >= 0 ? len : -len);
    }
}
