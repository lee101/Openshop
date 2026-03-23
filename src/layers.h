#ifndef LAYERS_H
#define LAYERS_H

#include "canvas.h"
#include <stdint.h>

#define MAX_LAYERS 8
#define LAYER_NAME_MAX 32

typedef struct {
    Canvas canvas;
    int visible;
    int locked;
    int opacity_percent;
    char name[LAYER_NAME_MAX];
} Layer;

typedef struct {
    int width;
    int height;
    int layer_count;
    int active_layer;
    int solo_index;
    Layer layers[MAX_LAYERS];
} LayerStack;

int layer_stack_init(LayerStack *stack, int width, int height, uint32_t background_color);
void layer_stack_free(LayerStack *stack);
Layer *layer_stack_active(LayerStack *stack);
const Layer *layer_stack_get(const LayerStack *stack, int index);
int layer_stack_add(LayerStack *stack, const char *name, uint32_t clear_color);
int layer_stack_insert(LayerStack *stack, int index, const char *name, uint32_t clear_color);
int layer_stack_cycle(LayerStack *stack, int direction);
int layer_stack_cycle_visible(LayerStack *stack, int direction);
int layer_stack_cycle_hidden(LayerStack *stack, int direction);
int layer_stack_cycle_locked(LayerStack *stack, int direction);
int layer_stack_cycle_unlocked(LayerStack *stack, int direction);
int layer_stack_cycle_editable(LayerStack *stack, int direction);
int layer_stack_select_bottom_visible(LayerStack *stack);
int layer_stack_select_top_visible(LayerStack *stack);
int layer_stack_select_bottom_hidden(LayerStack *stack);
int layer_stack_select_top_hidden(LayerStack *stack);
int layer_stack_select_bottom_locked(LayerStack *stack);
int layer_stack_select_top_locked(LayerStack *stack);
int layer_stack_select_bottom_unlocked(LayerStack *stack);
int layer_stack_select_top_unlocked(LayerStack *stack);
int layer_stack_select_bottom_editable(LayerStack *stack);
int layer_stack_select_top_editable(LayerStack *stack);
int layer_stack_toggle_visibility(LayerStack *stack, int index);
int layer_stack_toggle_lock(LayerStack *stack, int index);
int layer_stack_unlock_all(LayerStack *stack);
int layer_stack_show_all(LayerStack *stack);
int layer_stack_show(LayerStack *stack, int index);
int layer_stack_isolate(LayerStack *stack, int index);
int layer_stack_invert_visibility(LayerStack *stack, int preserve_index);
int layer_stack_show_hidden_only(LayerStack *stack, int preserve_index);
int layer_stack_show_locked_only(LayerStack *stack, int preserve_index);
int layer_stack_show_unlocked_only(LayerStack *stack, int preserve_index);
int layer_stack_reveal_hidden(LayerStack *stack, int direction);
int layer_stack_hide_and_advance(LayerStack *stack, int index);
int layer_stack_hide_and_retreat(LayerStack *stack, int index);
int layer_stack_lock_and_advance(LayerStack *stack, int index);
int layer_stack_visible_count(const LayerStack *stack);
void layer_stack_composite(const LayerStack *stack, Canvas *dest, uint32_t background_color);
int layer_stack_clear_layer(LayerStack *stack, int index, uint32_t color);
int layer_stack_set_opacity(LayerStack *stack, int index, int opacity_percent);
int layer_stack_delete(LayerStack *stack, int index);
int layer_stack_duplicate(LayerStack *stack, int index, const char *name);
int layer_stack_move(LayerStack *stack, int index, int direction);
int layer_stack_merge_down(LayerStack *stack, int index);
int layer_stack_merge_up(LayerStack *stack, int index);
int layer_stack_flatten(LayerStack *stack, uint32_t background_color);
int layer_stack_toggle_solo(LayerStack *stack, int index);
int layer_stack_stamp_visible_into(LayerStack *stack, int index, uint32_t background_color);
int layer_stack_stamp_visible_new(LayerStack *stack, const char *name, uint32_t background_color);

#endif
