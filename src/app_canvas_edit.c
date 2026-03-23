#include "app_canvas_edit.h"

#include "canvas.h"
#include <stddef.h>

static void app_canvas_edit_push_snapshot(const LayerStack *layers, const AppCanvasEditCallbacks *callbacks) {
    if (callbacks && callbacks->push_snapshot) {
        callbacks->push_snapshot(layers, callbacks->userdata);
    }
}

static int app_canvas_edit_active_ready(LayerStack *layers) {
    Layer *active = NULL;

    if (!layers) {
        return 0;
    }
    active = layer_stack_active(layers);
    return active && !active->locked && active->canvas.pixels;
}

int app_canvas_edit_transform_active(
    AppCanvasTransformAction action,
    LayerStack *layers,
    AppCanvasEditState *state,
    const AppCanvasEditCallbacks *callbacks
) {
    Layer *active = NULL;

    if (!state || !app_canvas_edit_active_ready(layers)) {
        return 0;
    }

    active = layer_stack_active(layers);
    app_canvas_edit_push_snapshot(layers, callbacks);

    switch (action) {
    case APP_CANVAS_TRANSFORM_FLIP_HORIZONTAL:
        canvas_flip_horizontal(&active->canvas);
        break;
    case APP_CANVAS_TRANSFORM_FLIP_VERTICAL:
        canvas_flip_vertical(&active->canvas);
        break;
    case APP_CANVAS_TRANSFORM_ROTATE_180:
        canvas_rotate_180(&active->canvas);
        break;
    case APP_CANVAS_TRANSFORM_INVERT_RGB:
        canvas_invert_rgb(&active->canvas);
        break;
    default:
        return 0;
    }

    state->needs_composite = 1;
    return 1;
}

int app_canvas_edit_translate_active(
    LayerStack *layers,
    AppCanvasEditState *state,
    int dx,
    int dy,
    uint32_t clear_color,
    const AppCanvasEditCallbacks *callbacks
) {
    Layer *active = NULL;

    if (!state || (dx == 0 && dy == 0) || !app_canvas_edit_active_ready(layers)) {
        return 0;
    }

    active = layer_stack_active(layers);
    app_canvas_edit_push_snapshot(layers, callbacks);
    canvas_translate(&active->canvas, dx, dy, clear_color);
    state->needs_composite = 1;
    return 1;
}
