#include "history.h"

#include <stdlib.h>
#include <string.h>

static void snapshot_drop_top_layer(LayerStack *stack) {
    int last_index = stack->layer_count - 1;
    canvas_free(&stack->layers[last_index].canvas);
    stack->layers[last_index].canvas.width = stack->width;
    stack->layers[last_index].canvas.height = stack->height;
    stack->layers[last_index].canvas.pixels = NULL;
    stack->layers[last_index].visible = 0;
    stack->layers[last_index].locked = 0;
    stack->layers[last_index].opacity_percent = 100;
    stack->layers[last_index].name[0] = '\0';
    stack->layer_count--;
    if (stack->active_layer >= stack->layer_count) {
        stack->active_layer = stack->layer_count - 1;
    }
    if (stack->solo_index >= stack->layer_count) {
        stack->solo_index = -1;
    }
}

static void history_push_existing(LayerSnapshot *stack, int *count, LayerSnapshot *snapshot) {
    if (!stack || !count || !snapshot) {
        return;
    }
    if (*count == HISTORY_CAPACITY) {
        layer_snapshot_free(&stack[0]);
        memmove(&stack[0], &stack[1], sizeof(LayerSnapshot) * (size_t)(HISTORY_CAPACITY - 1));
        *count = HISTORY_CAPACITY - 1;
    }
    stack[(*count)++] = *snapshot;
}

static int layer_snapshot_equals(const LayerSnapshot *a, const LayerSnapshot *b) {
    if (!a || !b) {
        return 0;
    }
    if (a->width != b->width || a->height != b->height || a->layer_count != b->layer_count ||
        a->active_layer != b->active_layer || a->solo_index != b->solo_index) {
        return 0;
    }
    if (memcmp(a->visibility, b->visibility, sizeof(a->visibility)) != 0 ||
        memcmp(a->locked, b->locked, sizeof(a->locked)) != 0 ||
        memcmp(a->opacity_percent, b->opacity_percent, sizeof(a->opacity_percent)) != 0 ||
        memcmp(a->names, b->names, sizeof(a->names)) != 0) {
        return 0;
    }

    size_t total_pixels = (size_t)a->width * (size_t)a->height * (size_t)a->layer_count;
    if (total_pixels == 0) {
        return 1;
    }
    if (!a->pixels || !b->pixels) {
        return a->pixels == b->pixels;
    }
    return memcmp(a->pixels, b->pixels, total_pixels * sizeof(uint32_t)) == 0;
}

void layer_snapshot_free(LayerSnapshot *snapshot) {
    if (!snapshot) {
        return;
    }
    free(snapshot->pixels);
    snapshot->pixels = NULL;
    snapshot->width = 0;
    snapshot->height = 0;
    snapshot->layer_count = 0;
    snapshot->active_layer = 0;
    snapshot->solo_index = -1;
}

int layer_snapshot_capture(LayerSnapshot *snapshot, const LayerStack *stack) {
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

int layer_snapshot_apply(const LayerSnapshot *snapshot, LayerStack *stack) {
    if (!snapshot || !stack || !snapshot->pixels) {
        return 0;
    }
    if (snapshot->width != stack->width || snapshot->height != stack->height) {
        return 0;
    }
    if (snapshot->layer_count <= 0 || snapshot->layer_count > MAX_LAYERS) {
        return 0;
    }

    while (stack->layer_count > snapshot->layer_count) {
        snapshot_drop_top_layer(stack);
    }
    while (stack->layer_count < snapshot->layer_count) {
        if (layer_stack_add(stack, NULL, 0x00000000) < 0) {
            return 0;
        }
    }

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
    if (stack->solo_index >= stack->layer_count) {
        stack->solo_index = -1;
    }
    return 1;
}

void layer_history_clear(LayerSnapshot *stack, int *count) {
    if (!stack || !count) {
        return;
    }
    for (int i = 0; i < *count; i++) {
        layer_snapshot_free(&stack[i]);
    }
    *count = 0;
}

void layer_history_push(const LayerStack *layers, LayerSnapshot *stack, int *count, LayerSnapshot *redo, int *redo_count) {
    if (!layers || !stack || !count) {
        return;
    }

    LayerSnapshot snapshot = {0};
    if (!layer_snapshot_capture(&snapshot, layers)) {
        layer_snapshot_free(&snapshot);
        return;
    }

    if (*count > 0 && layer_snapshot_equals(&stack[*count - 1], &snapshot)) {
        layer_snapshot_free(&snapshot);
        return;
    }

    history_push_existing(stack, count, &snapshot);
    if (redo && redo_count) {
        layer_history_clear(redo, redo_count);
    }
}

int layer_history_undo(LayerStack *layers, LayerSnapshot *undo_stack, int *undo_count, LayerSnapshot *redo_stack, int *redo_count) {
    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || *undo_count <= 0) {
        return 0;
    }

    LayerSnapshot current = {0};
    if (layer_snapshot_capture(&current, layers)) {
        history_push_existing(redo_stack, redo_count, &current);
    }

    LayerSnapshot previous = undo_stack[--(*undo_count)];
    int ok = layer_snapshot_apply(&previous, layers);
    layer_snapshot_free(&previous);
    return ok;
}

int layer_history_redo(LayerStack *layers, LayerSnapshot *undo_stack, int *undo_count, LayerSnapshot *redo_stack, int *redo_count) {
    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || *redo_count <= 0) {
        return 0;
    }

    LayerSnapshot current = {0};
    if (layer_snapshot_capture(&current, layers)) {
        history_push_existing(undo_stack, undo_count, &current);
    }

    LayerSnapshot next = redo_stack[--(*redo_count)];
    int ok = layer_snapshot_apply(&next, layers);
    layer_snapshot_free(&next);
    return ok;
}

void layer_history_reset(LayerHistory *history) {
    if (!history) {
        return;
    }
    layer_history_clear(history->undo, &history->undo_count);
    layer_history_clear(history->redo, &history->redo_count);
}

void layer_history_record(LayerHistory *history, const LayerStack *layers) {
    if (!history) {
        return;
    }
    layer_history_push(layers, history->undo, &history->undo_count, history->redo, &history->redo_count);
}

int layer_history_record_snapshot(LayerHistory *history, LayerSnapshot *snapshot) {
    if (!snapshot) {
        return 0;
    }
    if (!history) {
        layer_snapshot_free(snapshot);
        return 0;
    }
    if (history->undo_count > 0 && layer_snapshot_equals(&history->undo[history->undo_count - 1], snapshot)) {
        layer_snapshot_free(snapshot);
        return 0;
    }
    history_push_existing(history->undo, &history->undo_count, snapshot);
    layer_history_clear(history->redo, &history->redo_count);
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->solo_index = -1;
    return 1;
}

int layer_history_step_undo(LayerHistory *history, LayerStack *layers) {
    if (!history) {
        return 0;
    }
    return layer_history_undo(layers, history->undo, &history->undo_count, history->redo, &history->redo_count);
}

int layer_history_step_redo(LayerHistory *history, LayerStack *layers) {
    if (!history) {
        return 0;
    }
    return layer_history_redo(layers, history->undo, &history->undo_count, history->redo, &history->redo_count);
}
