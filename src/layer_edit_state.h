#ifndef LAYER_EDIT_STATE_H
#define LAYER_EDIT_STATE_H

#include "layers.h"

uint32_t active_layer_clear_color(const LayerStack *layers, uint32_t background_color);
int active_layer_editable(const LayerStack *layers);

#endif
