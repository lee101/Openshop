#include "shape_preview_state.h"

#include "layer_edit_state.h"

#include <stddef.h>
#include <string.h>

void shape_preview_cancel(int *shaping, int *preview_active) {
    if (shaping) {
        *shaping = 0;
    }
    if (preview_active) {
        *preview_active = 0;
    }
}

int shape_preview_begin_if_editable(const LayerStack *layers,
                                    int x, int y,
                                    const Canvas *composite,
                                    uint32_t *shape_base_pixels,
                                    int *shaping,
                                    int *shape_start_x,
                                    int *shape_start_y) {
    size_t pixel_count;

    if (!shaping || !shape_start_x || !shape_start_y || !active_layer_editable(layers)) {
        return 0;
    }

    *shaping = 1;
    *shape_start_x = x;
    *shape_start_y = y;

    if (shape_base_pixels && composite && composite->pixels && composite->width > 0 && composite->height > 0) {
        pixel_count = (size_t)composite->width * (size_t)composite->height;
        memcpy(shape_base_pixels, composite->pixels, pixel_count * sizeof(uint32_t));
    }

    return 1;
}
