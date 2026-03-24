#include "app_canvas_click.h"

#include "app_preview.h"
#include "app_sampled_color.h"

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
) {
    switch (app_handle_available_canvas_sample(
        shaping,
        preview_active,
        composite,
        preview_canvas,
        preview_canvas_active,
        x,
        y,
        tool,
        brush_color,
        brush_color_rgb,
        brush_opacity
    )) {
    case APP_SAMPLE_BRUSH_COLOR_PREVIEW_CANCELED:
        return APP_CANVAS_CLICK_PREVIEW_CANCELED;
    case APP_SAMPLE_BRUSH_COLOR_APPLIED:
        return APP_CANVAS_CLICK_COLOR_SAMPLED;
    case APP_SAMPLE_BRUSH_COLOR_NOOP:
    default:
        return APP_CANVAS_CLICK_NOOP;
    }
}
