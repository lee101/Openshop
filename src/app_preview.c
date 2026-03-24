#include "app_preview.h"
#include "app_shape.h"

#include <string.h>

static void copy_preview_pixels(
    uint32_t *destination_pixels,
    const uint32_t *source_pixels,
    size_t pixel_count
) {
    if (destination_pixels && source_pixels && pixel_count > 0) {
        memcpy(destination_pixels, source_pixels, pixel_count * sizeof(*destination_pixels));
    }
}

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
    copy_preview_pixels(shape_base_pixels, composite_pixels, pixel_count);
}

void app_begin_shape_preview_from_canvas(
    int start_x,
    int start_y,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y,
    uint32_t *shape_base_pixels,
    const Canvas *composite
) {
    app_begin_shape_preview(
        start_x,
        start_y,
        shaping,
        shape_start_x,
        shape_start_y,
        shape_base_pixels,
        composite ? composite->pixels : NULL,
        composite ? (size_t)composite->width * (size_t)composite->height : 0
    );
}

void app_cancel_shape_preview(int *shaping, int *preview_active) {
    if (shaping) {
        *shaping = 0;
    }
    if (preview_active) {
        *preview_active = 0;
    }
}

const Canvas *app_preview_canvas_or_composite(
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_active
) {
    if (preview_active && preview_canvas && preview_canvas->pixels) {
        return preview_canvas;
    }
    return composite;
}

void app_restore_shape_preview(
    uint32_t *preview_pixels,
    const uint32_t *shape_base_pixels,
    size_t pixel_count,
    int *preview_active
) {
    if (!preview_active) {
        return;
    }

    copy_preview_pixels(preview_pixels, shape_base_pixels, pixel_count);
    *preview_active = 1;
}

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
) {
    if (!preview_canvas || !preview_canvas->pixels || !preview_pixels || !shape_base_pixels || !out_x || !out_y) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= preview_canvas->width || y >= preview_canvas->height) {
        return 0;
    }

    app_constrain_shape_end(tool, shape_start_x, shape_start_y, x, y, shift, out_x, out_y);
    app_restore_shape_preview(preview_pixels, shape_base_pixels, pixel_count, preview_active);
    return 1;
}
