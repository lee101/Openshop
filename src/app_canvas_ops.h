#ifndef APP_CANVAS_OPS_H
#define APP_CANVAS_OPS_H

#include "canvas.h"
#include "history_state.h"
#include "layers.h"

int app_apply_canvas_transform(
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int history_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    void (*transform)(Canvas *)
);

int app_apply_canvas_translation(
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int history_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int dx,
    int dy
);

#endif
