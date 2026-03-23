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

AppToolCommand app_tool_command_for_key(
    int key,
    int tool,
    int brush_shape,
    int brush_radius,
    int brush_opacity,
    unsigned int brush_color_rgb
);

#endif
