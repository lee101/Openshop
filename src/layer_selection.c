#include "layer_selection.h"

int layer_selection_try_select_index(LayerStack *layers, int target) {
    if (!layers || target < 0 || target >= layers->layer_count) {
        return 0;
    }

    layers->active_layer = target;
    return 1;
}
