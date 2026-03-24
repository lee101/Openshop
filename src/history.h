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

// Frees pixel storage and clears scalar bookkeeping, but does not scrub metadata arrays.
void layer_snapshot_free(LayerSnapshot *snapshot);
// Fully resets a snapshot, including owned storage and metadata arrays.
void layer_snapshot_reset(LayerSnapshot *snapshot);
// Captures the current layer stack into caller-owned snapshot storage.
int layer_snapshot_capture(LayerSnapshot *snapshot, const LayerStack *stack);
int layer_snapshot_apply(const LayerSnapshot *snapshot, LayerStack *stack);
int layer_snapshot_matches_stack(const LayerSnapshot *snapshot, const LayerStack *stack);
// Low-level stack primitives kept for targeted tests and incremental callers.
void layer_history_clear(LayerSnapshot *stack, int *count);
void layer_history_push(const LayerStack *layers, LayerSnapshot *stack, int *count, LayerSnapshot *redo, int *redo_count);
int layer_history_undo(LayerStack *layers, LayerSnapshot *undo_stack, int *undo_count, LayerSnapshot *redo_stack, int *redo_count);
int layer_history_redo(LayerStack *layers, LayerSnapshot *undo_stack, int *undo_count, LayerSnapshot *redo_stack, int *redo_count);
// Preferred API for app integration: keep undo/redo state inside LayerHistory.
void layer_history_reset(LayerHistory *history);
void layer_history_record(LayerHistory *history, const LayerStack *layers);
// On success this transfers snapshot ownership into history and disowns the caller snapshot.
// On discard/failure it resets the caller snapshot back to an empty state.
int layer_history_record_snapshot(LayerHistory *history, LayerSnapshot *snapshot);
// Commits a previously captured snapshot only when the operation succeeded and changed the stack.
// Regardless of outcome, the caller snapshot is either transferred into history or reset in place.
int layer_history_commit_change(LayerHistory *history, LayerSnapshot *snapshot, const LayerStack *layers, int operation_succeeded);
int layer_history_step_undo(LayerHistory *history, LayerStack *layers);
int layer_history_step_redo(LayerHistory *history, LayerStack *layers);

#endif
