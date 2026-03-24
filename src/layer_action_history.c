#include "layer_action_history.h"

#include <string.h>

static int layer_action_history_snapshot_matches_layers(const Snapshot *snapshot,
                                                        const LayerStack *layers) {
    size_t per_layer;
    int layer_index;

    if (!snapshot || !layers) {
        return 0;
    }
    if (snapshot->width != layers->width || snapshot->height != layers->height ||
        snapshot->layer_count != layers->layer_count || snapshot->active_layer != layers->active_layer ||
        snapshot->solo_index != layers->solo_index) {
        return 0;
    }

    per_layer = (size_t)layers->width * (size_t)layers->height;
    for (layer_index = 0; layer_index < layers->layer_count; layer_index++) {
        const Layer *layer = &layers->layers[layer_index];
        const uint32_t *saved_pixels = snapshot->pixels + per_layer * (size_t)layer_index;
        size_t pixel_index;

        if ((snapshot->visibility[layer_index] ? 1 : 0) != layer->visible ||
            (snapshot->locked[layer_index] ? 1 : 0) != layer->locked ||
            snapshot->opacity_percent[layer_index] != (uint8_t)layer->opacity_percent ||
            strcmp(snapshot->names[layer_index], layer->name) != 0) {
            return 0;
        }

        for (pixel_index = 0; pixel_index < per_layer; pixel_index++) {
            uint32_t current_pixel = layer->canvas.pixels ? layer->canvas.pixels[pixel_index] : 0;
            if (saved_pixels[pixel_index] != current_pixel) {
                return 0;
            }
        }
    }

    return 1;
}

static int layer_action_history_push_snapshot(Snapshot *captured,
                                              Snapshot *undo_stack, int *undo_count,
                                              Snapshot *redo_stack, int *redo_count,
                                              int max_history) {
    if (!captured || !undo_stack || !undo_count || max_history <= 0) {
        return 0;
    }

    if (*undo_count == max_history) {
        snapshot_free(&undo_stack[0]);
        memmove(&undo_stack[0], &undo_stack[1], sizeof(Snapshot) * (size_t)(max_history - 1));
        *undo_count = max_history - 1;
    }

    undo_stack[(*undo_count)++] = *captured;
    memset(captured, 0, sizeof(*captured));
    if (redo_stack && redo_count) {
        snapshot_stack_clear(redo_stack, redo_count);
    }
    return 1;
}

static LayerActionHistoryResult layer_action_history_apply(LayerStack *layers,
                                                           Snapshot *undo_stack, int *undo_count,
                                                           Snapshot *redo_stack, int *redo_count,
                                                           int max_history,
                                                           LayerActionHistoryFn action,
                                                           int arg) {
    Snapshot before = {0};
    int changed;

    if (!layers || !action || !undo_stack || !undo_count || max_history <= 0) {
        return LAYER_ACTION_HISTORY_FAILED;
    }
    if (!snapshot_from_layers(&before, layers)) {
        return LAYER_ACTION_HISTORY_FAILED;
    }

    changed = action(layers, arg);
    if (!changed) {
        if (!layer_action_history_snapshot_matches_layers(&before, layers)) {
            snapshot_apply(&before, layers);
            snapshot_free(&before);
            return LAYER_ACTION_HISTORY_FAILED;
        }
        snapshot_free(&before);
        return LAYER_ACTION_HISTORY_UNCHANGED;
    }
    if (!layer_action_history_push_snapshot(&before, undo_stack, undo_count, redo_stack, redo_count, max_history)) {
        snapshot_apply(&before, layers);
        snapshot_free(&before);
        return LAYER_ACTION_HISTORY_FAILED;
    }
    return LAYER_ACTION_HISTORY_CHANGED;
}

LayerActionHistoryResult layer_action_history_apply_custom_with_result(LayerStack *layers,
                                                                       Snapshot *undo_stack, int *undo_count,
                                                                       Snapshot *redo_stack, int *redo_count,
                                                                       int max_history,
                                                                       LayerActionHistoryCustomFn action,
                                                                       void *ctx) {
    Snapshot before = {0};
    int changed;

    if (!layers || !action || !undo_stack || !undo_count || max_history <= 0) {
        return LAYER_ACTION_HISTORY_FAILED;
    }
    if (!snapshot_from_layers(&before, layers)) {
        return LAYER_ACTION_HISTORY_FAILED;
    }

    changed = action(layers, ctx);
    if (!changed) {
        if (!layer_action_history_snapshot_matches_layers(&before, layers)) {
            snapshot_apply(&before, layers);
            snapshot_free(&before);
            return LAYER_ACTION_HISTORY_FAILED;
        }
        snapshot_free(&before);
        return LAYER_ACTION_HISTORY_UNCHANGED;
    }
    if (!layer_action_history_push_snapshot(&before, undo_stack, undo_count, redo_stack, redo_count, max_history)) {
        snapshot_apply(&before, layers);
        snapshot_free(&before);
        return LAYER_ACTION_HISTORY_FAILED;
    }
    return LAYER_ACTION_HISTORY_CHANGED;
}

LayerActionHistoryResult layer_action_history_apply_indexed_with_result(LayerStack *layers,
                                                                        Snapshot *undo_stack, int *undo_count,
                                                                        Snapshot *redo_stack, int *redo_count,
                                                                        int max_history,
                                                                        LayerActionHistoryFn action,
                                                                        int index) {
    return layer_action_history_apply(layers, undo_stack, undo_count, redo_stack, redo_count,
                                      max_history, action, index);
}

int layer_action_history_apply_indexed(LayerStack *layers,
                                       Snapshot *undo_stack, int *undo_count,
                                       Snapshot *redo_stack, int *redo_count,
                                       int max_history,
                                       LayerActionHistoryFn action,
                                       int index) {
    return layer_action_history_apply_indexed_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                          max_history, action, index) == LAYER_ACTION_HISTORY_CHANGED;
}

LayerActionHistoryResult layer_action_history_apply_directional_with_result(LayerStack *layers,
                                                                            Snapshot *undo_stack, int *undo_count,
                                                                            Snapshot *redo_stack, int *redo_count,
                                                                            int max_history,
                                                                            LayerActionHistoryFn action,
                                                                            int arg) {
    return layer_action_history_apply(layers, undo_stack, undo_count, redo_stack, redo_count,
                                      max_history, action, arg);
}

int layer_action_history_apply_directional(LayerStack *layers,
                                           Snapshot *undo_stack, int *undo_count,
                                           Snapshot *redo_stack, int *redo_count,
                                           int max_history,
                                           LayerActionHistoryFn action,
                                           int arg) {
    return layer_action_history_apply_directional_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                              max_history, action, arg) == LAYER_ACTION_HISTORY_CHANGED;
}

int layer_action_history_apply_custom(LayerStack *layers,
                                      Snapshot *undo_stack, int *undo_count,
                                      Snapshot *redo_stack, int *redo_count,
                                      int max_history,
                                      LayerActionHistoryCustomFn action,
                                      void *ctx) {
    return layer_action_history_apply_custom_with_result(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                         max_history, action, ctx) == LAYER_ACTION_HISTORY_CHANGED;
}
