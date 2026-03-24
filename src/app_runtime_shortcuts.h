#ifndef APP_RUNTIME_SHORTCUTS_H
#define APP_RUNTIME_SHORTCUTS_H

#include "canvas_shortcuts.h"
#include "history_shortcuts.h"
#include "history_state.h"
#include "layers.h"

int app_handle_history_navigation_shortcut(
    int key,
    int ctrl,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
);

int app_handle_canvas_mutation_shortcut(
    CanvasShortcutAction canvas_action,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
);

#endif
