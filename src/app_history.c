#include "app_history.h"

#include <stdlib.h>
#include <string.h>

void snapshot_free(Snapshot *snapshot) {
    if (!snapshot) {
        return;
    }
    free(snapshot->pixels);
    snapshot->pixels = NULL;
    snapshot->width = 0;
    snapshot->height = 0;
    snapshot->layer_count = 0;
    snapshot->active_layer = 0;
}

int snapshot_from_layers(Snapshot *snapshot, const LayerStack *stack) {
    if (!snapshot || !stack) {
        return 0;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->width = stack->width;
    snapshot->height = stack->height;
    snapshot->layer_count = stack->layer_count;
    snapshot->active_layer = stack->active_layer;
    snapshot->solo_index = stack->solo_index;

    size_t per_layer = (size_t)stack->width * (size_t)stack->height;
    size_t total_pixels = per_layer * (size_t)stack->layer_count;
    if (total_pixels > 0) {
        snapshot->pixels = (uint32_t *)malloc(total_pixels * sizeof(uint32_t));
        if (!snapshot->pixels) {
            return 0;
        }
    }

    for (int layer_index = 0; layer_index < stack->layer_count; layer_index++) {
        const Layer *layer = &stack->layers[layer_index];
        snapshot->visibility[layer_index] = (uint8_t)layer->visible;
        snapshot->locked[layer_index] = (uint8_t)layer->locked;
        snapshot->opacity_percent[layer_index] = (uint8_t)layer->opacity_percent;
        strncpy(snapshot->names[layer_index], layer->name, LAYER_NAME_MAX - 1);
        snapshot->names[layer_index][LAYER_NAME_MAX - 1] = '\0';
        if (!snapshot->pixels) {
            continue;
        }
        uint32_t *dst = snapshot->pixels + per_layer * (size_t)layer_index;
        if (layer->canvas.pixels) {
            memcpy(dst, layer->canvas.pixels, per_layer * sizeof(uint32_t));
        } else {
            memset(dst, 0, per_layer * sizeof(uint32_t));
        }
    }

    return 1;
}

int snapshot_apply(const Snapshot *snapshot, LayerStack *stack) {
    if (!snapshot || !stack || !snapshot->pixels) {
        return 0;
    }
    if (snapshot->width != stack->width || snapshot->height != stack->height) {
        return 0;
    }
    if (snapshot->layer_count <= 0 || snapshot->layer_count > MAX_LAYERS) {
        return 0;
    }

    while (stack->layer_count < snapshot->layer_count) {
        if (layer_stack_add(stack, NULL, 0x00000000) < 0) {
            return 0;
        }
    }
    stack->layer_count = snapshot->layer_count;

    size_t per_layer = (size_t)stack->width * (size_t)stack->height;
    for (int layer_index = 0; layer_index < stack->layer_count; layer_index++) {
        Layer *layer = &stack->layers[layer_index];
        if (!layer->canvas.pixels) {
            layer->canvas.width = stack->width;
            layer->canvas.height = stack->height;
            if (!canvas_init(&layer->canvas, stack->width, stack->height)) {
                return 0;
            }
        }
        memcpy(layer->canvas.pixels, snapshot->pixels + per_layer * (size_t)layer_index, per_layer * sizeof(uint32_t));
        layer->visible = snapshot->visibility[layer_index] ? 1 : 0;
        layer->locked = snapshot->locked[layer_index] ? 1 : 0;
        layer->opacity_percent = snapshot->opacity_percent[layer_index];
        strncpy(layer->name, snapshot->names[layer_index], LAYER_NAME_MAX - 1);
        layer->name[LAYER_NAME_MAX - 1] = '\0';
    }

    stack->active_layer = snapshot->active_layer;
    if (stack->active_layer < 0) {
        stack->active_layer = 0;
    }
    if (stack->active_layer >= stack->layer_count) {
        stack->active_layer = stack->layer_count - 1;
    }
    stack->solo_index = snapshot->solo_index;
    if (stack->solo_index >= stack->layer_count) {
        stack->solo_index = -1;
    }
    return 1;
}

void snapshot_stack_clear(Snapshot *stack, int *count) {
    if (!stack || !count) {
        return;
    }
    for (int i = 0; i < *count; i++) {
        snapshot_free(&stack[i]);
    }
    *count = 0;
}

void snapshot_push(const LayerStack *layers, Snapshot *stack, int *count, Snapshot *redo, int *redo_count) {
    if (!layers || !stack || !count) {
        return;
    }
    if (*count == MAX_HISTORY) {
        snapshot_free(&stack[0]);
        memmove(&stack[0], &stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
        *count = MAX_HISTORY - 1;
    }

    Snapshot snapshot = {0};
    if (!snapshot_from_layers(&snapshot, layers)) {
        snapshot_free(&snapshot);
        return;
    }
    stack[(*count)++] = snapshot;
    if (redo && redo_count) {
        snapshot_stack_clear(redo, redo_count);
    }
}

int snapshot_restore(LayerStack *layers, Snapshot *from_stack, int *from_count, Snapshot *to_stack, int *to_count) {
    if (!layers || !from_stack || !from_count || !to_stack || !to_count || *from_count <= 0) {
        return 0;
    }

    int target_index = *to_count;
    if (target_index == MAX_HISTORY) {
        snapshot_free(&to_stack[0]);
        memmove(&to_stack[0], &to_stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
        target_index = MAX_HISTORY - 1;
    }

    Snapshot current = {0};
    if (!snapshot_from_layers(&current, layers)) {
        snapshot_free(&current);
        return 0;
    }

    int source_index = *from_count - 1;
    Snapshot *restored = &from_stack[source_index];
    if (!snapshot_apply(restored, layers)) {
        snapshot_free(&current);
        return 0;
    }

    to_stack[target_index] = current;
    if (*to_count < MAX_HISTORY) {
        (*to_count)++;
    }
    snapshot_free(restored);
    *from_count = source_index;
    return 1;
}
