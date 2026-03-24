#ifndef APP_SAMPLED_COLOR_H
#define APP_SAMPLED_COLOR_H

#include "app_brush.h"
#include "canvas.h"
#include <stdint.h>

void app_apply_sampled_brush_color(
    uint32_t sampled_color,
    Tool *tool,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity
);

void app_apply_sampled_brush_color_from_canvas(
    const Canvas *canvas,
    int x,
    int y,
    Tool *tool,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity
);

void app_apply_sampled_brush_color_from_available_canvas(
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_active,
    int x,
    int y,
    Tool *tool,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity
);

#endif
