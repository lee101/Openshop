#ifndef APP_CANVAS_CLICK_H
#define APP_CANVAS_CLICK_H

#include "app_brush.h"
#include "canvas.h"
#include "history_state.h"
#include "layers.h"

#include <stdint.h>

typedef enum {
    APP_CANVAS_CLICK_NOOP = 0,
    APP_CANVAS_CLICK_DIRECT_STROKE,
    APP_CANVAS_CLICK_SHAPE_PREVIEW,
    APP_CANVAS_CLICK_SHAPE_FINALIZED,
    APP_CANVAS_CLICK_PREVIEW_CANCELED,
    APP_CANVAS_CLICK_COLOR_SAMPLED,
} AppCanvasClickResult;

AppCanvasClickResult app_handle_left_canvas_press(
    LayerStack *layers,
    int x,
    int y,
    int *last_x,
    int *last_y,
    Tool tool,
    BrushShape brush_shape,
    int brush_radius,
    uint32_t brush_color,
    const Canvas *composite,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *drawing,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y,
    uint32_t *shape_base_pixels,
    int *needs_composite
);

AppCanvasClickResult app_handle_right_canvas_press(
    int *shaping,
    int *preview_active,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_canvas_active,
    int x,
    int y,
    Tool *tool,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity
);

AppCanvasClickResult app_handle_left_canvas_release(
    int *drawing,
    LayerStack *layers,
    int *shaping,
    int *preview_active,
    int shape_start_x,
    int shape_start_y,
    int x,
    int y,
    int shift,
    Tool tool,
    int brush_radius,
    uint32_t brush_color,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
);

#endif
