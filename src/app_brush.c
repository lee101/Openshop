#include "app_brush.h"
#include "app_brush_mask.h"
#include "app_layer_state.h"

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

int app_begin_direct_stroke(
    LayerStack *layers,
    int x,
    int y,
    Tool tool,
    BrushShape shape,
    int radius,
    uint32_t brush_color,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *drawing,
    int *needs_composite
) {
    Layer *active = NULL;

    if (!layers || !undo_stack || !undo_count || undo_capacity <= 0 || !redo_stack || !redo_count || !drawing) {
        return 0;
    }

    active = app_active_editable_layer(layers);
    if (!active) {
        return 0;
    }

    snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
    *drawing = 1;
    if (app_tool_stroke_mark(tool) == APP_STROKE_MARK_ERASE) {
        app_erase_brush(&active->canvas, x, y, radius, app_active_layer_clear_color(layers->active_layer), shape);
    } else {
        app_stamp_brush(&active->canvas, x, y, radius, brush_color, shape);
    }
    if (needs_composite) {
        *needs_composite = 1;
    }
    return 1;
}

int app_continue_direct_stroke(
    LayerStack *layers,
    int *last_x,
    int *last_y,
    int x,
    int y,
    Tool tool,
    BrushShape shape,
    int radius,
    uint32_t brush_color,
    int *needs_composite
) {
    Layer *active = NULL;

    if (!layers || !last_x || !last_y) {
        return 0;
    }

    active = app_active_editable_layer(layers);
    if (!active) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= active->canvas.width || y >= active->canvas.height) {
        return 0;
    }

    if (app_tool_stroke_mark(tool) == APP_STROKE_MARK_ERASE) {
        app_erase_brush_line(&active->canvas, *last_x, *last_y, x, y, radius, app_active_layer_clear_color(layers->active_layer), shape);
    } else {
        app_draw_brush_line(&active->canvas, *last_x, *last_y, x, y, radius, brush_color, shape);
    }
    *last_x = x;
    *last_y = y;
    if (needs_composite) {
        *needs_composite = 1;
    }
    return 1;
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
