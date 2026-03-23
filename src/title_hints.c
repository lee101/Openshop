#include "title_hints.h"

#include <stdio.h>

static int count_locked_layers(const LayerStack *layers) {
    int locked_layers = 0;

    if (!layers) {
        return 0;
    }

    for (int i = 0; i < layers->layer_count; i++) {
        if (layers->layers[i].locked) {
            locked_layers++;
        }
    }
    return locked_layers;
}

void format_hidden_layer_hint(const LayerStack *layers, char *buffer, size_t buffer_size) {
    int hidden_locked = 0;
    int hidden_unlocked = 0;

    if (!buffer || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';
    if (!layers) {
        return;
    }

    for (int i = 0; i < layers->layer_count; i++) {
        if (!layers->layers[i].visible) {
            if (layers->layers[i].locked) {
                hidden_locked++;
            } else {
                hidden_unlocked++;
            }
        }
    }

    if (hidden_locked > 0 && hidden_unlocked > 0) {
        snprintf(buffer, buffer_size, " | hints hu C-A-;/' hl C-S-,/.");
    } else if (hidden_unlocked > 0) {
        snprintf(buffer, buffer_size, " | hint hu C-A-;/'");
    } else if (hidden_locked > 0) {
        snprintf(buffer, buffer_size, " | hint hl C-S-,/.");
    }
}

void format_window_title(const LayerStack *layers, const char *tool_name, const char *brush_shape_name,
                         int radius, uint32_t color, int opacity_percent, char *buffer, size_t buffer_size) {
    const Layer *active;
    const char *layer_name;
    int visible_layers;
    int locked_layers;
    int hidden_layers;
    char hint[40];

    if (!buffer || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';
    if (!layers) {
        return;
    }

    active = layer_stack_get(layers, layers->active_layer);
    layer_name = active && active->name[0] ? active->name : "Layer";
    visible_layers = layer_stack_visible_count(layers);
    locked_layers = count_locked_layers(layers);
    hidden_layers = layers->layer_count - visible_layers;
    format_hidden_layer_hint(layers, hint, sizeof(hint));

    snprintf(
        buffer,
        buffer_size,
        "Openshop - %s (%s) | size %d | brush %d%% | layer %d/%d %s [%s%s %d%%]%s | vis %d hid %d lock %d solo %s | #%08X%s",
        tool_name ? tool_name : "Tool",
        brush_shape_name ? brush_shape_name : "Brush",
        radius,
        opacity_percent,
        layers->active_layer + 1,
        layers->layer_count,
        layer_name,
        active && active->visible ? "visible" : "hidden",
        active && active->locked ? ", locked" : "",
        active ? active->opacity_percent : 100,
        (layers->solo_index == layers->active_layer) ? " [solo]" : "",
        visible_layers,
        hidden_layers,
        locked_layers,
        layers->solo_index >= 0 ? "on" : "off",
        color,
        hint
    );
}
