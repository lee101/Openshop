#ifndef LAYER_ACTION_HISTORY_H
#define LAYER_ACTION_HISTORY_H

#include "snapshot_history.h"

typedef int (*LayerActionHistoryFn)(LayerStack *layers, int arg);

int layer_action_history_apply_indexed(LayerStack *layers,
                                       Snapshot *undo_stack, int *undo_count,
                                       Snapshot *redo_stack, int *redo_count,
                                       int max_history,
                                       LayerActionHistoryFn action,
                                       int index);

int layer_action_history_apply_directional(LayerStack *layers,
                                           Snapshot *undo_stack, int *undo_count,
                                           Snapshot *redo_stack, int *redo_count,
                                           int max_history,
                                           LayerActionHistoryFn action,
                                           int arg);

#endif
