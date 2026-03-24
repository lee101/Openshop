#ifndef HISTORY_STATE_H
#define HISTORY_STATE_H

#include "layers.h"
#include <stdint.h>

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

void snapshot_free(Snapshot *snapshot);
int snapshot_from_layers(Snapshot *snapshot, const LayerStack *stack);
int snapshot_apply(const Snapshot *snapshot, LayerStack *stack);
void snapshot_stack_clear(Snapshot *stack, int *count);
void snapshot_push(const LayerStack *layers, Snapshot *stack, int *count, int capacity, Snapshot *redo, int *redo_count);
void snapshot_push_existing(Snapshot *stack, int *count, int capacity, const Snapshot *snapshot);
int snapshot_undo(LayerStack *layers, Snapshot *undo_stack, int *undo_count, int capacity, Snapshot *redo_stack, int *redo_count);
int snapshot_redo(LayerStack *layers, Snapshot *undo_stack, int *undo_count, int capacity, Snapshot *redo_stack, int *redo_count);

#endif
