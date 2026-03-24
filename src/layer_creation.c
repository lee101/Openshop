#include "layer_creation.h"

#include <stddef.h>
#include <string.h>

int layer_creation_try_add(LayerStack *layers,
                           Snapshot *undo_stack, int *undo_count,
                           Snapshot *redo_stack, int *redo_count,
                           uint32_t clear_color, int max_history) {
    Snapshot before = {0};

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || max_history <= 0) {
        return 0;
    }
    if (layers->layer_count >= MAX_LAYERS) {
        return 0;
    }
    if (!snapshot_from_layers(&before, layers)) {
        return 0;
    }
    if (layer_stack_add(layers, NULL, clear_color) < 0) {
        snapshot_free(&before);
        return 0;
    }

    if (*undo_count == max_history) {
        snapshot_free(&undo_stack[0]);
        memmove(&undo_stack[0], &undo_stack[1], sizeof(Snapshot) * (size_t)(max_history - 1));
        *undo_count = max_history - 1;
    }
    undo_stack[(*undo_count)++] = before;
    snapshot_stack_clear(redo_stack, redo_count);
    return 1;
}
