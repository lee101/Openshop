#ifndef APP_TITLE_H
#define APP_TITLE_H

#include <stddef.h>
#include <stdint.h>

void app_title_format(
    char *title,
    size_t title_size,
    const char *tool_label,
    const char *brush_shape_label,
    int radius,
    int opacity_percent,
    int active_layer_index,
    int layer_count,
    const char *layer_name,
    int active_visible,
    int active_locked,
    int active_opacity_percent,
    int active_is_solo,
    int visible_layer_count,
    uint32_t color
);

#endif
