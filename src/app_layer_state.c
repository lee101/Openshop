#include "app_layer_state.h"

#include <stddef.h>

uint32_t app_active_layer_clear_color(int active_layer_index) {
    return active_layer_index == 0 ? 0xFFFFFFFFu : 0x00000000u;
}

int app_layer_editable(const Layer *layer) {
    return layer && !layer->locked && layer->canvas.pixels;
}

int app_active_layer_editable(const LayerStack *stack) {
    if (!stack || stack->active_layer < 0 || stack->active_layer >= stack->layer_count) {
        return 0;
    }
    return app_layer_editable(&stack->layers[stack->active_layer]);
}

Layer *app_active_editable_layer(LayerStack *stack) {
    if (!stack || stack->active_layer < 0 || stack->active_layer >= stack->layer_count) {
        return NULL;
    }
    return app_layer_editable(&stack->layers[stack->active_layer]) ? &stack->layers[stack->active_layer] : NULL;
}
