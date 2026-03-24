#include "app_layer_state.h"

uint32_t app_active_layer_clear_color(int active_layer_index) {
    return active_layer_index == 0 ? 0xFFFFFFFFu : 0x00000000u;
}

int app_layer_editable(const Layer *layer) {
    return layer && !layer->locked && layer->canvas.pixels;
}
