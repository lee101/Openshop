#include "app_shape.h"

#include <stdlib.h>

void app_constrain_shape_end(Tool tool, int x0, int y0, int x1, int y1, int shift, int *out_x, int *out_y) {
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

void app_draw_shape(Canvas *canvas, Tool tool, int x0, int y0, int x1, int y1, int radius, uint32_t color) {
    if (!canvas || !canvas->pixels) {
        return;
    }

    switch (tool) {
    case TOOL_LINE:
        canvas_draw_line(canvas, x0, y0, x1, y1, radius, color);
        break;
    case TOOL_RECT:
        canvas_draw_rect_outline(canvas, x0, y0, x1, y1, radius, color);
        break;
    case TOOL_FILLED_RECT:
        canvas_draw_rect_filled(canvas, x0, y0, x1, y1, color);
        break;
    case TOOL_ELLIPSE: {
        int cx = (x0 + x1) / 2;
        int cy = (y0 + y1) / 2;
        int rx = abs(x1 - x0) / 2;
        int ry = abs(y1 - y0) / 2;
        canvas_draw_ellipse_outline(canvas, cx, cy, rx, ry, radius, color);
        break;
    }
    case TOOL_FILLED_ELLIPSE: {
        int cx = (x0 + x1) / 2;
        int cy = (y0 + y1) / 2;
        int rx = abs(x1 - x0) / 2;
        int ry = abs(y1 - y0) / 2;
        canvas_draw_ellipse_filled(canvas, cx, cy, rx, ry, color);
        break;
    }
    default:
        break;
    }
}
