#ifndef HISTORY_H
#define HISTORY_H

#include "layers.h"
#include <stdint.h>

#define HISTORY_CAPACITY 20

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
} LayerSnapshot;

typedef struct {
    LayerSnapshot undo[HISTORY_CAPACITY];
    LayerSnapshot redo[HISTORY_CAPACITY];
    int undo_count;
    int redo_count;
} LayerHistory;

void layer_snapshot_free(LayerSnapshot *snapshot);
int layer_snapshot_capture(LayerSnapshot *snapshot, const LayerStack *stack);
int layer_snapshot_apply(const LayerSnapshot *snapshot, LayerStack *stack);
void layer_history_clear(LayerSnapshot *stack, int *count);
void layer_history_push(const LayerStack *layers, LayerSnapshot *stack, int *count, LayerSnapshot *redo, int *redo_count);
int layer_history_undo(LayerStack *layers, LayerSnapshot *undo_stack, int *undo_count, LayerSnapshot *redo_stack, int *redo_count);
int layer_history_redo(LayerStack *layers, LayerSnapshot *undo_stack, int *undo_count, LayerSnapshot *redo_stack, int *redo_count);
void layer_history_reset(LayerHistory *history);
void layer_history_record(LayerHistory *history, const LayerStack *layers);
int layer_history_step_undo(LayerHistory *history, LayerStack *layers);
int layer_history_step_redo(LayerHistory *history, LayerStack *layers);

#endif
