#ifndef APP_BRUSH_H
#define APP_BRUSH_H

#include "canvas.h"
#include "history_state.h"
#include "layers.h"

typedef enum {
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_LINE,
    TOOL_RECT,
    TOOL_FILLED_RECT,
    TOOL_ELLIPSE,
    TOOL_FILLED_ELLIPSE
} Tool;

typedef enum {
    BRUSH_SHAPE_ROUND = 0,
    BRUSH_SHAPE_SQUARE,
    BRUSH_SHAPE_DIAMOND,
    BRUSH_SHAPE_COUNT
} BrushShape;

typedef enum {
    APP_STROKE_MARK_BRUSH,
    APP_STROKE_MARK_ERASE
} AppStrokeMark;

const char *app_tool_label(Tool tool);
const char *app_brush_shape_label(BrushShape shape);
BrushShape app_cycle_brush_shape(BrushShape shape, int direction);
int app_tool_draws_directly(Tool tool);
AppStrokeMark app_tool_stroke_mark(Tool tool);
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
);
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
);
void app_stamp_brush(Canvas *canvas, int cx, int cy, int radius, uint32_t color, BrushShape shape);
void app_erase_brush(Canvas *canvas, int cx, int cy, int radius, uint32_t clear_color, BrushShape shape);
void app_draw_brush_line(Canvas *canvas, int x0, int y0, int x1, int y1, int radius, uint32_t color, BrushShape shape);
void app_erase_brush_line(Canvas *canvas, int x0, int y0, int x1, int y1, int radius, uint32_t clear_color, BrushShape shape);

#endif
