#include "app_title.h"

#include <stdio.h>

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
) {
    if (!title || title_size == 0) {
        return;
    }

    snprintf(
        title,
        title_size,
        "Openshop - %s (%s) | size %d | brush %d%% | layer %d/%d %s [%s%s %d%%]%s | visible %d/%d | #%08X",
        tool_label ? tool_label : "Brush",
        brush_shape_label ? brush_shape_label : "Round",
        radius,
        opacity_percent,
        active_layer_index + 1,
        layer_count,
        (layer_name && layer_name[0]) ? layer_name : "Layer",
        active_visible ? "visible" : "hidden",
        active_locked ? ", locked" : "",
        active_opacity_percent,
        active_is_solo ? " [solo]" : "",
        visible_layer_count,
        layer_count,
        color
    );
}
