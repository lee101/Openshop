#include "layer_action_history.h"

#include <string.h>

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
        snapshot_free(&before);
        return LAYER_ACTION_HISTORY_UNCHANGED;
    }
    if (!layer_action_history_push_snapshot(&before, undo_stack, undo_count, redo_stack, redo_count, max_history)) {
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
        snapshot_free(&before);
        return LAYER_ACTION_HISTORY_UNCHANGED;
    }
    if (!layer_action_history_push_snapshot(&before, undo_stack, undo_count, redo_stack, redo_count, max_history)) {
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
