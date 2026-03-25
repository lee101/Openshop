#include "app_runtime_shortcuts.h"

#include "app_canvas_ops.h"
#include "app_color.h"
#include "app_layer_state.h"
#include "app_sampled_color.h"

#include <stdio.h>

static const uint32_t COLOR_BRUSH = 0xFF1B1F24;
static const uint32_t COLOR_ERASE = 0xFFFFFFFF;
static const uint32_t COLOR_RED = 0xFFE53935;
static const uint32_t COLOR_GREEN = 0xFF43A047;
static const uint32_t COLOR_BLUE = 0xFF1E88E5;
static const uint32_t COLOR_YELLOW = 0xFFFDD835;
static const uint32_t COLOR_PURPLE = 0xFF8E24AA;
static const uint32_t COLOR_BG = 0xFFFFFFFF;

int app_handle_history_navigation_shortcut(
    int key,
    int ctrl,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    HistoryShortcutAction action;

    if (!ctrl || !layers || !undo_stack || !undo_count || !redo_stack || !redo_count) {
        return 0;
    }

    action = history_shortcut_action(ctrl, key);

    if (action == HISTORY_SHORTCUT_UNDO) {
        if (snapshot_undo(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count)) {
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (action == HISTORY_SHORTCUT_REDO) {
        if (snapshot_redo(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count)) {
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    return 0;
}

int app_handle_canvas_mutation_shortcut(
    CanvasShortcutAction canvas_action,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    int changed = 0;

    if (!layers) {
        return 0;
    }

    if (canvas_action == CANVAS_SHORTCUT_CLEAR) {
        if (app_active_layer_editable(layers)) {
            snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
        }
        changed = layer_stack_clear_layer(
            layers,
            layers->active_layer,
            app_active_layer_clear_color(layers->active_layer)
        );
    } else if (canvas_action == CANVAS_SHORTCUT_FLIP_HORIZONTAL) {
        changed = app_apply_canvas_transform(
            layers,
            undo_stack,
            undo_count,
            undo_capacity,
            redo_stack,
            redo_count,
            canvas_flip_horizontal
        );
    } else if (canvas_action == CANVAS_SHORTCUT_FLIP_VERTICAL) {
        changed = app_apply_canvas_transform(
            layers,
            undo_stack,
            undo_count,
            undo_capacity,
            redo_stack,
            redo_count,
            canvas_flip_vertical
        );
    } else if (canvas_action == CANVAS_SHORTCUT_ROTATE_180) {
        changed = app_apply_canvas_transform(
            layers,
            undo_stack,
            undo_count,
            undo_capacity,
            redo_stack,
            redo_count,
            canvas_rotate_180
        );
    } else if (canvas_action == CANVAS_SHORTCUT_INVERT_RGB) {
        changed = app_apply_canvas_transform(
            layers,
            undo_stack,
            undo_count,
            undo_capacity,
            redo_stack,
            redo_count,
            canvas_invert_rgb
        );
    } else {
        return 0;
    }

    if (changed && needs_composite) {
        *needs_composite = 1;
    }
    return 1;
}

int app_handle_brush_and_paint_shortcut(
    PaintShortcutAction paint_action,
    BrushShortcutAction brush_action,
    Tool *tool,
    BrushShape *brush_shape,
    int *brush_radius,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity
) {
    if (!tool || !brush_shape || !brush_radius || !brush_color || !brush_color_rgb || !brush_opacity) {
        return 0;
    }

    if (paint_action == PAINT_SHORTCUT_TOOL_BRUSH) {
        *brush_color_rgb = COLOR_BRUSH & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (paint_action == PAINT_SHORTCUT_TOOL_ERASER) {
        *brush_color_rgb = COLOR_ERASE & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_ERASER;
    } else if (paint_action == PAINT_SHORTCUT_TOOL_LINE) {
        *tool = TOOL_LINE;
    } else if (paint_action == PAINT_SHORTCUT_TOOL_RECT) {
        *tool = TOOL_RECT;
    } else if (paint_action == PAINT_SHORTCUT_TOOL_FILLED_RECT) {
        *tool = TOOL_FILLED_RECT;
    } else if (paint_action == PAINT_SHORTCUT_TOOL_ELLIPSE) {
        *tool = TOOL_ELLIPSE;
    } else if (paint_action == PAINT_SHORTCUT_TOOL_FILLED_ELLIPSE) {
        *tool = TOOL_FILLED_ELLIPSE;
    } else if (brush_action == BRUSH_SHORTCUT_RADIUS_DOWN) {
        if (*brush_radius > 1) {
            *brush_radius -= 1;
        }
    } else if (brush_action == BRUSH_SHORTCUT_RADIUS_UP) {
        if (*brush_radius < 64) {
            *brush_radius += 1;
        }
    } else if (brush_action == BRUSH_SHORTCUT_SHAPE_PREV) {
        *brush_shape = app_cycle_brush_shape(*brush_shape, -1);
    } else if (brush_action == BRUSH_SHORTCUT_SHAPE_NEXT) {
        *brush_shape = app_cycle_brush_shape(*brush_shape, 1);
    } else if (brush_action == BRUSH_SHORTCUT_OPACITY_DOWN) {
        if (*brush_opacity > 1) {
            *brush_opacity -= 5;
            if (*brush_opacity < 1) {
                *brush_opacity = 1;
            }
            *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        }
    } else if (brush_action == BRUSH_SHORTCUT_OPACITY_UP) {
        if (*brush_opacity < 100) {
            *brush_opacity += 5;
            if (*brush_opacity > 100) {
                *brush_opacity = 100;
            }
            *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        }
    } else if (paint_action == PAINT_SHORTCUT_COLOR_BRUSH) {
        *brush_color_rgb = COLOR_BRUSH & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (paint_action == PAINT_SHORTCUT_COLOR_RED) {
        *brush_color_rgb = COLOR_RED & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (paint_action == PAINT_SHORTCUT_COLOR_GREEN) {
        *brush_color_rgb = COLOR_GREEN & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (paint_action == PAINT_SHORTCUT_COLOR_BLUE) {
        *brush_color_rgb = COLOR_BLUE & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (paint_action == PAINT_SHORTCUT_COLOR_YELLOW) {
        *brush_color_rgb = COLOR_YELLOW & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (paint_action == PAINT_SHORTCUT_COLOR_PURPLE) {
        *brush_color_rgb = COLOR_PURPLE & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else {
        return 0;
    }

    return 1;
}

int app_handle_layer_opacity_reset_shortcut(
    int key,
    int ctrl,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    Layer *active = NULL;

    if (!layers || !ctrl || key != '0') {
        return 0;
    }

    active = layer_stack_active(layers);
    if (active && active->opacity_percent != 100) {
        snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
        layer_stack_set_opacity(layers, layers->active_layer, 100);
        if (needs_composite) {
            *needs_composite = 1;
        }
    }
    return 1;
}

int app_handle_layer_visibility_shortcut(
    int key,
    int ctrl,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    const Layer *active = NULL;

    if (!layers || !ctrl) {
        return 0;
    }

    if (key == 'a') {
        if (layers->solo_index >= 0 || layer_stack_visible_count(layers) != layers->layer_count) {
            snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
            if (layer_stack_show_all(layers) && needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (shift && key == 'r') {
        active = layer_stack_get(layers, layers->active_layer);
        if (active && !active->visible) {
            snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
            if (layer_stack_show(layers, layers->active_layer) && needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    return 0;
}

int app_handle_active_layer_state_shortcut(
    int key,
    int ctrl,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    const Layer *active = NULL;

    if (!layers || !ctrl) {
        return 0;
    }

    if (shift && key == 'l') {
        snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
        if (!layer_stack_toggle_lock(layers, layers->active_layer)) {
            fprintf(stderr, "Could not toggle layer lock\n");
        }
        return 1;
    }

    active = layer_stack_get(layers, layers->active_layer);
    if (shift && key == 'v') {
        if (active && (!active->visible || layer_stack_visible_count(layers) > 1)) {
            snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
            if (!layer_stack_toggle_visibility(layers, layers->active_layer)) {
                fprintf(stderr, "Cannot hide the final visible layer\n");
            } else if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (shift && key == 'h') {
        if (active && active->visible && layer_stack_visible_count(layers) > 1) {
            snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
            if (!layer_stack_hide_and_advance(layers, layers->active_layer)) {
                fprintf(stderr, "Cannot hide the final visible layer\n");
            } else if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (key == '/') {
        snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
        if (!layer_stack_toggle_solo(layers, layers->active_layer)) {
            fprintf(stderr, "Could not toggle solo mode\n");
        } else if (needs_composite) {
            *needs_composite = 1;
        }
        return 1;
    }

    return 0;
}

int app_handle_active_layer_opacity_step(
    LayerStack *layers,
    int delta,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    Layer *active = NULL;
    int next_opacity = 0;

    if (!layers || delta == 0) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!active) {
        return 1;
    }

    next_opacity = active->opacity_percent + delta;
    if (next_opacity < 0) {
        next_opacity = 0;
    } else if (next_opacity > 100) {
        next_opacity = 100;
    }

    if (next_opacity != active->opacity_percent) {
        snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
        if (layer_stack_set_opacity(layers, layers->active_layer, next_opacity) && needs_composite) {
            *needs_composite = 1;
        }
    }

    return 1;
}

int app_handle_active_layer_reorder_shortcut(
    int key,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    if (!layers) {
        return 0;
    }

    if (shift && key == '[') {
        if (layers->active_layer > 0) {
            snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
            if (layer_stack_move_to_edge(layers, layers->active_layer, -1) && needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (shift && key == ']') {
        if (layers->active_layer + 1 < layers->layer_count) {
            snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
            if (layer_stack_move_to_edge(layers, layers->active_layer, 1) && needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (key == '[') {
        if (layers->active_layer > 0) {
            snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
            if (layer_stack_move(layers, layers->active_layer, -1) && needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (key == ']') {
        if (layers->active_layer + 1 < layers->layer_count) {
            snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
            if (layer_stack_move(layers, layers->active_layer, 1) && needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    return 0;
}

int app_handle_active_layer_duplicate_shortcut(
    int key,
    int ctrl,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    if (!layers || !ctrl || key != 'd') {
        return 0;
    }

    if (!layer_stack_can_duplicate(layers, layers->active_layer)) {
        return 1;
    }

    snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
    layer_stack_duplicate(layers, layers->active_layer, NULL);
    if (needs_composite) {
        *needs_composite = 1;
    }
    return 1;
}

int app_handle_active_layer_insert_shortcut(
    int key,
    int ctrl,
    int active_offset,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    int insert_at = 0;

    if (!layers || !ctrl) {
        return 0;
    }
    if (key != 'n' && key != ',') {
        return 0;
    }
    if (!layer_stack_can_insert(layers)) {
        return 1;
    }

    insert_at = layers->active_layer + active_offset;
    snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
    layer_stack_insert(layers, insert_at, NULL, 0x00000000);
    if (needs_composite) {
        *needs_composite = 1;
    }
    return 1;
}

int app_handle_active_layer_delete_shortcut(
    int key,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    if (!layers) {
        return 0;
    }
    if (key != 127 && key != '\b') {
        return 0;
    }
    if (!layer_stack_can_delete(layers, layers->active_layer)) {
        return 1;
    }

    snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
    layer_stack_delete(layers, layers->active_layer);
    if (needs_composite) {
        *needs_composite = 1;
    }
    return 1;
}

int app_handle_active_layer_add_shortcut(
    int key,
    int ctrl,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    if (!layers || !ctrl || !shift || key != 'n') {
        return 0;
    }
    if (!layer_stack_can_insert(layers)) {
        fprintf(stderr, "Max layers reached (%d)\n", MAX_LAYERS);
        return 1;
    }

    snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
    layer_stack_add(layers, NULL, 0x00000000);
    if (needs_composite) {
        *needs_composite = 1;
    }
    return 1;
}

int app_handle_active_layer_composite_shortcut(
    int key,
    int ctrl,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    if (!layers || !ctrl || !shift) {
        return 0;
    }

    if (key == 'm') {
        if (!layer_stack_can_flatten(layers)) {
            fprintf(stderr, "Flatten failed (check for locked layers)\n");
        } else {
            snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
            layer_stack_flatten(layers, COLOR_BG);
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (key == 'e') {
        if (layer_stack_stamp_visible_would_change(layers, layers->active_layer, COLOR_BG)) {
            snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
            if (!layer_stack_stamp_visible_into(layers, layers->active_layer, COLOR_BG)) {
                fprintf(stderr, "Stamp visible failed (active layer may be locked)\n");
            } else if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (key == 'g') {
        if (!layer_stack_can_insert(layers)) {
            fprintf(stderr, "Could not stamp visible image into a new layer\n");
        } else {
            snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
            layer_stack_stamp_visible_new(layers, "Visible Stamp", COLOR_BG);
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    return 0;
}

int app_handle_canvas_sample_shortcut_at(
    CanvasShortcutAction canvas_action,
    LayerStack *layers,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_active,
    int x,
    int y,
    int canvas_width,
    int canvas_height,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    Tool *tool,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity,
    int *needs_composite
) {
    Layer *active = NULL;

    if (!layers || !tool || !brush_color || !brush_color_rgb || !brush_opacity) {
        return 0;
    }
    if (canvas_action != CANVAS_SHORTCUT_FILL && canvas_action != CANVAS_SHORTCUT_EYEDROPPER) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= canvas_width || y >= canvas_height) {
        return 1;
    }

    if (canvas_action == CANVAS_SHORTCUT_FILL) {
        active = app_active_editable_layer(layers);
        if (!active) {
            return 1;
        }

        snapshot_push(layers, undo_stack, undo_count, undo_capacity, redo_stack, redo_count);
        if (!canvas_flood_fill(&active->canvas, x, y, *brush_color)) {
            fprintf(stderr, "Fill failed\n");
        } else if (needs_composite) {
            *needs_composite = 1;
        }
        return 1;
    }

    app_apply_sampled_brush_color_from_available_canvas(
        composite,
        preview_canvas,
        preview_active,
        x,
        y,
        tool,
        brush_color,
        brush_color_rgb,
        brush_opacity
    );
    return 1;
}

int app_handle_view_shortcut(
    ViewShortcutResult view_result,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    if (!layers) {
        return 0;
    }

    if (view_result.action == VIEW_SHORTCUT_CYCLE) {
        return layer_stack_cycle(layers, view_result.cycle_direction) >= 0;
    }

    if (view_result.action == VIEW_SHORTCUT_TRANSLATE) {
        if (app_apply_canvas_translation(
                layers,
                undo_stack,
                undo_count,
                undo_capacity,
                redo_stack,
                redo_count,
                view_result.dx,
                view_result.dy
            ) && needs_composite) {
            *needs_composite = 1;
        }
        return 1;
    }

    return 0;
}
