#ifndef APP_LAYER_STATE_H
#define APP_LAYER_STATE_H

#include "layers.h"
#include <stdint.h>

uint32_t app_active_layer_clear_color(int active_layer_index);
int app_layer_editable(const Layer *layer);
int app_active_layer_editable(const LayerStack *stack);

#endif
