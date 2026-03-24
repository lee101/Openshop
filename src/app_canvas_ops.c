#include "app_canvas_ops.h"

#include "app_layer_state.h"

int app_apply_canvas_transform(
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int history_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    void (*transform)(Canvas *)
) {
    Layer *active;

    if (!layers || !transform) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!app_layer_editable(active)) {
        return 0;
    }

    snapshot_push(layers, undo_stack, undo_count, history_capacity, redo_stack, redo_count);
    transform(&active->canvas);
    return 1;
}

int app_apply_canvas_translation(
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int history_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int dx,
    int dy
) {
    Layer *active;

    if (!layers || (dx == 0 && dy == 0)) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!app_layer_editable(active)) {
        return 0;
    }

    snapshot_push(layers, undo_stack, undo_count, history_capacity, redo_stack, redo_count);
    canvas_translate(&active->canvas, dx, dy, app_active_layer_clear_color(layers->active_layer));
    return 1;
}
