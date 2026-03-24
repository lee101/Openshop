#include "app_brush.h"
#include "app_brush_mask.h"

#include <stdlib.h>

const char *app_tool_label(Tool tool) {
    switch (tool) {
    case TOOL_BRUSH:
        return "Brush";
    case TOOL_ERASER:
        return "Eraser";
    case TOOL_LINE:
        return "Line";
    case TOOL_RECT:
        return "Rectangle";
    case TOOL_FILLED_RECT:
        return "Filled Rectangle";
    case TOOL_ELLIPSE:
        return "Ellipse";
    case TOOL_FILLED_ELLIPSE:
        return "Filled Ellipse";
    default:
        return "Brush";
    }
}

const char *app_brush_shape_label(BrushShape shape) {
    switch (shape) {
    case BRUSH_SHAPE_ROUND:
        return "Round";
    case BRUSH_SHAPE_SQUARE:
        return "Square";
    case BRUSH_SHAPE_DIAMOND:
        return "Diamond";
    default:
        return "Round";
    }
}

BrushShape app_cycle_brush_shape(BrushShape shape, int direction) {
    int idx = (int)shape + direction;
    if (idx < 0) {
        idx = BRUSH_SHAPE_COUNT - 1;
    } else if (idx >= BRUSH_SHAPE_COUNT) {
        idx = 0;
    }
    return (BrushShape)idx;
}

int app_tool_draws_directly(Tool tool) {
    return tool == TOOL_BRUSH || tool == TOOL_ERASER;
}

AppStrokeMark app_tool_stroke_mark(Tool tool) {
    return tool == TOOL_ERASER ? APP_STROKE_MARK_ERASE : APP_STROKE_MARK_BRUSH;
}

void app_stamp_brush(Canvas *canvas, int cx, int cy, int radius, uint32_t color, BrushShape shape) {
    if (!canvas || !canvas->pixels || radius <= 0) {
        return;
    }
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (!app_brush_mask_contains(shape, dx, dy, radius)) {
                continue;
            }
            canvas_set_pixel(canvas, cx + dx, cy + dy, color);
        }
    }
}

void app_erase_brush(Canvas *canvas, int cx, int cy, int radius, uint32_t clear_color, BrushShape shape) {
    if (!canvas || !canvas->pixels || radius <= 0) {
        return;
    }
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (!app_brush_mask_contains(shape, dx, dy, radius)) {
                continue;
            }
            canvas_set_pixel_raw(canvas, cx + dx, cy + dy, clear_color);
        }
    }
}

void app_draw_brush_line(Canvas *canvas, int x0, int y0, int x1, int y1, int radius, uint32_t color, BrushShape shape) {
    if (!canvas || !canvas->pixels) {
        return;
    }
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        app_stamp_brush(canvas, x0, y0, radius, color, shape);
        if (x0 == x1 && y0 == y1) {
            break;
        }
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

void app_erase_brush_line(Canvas *canvas, int x0, int y0, int x1, int y1, int radius, uint32_t clear_color, BrushShape shape) {
    if (!canvas || !canvas->pixels) {
        return;
    }
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        app_erase_brush(canvas, x0, y0, radius, clear_color, shape);
        if (x0 == x1 && y0 == y1) {
            break;
        }
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
