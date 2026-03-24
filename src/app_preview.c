#include "app_preview.h"

#include <string.h>

void app_begin_shape_preview(
    int start_x,
    int start_y,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y,
    uint32_t *shape_base_pixels,
    const uint32_t *composite_pixels,
    size_t pixel_count
) {
    if (!shaping || !shape_start_x || !shape_start_y) {
        return;
    }

    *shaping = 1;
    *shape_start_x = start_x;
    *shape_start_y = start_y;
    if (shape_base_pixels && composite_pixels && pixel_count > 0) {
        memcpy(shape_base_pixels, composite_pixels, pixel_count * sizeof(*shape_base_pixels));
    }
}

void app_cancel_shape_preview(int *shaping, int *preview_active) {
    if (shaping) {
        *shaping = 0;
    }
    if (preview_active) {
        *preview_active = 0;
    }
}
