#ifndef HISTORY_H
#define HISTORY_H

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
} LayerSnapshot;

void layer_snapshot_free(LayerSnapshot *snapshot);
int layer_snapshot_capture(LayerSnapshot *snapshot, const LayerStack *stack);
int layer_snapshot_apply(const LayerSnapshot *snapshot, LayerStack *stack);

#endif
