#ifndef ACTIVE_LAYER_OPS_H
#define ACTIVE_LAYER_OPS_H

#include "brush_state.h"
#include "snapshot_history.h"

typedef enum {
    ACTIVE_LAYER_ACTION_FAILED = 0,
    ACTIVE_LAYER_ACTION_UNCHANGED,
    ACTIVE_LAYER_ACTION_CHANGED
} ActiveLayerActionResult;

ActiveLayerActionResult active_layer_try_clear_with_result(LayerStack *layers,
                                                           Snapshot *undo_stack, int *undo_count,
                                                           Snapshot *redo_stack, int *redo_count,
                                                           uint32_t background_color, int max_history);
int active_layer_try_clear(LayerStack *layers,
                           Snapshot *undo_stack, int *undo_count,
                           Snapshot *redo_stack, int *redo_count,
                           uint32_t background_color, int max_history);
ActiveLayerActionResult active_layer_try_flip_horizontal_with_result(LayerStack *layers,
                                                                     Snapshot *undo_stack, int *undo_count,
                                                                     Snapshot *redo_stack, int *redo_count,
                                                                     int max_history);
int active_layer_try_flip_horizontal(LayerStack *layers,
                                     Snapshot *undo_stack, int *undo_count,
                                     Snapshot *redo_stack, int *redo_count,
                                     int max_history);
ActiveLayerActionResult active_layer_try_flip_vertical_with_result(LayerStack *layers,
                                                                   Snapshot *undo_stack, int *undo_count,
                                                                   Snapshot *redo_stack, int *redo_count,
                                                                   int max_history);
int active_layer_try_flip_vertical(LayerStack *layers,
                                   Snapshot *undo_stack, int *undo_count,
                                   Snapshot *redo_stack, int *redo_count,
                                   int max_history);
ActiveLayerActionResult active_layer_try_rotate_180_with_result(LayerStack *layers,
                                                                Snapshot *undo_stack, int *undo_count,
                                                                Snapshot *redo_stack, int *redo_count,
                                                                int max_history);
int active_layer_try_rotate_180(LayerStack *layers,
                                Snapshot *undo_stack, int *undo_count,
                                Snapshot *redo_stack, int *redo_count,
                                int max_history);
ActiveLayerActionResult active_layer_try_invert_rgb_with_result(LayerStack *layers,
                                                                Snapshot *undo_stack, int *undo_count,
                                                                Snapshot *redo_stack, int *redo_count,
                                                                int max_history);
int active_layer_try_invert_rgb(LayerStack *layers,
                                Snapshot *undo_stack, int *undo_count,
                                Snapshot *redo_stack, int *redo_count,
                                int max_history);
int active_layer_try_adjust_opacity(LayerStack *layers,
                                    Snapshot *undo_stack, int *undo_count,
                                    Snapshot *redo_stack, int *redo_count,
                                    int target_opacity, int max_history);
ActiveLayerActionResult active_layer_try_adjust_opacity_with_result(LayerStack *layers,
                                                                    Snapshot *undo_stack, int *undo_count,
                                                                    Snapshot *redo_stack, int *redo_count,
                                                                    int target_opacity, int max_history);
int active_layer_try_nudge_opacity(LayerStack *layers,
                                   Snapshot *undo_stack, int *undo_count,
                                   Snapshot *redo_stack, int *redo_count,
                                   int delta_percent, int max_history);
ActiveLayerActionResult active_layer_try_nudge_opacity_with_result(LayerStack *layers,
                                                                   Snapshot *undo_stack, int *undo_count,
                                                                   Snapshot *redo_stack, int *redo_count,
                                                                   int delta_percent, int max_history);
int active_layer_try_flood_fill(LayerStack *layers,
                                Snapshot *undo_stack, int *undo_count,
                                Snapshot *redo_stack, int *redo_count,
                                int x, int y, uint32_t brush_color, int max_history);
int active_layer_try_flood_fill_with_result(LayerStack *layers,
                                            Snapshot *undo_stack, int *undo_count,
                                            Snapshot *redo_stack, int *redo_count,
                                            int x, int y, uint32_t brush_color, int max_history,
                                            int *changed);
int active_layer_try_commit_shape(LayerStack *layers,
                                  Snapshot *undo_stack, int *undo_count,
                                  Snapshot *redo_stack, int *redo_count,
                                  Tool tool, int shape_start_x, int shape_start_y,
                                  int end_x, int end_y, int brush_radius,
                                  uint32_t brush_color, int max_history);
ActiveLayerActionResult active_layer_try_commit_shape_with_result(LayerStack *layers,
                                                                  Snapshot *undo_stack, int *undo_count,
                                                                  Snapshot *redo_stack, int *redo_count,
                                                                  Tool tool, int shape_start_x, int shape_start_y,
                                                                  int end_x, int end_y, int brush_radius,
                                                                  uint32_t brush_color, int max_history);
int active_layer_try_begin_brush_stroke(LayerStack *layers,
                                        Snapshot *undo_stack, int *undo_count,
                                        Snapshot *redo_stack, int *redo_count,
                                        Tool tool, int x, int y, int brush_radius,
                                        uint32_t brush_color, BrushShape brush_shape,
                                        uint32_t background_color, int max_history);
ActiveLayerActionResult active_layer_try_begin_brush_stroke_with_result(LayerStack *layers,
                                                                        Snapshot *undo_stack, int *undo_count,
                                                                        Snapshot *redo_stack, int *redo_count,
                                                                        Tool tool, int x, int y, int brush_radius,
                                                                        uint32_t brush_color, BrushShape brush_shape,
                                                                        uint32_t background_color, int max_history);
int active_layer_continue_brush_stroke(LayerStack *layers,
                                       Tool tool, int x, int y,
                                       int brush_radius, uint32_t brush_color,
                                       BrushShape brush_shape,
                                       int *last_x, int *last_y,
                                       uint32_t background_color);
ActiveLayerActionResult active_layer_continue_brush_stroke_with_result(LayerStack *layers,
                                                                       Tool tool, int x, int y,
                                                                       int brush_radius, uint32_t brush_color,
                                                                       BrushShape brush_shape,
                                                                       int *last_x, int *last_y,
                                                                       uint32_t background_color);
int active_layer_apply_translation(LayerStack *layers,
                                   Snapshot *undo_stack, int *undo_count,
                                   Snapshot *redo_stack, int *redo_count,
                                   int dx, int dy,
                                   uint32_t background_color, int max_history);
ActiveLayerActionResult active_layer_apply_translation_with_result(LayerStack *layers,
                                                                   Snapshot *undo_stack, int *undo_count,
                                                                   Snapshot *redo_stack, int *redo_count,
                                                                   int dx, int dy,
                                                                   uint32_t background_color, int max_history);

#endif
