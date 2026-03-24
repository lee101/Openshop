#ifndef LAYER_ACTION_HISTORY_H
#define LAYER_ACTION_HISTORY_H

#include "snapshot_history.h"

typedef int (*LayerActionHistoryFn)(LayerStack *layers, int arg);
typedef int (*LayerActionHistoryCustomFn)(LayerStack *layers, void *ctx);

typedef enum {
    LAYER_ACTION_HISTORY_FAILED = 0,
    LAYER_ACTION_HISTORY_UNCHANGED,
    LAYER_ACTION_HISTORY_CHANGED
} LayerActionHistoryResult;

LayerActionHistoryResult layer_action_history_apply_indexed_with_result(LayerStack *layers,
                                                                        Snapshot *undo_stack, int *undo_count,
                                                                        Snapshot *redo_stack, int *redo_count,
                                                                        int max_history,
                                                                        LayerActionHistoryFn action,
                                                                        int index);

int layer_action_history_apply_indexed(LayerStack *layers,
                                       Snapshot *undo_stack, int *undo_count,
                                       Snapshot *redo_stack, int *redo_count,
                                       int max_history,
                                       LayerActionHistoryFn action,
                                       int index);

LayerActionHistoryResult layer_action_history_apply_directional_with_result(LayerStack *layers,
                                                                            Snapshot *undo_stack, int *undo_count,
                                                                            Snapshot *redo_stack, int *redo_count,
                                                                            int max_history,
                                                                            LayerActionHistoryFn action,
                                                                            int arg);

int layer_action_history_apply_directional(LayerStack *layers,
                                           Snapshot *undo_stack, int *undo_count,
                                           Snapshot *redo_stack, int *redo_count,
                                           int max_history,
                                           LayerActionHistoryFn action,
                                           int arg);

LayerActionHistoryResult layer_action_history_apply_custom_with_result(LayerStack *layers,
                                                                       Snapshot *undo_stack, int *undo_count,
                                                                       Snapshot *redo_stack, int *redo_count,
                                                                       int max_history,
                                                                       LayerActionHistoryCustomFn action,
                                                                       void *ctx);

int layer_action_history_apply_custom(LayerStack *layers,
                                      Snapshot *undo_stack, int *undo_count,
                                      Snapshot *redo_stack, int *redo_count,
                                      int max_history,
                                      LayerActionHistoryCustomFn action,
                                      void *ctx);

#endif
