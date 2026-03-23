#include "app_tool.h"

#include <stddef.h>

enum {
    APP_TOOL_BRUSH = 0,
    APP_TOOL_ERASER,
    APP_TOOL_LINE,
    APP_TOOL_RECT,
    APP_TOOL_FILLED_RECT,
    APP_TOOL_ELLIPSE,
    APP_TOOL_FILLED_ELLIPSE
};

enum {
    APP_BRUSH_SHAPE_ROUND = 0,
    APP_BRUSH_SHAPE_SQUARE,
    APP_BRUSH_SHAPE_DIAMOND,
    APP_BRUSH_SHAPE_COUNT
};

static unsigned int compose_brush_color(unsigned int rgb_color, int opacity_percent) {
    if (opacity_percent < 1) {
        opacity_percent = 1;
    } else if (opacity_percent > 100) {
        opacity_percent = 100;
    }
    return (unsigned int)(((opacity_percent * 255 + 50) / 100) << 24) | (rgb_color & 0x00FFFFFFu);
}

static int cycle_brush_shape(int shape, int direction) {
    int idx = shape + direction;
    if (idx < 0) {
        idx = APP_BRUSH_SHAPE_COUNT - 1;
    } else if (idx >= APP_BRUSH_SHAPE_COUNT) {
        idx = 0;
    }
    return idx;
}

AppToolCommand app_tool_command_for_key(
    int key,
    int tool,
    int brush_shape,
    int brush_radius,
    int brush_opacity,
    unsigned int brush_color_rgb
) {
    AppToolCommand command = {
        .handled = 1,
        .tool = tool,
        .brush_shape = brush_shape,
        .brush_radius = brush_radius,
        .brush_opacity = brush_opacity,
        .brush_color_rgb = brush_color_rgb,
        .brush_color = compose_brush_color(brush_color_rgb, brush_opacity),
    };

    switch (key) {
    case 'b':
        command.tool = APP_TOOL_BRUSH;
        command.brush_color_rgb = 0x001B1F24u;
        break;
    case 'e':
        command.tool = APP_TOOL_ERASER;
        command.brush_color_rgb = 0x00FFFFFFu;
        break;
    case 'l':
        command.tool = APP_TOOL_LINE;
        break;
    case 'r':
        command.tool = APP_TOOL_RECT;
        break;
    case 't':
        command.tool = APP_TOOL_FILLED_RECT;
        break;
    case 'o':
        command.tool = APP_TOOL_ELLIPSE;
        break;
    case 'p':
        command.tool = APP_TOOL_FILLED_ELLIPSE;
        break;
    case '[':
        if (command.brush_radius > 1) {
            command.brush_radius -= 1;
        }
        break;
    case ']':
        if (command.brush_radius < 64) {
            command.brush_radius += 1;
        }
        break;
    case ',':
        command.brush_shape = cycle_brush_shape(command.brush_shape, -1);
        break;
    case '.':
        command.brush_shape = cycle_brush_shape(command.brush_shape, 1);
        break;
    case '-':
    case 1073741910:
        if (command.brush_opacity > 1) {
            command.brush_opacity -= 5;
            if (command.brush_opacity < 1) {
                command.brush_opacity = 1;
            }
        }
        break;
    case '=':
    case 1073741911:
        if (command.brush_opacity < 100) {
            command.brush_opacity += 5;
            if (command.brush_opacity > 100) {
                command.brush_opacity = 100;
            }
        }
        break;
    case '1':
        command.tool = APP_TOOL_BRUSH;
        command.brush_color_rgb = 0x001B1F24u;
        break;
    case '2':
        command.tool = APP_TOOL_BRUSH;
        command.brush_color_rgb = 0x00E53935u;
        break;
    case '3':
        command.tool = APP_TOOL_BRUSH;
        command.brush_color_rgb = 0x0043A047u;
        break;
    case '4':
        command.tool = APP_TOOL_BRUSH;
        command.brush_color_rgb = 0x001E88E5u;
        break;
    case '5':
        command.tool = APP_TOOL_BRUSH;
        command.brush_color_rgb = 0x00FDD835u;
        break;
    case '6':
        command.tool = APP_TOOL_BRUSH;
        command.brush_color_rgb = 0x008E24AAu;
        break;
    default:
        command.handled = 0;
        return command;
    }

    command.brush_color = compose_brush_color(command.brush_color_rgb, command.brush_opacity);
    return command;
}

AppToolEffectCommand app_tool_effect_command_for_key(int key) {
    AppToolEffectCommand command = {1, APP_TOOL_EFFECT_NONE};

    switch (key) {
    case 'c':
        command.action = APP_TOOL_EFFECT_CLEAR_LAYER;
        break;
    case 'h':
        command.action = APP_TOOL_EFFECT_FLIP_HORIZONTAL;
        break;
    case 'v':
        command.action = APP_TOOL_EFFECT_FLIP_VERTICAL;
        break;
    case 'j':
        command.action = APP_TOOL_EFFECT_ROTATE_180;
        break;
    case 'x':
        command.action = APP_TOOL_EFFECT_INVERT_RGB;
        break;
    case 'f':
        command.action = APP_TOOL_EFFECT_FLOOD_FILL;
        break;
    case 'i':
        command.action = APP_TOOL_EFFECT_PICK_COLOR;
        break;
    default:
        command.handled = 0;
        command.action = APP_TOOL_EFFECT_NONE;
        break;
    }

    return command;
}

static void app_tool_effect_push_snapshot(const LayerStack *layers, const AppToolEffectCallbacks *callbacks) {
    if (callbacks && callbacks->push_snapshot) {
        callbacks->push_snapshot(layers, callbacks->userdata);
    }
}

static unsigned int app_tool_compose_brush_color(unsigned int rgb_color, int opacity_percent) {
    return compose_brush_color(rgb_color, opacity_percent);
}

int app_tool_pick_sample(
    AppToolEffectState *state,
    const Canvas *preview_canvas,
    const Canvas *composite,
    int mouse_x,
    int mouse_y,
    int canvas_width,
    int canvas_height,
    const AppToolEffectCallbacks *callbacks
) {
    const Canvas *sample = NULL;
    uint32_t sampled_color = 0;

    if (!state || !callbacks || !callbacks->sample_canvas) {
        return 0;
    }
    if (mouse_x < 0 || mouse_y < 0 || mouse_x >= canvas_width || mouse_y >= canvas_height) {
        return 0;
    }

    sample = (state->preview_active && preview_canvas && preview_canvas->pixels) ? preview_canvas : composite;
    if (!sample) {
        return 0;
    }

    sampled_color = callbacks->sample_canvas(sample, mouse_x, mouse_y, callbacks->userdata);
    state->brush_color = sampled_color;
    state->brush_color_rgb = sampled_color & 0x00FFFFFFu;
    state->brush_opacity = (int)((((sampled_color >> 24) & 0xFF) * 100 + 127) / 255);
    if (state->brush_opacity < 1) {
        state->brush_opacity = 1;
    }
    state->brush_color = app_tool_compose_brush_color(state->brush_color_rgb, state->brush_opacity);
    state->tool = APP_TOOL_BRUSH;
    return 1;
}

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
) {
    Layer *active = NULL;

    if (!layers || !state || !command.handled) {
        return 0;
    }

    switch (command.action) {
    case APP_TOOL_EFFECT_CLEAR_LAYER:
        active = layer_stack_active(layers);
        if (active && !active->locked) {
            app_tool_effect_push_snapshot(layers, callbacks);
        }
        if (!layer_stack_clear_layer(layers, layers->active_layer, clear_color)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_TOOL_EFFECT_FLIP_HORIZONTAL:
    case APP_TOOL_EFFECT_FLIP_VERTICAL:
    case APP_TOOL_EFFECT_ROTATE_180:
    case APP_TOOL_EFFECT_INVERT_RGB:
        if (!callbacks || !callbacks->transform_layer) {
            return 0;
        }
        if (!callbacks->transform_layer(layers, layers->active_layer, command.action, callbacks->userdata)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_TOOL_EFFECT_FLOOD_FILL:
        if (mouse_x < 0 || mouse_y < 0 || mouse_x >= layers->width || mouse_y >= layers->height) {
            return 0;
        }
        active = layer_stack_active(layers);
        if (active && !active->locked) {
            app_tool_effect_push_snapshot(layers, callbacks);
        }
        if (!active || active->locked || !callbacks || !callbacks->flood_fill ||
            !callbacks->flood_fill(&active->canvas, mouse_x, mouse_y, state->brush_color, callbacks->userdata)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_TOOL_EFFECT_PICK_COLOR:
        return app_tool_pick_sample(
            state,
            preview_canvas,
            composite,
            mouse_x,
            mouse_y,
            layers->width,
            layers->height,
            callbacks
        );
    case APP_TOOL_EFFECT_NONE:
    default:
        return 0;
    }
}
