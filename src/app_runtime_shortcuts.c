#include "app_runtime_shortcuts.h"

#include "app_canvas_ops.h"
#include "app_layer_state.h"
#include "app_sampled_color.h"

#include <stdio.h>

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
