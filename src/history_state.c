#include "history_state.h"

#include <stdlib.h>
#include <string.h>

static int snapshot_equals(const Snapshot *left, const Snapshot *right) {
    size_t per_layer;
    size_t total_pixels;

    if (!left || !right) {
        return 0;
    }
    if (left->width != right->width ||
        left->height != right->height ||
        left->layer_count != right->layer_count ||
        left->active_layer != right->active_layer ||
        left->solo_index != right->solo_index) {
        return 0;
    }

    for (int layer_index = 0; layer_index < left->layer_count; layer_index++) {
        if (left->visibility[layer_index] != right->visibility[layer_index] ||
            left->locked[layer_index] != right->locked[layer_index] ||
            left->opacity_percent[layer_index] != right->opacity_percent[layer_index] ||
            strcmp(left->names[layer_index], right->names[layer_index]) != 0) {
            return 0;
        }
    }

    per_layer = (size_t)left->width * (size_t)left->height;
    total_pixels = per_layer * (size_t)left->layer_count;
    if (total_pixels == 0) {
        return left->pixels == NULL && right->pixels == NULL;
    }
    if (!left->pixels || !right->pixels) {
        return 0;
    }
    return memcmp(left->pixels, right->pixels, total_pixels * sizeof(uint32_t)) == 0;
}

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
        if (!layer->canvas.pixels && !layer_stack_clear_layer(stack, layer_index, 0x00000000)) {
            return 0;
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
    if (stack->solo_index < 0) {
        stack->solo_index = -1;
    } else if (stack->solo_index >= stack->layer_count) {
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

void snapshot_push(const LayerStack *layers, Snapshot *stack, int *count, int capacity, Snapshot *redo, int *redo_count) {
    Snapshot snapshot = {0};

    if (!layers || !stack || !count || capacity <= 0) {
        return;
    }
    if (!snapshot_from_layers(&snapshot, layers)) {
        snapshot_free(&snapshot);
        return;
    }
    if (*count > 0 && snapshot_equals(&stack[*count - 1], &snapshot)) {
        snapshot_free(&snapshot);
        return;
    }
    if (*count == capacity) {
        snapshot_free(&stack[0]);
        memmove(&stack[0], &stack[1], sizeof(Snapshot) * (size_t)(capacity - 1));
        *count = capacity - 1;
    }

    stack[(*count)++] = snapshot;
    if (redo && redo_count) {
        snapshot_stack_clear(redo, redo_count);
    }
}

void snapshot_push_existing(Snapshot *stack, int *count, int capacity, const Snapshot *snapshot) {
    if (!stack || !count || !snapshot || capacity <= 0) {
        return;
    }
    if (*count == capacity) {
        snapshot_free(&stack[0]);
        memmove(&stack[0], &stack[1], sizeof(Snapshot) * (size_t)(capacity - 1));
        *count = capacity - 1;
    }
    stack[(*count)++] = *snapshot;
}

int snapshot_undo(LayerStack *layers, Snapshot *undo_stack, int *undo_count, int capacity, Snapshot *redo_stack, int *redo_count) {
    Snapshot current = {0};

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || capacity <= 0 || *undo_count <= 0) {
        return 0;
    }

    if (!snapshot_from_layers(&current, layers)) {
        return 0;
    }
    if (!snapshot_apply(&undo_stack[*undo_count - 1], layers)) {
        snapshot_free(&current);
        return 0;
    }

    snapshot_push_existing(redo_stack, redo_count, capacity, &current);
    snapshot_free(&undo_stack[*undo_count - 1]);
    (*undo_count)--;
    return 1;
}

int snapshot_redo(LayerStack *layers, Snapshot *undo_stack, int *undo_count, int capacity, Snapshot *redo_stack, int *redo_count) {
    Snapshot current = {0};

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || capacity <= 0 || *redo_count <= 0) {
        return 0;
    }

    if (!snapshot_from_layers(&current, layers)) {
        return 0;
    }
    if (!snapshot_apply(&redo_stack[*redo_count - 1], layers)) {
        snapshot_free(&current);
        return 0;
    }

    snapshot_push_existing(undo_stack, undo_count, capacity, &current);
    snapshot_free(&redo_stack[*redo_count - 1]);
    (*redo_count)--;
    return 1;
}
