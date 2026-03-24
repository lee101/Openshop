#include "color_sample.h"

int sample_canvas_brush_state(const Canvas *sample, int x, int y,
                              uint32_t *brush_color_rgb, uint32_t *brush_color,
                              int *brush_opacity, Tool *tool) {
    uint32_t sampled_color;
    int sampled_alpha;

    if (!sample || !brush_color_rgb || !brush_color || !brush_opacity || !tool) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= sample->width || y >= sample->height) {
        return 0;
    }

    sampled_color = canvas_get_pixel(sample, x, y);
    *brush_color_rgb = sampled_color & 0x00FFFFFF;
    sampled_alpha = (int)((sampled_color >> 24) & 0xFF);
    *brush_opacity = (sampled_alpha * 100 + 127) / 255;
    if (*brush_opacity < 1) {
        *brush_opacity = 1;
    }
    *brush_color = compose_brush_color(*brush_color_rgb, *brush_opacity);
    *tool = TOOL_BRUSH;
    return 1;
}
