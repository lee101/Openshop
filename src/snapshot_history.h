#ifndef SNAPSHOT_HISTORY_H
#define SNAPSHOT_HISTORY_H

#include "layers.h"

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

void snapshot_free(Snapshot *s);
int snapshot_from_layers(Snapshot *s, const LayerStack *stack);
int snapshot_apply(const Snapshot *s, LayerStack *stack);
void snapshot_stack_clear(Snapshot *stack, int *count);
void snapshot_push(const LayerStack *layers, Snapshot *stack, int *count,
                   Snapshot *redo, int *redo_count, int max_history);
int snapshot_restore(LayerStack *layers,
                     Snapshot *source_stack, int *source_count,
                     Snapshot *target_stack, int *target_count,
                     int max_history);

#endif
