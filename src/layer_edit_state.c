#include "layer_edit_state.h"

#include <stddef.h>

uint32_t active_layer_clear_color(const LayerStack *layers, uint32_t background_color) {
    if (!layers) {
        return background_color;
    }
    return (layers->active_layer == 0) ? background_color : 0x00000000;
}

int active_layer_editable(const LayerStack *layers) {
    const Layer *active = layers ? layer_stack_get(layers, layers->active_layer) : NULL;
    return active && !active->locked && active->canvas.pixels;
}
