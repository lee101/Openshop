#ifndef APP_PREVIEW_H
#define APP_PREVIEW_H

#include "canvas.h"
#include "app_brush.h"

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

void app_begin_shape_preview_from_canvas(
    int start_x,
    int start_y,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y,
    uint32_t *shape_base_pixels,
    const Canvas *composite
);

void app_cancel_shape_preview(int *shaping, int *preview_active);

const Canvas *app_preview_canvas_or_composite(
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_active
);

void app_restore_shape_preview(
    uint32_t *preview_pixels,
    const uint32_t *shape_base_pixels,
    size_t pixel_count,
    int *preview_active
);

int app_prepare_shape_preview_motion(
    Canvas *preview_canvas,
    uint32_t *preview_pixels,
    const uint32_t *shape_base_pixels,
    size_t pixel_count,
    int *preview_active,
    Tool tool,
    int shape_start_x,
    int shape_start_y,
    int x,
    int y,
    int shift,
    int *out_x,
    int *out_y
);

int app_prepare_shape_commit(
    const int *shaping,
    Tool tool,
    int shape_start_x,
    int shape_start_y,
    int x,
    int y,
    int shift,
    int *out_x,
    int *out_y
);

#endif
