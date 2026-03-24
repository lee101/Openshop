#ifndef LAYER_CREATION_H
#define LAYER_CREATION_H

#include "snapshot_history.h"

int layer_creation_try_add(LayerStack *layers,
                           Snapshot *undo_stack, int *undo_count,
                           Snapshot *redo_stack, int *redo_count,
                           uint32_t clear_color, int max_history);

#endif
