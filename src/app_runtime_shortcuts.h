#ifndef APP_RUNTIME_SHORTCUTS_H
#define APP_RUNTIME_SHORTCUTS_H

#include "app_brush.h"
#include "brush_shortcuts.h"
#include "canvas_shortcuts.h"
#include "history_shortcuts.h"
#include "history_state.h"
#include "layers.h"
#include "paint_shortcuts.h"
#include "view_shortcuts.h"
#include <stdint.h>

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

int app_handle_brush_and_paint_shortcut(
    PaintShortcutAction paint_action,
    BrushShortcutAction brush_action,
    Tool *tool,
    BrushShape *brush_shape,
    int *brush_radius,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity
);

int app_handle_layer_opacity_reset_shortcut(
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

int app_handle_layer_visibility_shortcut(
    int key,
    int ctrl,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
);

int app_handle_active_layer_opacity_step(
    LayerStack *layers,
    int delta,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
);

int app_handle_active_layer_reorder_shortcut(
    int key,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
);

int app_handle_active_layer_duplicate_shortcut(
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

int app_handle_canvas_sample_shortcut_at(
    CanvasShortcutAction canvas_action,
    LayerStack *layers,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_active,
    int x,
    int y,
    int canvas_width,
    int canvas_height,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    Tool *tool,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity,
    int *needs_composite
);

int app_handle_view_shortcut(
    ViewShortcutResult view_result,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    int undo_capacity,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
);

#endif
