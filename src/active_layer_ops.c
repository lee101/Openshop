#include "active_layer_ops.h"

#include "brush_render.h"
#include "layer_edit_state.h"
#include "shape_draw.h"

#include <stddef.h>

static int canvas_has_non_matching_pixel(const Canvas *canvas, uint32_t color) {
    size_t pixel_count;
    size_t i;

    if (!canvas || !canvas->pixels) {
        return 0;
    }

    pixel_count = (size_t)canvas->width * (size_t)canvas->height;
    for (i = 0; i < pixel_count; i++) {
        if (canvas->pixels[i] != color) {
            return 1;
        }
    }
    return 0;
}

static int active_layer_apply_transform(LayerStack *layers,
                                        Snapshot *undo_stack, int *undo_count,
                                        Snapshot *redo_stack, int *redo_count,
                                        int max_history,
                                        void (*transform)(Canvas *)) {
    Layer *active;

    if (!layers || !transform || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return 0;
    }
    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }
    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    transform(&active->canvas);
    return 1;
}

int active_layer_try_clear(LayerStack *layers,
                           Snapshot *undo_stack, int *undo_count,
                           Snapshot *redo_stack, int *redo_count,
                           uint32_t background_color, int max_history) {
    Layer *active;
    uint32_t clear_color;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return 0;
    }
    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }
    clear_color = active_layer_clear_color(layers, background_color);
    if (!canvas_has_non_matching_pixel(&active->canvas, clear_color)) {
        return 0;
    }
    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    return layer_stack_clear_layer(layers, layers->active_layer, clear_color);
}

int active_layer_try_flip_horizontal(LayerStack *layers,
                                     Snapshot *undo_stack, int *undo_count,
                                     Snapshot *redo_stack, int *redo_count,
                                     int max_history) {
    return active_layer_apply_transform(layers, undo_stack, undo_count, redo_stack, redo_count,
                                        max_history, canvas_flip_horizontal);
}

int active_layer_try_flip_vertical(LayerStack *layers,
                                   Snapshot *undo_stack, int *undo_count,
                                   Snapshot *redo_stack, int *redo_count,
                                   int max_history) {
    return active_layer_apply_transform(layers, undo_stack, undo_count, redo_stack, redo_count,
                                        max_history, canvas_flip_vertical);
}

int active_layer_try_rotate_180(LayerStack *layers,
                                Snapshot *undo_stack, int *undo_count,
                                Snapshot *redo_stack, int *redo_count,
                                int max_history) {
    return active_layer_apply_transform(layers, undo_stack, undo_count, redo_stack, redo_count,
                                        max_history, canvas_rotate_180);
}

int active_layer_try_invert_rgb(LayerStack *layers,
                                Snapshot *undo_stack, int *undo_count,
                                Snapshot *redo_stack, int *redo_count,
                                int max_history) {
    return active_layer_apply_transform(layers, undo_stack, undo_count, redo_stack, redo_count,
                                        max_history, canvas_invert_rgb);
}

int active_layer_try_adjust_opacity(LayerStack *layers,
                                    Snapshot *undo_stack, int *undo_count,
                                    Snapshot *redo_stack, int *redo_count,
                                    int target_opacity, int max_history) {
    Layer *active = layer_stack_active(layers);

    if (target_opacity < 0) {
        target_opacity = 0;
    } else if (target_opacity > 100) {
        target_opacity = 100;
    }

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0 ||
        !active || active->opacity_percent == target_opacity) {
        return 0;
    }

    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    return layer_stack_set_opacity(layers, layers->active_layer, target_opacity);
}

int active_layer_try_nudge_opacity(LayerStack *layers,
                                   Snapshot *undo_stack, int *undo_count,
                                   Snapshot *redo_stack, int *redo_count,
                                   int delta_percent, int max_history) {
    Layer *active;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!active) {
        return 0;
    }

    return active_layer_try_adjust_opacity(layers, undo_stack, undo_count, redo_stack, redo_count,
                                           active->opacity_percent + delta_percent, max_history);
}

int active_layer_try_flood_fill(LayerStack *layers,
                                Snapshot *undo_stack, int *undo_count,
                                Snapshot *redo_stack, int *redo_count,
                                int x, int y, uint32_t brush_color, int max_history) {
    Layer *active;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= active->canvas.width || y >= active->canvas.height) {
        return 0;
    }
    if (canvas_get_pixel(&active->canvas, x, y) == brush_color) {
        return 0;
    }

    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    return canvas_flood_fill(&active->canvas, x, y, brush_color);
}

int active_layer_try_commit_shape(LayerStack *layers,
                                  Snapshot *undo_stack, int *undo_count,
                                  Snapshot *redo_stack, int *redo_count,
                                  Tool tool, int shape_start_x, int shape_start_y,
                                  int end_x, int end_y, int brush_radius,
                                  uint32_t brush_color, int max_history) {
    Layer *active;
    int is_shape_tool;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }
    is_shape_tool = tool == TOOL_LINE || tool == TOOL_RECT || tool == TOOL_FILLED_RECT ||
                    tool == TOOL_ELLIPSE || tool == TOOL_FILLED_ELLIPSE;
    if (!is_shape_tool) {
        return 0;
    }
    if ((tool == TOOL_ELLIPSE || tool == TOOL_FILLED_ELLIPSE) &&
        (shape_start_x == end_x || shape_start_y == end_y)) {
        return 0;
    }
    if (brush_radius <= 0 &&
        (tool == TOOL_LINE || tool == TOOL_RECT || tool == TOOL_ELLIPSE)) {
        return 0;
    }

    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    draw_shape(&active->canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
    return 1;
}

int active_layer_try_begin_brush_stroke(LayerStack *layers,
                                        Snapshot *undo_stack, int *undo_count,
                                        Snapshot *redo_stack, int *redo_count,
                                        Tool tool, int x, int y, int brush_radius,
                                        uint32_t brush_color, BrushShape brush_shape,
                                        uint32_t background_color, int max_history) {
    Layer *active;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }
    if (tool != TOOL_BRUSH && tool != TOOL_ERASER) {
        return 0;
    }
    if (brush_radius <= 0) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= active->canvas.width || y >= active->canvas.height) {
        return 0;
    }

    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    if (tool == TOOL_ERASER) {
        erase_stamp(&active->canvas, x, y, brush_radius, active_layer_clear_color(layers, background_color), brush_shape);
    } else {
        stamp_brush(&active->canvas, x, y, brush_radius, brush_color, brush_shape);
    }
    return 1;
}

int active_layer_continue_brush_stroke(LayerStack *layers,
                                       Tool tool, int x, int y,
                                       int brush_radius, uint32_t brush_color,
                                       BrushShape brush_shape,
                                       int *last_x, int *last_y,
                                       uint32_t background_color) {
    Layer *active;

    if (!layers || !last_x || !last_y) {
        return 0;
    }
    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= active->canvas.width || y >= active->canvas.height) {
        return 0;
    }

    if (tool == TOOL_ERASER) {
        erase_line(&active->canvas, *last_x, *last_y, x, y, brush_radius,
                   active_layer_clear_color(layers, background_color), brush_shape);
    } else {
        draw_brush_line(&active->canvas, *last_x, *last_y, x, y, brush_radius, brush_color, brush_shape);
    }
    *last_x = x;
    *last_y = y;
    return 1;
}

int active_layer_apply_translation(LayerStack *layers,
                                   Snapshot *undo_stack, int *undo_count,
                                   Snapshot *redo_stack, int *redo_count,
                                   int dx, int dy,
                                   uint32_t background_color, int max_history) {
    Layer *active;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0 ||
        (dx == 0 && dy == 0)) {
        return 0;
    }
    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }
    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    canvas_translate(&active->canvas, dx, dy, active_layer_clear_color(layers, background_color));
    return 1;
}
