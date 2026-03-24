#ifndef APP_SAMPLED_COLOR_H
#define APP_SAMPLED_COLOR_H

#include "app_brush.h"
#include <stdint.h>

void app_apply_sampled_brush_color(
    uint32_t sampled_color,
    Tool *tool,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity
);

#endif
