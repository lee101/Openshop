#include "app_color.h"

uint32_t app_compose_brush_color(uint32_t rgb_color, int opacity_percent) {
    uint32_t alpha;

    if (opacity_percent < 1) {
        opacity_percent = 1;
    } else if (opacity_percent > 100) {
        opacity_percent = 100;
    }

    alpha = (uint32_t)((opacity_percent * 255 + 50) / 100);
    return (alpha << 24) | (rgb_color & 0x00FFFFFF);
}
