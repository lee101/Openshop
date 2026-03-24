#ifndef APP_PREVIEW_H
#define APP_PREVIEW_H

#include <stddef.h>
#include <stdint.h>

void app_begin_shape_preview(
    int start_x,
    int start_y,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y,
    uint32_t *shape_base_pixels,
    const uint32_t *composite_pixels,
    size_t pixel_count
);

void app_cancel_shape_preview(int *shaping, int *preview_active);

#endif
