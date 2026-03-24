#include "app_canvas_click.h"

#include "app_preview.h"

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
) {
    if (!layers || !last_x || !last_y || !shaping || !shape_start_x || !shape_start_y) {
        return APP_CANVAS_CLICK_NOOP;
    }

    *last_x = x;
    *last_y = y;
    if (app_tool_draws_directly(tool)) {
        return app_begin_direct_stroke(
            layers,
            x,
            y,
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            undo_stack,
            undo_count,
            undo_capacity,
            redo_stack,
            redo_count,
            drawing,
            needs_composite
        ) ? APP_CANVAS_CLICK_DIRECT_STROKE : APP_CANVAS_CLICK_NOOP;
    }

    return app_begin_shape_preview_to_active_layer(
        layers,
        x,
        y,
        shaping,
        shape_start_x,
        shape_start_y,
        shape_base_pixels,
        composite
    ) ? APP_CANVAS_CLICK_SHAPE_PREVIEW : APP_CANVAS_CLICK_NOOP;
}
