#include "active_layer_ops.h"

#include "brush_render.h"
#include "geometry_helpers.h"
#include "layer_edit_state.h"
#include "shape_draw.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

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

static int canvas_is_uniform(const Canvas *canvas) {
    if (!canvas || !canvas->pixels || canvas->width <= 0 || canvas->height <= 0) {
        return 0;
    }

    return !canvas_has_non_matching_pixel(canvas, canvas->pixels[0]);
}

static int canvas_has_visible_pixel(const Canvas *canvas) {
    size_t pixel_count;
    size_t i;

    if (!canvas || !canvas->pixels) {
        return 0;
    }

    pixel_count = (size_t)canvas->width * (size_t)canvas->height;
    for (i = 0; i < pixel_count; i++) {
        if ((canvas->pixels[i] & 0xFF000000U) != 0) {
            return 1;
        }
    }

    return 0;
}

static int canvas_stamp_would_change(const Canvas *canvas,
                                     int cx, int cy, int radius,
                                     uint32_t color, BrushShape shape) {
    int dx;
    int dy;

    if (!canvas || !canvas->pixels || radius <= 0) {
        return 0;
    }

    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int px;
            int py;

            if (!brush_mask_contains(shape, dx, dy, radius)) {
                continue;
            }

            px = cx + dx;
            py = cy + dy;
            if (px < 0 || py < 0 || px >= canvas->width || py >= canvas->height) {
                continue;
            }
            if (canvas_get_pixel(canvas, px, py) != color) {
                return 1;
            }
        }
    }

    return 0;
}

static int canvas_line_would_change(const Canvas *canvas,
                                    int x0, int y0, int x1, int y1,
                                    int radius, uint32_t color, BrushShape shape) {
    int dx;
    int sx;
    int dy;
    int sy;
    int err;

    if (!canvas || !canvas->pixels || radius <= 0) {
        return 0;
    }

    dx = x1 > x0 ? x1 - x0 : x0 - x1;
    sx = x0 < x1 ? 1 : -1;
    dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;

    while (1) {
        if (canvas_stamp_would_change(canvas, x0, y0, radius, color, shape)) {
            return 1;
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        {
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

    return 0;
}

static int canvas_filled_rect_would_change(const Canvas *canvas,
                                           int x0, int y0, int x1, int y1,
                                           uint32_t color) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int x;
    int y;

    if (!canvas || !canvas->pixels) {
        return 0;
    }

    min_x = x0 < x1 ? x0 : x1;
    max_x = x0 > x1 ? x0 : x1;
    min_y = y0 < y1 ? y0 : y1;
    max_y = y0 > y1 ? y0 : y1;

    for (y = min_y; y <= max_y; y++) {
        if (y < 0 || y >= canvas->height) {
            continue;
        }
        for (x = min_x; x <= max_x; x++) {
            if (x < 0 || x >= canvas->width) {
                continue;
            }
            if (canvas_get_pixel(canvas, x, y) != color) {
                return 1;
            }
        }
    }

    return 0;
}

static int canvas_filled_ellipse_would_change(const Canvas *canvas,
                                              int x0, int y0, int x1, int y1,
                                              uint32_t color) {
    int cx;
    int cy;
    int rx;
    int ry;
    int y;

    if (!canvas || !canvas->pixels) {
        return 0;
    }

    cx = (x0 + x1) / 2;
    cy = (y0 + y1) / 2;
    rx = abs(x1 - x0) / 2;
    ry = abs(y1 - y0) / 2;
    if (rx <= 0 || ry <= 0) {
        return 0;
    }

    for (y = -ry; y <= ry; y++) {
        double norm = 1.0 - ((double)(y * y) / (double)(ry * ry));
        int x;
        int fill_x;

        if (norm < 0.0) {
            continue;
        }
        x = (int)((double)rx * sqrt(norm) + 0.5);
        for (fill_x = -x; fill_x <= x; fill_x++) {
            int px = cx + fill_x;
            int py = cy + y;

            if (px < 0 || py < 0 || px >= canvas->width || py >= canvas->height) {
                continue;
            }
            if (canvas_get_pixel(canvas, px, py) != color) {
                return 1;
            }
        }
    }

    return 0;
}

static int canvas_ellipse_outline_would_change(const Canvas *canvas,
                                               int x0, int y0, int x1, int y1,
                                               int radius, uint32_t color, BrushShape shape) {
    int cx;
    int cy;
    int rx;
    int ry;
    int y;

    if (!canvas || !canvas->pixels || radius <= 0) {
        return 0;
    }

    cx = (x0 + x1) / 2;
    cy = (y0 + y1) / 2;
    rx = abs(x1 - x0) / 2;
    ry = abs(y1 - y0) / 2;
    if (rx <= 0 || ry <= 0) {
        return 0;
    }

    for (y = -ry; y <= ry; y++) {
        double norm = 1.0 - ((double)(y * y) / (double)(ry * ry));
        int x;

        if (norm < 0.0) {
            continue;
        }
        x = (int)((double)rx * sqrt(norm) + 0.5);
        if (canvas_stamp_would_change(canvas, cx + x, cy + y, radius, color, shape) ||
            canvas_stamp_would_change(canvas, cx - x, cy + y, radius, color, shape)) {
            return 1;
        }
    }

    return 0;
}

static ActiveLayerActionResult active_layer_apply_transform_with_result(LayerStack *layers,
                                                                       Snapshot *undo_stack, int *undo_count,
                                                                       Snapshot *redo_stack, int *redo_count,
                                                                       int max_history,
                                                                       void (*transform)(Canvas *)) {
    Layer *active;

    if (!layers || !transform || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    if ((transform == canvas_flip_horizontal && active->canvas.width <= 1) ||
        (transform == canvas_flip_vertical && active->canvas.height <= 1) ||
        (transform == canvas_rotate_180 &&
         active->canvas.width * active->canvas.height <= 1)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if ((transform == canvas_flip_horizontal || transform == canvas_flip_vertical ||
         transform == canvas_rotate_180) &&
        canvas_is_uniform(&active->canvas)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (transform == canvas_invert_rgb && !canvas_has_visible_pixel(&active->canvas)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    transform(&active->canvas);
    return ACTIVE_LAYER_ACTION_CHANGED;
}

ActiveLayerActionResult active_layer_try_clear_with_result(LayerStack *layers,
                                                           Snapshot *undo_stack, int *undo_count,
                                                           Snapshot *redo_stack, int *redo_count,
                                                           uint32_t background_color, int max_history) {
    Layer *active;
    uint32_t clear_color;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    clear_color = active_layer_clear_color(layers, background_color);
    if (!canvas_has_non_matching_pixel(&active->canvas, clear_color)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    if (!layer_stack_clear_layer(layers, layers->active_layer, clear_color)) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    return ACTIVE_LAYER_ACTION_CHANGED;
}

int active_layer_try_clear(LayerStack *layers,
                           Snapshot *undo_stack, int *undo_count,
                           Snapshot *redo_stack, int *redo_count,
                           uint32_t background_color, int max_history) {
    return active_layer_try_clear_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                              background_color, max_history) == ACTIVE_LAYER_ACTION_CHANGED;
}

ActiveLayerActionResult active_layer_try_flip_horizontal_with_result(LayerStack *layers,
                                                                     Snapshot *undo_stack, int *undo_count,
                                                                     Snapshot *redo_stack, int *redo_count,
                                                                     int max_history) {
    return active_layer_apply_transform_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                    max_history, canvas_flip_horizontal);
}

int active_layer_try_flip_horizontal(LayerStack *layers,
                                     Snapshot *undo_stack, int *undo_count,
                                     Snapshot *redo_stack, int *redo_count,
                                     int max_history) {
    return active_layer_try_flip_horizontal_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                        max_history) == ACTIVE_LAYER_ACTION_CHANGED;
}

ActiveLayerActionResult active_layer_try_flip_vertical_with_result(LayerStack *layers,
                                                                   Snapshot *undo_stack, int *undo_count,
                                                                   Snapshot *redo_stack, int *redo_count,
                                                                   int max_history) {
    return active_layer_apply_transform_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                    max_history, canvas_flip_vertical);
}

int active_layer_try_flip_vertical(LayerStack *layers,
                                   Snapshot *undo_stack, int *undo_count,
                                   Snapshot *redo_stack, int *redo_count,
                                   int max_history) {
    return active_layer_try_flip_vertical_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                      max_history) == ACTIVE_LAYER_ACTION_CHANGED;
}

ActiveLayerActionResult active_layer_try_rotate_180_with_result(LayerStack *layers,
                                                                Snapshot *undo_stack, int *undo_count,
                                                                Snapshot *redo_stack, int *redo_count,
                                                                int max_history) {
    return active_layer_apply_transform_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                    max_history, canvas_rotate_180);
}

int active_layer_try_rotate_180(LayerStack *layers,
                                Snapshot *undo_stack, int *undo_count,
                                Snapshot *redo_stack, int *redo_count,
                                int max_history) {
    return active_layer_try_rotate_180_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                   max_history) == ACTIVE_LAYER_ACTION_CHANGED;
}

ActiveLayerActionResult active_layer_try_invert_rgb_with_result(LayerStack *layers,
                                                                Snapshot *undo_stack, int *undo_count,
                                                                Snapshot *redo_stack, int *redo_count,
                                                                int max_history) {
    return active_layer_apply_transform_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                    max_history, canvas_invert_rgb);
}

int active_layer_try_invert_rgb(LayerStack *layers,
                                Snapshot *undo_stack, int *undo_count,
                                Snapshot *redo_stack, int *redo_count,
                                int max_history) {
    return active_layer_try_invert_rgb_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                   max_history) == ACTIVE_LAYER_ACTION_CHANGED;
}

ActiveLayerActionResult active_layer_try_adjust_opacity_with_result(LayerStack *layers,
                                                                    Snapshot *undo_stack, int *undo_count,
                                                                    Snapshot *redo_stack, int *redo_count,
                                                                    int target_opacity, int max_history) {
    Layer *active;

    if (target_opacity < 0) {
        target_opacity = 0;
    } else if (target_opacity > 100) {
        target_opacity = 100;
    }

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }

    active = layer_stack_active(layers);
    if (!active) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    if (active->opacity_percent == target_opacity) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }

    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    if (!layer_stack_set_opacity(layers, layers->active_layer, target_opacity)) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    return ACTIVE_LAYER_ACTION_CHANGED;
}

int active_layer_try_adjust_opacity(LayerStack *layers,
                                    Snapshot *undo_stack, int *undo_count,
                                    Snapshot *redo_stack, int *redo_count,
                                    int target_opacity, int max_history) {
    return active_layer_try_adjust_opacity_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                       target_opacity, max_history) == ACTIVE_LAYER_ACTION_CHANGED;
}

ActiveLayerActionResult active_layer_try_nudge_opacity_with_result(LayerStack *layers,
                                                                   Snapshot *undo_stack, int *undo_count,
                                                                   Snapshot *redo_stack, int *redo_count,
                                                                   int delta_percent, int max_history) {
    Layer *active;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }

    active = layer_stack_active(layers);
    if (!active) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }

    return active_layer_try_adjust_opacity_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                       active->opacity_percent + delta_percent, max_history);
}

int active_layer_try_nudge_opacity(LayerStack *layers,
                                   Snapshot *undo_stack, int *undo_count,
                                   Snapshot *redo_stack, int *redo_count,
                                   int delta_percent, int max_history) {
    return active_layer_try_nudge_opacity_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                      delta_percent, max_history) == ACTIVE_LAYER_ACTION_CHANGED;
}

int active_layer_try_flood_fill_with_result(LayerStack *layers,
                                            Snapshot *undo_stack, int *undo_count,
                                            Snapshot *redo_stack, int *redo_count,
                                            int x, int y, uint32_t brush_color, int max_history,
                                            int *changed) {
    Layer *active;
    uint8_t brush_alpha;

    if (changed) {
        *changed = 0;
    }
    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }
    brush_alpha = (uint8_t)((brush_color >> 24) & 0xFF);
    if (brush_alpha == 0) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= active->canvas.width || y >= active->canvas.height) {
        return 0;
    }
    if (canvas_get_pixel(&active->canvas, x, y) == brush_color) {
        return 1;
    }

    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    if (!canvas_flood_fill(&active->canvas, x, y, brush_color)) {
        return 0;
    }
    if (changed) {
        *changed = 1;
    }
    return 1;
}

int active_layer_try_flood_fill(LayerStack *layers,
                                Snapshot *undo_stack, int *undo_count,
                                Snapshot *redo_stack, int *redo_count,
                                int x, int y, uint32_t brush_color, int max_history) {
    int changed = 0;

    if (!active_layer_try_flood_fill_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                 x, y, brush_color, max_history, &changed)) {
        return 0;
    }
    return changed;
}

ActiveLayerActionResult active_layer_try_commit_shape_with_result(LayerStack *layers,
                                                                  Snapshot *undo_stack, int *undo_count,
                                                                  Snapshot *redo_stack, int *redo_count,
                                                                  Tool tool, int shape_start_x, int shape_start_y,
                                                                  int end_x, int end_y, int brush_radius,
                                                                  uint32_t brush_color, int max_history) {
    Layer *active;
    int is_shape_tool;
    uint8_t brush_alpha;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }

    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    is_shape_tool = tool == TOOL_LINE || tool == TOOL_RECT || tool == TOOL_FILLED_RECT ||
                    tool == TOOL_ELLIPSE || tool == TOOL_FILLED_ELLIPSE;
    if (!is_shape_tool) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    brush_alpha = (uint8_t)((brush_color >> 24) & 0xFF);
    if (brush_alpha == 0) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if ((tool == TOOL_ELLIPSE || tool == TOOL_FILLED_ELLIPSE) &&
        (shape_start_x == end_x || shape_start_y == end_y)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (tool == TOOL_FILLED_RECT &&
        !canvas_filled_rect_would_change(&active->canvas, shape_start_x, shape_start_y, end_x, end_y, brush_color)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (tool == TOOL_FILLED_ELLIPSE &&
        !canvas_filled_ellipse_would_change(&active->canvas, shape_start_x, shape_start_y, end_x, end_y, brush_color)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (tool == TOOL_LINE &&
        !canvas_line_would_change(&active->canvas, shape_start_x, shape_start_y, end_x, end_y,
                                  brush_radius, brush_color, BRUSH_SHAPE_ROUND)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (tool == TOOL_RECT &&
        !canvas_line_would_change(&active->canvas, shape_start_x, shape_start_y, end_x, shape_start_y,
                                  brush_radius, brush_color, BRUSH_SHAPE_ROUND) &&
        !canvas_line_would_change(&active->canvas, end_x, shape_start_y, end_x, end_y,
                                  brush_radius, brush_color, BRUSH_SHAPE_ROUND) &&
        !canvas_line_would_change(&active->canvas, end_x, end_y, shape_start_x, end_y,
                                  brush_radius, brush_color, BRUSH_SHAPE_ROUND) &&
        !canvas_line_would_change(&active->canvas, shape_start_x, end_y, shape_start_x, shape_start_y,
                                  brush_radius, brush_color, BRUSH_SHAPE_ROUND)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (tool == TOOL_ELLIPSE &&
        !canvas_ellipse_outline_would_change(&active->canvas, shape_start_x, shape_start_y, end_x, end_y,
                                             brush_radius, brush_color, BRUSH_SHAPE_ROUND)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (brush_radius <= 0 &&
        (tool == TOOL_LINE || tool == TOOL_RECT || tool == TOOL_ELLIPSE)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }

    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    draw_shape(&active->canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
    return ACTIVE_LAYER_ACTION_CHANGED;
}

int active_layer_try_commit_shape(LayerStack *layers,
                                  Snapshot *undo_stack, int *undo_count,
                                  Snapshot *redo_stack, int *redo_count,
                                  Tool tool, int shape_start_x, int shape_start_y,
                                  int end_x, int end_y, int brush_radius,
                                  uint32_t brush_color, int max_history) {
    return active_layer_try_commit_shape_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                     tool, shape_start_x, shape_start_y, end_x, end_y,
                                                     brush_radius, brush_color, max_history) == ACTIVE_LAYER_ACTION_CHANGED;
}

ActiveLayerActionResult active_layer_try_begin_brush_stroke_with_result(LayerStack *layers,
                                                                        Snapshot *undo_stack, int *undo_count,
                                                                        Snapshot *redo_stack, int *redo_count,
                                                                        Tool tool, int x, int y, int brush_radius,
                                                                        uint32_t brush_color, BrushShape brush_shape,
                                                                        uint32_t background_color, int max_history) {
    Layer *active;
    uint8_t brush_alpha;
    uint32_t clear_color;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }

    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    if (tool != TOOL_BRUSH && tool != TOOL_ERASER) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    brush_alpha = (uint8_t)((brush_color >> 24) & 0xFF);
    if (tool == TOOL_BRUSH && brush_alpha == 0) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (brush_radius <= 0) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (x < 0 || y < 0 || x >= active->canvas.width || y >= active->canvas.height) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    clear_color = active_layer_clear_color(layers, background_color);
    if (tool == TOOL_ERASER &&
        !canvas_stamp_would_change(&active->canvas, x, y, brush_radius, clear_color, brush_shape)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (tool == TOOL_BRUSH &&
        !canvas_stamp_would_change(&active->canvas, x, y, brush_radius, brush_color, brush_shape)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }

    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    if (tool == TOOL_ERASER) {
        erase_stamp(&active->canvas, x, y, brush_radius, clear_color, brush_shape);
    } else {
        stamp_brush(&active->canvas, x, y, brush_radius, brush_color, brush_shape);
    }
    return ACTIVE_LAYER_ACTION_CHANGED;
}

int active_layer_try_begin_brush_stroke(LayerStack *layers,
                                        Snapshot *undo_stack, int *undo_count,
                                        Snapshot *redo_stack, int *redo_count,
                                        Tool tool, int x, int y, int brush_radius,
                                        uint32_t brush_color, BrushShape brush_shape,
                                        uint32_t background_color, int max_history) {
    return active_layer_try_begin_brush_stroke_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                           tool, x, y, brush_radius, brush_color, brush_shape,
                                                           background_color, max_history) == ACTIVE_LAYER_ACTION_CHANGED;
}

ActiveLayerActionResult active_layer_continue_brush_stroke_with_result(LayerStack *layers,
                                                                       Tool tool, int x, int y,
                                                                       int brush_radius, uint32_t brush_color,
                                                                       BrushShape brush_shape,
                                                                       int *last_x, int *last_y,
                                                                       uint32_t background_color) {
    Layer *active;
    uint8_t brush_alpha;
    uint32_t clear_color;

    if (!layers || !last_x || !last_y) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    if (tool != TOOL_BRUSH && tool != TOOL_ERASER) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    brush_alpha = (uint8_t)((brush_color >> 24) & 0xFF);
    if (tool == TOOL_BRUSH && brush_alpha == 0) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (brush_radius <= 0) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (*last_x < 0 || *last_y < 0 || *last_x >= active->canvas.width || *last_y >= active->canvas.height) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    if (x < 0 || y < 0 || x >= active->canvas.width || y >= active->canvas.height) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    clear_color = active_layer_clear_color(layers, background_color);
    if (tool == TOOL_ERASER &&
        !canvas_line_would_change(&active->canvas, *last_x, *last_y, x, y, brush_radius,
                                  clear_color, brush_shape)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    if (tool == TOOL_BRUSH &&
        !canvas_line_would_change(&active->canvas, *last_x, *last_y, x, y, brush_radius,
                                  brush_color, brush_shape)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }

    if (tool == TOOL_ERASER) {
        erase_line(&active->canvas, *last_x, *last_y, x, y, brush_radius, clear_color, brush_shape);
    } else {
        draw_brush_line(&active->canvas, *last_x, *last_y, x, y, brush_radius, brush_color, brush_shape);
    }
    *last_x = x;
    *last_y = y;
    return ACTIVE_LAYER_ACTION_CHANGED;
}

int active_layer_continue_brush_stroke(LayerStack *layers,
                                       Tool tool, int x, int y,
                                       int brush_radius, uint32_t brush_color,
                                       BrushShape brush_shape,
                                       int *last_x, int *last_y,
                                       uint32_t background_color) {
    return active_layer_continue_brush_stroke_with_result(layers, tool, x, y, brush_radius, brush_color,
                                                          brush_shape, last_x, last_y,
                                                          background_color) == ACTIVE_LAYER_ACTION_CHANGED;
}

ActiveLayerActionResult active_layer_apply_translation_with_result(LayerStack *layers,
                                                                   Snapshot *undo_stack, int *undo_count,
                                                                   Snapshot *redo_stack, int *redo_count,
                                                                   int dx, int dy,
                                                                   uint32_t background_color, int max_history) {
    Layer *active;
    uint32_t clear_color;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    if (dx == 0 && dy == 0) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return ACTIVE_LAYER_ACTION_FAILED;
    }
    clear_color = active_layer_clear_color(layers, background_color);
    if (!canvas_has_non_matching_pixel(&active->canvas, clear_color)) {
        return ACTIVE_LAYER_ACTION_UNCHANGED;
    }
    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    canvas_translate(&active->canvas, dx, dy, clear_color);
    return ACTIVE_LAYER_ACTION_CHANGED;
}

int active_layer_apply_translation(LayerStack *layers,
                                   Snapshot *undo_stack, int *undo_count,
                                   Snapshot *redo_stack, int *redo_count,
                                   int dx, int dy,
                                   uint32_t background_color, int max_history) {
    return active_layer_apply_translation_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                      dx, dy, background_color, max_history) == ACTIVE_LAYER_ACTION_CHANGED;
}
