#ifndef APP_CANVAS_EDIT_H
#define APP_CANVAS_EDIT_H

#include "layers.h"
#include <stdint.h>

typedef enum {
    APP_CANVAS_TRANSFORM_FLIP_HORIZONTAL = 0,
    APP_CANVAS_TRANSFORM_FLIP_VERTICAL,
    APP_CANVAS_TRANSFORM_ROTATE_180,
    APP_CANVAS_TRANSFORM_INVERT_RGB
} AppCanvasTransformAction;

typedef struct {
    int needs_composite;
} AppCanvasEditState;

typedef struct {
    void (*push_snapshot)(const LayerStack *layers, void *userdata);
    void *userdata;
} AppCanvasEditCallbacks;

int app_canvas_edit_transform_active(
    AppCanvasTransformAction action,
    LayerStack *layers,
    AppCanvasEditState *state,
    const AppCanvasEditCallbacks *callbacks
);

int app_canvas_edit_translate_active(
    LayerStack *layers,
    AppCanvasEditState *state,
    int dx,
    int dy,
    uint32_t clear_color,
    const AppCanvasEditCallbacks *callbacks
);

#endif
