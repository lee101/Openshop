#include "snapshot_history.h"

#include <stdlib.h>
#include <string.h>

void snapshot_free(Snapshot *s) {
    if (!s) {
        return;
    }
    free(s->pixels);
    s->pixels = NULL;
    s->width = 0;
    s->height = 0;
    s->layer_count = 0;
    s->active_layer = 0;
}

int snapshot_from_layers(Snapshot *s, const LayerStack *stack) {
    int layer_index;
    size_t per_layer;
    size_t total_pixels;

    if (!s || !stack) {
        return 0;
    }

    memset(s, 0, sizeof(*s));
    s->width = stack->width;
    s->height = stack->height;
    s->layer_count = stack->layer_count;
    s->active_layer = stack->active_layer;
    s->solo_index = stack->solo_index;

    per_layer = (size_t)stack->width * (size_t)stack->height;
    total_pixels = per_layer * (size_t)stack->layer_count;
    if (total_pixels > 0) {
        s->pixels = (uint32_t *)malloc(total_pixels * sizeof(uint32_t));
        if (!s->pixels) {
            return 0;
        }
    }

    for (layer_index = 0; layer_index < stack->layer_count; layer_index++) {
        const Layer *layer = &stack->layers[layer_index];
        s->visibility[layer_index] = (uint8_t)layer->visible;
        s->locked[layer_index] = (uint8_t)layer->locked;
        s->opacity_percent[layer_index] = (uint8_t)layer->opacity_percent;
        strncpy(s->names[layer_index], layer->name, LAYER_NAME_MAX - 1);
        s->names[layer_index][LAYER_NAME_MAX - 1] = '\0';
        if (!s->pixels) {
            continue;
        }
        {
            uint32_t *dst = s->pixels + per_layer * (size_t)layer_index;
            if (layer->canvas.pixels) {
                memcpy(dst, layer->canvas.pixels, per_layer * sizeof(uint32_t));
            } else {
                memset(dst, 0, per_layer * sizeof(uint32_t));
            }
        }
    }

    return 1;
}

int snapshot_apply(const Snapshot *s, LayerStack *stack) {
    int layer_index;
    size_t per_layer;

    if (!s || !stack || !s->pixels) {
        return 0;
    }
    if (s->width != stack->width || s->height != stack->height) {
        return 0;
    }
    if (s->layer_count <= 0 || s->layer_count > MAX_LAYERS) {
        return 0;
    }

    while (stack->layer_count < s->layer_count) {
        if (layer_stack_add(stack, NULL, 0x00000000) < 0) {
            return 0;
        }
    }
    stack->layer_count = s->layer_count;

    per_layer = (size_t)stack->width * (size_t)stack->height;
    for (layer_index = 0; layer_index < stack->layer_count; layer_index++) {
        Layer *layer = &stack->layers[layer_index];
        if (!layer->canvas.pixels && !layer_stack_clear_layer(stack, layer_index, 0x00000000)) {
            return 0;
        }
        memcpy(layer->canvas.pixels, s->pixels + per_layer * (size_t)layer_index, per_layer * sizeof(uint32_t));
        layer->visible = s->visibility[layer_index] ? 1 : 0;
        layer->locked = s->locked[layer_index] ? 1 : 0;
        layer->opacity_percent = s->opacity_percent[layer_index];
        strncpy(layer->name, s->names[layer_index], LAYER_NAME_MAX - 1);
        layer->name[LAYER_NAME_MAX - 1] = '\0';
    }

    stack->active_layer = s->active_layer;
    if (stack->active_layer < 0) {
        stack->active_layer = 0;
    }
    if (stack->active_layer >= stack->layer_count) {
        stack->active_layer = stack->layer_count - 1;
    }
    stack->solo_index = s->solo_index;
    if (stack->solo_index >= stack->layer_count) {
        stack->solo_index = -1;
    }
    return 1;
}

void snapshot_stack_clear(Snapshot *stack, int *count) {
    int i;

    if (!stack || !count) {
        return;
    }
    for (i = 0; i < *count; i++) {
        snapshot_free(&stack[i]);
    }
    *count = 0;
}

void snapshot_push(const LayerStack *layers, Snapshot *stack, int *count,
                   Snapshot *redo, int *redo_count, int max_history) {
    Snapshot s = {0};

    if (!layers || !stack || !count || max_history <= 0) {
        return;
    }
    if (*count == max_history) {
        snapshot_free(&stack[0]);
        memmove(&stack[0], &stack[1], sizeof(Snapshot) * (size_t)(max_history - 1));
        *count = max_history - 1;
    }

    if (!snapshot_from_layers(&s, layers)) {
        snapshot_free(&s);
        return;
    }
    stack[(*count)++] = s;
    if (redo && redo_count) {
        snapshot_stack_clear(redo, redo_count);
    }
}

int snapshot_restore(LayerStack *layers,
                     Snapshot *source_stack, int *source_count,
                     Snapshot *target_stack, int *target_count,
                     int max_history) {
    Snapshot current = {0};
    Snapshot restored;

    if (!layers || !source_stack || !source_count || !target_stack || !target_count ||
        *source_count <= 0 || max_history <= 0) {
        return 0;
    }

    if (snapshot_from_layers(&current, layers)) {
        if (*target_count == max_history) {
            snapshot_free(&target_stack[0]);
            memmove(&target_stack[0], &target_stack[1], sizeof(Snapshot) * (size_t)(max_history - 1));
            *target_count = max_history - 1;
        }
        target_stack[(*target_count)++] = current;
    }

    restored = source_stack[--(*source_count)];
    snapshot_apply(&restored, layers);
    snapshot_free(&restored);
    return 1;
}
