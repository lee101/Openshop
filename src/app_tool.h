#ifndef APP_TOOL_H
#define APP_TOOL_H

#include "canvas.h"
#include "layers.h"
#include <stdint.h>

typedef struct {
    int handled;
    int tool;
    int brush_shape;
    int brush_radius;
    int brush_opacity;
    unsigned int brush_color_rgb;
    unsigned int brush_color;
} AppToolCommand;

typedef enum {
    APP_TOOL_EFFECT_NONE = 0,
    APP_TOOL_EFFECT_CLEAR_LAYER,
    APP_TOOL_EFFECT_FLIP_HORIZONTAL,
    APP_TOOL_EFFECT_FLIP_VERTICAL,
    APP_TOOL_EFFECT_ROTATE_180,
    APP_TOOL_EFFECT_INVERT_RGB,
    APP_TOOL_EFFECT_FLOOD_FILL,
    APP_TOOL_EFFECT_PICK_COLOR
} AppToolEffectAction;

typedef struct {
    int handled;
    AppToolEffectAction action;
} AppToolEffectCommand;

typedef struct {
    int tool;
    int brush_opacity;
    unsigned int brush_color_rgb;
    unsigned int brush_color;
    int preview_active;
    int needs_composite;
} AppToolEffectState;

typedef struct {
    void (*push_snapshot)(const LayerStack *layers, void *userdata);
    int (*transform_layer)(LayerStack *layers, int active_layer, AppToolEffectAction action, void *userdata);
    int (*flood_fill)(Canvas *canvas, int x, int y, uint32_t color, void *userdata);
    uint32_t (*sample_canvas)(const Canvas *canvas, int x, int y, void *userdata);
    void *userdata;
} AppToolEffectCallbacks;

AppToolCommand app_tool_command_for_key(
    int key,
    int tool,
    int brush_shape,
    int brush_radius,
    int brush_opacity,
    unsigned int brush_color_rgb
);

AppToolEffectCommand app_tool_effect_command_for_key(int key);
int app_tool_effect_apply(
    AppToolEffectCommand command,
    LayerStack *layers,
    AppToolEffectState *state,
    const Canvas *preview_canvas,
    const Canvas *composite,
    int mouse_x,
    int mouse_y,
    uint32_t clear_color,
    const AppToolEffectCallbacks *callbacks
);
int app_tool_pick_sample(
    AppToolEffectState *state,
    const Canvas *preview_canvas,
    const Canvas *composite,
    int mouse_x,
    int mouse_y,
    int canvas_width,
    int canvas_height,
    const AppToolEffectCallbacks *callbacks
);

#endif
