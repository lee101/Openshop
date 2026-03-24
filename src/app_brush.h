#ifndef APP_BRUSH_H
#define APP_BRUSH_H

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

const char *app_tool_label(Tool tool);
const char *app_brush_shape_label(BrushShape shape);
BrushShape app_cycle_brush_shape(BrushShape shape, int direction);
int app_tool_draws_directly(Tool tool);

#endif
