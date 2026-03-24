#include "brush_state.h"

uint32_t compose_brush_color(uint32_t rgb_color, int opacity_percent) {
    if (opacity_percent < 1) {
        opacity_percent = 1;
    } else if (opacity_percent > 100) {
        opacity_percent = 100;
    }
    return (uint32_t)(((opacity_percent * 255 + 50) / 100) << 24) | (rgb_color & 0x00FFFFFF);
}

const char *tool_label(Tool tool) {
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

const char *brush_shape_label(BrushShape shape) {
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

BrushShape cycle_brush_shape(BrushShape shape, int direction) {
    int idx = (int)shape + direction;

    if (idx < 0) {
        idx = BRUSH_SHAPE_COUNT - 1;
    } else if (idx >= BRUSH_SHAPE_COUNT) {
        idx = 0;
    }
    return (BrushShape)idx;
}

void brush_state_set_color_tool(uint32_t color_rgb, int brush_opacity,
                                uint32_t *brush_color_rgb, uint32_t *brush_color,
                                Tool *tool, Tool next_tool) {
    if (!brush_color_rgb || !brush_color || !tool) {
        return;
    }
    *brush_color_rgb = color_rgb & 0x00FFFFFF;
    *brush_color = compose_brush_color(*brush_color_rgb, brush_opacity);
    *tool = next_tool;
}

void brush_state_adjust_opacity(int delta, uint32_t brush_color_rgb,
                                int *brush_opacity, uint32_t *brush_color) {
    if (!brush_opacity || !brush_color) {
        return;
    }
    *brush_opacity += delta;
    if (*brush_opacity < 1) {
        *brush_opacity = 1;
    } else if (*brush_opacity > 100) {
        *brush_opacity = 100;
    }
    *brush_color = compose_brush_color(brush_color_rgb, *brush_opacity);
}

void brush_state_set_tool(Tool next_tool, Tool *tool) {
    if (tool) {
        *tool = next_tool;
    }
}

void brush_state_adjust_radius(int delta, int *brush_radius) {
    if (!brush_radius) {
        return;
    }
    *brush_radius += delta;
    if (*brush_radius < 1) {
        *brush_radius = 1;
    } else if (*brush_radius > 64) {
        *brush_radius = 64;
    }
}

void brush_state_cycle_shape_in_place(BrushShape *brush_shape, int direction) {
    if (brush_shape) {
        *brush_shape = cycle_brush_shape(*brush_shape, direction);
    }
}
