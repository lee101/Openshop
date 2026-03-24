#include "shape_draw.h"

#include <stdlib.h>

void draw_shape(Canvas *c, Tool tool, int x0, int y0, int x1, int y1, int radius, uint32_t color) {
    switch (tool) {
    case TOOL_LINE:
        canvas_draw_line(c, x0, y0, x1, y1, radius, color);
        break;
    case TOOL_RECT:
        canvas_draw_rect_outline(c, x0, y0, x1, y1, radius, color);
        break;
    case TOOL_FILLED_RECT:
        canvas_draw_rect_filled(c, x0, y0, x1, y1, color);
        break;
    case TOOL_ELLIPSE: {
        int cx = (x0 + x1) / 2;
        int cy = (y0 + y1) / 2;
        int rx = abs(x1 - x0) / 2;
        int ry = abs(y1 - y0) / 2;
        canvas_draw_ellipse_outline(c, cx, cy, rx, ry, radius, color);
        break;
    }
    case TOOL_FILLED_ELLIPSE: {
        int cx = (x0 + x1) / 2;
        int cy = (y0 + y1) / 2;
        int rx = abs(x1 - x0) / 2;
        int ry = abs(y1 - y0) / 2;
        canvas_draw_ellipse_filled(c, cx, cy, rx, ry, color);
        break;
    }
    default:
        break;
    }
}
