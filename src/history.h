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

// Frees pixel storage and clears scalar bookkeeping, but preserves metadata arrays for callers that inspect or reuse them.
// Null inputs are ignored.
void layer_snapshot_free(LayerSnapshot *snapshot);
// Fully resets a snapshot, including owned storage and scrubbing metadata arrays that `layer_snapshot_free` preserves.
// Null inputs are ignored.
void layer_snapshot_reset(LayerSnapshot *snapshot);
// Captures the current layer stack into caller-owned snapshot storage.
// Null inputs fail without mutating the caller snapshot.
// Successful captures overwrite any existing snapshot contents.
// Other failures leave the caller snapshot reset to an empty state.
int layer_snapshot_capture(LayerSnapshot *snapshot, const LayerStack *stack);
// Applies a populated snapshot back onto a same-sized stack; null inputs, missing pixels, and invalid layer counts fail.
// All rejected snapshots, including early null/empty failures, leave the destination stack unchanged.
// Applied active/solo indices are clamped back into the destination stack's valid range.
int layer_snapshot_apply(const LayerSnapshot *snapshot, LayerStack *stack);
// Compares a snapshot to the current stack; null inputs or failed current-state capture return false.
// Failed comparisons leave the caller snapshot unchanged.
int layer_snapshot_matches_stack(const LayerSnapshot *snapshot, const LayerStack *stack);
// Low-level stack primitives kept for targeted tests and incremental callers.
// Fully resets each populated entry in `stack` and zeroes `count`; null inputs are ignored.
void layer_history_clear(LayerSnapshot *stack, int *count);
// Captures the current stack into `stack` when `layers`, `stack`, and `count` are valid.
// `redo`/`redo_count` are optional; when both are provided they are cleared only after a new snapshot is pushed.
void layer_history_push(const LayerStack *layers, LayerSnapshot *stack, int *count, LayerSnapshot *redo, int *redo_count);
// On success these move the current stack state to the opposite history stack and apply the pending snapshot.
// On failure they leave both history stacks unchanged.
int layer_history_undo(LayerStack *layers, LayerSnapshot *undo_stack, int *undo_count, LayerSnapshot *redo_stack, int *redo_count);
int layer_history_redo(LayerStack *layers, LayerSnapshot *undo_stack, int *undo_count, LayerSnapshot *redo_stack, int *redo_count);
// Preferred API for app integration: keep undo/redo state inside LayerHistory.
void layer_history_reset(LayerHistory *history);
// Records the current stack into history when both `history` and `layers` are valid; otherwise this is a no-op.
void layer_history_record(LayerHistory *history, const LayerStack *layers);
// On success this transfers snapshot ownership into history and disowns the caller snapshot.
// A null snapshot fails cleanly; other discard/failure paths reset the caller snapshot back to an empty state.
int layer_history_record_snapshot(LayerHistory *history, LayerSnapshot *snapshot);
// Commits a previously captured snapshot only when the operation succeeded and changed the stack.
// A null snapshot fails cleanly; otherwise the caller snapshot is either transferred into history or reset in place.
int layer_history_commit_change(LayerHistory *history, LayerSnapshot *snapshot, const LayerStack *layers, int operation_succeeded);
// Wrapper versions of undo/redo that preserve history state if the stored snapshot cannot be applied.
int layer_history_step_undo(LayerHistory *history, LayerStack *layers);
int layer_history_step_redo(LayerHistory *history, LayerStack *layers);

#endif
