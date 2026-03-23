#include "app_history.h"

#include <stdlib.h>
#include <string.h>

static void *app_history_default_malloc(size_t size) {
    return malloc(size);
}

static void app_history_default_free(void *ptr) {
    free(ptr);
}

static int app_history_default_canvas_init(Canvas *canvas, int width, int height) {
    return canvas_init(canvas, width, height);
}

static int app_history_default_layer_add(LayerStack *stack, const char *name, uint32_t clear_color) {
    return layer_stack_add(stack, name, clear_color);
}

static AppHistoryMallocFn app_history_malloc_fn = app_history_default_malloc;
static AppHistoryFreeFn app_history_free_fn = app_history_default_free;
static AppHistoryCanvasInitFn app_history_canvas_init_fn = app_history_default_canvas_init;
static AppHistoryLayerAddFn app_history_layer_add_fn = app_history_default_layer_add;

void app_history_set_allocators(AppHistoryMallocFn malloc_fn, AppHistoryFreeFn free_fn) {
    app_history_malloc_fn = malloc_fn ? malloc_fn : app_history_default_malloc;
    app_history_free_fn = free_fn ? free_fn : app_history_default_free;
}

void app_history_set_canvas_init(AppHistoryCanvasInitFn canvas_init_fn) {
    app_history_canvas_init_fn = canvas_init_fn ? canvas_init_fn : app_history_default_canvas_init;
}

void app_history_set_layer_add(AppHistoryLayerAddFn layer_add_fn) {
    app_history_layer_add_fn = layer_add_fn ? layer_add_fn : app_history_default_layer_add;
}

void snapshot_free(Snapshot *snapshot) {
    if (!snapshot) {
        return;
    }
    app_history_free_fn(snapshot->pixels);
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
        snapshot->pixels = (uint32_t *)app_history_malloc_fn(total_pixels * sizeof(uint32_t));
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

    int original_layer_count = stack->layer_count;
    int original_active_layer = stack->active_layer;
    int original_solo_index = stack->solo_index;

    while (stack->layer_count < snapshot->layer_count) {
        if (app_history_layer_add_fn(stack, NULL, 0x00000000) < 0) {
            while (stack->layer_count > original_layer_count) {
                stack->layer_count--;
                canvas_free(&stack->layers[stack->layer_count].canvas);
                stack->layers[stack->layer_count].visible = 0;
                stack->layers[stack->layer_count].locked = 0;
                stack->layers[stack->layer_count].opacity_percent = 100;
                stack->layers[stack->layer_count].name[0] = '\0';
            }
            stack->active_layer = original_active_layer;
            stack->solo_index = original_solo_index;
            return 0;
        }
    }
    stack->layer_count = snapshot->layer_count;

    for (int layer_index = 0; layer_index < stack->layer_count; layer_index++) {
        Layer *layer = &stack->layers[layer_index];
        if (!layer->canvas.pixels) {
            if (!app_history_canvas_init_fn(&layer->canvas, stack->width, stack->height)) {
                return 0;
            }
        }
    }

    size_t per_layer = (size_t)stack->width * (size_t)stack->height;
    for (int layer_index = 0; layer_index < stack->layer_count; layer_index++) {
        Layer *layer = &stack->layers[layer_index];
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

    Snapshot snapshot = {0};
    if (!snapshot_from_layers(&snapshot, layers)) {
        snapshot_free(&snapshot);
        return;
    }

    int target_index = *count;
    if (target_index == MAX_HISTORY) {
        snapshot_free(&stack[0]);
        memmove(&stack[0], &stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
        target_index = MAX_HISTORY - 1;
    }
    stack[target_index] = snapshot;
    if (*count < MAX_HISTORY) {
        (*count)++;
    }
    if (redo && redo_count) {
        snapshot_stack_clear(redo, redo_count);
    }
}

int snapshot_restore(LayerStack *layers, Snapshot *from_stack, int *from_count, Snapshot *to_stack, int *to_count) {
    if (!layers || !from_stack || !from_count || !to_stack || !to_count || *from_count <= 0) {
        return 0;
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

    int target_index = *to_count;
    if (target_index == MAX_HISTORY) {
        snapshot_free(&to_stack[0]);
        memmove(&to_stack[0], &to_stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
        target_index = MAX_HISTORY - 1;
    }
    to_stack[target_index] = current;
    if (*to_count < MAX_HISTORY) {
        (*to_count)++;
    }
    snapshot_free(restored);
    *from_count = source_index;
    return 1;
}
