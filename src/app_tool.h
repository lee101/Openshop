#ifndef APP_TOOL_H
#define APP_TOOL_H

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

AppToolCommand app_tool_command_for_key(
    int key,
    int tool,
    int brush_shape,
    int brush_radius,
    int brush_opacity,
    unsigned int brush_color_rgb
);

AppToolEffectCommand app_tool_effect_command_for_key(int key);

#endif
