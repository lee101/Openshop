#include "app_sampled_color.h"

#include "app_color.h"

void app_apply_sampled_brush_color(
    uint32_t sampled_color,
    Tool *tool,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity
) {
    int sampled_alpha;

    if (!tool || !brush_color || !brush_color_rgb || !brush_opacity) {
        return;
    }

    *brush_color = sampled_color;
    *brush_color_rgb = *brush_color & 0x00FFFFFF;
    sampled_alpha = (int)((*brush_color >> 24) & 0xFF);
    *brush_opacity = (sampled_alpha * 100 + 127) / 255;
    if (*brush_opacity < 1) {
        *brush_opacity = 1;
    }
    *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
    *tool = TOOL_BRUSH;
}
