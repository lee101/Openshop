#include "active_layer_ops.h"

#include "layer_edit_state.h"

static int active_layer_apply_transform(LayerStack *layers,
                                        Snapshot *undo_stack, int *undo_count,
                                        Snapshot *redo_stack, int *redo_count,
                                        int max_history,
                                        void (*transform)(Canvas *)) {
    Layer *active;

    if (!layers || !transform) {
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
    if (active_layer_editable(layers)) {
        snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    }
    return layer_stack_clear_layer(layers, layers->active_layer, active_layer_clear_color(layers, background_color));
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

    if (!active || active->opacity_percent == target_opacity) {
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

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!active) {
        return 0;
    }

    return active_layer_try_adjust_opacity(layers, undo_stack, undo_count, redo_stack, redo_count,
                                           active->opacity_percent + delta_percent, max_history);
}

int active_layer_apply_translation(LayerStack *layers,
                                   Snapshot *undo_stack, int *undo_count,
                                   Snapshot *redo_stack, int *redo_count,
                                   int dx, int dy,
                                   uint32_t background_color, int max_history) {
    Layer *active;

    if (!layers || (dx == 0 && dy == 0)) {
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
