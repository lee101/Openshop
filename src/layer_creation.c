#include "layer_creation.h"

#include <stddef.h>

int layer_creation_try_add(LayerStack *layers,
                           Snapshot *undo_stack, int *undo_count,
                           Snapshot *redo_stack, int *redo_count,
                           uint32_t clear_color, int max_history) {
    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count) {
        return 0;
    }

    snapshot_push(layers, undo_stack, undo_count, redo_stack, redo_count, max_history);
    return layer_stack_add(layers, NULL, clear_color) >= 0;
}
