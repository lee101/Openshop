#include "app_canvas_click.h"

#include "app_preview.h"
#include "app_sampled_color.h"
#include "app_shape.h"

int app_canvas_click_result_refreshes_title(AppCanvasClickResult result) {
    return result == APP_CANVAS_CLICK_COLOR_SAMPLED;
}

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
) {
    if (drawing) {
        *drawing = 0;
    }

    return app_finalize_shape_preview(
        layers,
        shaping,
        preview_active,
        shape_start_x,
        shape_start_y,
        x,
        y,
        shift,
        tool,
        brush_radius,
        brush_color,
        undo_stack,
        undo_count,
        undo_capacity,
        redo_stack,
        redo_count,
        needs_composite
    ) ? APP_CANVAS_CLICK_SHAPE_FINALIZED : APP_CANVAS_CLICK_NOOP;
}

AppCanvasClickResult app_handle_canvas_motion(
    int x,
    int y,
    int *drawing,
    int *last_x,
    int *last_y,
    int *shaping,
    int shape_start_x,
    int shape_start_y,
    int shift,
    LayerStack *layers,
    Tool tool,
    BrushShape brush_shape,
    int brush_radius,
    uint32_t brush_color,
    uint32_t *shape_base_pixels,
    uint32_t *preview_pixels,
    Canvas *preview_canvas,
    int *preview_active,
    int *needs_composite,
    size_t pixel_count
) {
    int end_x = x;
    int end_y = y;

    if (!drawing || !last_x || !last_y || !shaping || !layers || !preview_active) {
        return APP_CANVAS_CLICK_NOOP;
    }

    if (*drawing) {
        return app_continue_direct_stroke(
            layers,
            last_x,
            last_y,
            x,
            y,
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            needs_composite
        ) ? APP_CANVAS_CLICK_DIRECT_STROKE : APP_CANVAS_CLICK_NOOP;
    }

    if (!*shaping) {
        return APP_CANVAS_CLICK_NOOP;
    }
    if (!app_prepare_shape_preview_motion(
        preview_canvas,
        preview_pixels,
        shape_base_pixels,
        pixel_count,
        preview_active,
        tool,
        shape_start_x,
        shape_start_y,
        x,
        y,
        shift,
        &end_x,
        &end_y
    )) {
        return APP_CANVAS_CLICK_NOOP;
    }
    app_draw_shape(preview_canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
    return APP_CANVAS_CLICK_SHAPE_PREVIEW;
}
