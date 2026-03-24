#include "history.h"

#include <stdlib.h>
#include <string.h>

static int layer_snapshot_equals(const LayerSnapshot *a, const LayerSnapshot *b);

static void *layer_snapshot_alloc_default(size_t size) {
    return malloc(size);
}

static void *(*layer_snapshot_alloc_pixels)(size_t size) = layer_snapshot_alloc_default;

#ifdef OPENSHOP_TESTING
void layer_snapshot_set_alloc_for_tests(layer_snapshot_alloc_fn alloc_fn) {
    layer_snapshot_alloc_pixels = alloc_fn ? alloc_fn : layer_snapshot_alloc_default;
}
#endif

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

static int history_capture_and_push(
    const LayerStack *layers,
    LayerSnapshot *stack,
    int *count,
    LayerSnapshot *other_stack,
    int *other_count
) {
    LayerSnapshot snapshot = {0};
    if (!layers || !stack || !count) {
        return 0;
    }
    if (!layer_snapshot_capture(&snapshot, layers)) {
        layer_snapshot_free(&snapshot);
        return 0;
    }
    if (*count > 0 && layer_snapshot_equals(&stack[*count - 1], &snapshot)) {
        layer_snapshot_reset(&snapshot);
        return 0;
    }
    history_push_existing(stack, count, &snapshot);
    if (other_stack && other_count) {
        layer_history_clear(other_stack, other_count);
    }
    return 1;
}

static int history_step_apply(
    LayerStack *layers,
    LayerSnapshot *from_stack,
    int *from_count,
    LayerSnapshot *to_stack,
    int *to_count
) {
    int pushed_current = 0;
    LayerSnapshot snapshot = {0};
    int ok = 0;

    if (!layers || !from_stack || !from_count || !to_stack || !to_count || *from_count <= 0) {
        return 0;
    }

    pushed_current = history_capture_and_push(layers, to_stack, to_count, NULL, NULL);
    snapshot = from_stack[--(*from_count)];
    ok = layer_snapshot_apply(&snapshot, layers);
    if (!ok) {
        from_stack[(*from_count)++] = snapshot;
        if (pushed_current) {
            layer_snapshot_free(&to_stack[--(*to_count)]);
        }
        return 0;
    }

    layer_snapshot_free(&snapshot);
    return 1;
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

static void layer_snapshot_disown(LayerSnapshot *snapshot) {
    if (!snapshot) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->solo_index = -1;
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

void layer_snapshot_reset(LayerSnapshot *snapshot) {
    if (!snapshot) {
        return;
    }
    layer_snapshot_free(snapshot);
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->solo_index = -1;
}

int layer_snapshot_capture(LayerSnapshot *snapshot, const LayerStack *stack) {
    if (!snapshot || !stack) {
        return 0;
    }

    layer_snapshot_reset(snapshot);
    snapshot->width = stack->width;
    snapshot->height = stack->height;
    snapshot->layer_count = stack->layer_count;
    snapshot->active_layer = stack->active_layer;
    snapshot->solo_index = stack->solo_index;

    size_t per_layer = (size_t)stack->width * (size_t)stack->height;
    size_t total_pixels = per_layer * (size_t)stack->layer_count;
    if (total_pixels > 0) {
        snapshot->pixels = (uint32_t *)layer_snapshot_alloc_pixels(total_pixels * sizeof(uint32_t));
        if (!snapshot->pixels) {
            layer_snapshot_reset(snapshot);
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
    if (stack->solo_index < 0 || stack->solo_index >= stack->layer_count) {
        stack->solo_index = -1;
    }
    return 1;
}

int layer_snapshot_matches_stack(const LayerSnapshot *snapshot, const LayerStack *stack) {
    LayerSnapshot current = {0};
    int equal = 0;
    if (!snapshot || !stack) {
        return 0;
    }
    if (!layer_snapshot_capture(&current, stack)) {
        return 0;
    }
    equal = layer_snapshot_equals(snapshot, &current);
    layer_snapshot_free(&current);
    return equal;
}

void layer_history_clear(LayerSnapshot *stack, int *count) {
    if (!stack || !count) {
        return;
    }
    for (int i = 0; i < *count; i++) {
        layer_snapshot_reset(&stack[i]);
    }
    *count = 0;
}

void layer_history_push(const LayerStack *layers, LayerSnapshot *stack, int *count, LayerSnapshot *redo, int *redo_count) {
    history_capture_and_push(layers, stack, count, redo, redo_count);
}

int layer_history_undo(LayerStack *layers, LayerSnapshot *undo_stack, int *undo_count, LayerSnapshot *redo_stack, int *redo_count) {
    return history_step_apply(layers, undo_stack, undo_count, redo_stack, redo_count);
}

int layer_history_redo(LayerStack *layers, LayerSnapshot *undo_stack, int *undo_count, LayerSnapshot *redo_stack, int *redo_count) {
    return history_step_apply(layers, redo_stack, redo_count, undo_stack, undo_count);
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
        layer_snapshot_reset(snapshot);
        return 0;
    }
    if (history->undo_count > 0 && layer_snapshot_equals(&history->undo[history->undo_count - 1], snapshot)) {
        layer_snapshot_reset(snapshot);
        return 0;
    }
    history_push_existing(history->undo, &history->undo_count, snapshot);
    layer_history_clear(history->redo, &history->redo_count);
    layer_snapshot_disown(snapshot);
    return 1;
}

int layer_history_commit_change(LayerHistory *history, LayerSnapshot *snapshot, const LayerStack *layers, int operation_succeeded) {
    if (!snapshot) {
        return 0;
    }
    if (!operation_succeeded || !layers || layer_snapshot_matches_stack(snapshot, layers)) {
        layer_snapshot_reset(snapshot);
        return 0;
    }
    if (!history) {
        layer_snapshot_reset(snapshot);
        return 0;
    }
    if (!layer_history_record_snapshot(history, snapshot)) {
        return 0;
    }
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
