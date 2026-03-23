#ifndef APP_HISTORY_H
#define APP_HISTORY_H

#include <stddef.h>
#include "layers.h"

#define MAX_HISTORY 20

typedef struct {
    int width;
    int height;
    int layer_count;
    int active_layer;
    int solo_index;
    uint8_t visibility[MAX_LAYERS];
    uint8_t locked[MAX_LAYERS];
    uint8_t opacity_percent[MAX_LAYERS];
    char names[MAX_LAYERS][LAYER_NAME_MAX];
    uint32_t *pixels;
} Snapshot;

typedef void *(*AppHistoryMallocFn)(size_t size);
typedef void (*AppHistoryFreeFn)(void *ptr);
typedef int (*AppHistoryCanvasInitFn)(Canvas *canvas, int width, int height);

void snapshot_free(Snapshot *snapshot);
int snapshot_from_layers(Snapshot *snapshot, const LayerStack *stack);
int snapshot_apply(const Snapshot *snapshot, LayerStack *stack);
void snapshot_stack_clear(Snapshot *stack, int *count);
void snapshot_push(const LayerStack *layers, Snapshot *stack, int *count, Snapshot *redo, int *redo_count);
int snapshot_restore(LayerStack *layers, Snapshot *from_stack, int *from_count, Snapshot *to_stack, int *to_count);
void app_history_set_allocators(AppHistoryMallocFn malloc_fn, AppHistoryFreeFn free_fn);
void app_history_set_canvas_init(AppHistoryCanvasInitFn canvas_init_fn);

#endif
