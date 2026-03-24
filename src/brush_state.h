#ifndef BRUSH_STATE_H
#define BRUSH_STATE_H

#include <stdint.h>

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

uint32_t compose_brush_color(uint32_t rgb_color, int opacity_percent);
const char *tool_label(Tool tool);
const char *brush_shape_label(BrushShape shape);
BrushShape cycle_brush_shape(BrushShape shape, int direction);
void brush_state_set_color_tool(uint32_t color_rgb, int brush_opacity,
                                uint32_t *brush_color_rgb, uint32_t *brush_color,
                                Tool *tool, Tool next_tool);
void brush_state_adjust_opacity(int delta, uint32_t brush_color_rgb,
                                int *brush_opacity, uint32_t *brush_color);
void brush_state_set_tool(Tool next_tool, Tool *tool);
void brush_state_adjust_radius(int delta, int *brush_radius);
void brush_state_cycle_shape_in_place(BrushShape *brush_shape, int direction);

#endif
