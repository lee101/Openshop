#include "layers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t blend_channel(uint8_t src, uint8_t dst, uint8_t src_alpha) {
    int inv = 255 - src_alpha;
    int value = src * src_alpha + dst * inv + 127;
    return (uint8_t)(value / 255);
}

static uint32_t blend_pixel(uint32_t dst, uint32_t src) {
    uint8_t sa = (uint8_t)((src >> 24) & 0xFF);
    if (sa == 0) {
        return dst;
    }
    if (sa == 255) {
        return src;
    }
    uint8_t da = (uint8_t)((dst >> 24) & 0xFF);
    uint8_t sr = (uint8_t)((src >> 16) & 0xFF);
    uint8_t sg = (uint8_t)((src >> 8) & 0xFF);
    uint8_t sb = (uint8_t)(src & 0xFF);
    uint8_t dr = (uint8_t)((dst >> 16) & 0xFF);
    uint8_t dg = (uint8_t)((dst >> 8) & 0xFF);
    uint8_t db = (uint8_t)(dst & 0xFF);

    uint8_t out_r = blend_channel(sr, dr, sa);
    uint8_t out_g = blend_channel(sg, dg, sa);
    uint8_t out_b = blend_channel(sb, db, sa);
    uint8_t out_a = (uint8_t)(sa + ((da * (255 - sa) + 127) / 255));

    return ((uint32_t)out_a << 24) | ((uint32_t)out_r << 16) | ((uint32_t)out_g << 8) | out_b;
}

static int ensure_layer_canvas(Layer *layer, int width, int height) {
    if (!layer) {
        return 0;
    }
    if (layer->canvas.pixels) {
        if (layer->canvas.width == width && layer->canvas.height == height) {
            return 1;
        }
        canvas_free(&layer->canvas);
    }
    return canvas_init(&layer->canvas, width, height);
}

int layer_stack_init(LayerStack *stack, int width, int height, uint32_t background_color) {
    if (!stack || width <= 0 || height <= 0) {
        return 0;
    }
    stack->width = width;
    stack->height = height;
    stack->layer_count = 0;
    stack->active_layer = 0;
    for (int i = 0; i < MAX_LAYERS; i++) {
        stack->layers[i].canvas.width = width;
        stack->layers[i].canvas.height = height;
        stack->layers[i].canvas.pixels = NULL;
        stack->layers[i].visible = 0;
        stack->layers[i].name[0] = '\0';
    }
    if (layer_stack_add(stack, "Background", background_color) < 0) {
        layer_stack_free(stack);
        return 0;
    }
    stack->active_layer = 0;
    return 1;
}

void layer_stack_free(LayerStack *stack) {
    if (!stack) {
        return;
    }
    for (int i = 0; i < MAX_LAYERS; i++) {
        canvas_free(&stack->layers[i].canvas);
        stack->layers[i].visible = 0;
        stack->layers[i].name[0] = '\0';
    }
    stack->layer_count = 0;
    stack->active_layer = 0;
    stack->width = 0;
    stack->height = 0;
}

Layer *layer_stack_active(LayerStack *stack) {
    if (!stack || stack->layer_count == 0) {
        return NULL;
    }
    if (stack->active_layer < 0 || stack->active_layer >= stack->layer_count) {
        stack->active_layer = stack->layer_count - 1;
    }
    return &stack->layers[stack->active_layer];
}

const Layer *layer_stack_get(const LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return NULL;
    }
    return &stack->layers[index];
}

int layer_stack_add(LayerStack *stack, const char *name, uint32_t clear_color) {
    if (!stack || stack->layer_count >= MAX_LAYERS) {
        return -1;
    }
    int index = stack->layer_count;
    Layer *layer = &stack->layers[index];
    if (!ensure_layer_canvas(layer, stack->width, stack->height)) {
        return -1;
    }
    canvas_clear(&layer->canvas, clear_color);
    layer->visible = 1;
    if (name && name[0]) {
        strncpy(layer->name, name, LAYER_NAME_MAX - 1);
        layer->name[LAYER_NAME_MAX - 1] = '\0';
    } else {
        snprintf(layer->name, LAYER_NAME_MAX, "Layer %d", index + 1);
    }
    stack->layer_count++;
    stack->active_layer = index;
    return index;
}

int layer_stack_cycle(LayerStack *stack, int direction) {
    if (!stack || stack->layer_count == 0) {
        return -1;
    }
    int count = stack->layer_count;
    int idx = stack->active_layer + direction;
    if (idx < 0) {
        idx = count - 1;
    } else if (idx >= count) {
        idx = 0;
    }
    stack->active_layer = idx;
    return idx;
}

int layer_stack_visible_count(const LayerStack *stack) {
    if (!stack) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < stack->layer_count; i++) {
        if (stack->layers[i].visible) {
            count++;
        }
    }
    return count;
}

int layer_stack_toggle_visibility(LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }
    Layer *layer = &stack->layers[index];
    if (layer->visible && layer_stack_visible_count(stack) == 1) {
        return 0;
    }
    layer->visible = !layer->visible;
    return 1;
}

int layer_stack_clear_layer(LayerStack *stack, int index, uint32_t color) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }
    Layer *layer = &stack->layers[index];
    if (!layer->canvas.pixels && !ensure_layer_canvas(layer, stack->width, stack->height)) {
        return 0;
    }
    canvas_clear(&layer->canvas, color);
    return 1;
}

void layer_stack_composite(const LayerStack *stack, Canvas *dest, uint32_t background_color) {
    if (!stack || !dest || !dest->pixels) {
        return;
    }
    if (dest->width != stack->width || dest->height != stack->height) {
        return;
    }
    size_t total = (size_t)stack->width * (size_t)stack->height;
    for (size_t i = 0; i < total; i++) {
        uint32_t out = background_color;
        for (int layer_index = 0; layer_index < stack->layer_count; layer_index++) {
            const Layer *layer = &stack->layers[layer_index];
            if (!layer->visible || !layer->canvas.pixels) {
                continue;
            }
            uint32_t src = layer->canvas.pixels[i];
            out = blend_pixel(out, src);
        }
        dest->pixels[i] = out;
    }
}
