#include "app_runtime_shortcuts.h"

#include "app_canvas_ops.h"
#include "app_layer_state.h"

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
