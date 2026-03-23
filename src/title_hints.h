#ifndef TITLE_HINTS_H
#define TITLE_HINTS_H

#include "layers.h"
#include <stddef.h>
#include <stdint.h>

void format_hidden_layer_hint(const LayerStack *layers, char *buffer, size_t buffer_size);
void format_window_title(const LayerStack *layers, const char *tool_name, const char *brush_shape_name,
                         int radius, uint32_t color, int opacity_percent, char *buffer, size_t buffer_size);

#endif
