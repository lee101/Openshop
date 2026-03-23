#include "layers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int layer_stack_cycle_bool_field(LayerStack *stack, int direction, int want_value, int use_locked);
static int layer_stack_select_bool_edge(LayerStack *stack, int want_value, int from_top, int use_locked);
static int layer_stack_matches_bool_filter(int value, int required_value);

typedef enum {
    EDITABLE_ANY_VISIBILITY = 0,
    EDITABLE_VISIBLE_ONLY,
    EDITABLE_HIDDEN_ONLY
} EditableVisibilityMode;

static uint8_t blend_channel(uint8_t src, uint8_t dst, uint8_t src_alpha) {
    int inv = 255 - src_alpha;
    int value = src * src_alpha + dst * inv + 127;
    return (uint8_t)(value / 255);
}

static uint32_t blend_pixel(uint32_t dst, uint32_t src) {
    uint8_t sa = (uint8_t)((src >> 24) & 0xFF);
    if (sa == 0) {
        return dst;
    }
    if (sa == 255) {
        return src;
    }

    uint8_t da = (uint8_t)((dst >> 24) & 0xFF);
    uint8_t sr = (uint8_t)((src >> 16) & 0xFF);
    uint8_t sg = (uint8_t)((src >> 8) & 0xFF);
    uint8_t sb = (uint8_t)(src & 0xFF);
    uint8_t dr = (uint8_t)((dst >> 16) & 0xFF);
    uint8_t dg = (uint8_t)((dst >> 8) & 0xFF);
    uint8_t db = (uint8_t)(dst & 0xFF);

    uint8_t out_r = blend_channel(sr, dr, sa);
    uint8_t out_g = blend_channel(sg, dg, sa);
    uint8_t out_b = blend_channel(sb, db, sa);
    uint8_t out_a = (uint8_t)(sa + ((da * (255 - sa) + 127) / 255));

    return ((uint32_t)out_a << 24) | ((uint32_t)out_r << 16) | ((uint32_t)out_g << 8) | out_b;
}

static uint32_t apply_layer_opacity(uint32_t src, int opacity_percent) {
    if (opacity_percent >= 100) {
        return src;
    }
    if (opacity_percent <= 0) {
        return 0;
    }
    uint32_t alpha = (src >> 24) & 0xFF;
    alpha = (uint32_t)((alpha * opacity_percent + 50) / 100);
    return (src & 0x00FFFFFF) | (alpha << 24);
}

static int ensure_layer_canvas(Layer *layer, int width, int height) {
    if (!layer) {
        return 0;
    }
    if (layer->canvas.pixels) {
        if (layer->canvas.width == width && layer->canvas.height == height) {
            return 1;
        }
        canvas_free(&layer->canvas);
    }
    return canvas_init(&layer->canvas, width, height);
}

static int layer_stack_cycle_filtered(LayerStack *stack, int direction, int want_visible) {
    return layer_stack_cycle_bool_field(stack, direction, want_visible, 0);
}

static int layer_stack_select_edge_filtered(LayerStack *stack, int want_visible, int from_top) {
    return layer_stack_select_bool_edge(stack, want_visible, from_top, 0);
}

static int layer_stack_cycle_combined_filter(LayerStack *stack, int direction, int required_hidden, int required_locked) {
    if (!stack || stack->layer_count <= 0) {
        return -1;
    }
    for (int offset = 1; offset <= stack->layer_count; offset++) {
        int idx = stack->active_layer + (direction * offset);
        while (idx < 0) {
            idx += stack->layer_count;
        }
        idx %= stack->layer_count;
        int is_hidden = stack->layers[idx].visible ? 0 : 1;
        if (layer_stack_matches_bool_filter(is_hidden, required_hidden) &&
            layer_stack_matches_bool_filter(stack->layers[idx].locked ? 1 : 0, required_locked)) {
            stack->active_layer = idx;
            return idx;
        }
    }
    {
        int active_hidden = stack->layers[stack->active_layer].visible ? 0 : 1;
        if (layer_stack_matches_bool_filter(active_hidden, required_hidden) &&
            layer_stack_matches_bool_filter(stack->layers[stack->active_layer].locked ? 1 : 0, required_locked)) {
            return stack->active_layer;
        }
    }
    return -1;
}

static int layer_stack_cycle_bool_field(LayerStack *stack, int direction, int want_value, int use_locked) {
    if (!stack || stack->layer_count <= 0) {
        return -1;
    }
    for (int offset = 1; offset <= stack->layer_count; offset++) {
        int idx = stack->active_layer + (direction * offset);
        while (idx < 0) {
            idx += stack->layer_count;
        }
        idx %= stack->layer_count;
        int value = use_locked ? (stack->layers[idx].locked ? 1 : 0) : (stack->layers[idx].visible ? 1 : 0);
        if (value == want_value) {
            stack->active_layer = idx;
            return idx;
        }
    }
    if ((use_locked ? (stack->layers[stack->active_layer].locked ? 1 : 0) : (stack->layers[stack->active_layer].visible ? 1 : 0)) == want_value) {
        return stack->active_layer;
    }
    return -1;
}

static int layer_stack_select_bool_edge(LayerStack *stack, int want_value, int from_top, int use_locked) {
    if (!stack || stack->layer_count <= 0) {
        return -1;
    }
    if (from_top) {
        for (int i = stack->layer_count - 1; i >= 0; i--) {
            int value = use_locked ? (stack->layers[i].locked ? 1 : 0) : (stack->layers[i].visible ? 1 : 0);
            if (value == want_value) {
                stack->active_layer = i;
                return i;
            }
        }
        return -1;
    }
    for (int i = 0; i < stack->layer_count; i++) {
        int value = use_locked ? (stack->layers[i].locked ? 1 : 0) : (stack->layers[i].visible ? 1 : 0);
        if (value == want_value) {
            stack->active_layer = i;
            return i;
        }
    }
    return -1;
}

static int layer_stack_matches_editable(const Layer *layer, EditableVisibilityMode visibility_mode) {
    if (!layer || layer->locked) {
        return 0;
    }
    if (visibility_mode == EDITABLE_VISIBLE_ONLY) {
        return layer->visible;
    }
    if (visibility_mode == EDITABLE_HIDDEN_ONLY) {
        return !layer->visible;
    }
    return 1;
}

static int layer_stack_cycle_editable_filtered(LayerStack *stack, int direction, EditableVisibilityMode visibility_mode) {
    if (!stack || stack->layer_count <= 0) {
        return -1;
    }
    for (int offset = 1; offset <= stack->layer_count; offset++) {
        int idx = stack->active_layer + (direction * offset);
        while (idx < 0) {
            idx += stack->layer_count;
        }
        idx %= stack->layer_count;
        if (layer_stack_matches_editable(&stack->layers[idx], visibility_mode)) {
            stack->active_layer = idx;
            return idx;
        }
    }
    if (layer_stack_matches_editable(&stack->layers[stack->active_layer], visibility_mode)) {
        return stack->active_layer;
    }
    return -1;
}

static int layer_stack_select_editable_edge(LayerStack *stack, int from_top, EditableVisibilityMode visibility_mode) {
    if (!stack || stack->layer_count <= 0) {
        return -1;
    }
    if (from_top) {
        for (int i = stack->layer_count - 1; i >= 0; i--) {
            if (layer_stack_matches_editable(&stack->layers[i], visibility_mode)) {
                stack->active_layer = i;
                return i;
            }
        }
        return -1;
    }
    for (int i = 0; i < stack->layer_count; i++) {
        if (layer_stack_matches_editable(&stack->layers[i], visibility_mode)) {
            stack->active_layer = i;
            return i;
        }
    }
    return -1;
}

static int layer_stack_hide_and_focus_direction(LayerStack *stack, int index, int direction) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }
    Layer *layer = &stack->layers[index];
    if (!layer->visible || layer_stack_visible_count(stack) == 1) {
        return 0;
    }
    layer->visible = 0;
    if (stack->solo_index == index) {
        stack->solo_index = -1;
    }
    for (int offset = 1; offset < stack->layer_count; offset++) {
        int next = index + (direction * offset);
        while (next < 0) {
            next += stack->layer_count;
        }
        next %= stack->layer_count;
        if (stack->layers[next].visible) {
            stack->active_layer = next;
            return 1;
        }
    }
    return 1;
}

static int layer_stack_lock_and_focus_direction(LayerStack *stack, int index, int direction) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }
    stack->layers[index].locked = 1;
    if (stack->layer_count == 1) {
        stack->active_layer = index;
        return 1;
    }
    for (int offset = 1; offset < stack->layer_count; offset++) {
        int next = index + (direction * offset);
        while (next < 0) {
            next += stack->layer_count;
        }
        next %= stack->layer_count;
        if (!stack->layers[next].locked) {
            stack->active_layer = next;
            return 1;
        }
    }
    stack->active_layer = index;
    return 1;
}

static int layer_stack_matches_bool_filter(int value, int required_value) {
    return required_value < 0 || value == required_value;
}

static int layer_stack_remap_visibility(LayerStack *stack, int preserve_index, int invert_visibility,
                                        int required_hidden, int required_locked) {
    if (!stack || stack->layer_count <= 0) {
        return 0;
    }
    if (preserve_index < 0 || preserve_index >= stack->layer_count) {
        preserve_index = stack->active_layer;
    }

    int visible_count = 0;
    for (int i = 0; i < stack->layer_count; i++) {
        int next_visible = 0;
        if (invert_visibility) {
            next_visible = stack->layers[i].visible ? 0 : 1;
        } else {
            int is_hidden = stack->layers[i].visible ? 0 : 1;
            next_visible = layer_stack_matches_bool_filter(is_hidden, required_hidden) &&
                           layer_stack_matches_bool_filter(stack->layers[i].locked ? 1 : 0, required_locked);
        }
        stack->layers[i].visible = next_visible;
        visible_count += next_visible;
    }

    if (visible_count == 0) {
        stack->layers[preserve_index].visible = 1;
    }
    stack->active_layer = preserve_index;
    stack->solo_index = -1;
    return 1;
}

int layer_stack_init(LayerStack *stack, int width, int height, uint32_t background_color) {
    if (!stack || width <= 0 || height <= 0) {
        return 0;
    }

    stack->width = width;
    stack->height = height;
    stack->layer_count = 0;
    stack->active_layer = 0;
    stack->solo_index = -1;
    for (int i = 0; i < MAX_LAYERS; i++) {
        stack->layers[i].canvas.width = width;
        stack->layers[i].canvas.height = height;
        stack->layers[i].canvas.pixels = NULL;
        stack->layers[i].visible = 0;
        stack->layers[i].locked = 0;
        stack->layers[i].opacity_percent = 100;
        stack->layers[i].name[0] = '\0';
    }

    if (layer_stack_add(stack, "Background", background_color) < 0) {
        layer_stack_free(stack);
        return 0;
    }
    stack->active_layer = 0;
    return 1;
}

void layer_stack_free(LayerStack *stack) {
    if (!stack) {
        return;
    }
    for (int i = 0; i < MAX_LAYERS; i++) {
        canvas_free(&stack->layers[i].canvas);
        stack->layers[i].visible = 0;
        stack->layers[i].locked = 0;
        stack->layers[i].opacity_percent = 100;
        stack->layers[i].name[0] = '\0';
    }
    stack->width = 0;
    stack->height = 0;
    stack->layer_count = 0;
    stack->active_layer = 0;
    stack->solo_index = -1;
}

Layer *layer_stack_active(LayerStack *stack) {
    if (!stack || stack->layer_count <= 0) {
        return NULL;
    }
    if (stack->active_layer < 0 || stack->active_layer >= stack->layer_count) {
        stack->active_layer = stack->layer_count - 1;
    }
    return &stack->layers[stack->active_layer];
}

const Layer *layer_stack_get(const LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return NULL;
    }
    return &stack->layers[index];
}

int layer_stack_add(LayerStack *stack, const char *name, uint32_t clear_color) {
    if (!stack) {
        return -1;
    }
    return layer_stack_insert(stack, stack->layer_count, name, clear_color);
}

int layer_stack_insert(LayerStack *stack, int index, const char *name, uint32_t clear_color) {
    if (!stack || stack->layer_count >= MAX_LAYERS) {
        return -1;
    }
    if (index < 0) {
        index = 0;
    } else if (index > stack->layer_count) {
        index = stack->layer_count;
    }

    for (int i = stack->layer_count; i > index; i--) {
        stack->layers[i] = stack->layers[i - 1];
    }

    Layer *layer = &stack->layers[index];
    layer->canvas.width = stack->width;
    layer->canvas.height = stack->height;
    layer->canvas.pixels = NULL;
    if (!ensure_layer_canvas(layer, stack->width, stack->height)) {
        for (int i = index; i < stack->layer_count; i++) {
            stack->layers[i] = stack->layers[i + 1];
        }
        return -1;
    }

    canvas_clear(&layer->canvas, clear_color);
    layer->visible = 1;
    layer->locked = 0;
    layer->opacity_percent = 100;
    if (name && name[0]) {
        strncpy(layer->name, name, LAYER_NAME_MAX - 1);
        layer->name[LAYER_NAME_MAX - 1] = '\0';
    } else {
        snprintf(layer->name, LAYER_NAME_MAX, "Layer %d", index + 1);
    }

    stack->layer_count++;
    stack->active_layer = index;
    if (stack->solo_index >= index) {
        stack->solo_index++;
    }
    return index;
}

int layer_stack_cycle(LayerStack *stack, int direction) {
    if (!stack || stack->layer_count <= 0) {
        return -1;
    }
    int idx = stack->active_layer + direction;
    if (idx < 0) {
        idx = stack->layer_count - 1;
    } else if (idx >= stack->layer_count) {
        idx = 0;
    }
    stack->active_layer = idx;
    return idx;
}

int layer_stack_cycle_visible(LayerStack *stack, int direction) {
    return layer_stack_cycle_filtered(stack, direction, 1);
}

int layer_stack_cycle_hidden(LayerStack *stack, int direction) {
    return layer_stack_cycle_filtered(stack, direction, 0);
}

int layer_stack_cycle_locked(LayerStack *stack, int direction) {
    return layer_stack_cycle_bool_field(stack, direction, 1, 1);
}

int layer_stack_cycle_unlocked(LayerStack *stack, int direction) {
    return layer_stack_cycle_bool_field(stack, direction, 0, 1);
}

int layer_stack_cycle_hidden_locked(LayerStack *stack, int direction) {
    return layer_stack_cycle_combined_filter(stack, direction, 1, 1);
}

int layer_stack_cycle_hidden_unlocked(LayerStack *stack, int direction) {
    return layer_stack_cycle_combined_filter(stack, direction, 1, 0);
}

int layer_stack_cycle_editable(LayerStack *stack, int direction) {
    return layer_stack_cycle_editable_filtered(stack, direction, EDITABLE_VISIBLE_ONLY);
}

int layer_stack_select_bottom_visible(LayerStack *stack) {
    return layer_stack_select_edge_filtered(stack, 1, 0);
}

int layer_stack_select_top_visible(LayerStack *stack) {
    return layer_stack_select_edge_filtered(stack, 1, 1);
}

int layer_stack_select_bottom_hidden(LayerStack *stack) {
    return layer_stack_select_edge_filtered(stack, 0, 0);
}

int layer_stack_select_top_hidden(LayerStack *stack) {
    return layer_stack_select_edge_filtered(stack, 0, 1);
}

int layer_stack_select_bottom_locked(LayerStack *stack) {
    return layer_stack_select_bool_edge(stack, 1, 0, 1);
}

int layer_stack_select_top_locked(LayerStack *stack) {
    return layer_stack_select_bool_edge(stack, 1, 1, 1);
}

int layer_stack_select_bottom_unlocked(LayerStack *stack) {
    return layer_stack_select_bool_edge(stack, 0, 0, 1);
}

int layer_stack_select_top_unlocked(LayerStack *stack) {
    return layer_stack_select_bool_edge(stack, 0, 1, 1);
}

int layer_stack_select_bottom_editable(LayerStack *stack) {
    return layer_stack_select_editable_edge(stack, 0, EDITABLE_VISIBLE_ONLY);
}

int layer_stack_select_top_editable(LayerStack *stack) {
    return layer_stack_select_editable_edge(stack, 1, EDITABLE_VISIBLE_ONLY);
}

int layer_stack_toggle_solo(LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }
    if (stack->solo_index == index) {
        stack->solo_index = -1;
    } else {
        stack->solo_index = index;
    }
    return 1;
}

int layer_stack_visible_count(const LayerStack *stack) {
    if (!stack) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < stack->layer_count; i++) {
        if (stack->layers[i].visible) {
            count++;
        }
    }
    return count;
}

int layer_stack_toggle_lock(LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }
    stack->layers[index].locked = !stack->layers[index].locked;
    return 1;
}

int layer_stack_unlock_all(LayerStack *stack) {
    if (!stack) {
        return 0;
    }
    for (int i = 0; i < stack->layer_count; i++) {
        stack->layers[i].locked = 0;
    }
    return 1;
}

int layer_stack_show_all(LayerStack *stack) {
    if (!stack) {
        return 0;
    }
    for (int i = 0; i < stack->layer_count; i++) {
        stack->layers[i].visible = 1;
    }
    stack->solo_index = -1;
    return 1;
}

int layer_stack_show(LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }
    stack->layers[index].visible = 1;
    return 1;
}

int layer_stack_isolate(LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }
    for (int i = 0; i < stack->layer_count; i++) {
        stack->layers[i].visible = (i == index) ? 1 : 0;
    }
    stack->active_layer = index;
    stack->solo_index = -1;
    return 1;
}

int layer_stack_invert_visibility(LayerStack *stack, int preserve_index) {
    return layer_stack_remap_visibility(stack, preserve_index, 1, -1, -1);
}

int layer_stack_show_hidden_only(LayerStack *stack, int preserve_index) {
    return layer_stack_remap_visibility(stack, preserve_index, 0, 1, -1);
}

int layer_stack_show_hidden_locked_only(LayerStack *stack, int preserve_index) {
    return layer_stack_remap_visibility(stack, preserve_index, 0, 1, 1);
}

int layer_stack_show_hidden_unlocked_only(LayerStack *stack, int preserve_index) {
    return layer_stack_remap_visibility(stack, preserve_index, 0, 1, 0);
}

int layer_stack_show_locked_only(LayerStack *stack, int preserve_index) {
    return layer_stack_remap_visibility(stack, preserve_index, 0, -1, 1);
}

int layer_stack_show_unlocked_only(LayerStack *stack, int preserve_index) {
    return layer_stack_remap_visibility(stack, preserve_index, 0, -1, 0);
}

int layer_stack_reveal_hidden(LayerStack *stack, int direction) {
    if (!stack || stack->layer_count <= 0) {
        return 0;
    }
    int target = layer_stack_cycle_filtered(stack, direction, 0);
    if (target < 0) {
        return 0;
    }
    stack->layers[target].visible = 1;
    stack->active_layer = target;
    stack->solo_index = -1;
    return 1;
}

int layer_stack_reveal_editable(LayerStack *stack, int direction) {
    if (!stack || stack->layer_count <= 0) {
        return 0;
    }
    int target = layer_stack_cycle_editable_filtered(stack, direction, EDITABLE_ANY_VISIBILITY);
    if (target >= 0) {
        stack->layers[target].visible = 1;
        stack->active_layer = target;
        stack->solo_index = -1;
        return 1;
    }
    return 0;
}

int layer_stack_reveal_hidden_editable(LayerStack *stack, int from_top) {
    if (!stack || stack->layer_count <= 0) {
        return 0;
    }
    int target = layer_stack_select_editable_edge(stack, from_top, EDITABLE_HIDDEN_ONLY);
    if (target >= 0) {
        stack->layers[target].visible = 1;
        stack->active_layer = target;
        stack->solo_index = -1;
        return 1;
    }
    return 0;
}

int layer_stack_hide_and_advance(LayerStack *stack, int index) {
    return layer_stack_hide_and_focus_direction(stack, index, 1);
}

int layer_stack_hide_and_retreat(LayerStack *stack, int index) {
    return layer_stack_hide_and_focus_direction(stack, index, -1);
}

int layer_stack_lock_and_advance(LayerStack *stack, int index) {
    return layer_stack_lock_and_focus_direction(stack, index, 1);
}

int layer_stack_lock_and_retreat(LayerStack *stack, int index) {
    return layer_stack_lock_and_focus_direction(stack, index, -1);
}

int layer_stack_toggle_visibility(LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }

    Layer *layer = &stack->layers[index];
    if (layer->visible && layer_stack_visible_count(stack) == 1) {
        return 0;
    }
    layer->visible = !layer->visible;
    return 1;
}

int layer_stack_clear_layer(LayerStack *stack, int index, uint32_t color) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }

    Layer *layer = &stack->layers[index];
    if (layer->locked || (!layer->canvas.pixels && !ensure_layer_canvas(layer, stack->width, stack->height))) {
        return 0;
    }
    canvas_clear(&layer->canvas, color);
    return 1;
}

int layer_stack_set_opacity(LayerStack *stack, int index, int opacity_percent) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }
    if (opacity_percent < 0) {
        opacity_percent = 0;
    } else if (opacity_percent > 100) {
        opacity_percent = 100;
    }
    stack->layers[index].opacity_percent = opacity_percent;
    return 1;
}

int layer_stack_delete(LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count || stack->layer_count == 1) {
        return 0;
    }
    if (stack->layers[index].locked) {
        return 0;
    }

    canvas_free(&stack->layers[index].canvas);
    for (int i = index; i < stack->layer_count - 1; i++) {
        stack->layers[i] = stack->layers[i + 1];
    }

    stack->layer_count--;
    stack->layers[stack->layer_count].canvas.width = stack->width;
    stack->layers[stack->layer_count].canvas.height = stack->height;
    stack->layers[stack->layer_count].canvas.pixels = NULL;
    stack->layers[stack->layer_count].visible = 0;
    stack->layers[stack->layer_count].locked = 0;
    stack->layers[stack->layer_count].name[0] = '\0';

    if (stack->active_layer >= stack->layer_count) {
        stack->active_layer = stack->layer_count - 1;
    } else if (stack->active_layer > index) {
        stack->active_layer--;
    }
    if (stack->solo_index == index) {
        stack->solo_index = -1;
    } else if (stack->solo_index > index) {
        stack->solo_index--;
    }

    return 1;
}

int layer_stack_duplicate(LayerStack *stack, int index, const char *name) {
    if (!stack || index < 0 || index >= stack->layer_count || stack->layer_count >= MAX_LAYERS) {
        return -1;
    }

    Layer *source = &stack->layers[index];
    if (!source->canvas.pixels) {
        return -1;
    }

    int insert_at = index + 1;
    for (int i = stack->layer_count; i > insert_at; i--) {
        stack->layers[i] = stack->layers[i - 1];
    }

    Layer *dup = &stack->layers[insert_at];
    dup->canvas.width = stack->width;
    dup->canvas.height = stack->height;
    dup->canvas.pixels = NULL;
    dup->visible = source->visible;
    dup->locked = source->locked;
    dup->opacity_percent = source->opacity_percent;
    dup->name[0] = '\0';

    if (!canvas_init(&dup->canvas, stack->width, stack->height)) {
        for (int i = insert_at; i < stack->layer_count; i++) {
            stack->layers[i] = stack->layers[i + 1];
        }
        return -1;
    }

    size_t total = (size_t)stack->width * (size_t)stack->height;
    memcpy(dup->canvas.pixels, source->canvas.pixels, total * sizeof(uint32_t));

    if (name && name[0]) {
        strncpy(dup->name, name, LAYER_NAME_MAX - 1);
        dup->name[LAYER_NAME_MAX - 1] = '\0';
    } else {
        snprintf(dup->name, LAYER_NAME_MAX, "%s Copy", source->name[0] ? source->name : "Layer");
    }

    stack->layer_count++;
    stack->active_layer = insert_at;
    if (stack->solo_index >= insert_at) {
        stack->solo_index++;
    }
    return insert_at;
}

int layer_stack_move(LayerStack *stack, int index, int direction) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }

    int other = index + direction;
    if (other < 0 || other >= stack->layer_count) {
        return 0;
    }

    Layer tmp = stack->layers[index];
    stack->layers[index] = stack->layers[other];
    stack->layers[other] = tmp;
    stack->active_layer = other;
    if (stack->solo_index == index) {
        stack->solo_index = other;
    } else if (stack->solo_index == other) {
        stack->solo_index = index;
    }
    return 1;
}

int layer_stack_merge_down(LayerStack *stack, int index) {
    if (!stack || index <= 0 || index >= stack->layer_count) {
        return 0;
    }

    Layer *lower = &stack->layers[index - 1];
    Layer *upper = &stack->layers[index];
    if (lower->locked || upper->locked || !lower->canvas.pixels || !upper->canvas.pixels) {
        return 0;
    }

    size_t total = (size_t)stack->width * (size_t)stack->height;
    for (size_t i = 0; i < total; i++) {
        lower->canvas.pixels[i] = blend_pixel(
            lower->canvas.pixels[i],
            apply_layer_opacity(upper->canvas.pixels[i], upper->opacity_percent)
        );
    }
    lower->visible = lower->visible || upper->visible;
    lower->opacity_percent = 100;

    canvas_free(&upper->canvas);
    for (int i = index; i < stack->layer_count - 1; i++) {
        stack->layers[i] = stack->layers[i + 1];
    }

    stack->layer_count--;
    stack->layers[stack->layer_count].canvas.width = stack->width;
    stack->layers[stack->layer_count].canvas.height = stack->height;
    stack->layers[stack->layer_count].canvas.pixels = NULL;
    stack->layers[stack->layer_count].visible = 0;
    stack->layers[stack->layer_count].locked = 0;
    stack->layers[stack->layer_count].opacity_percent = 100;
    stack->layers[stack->layer_count].name[0] = '\0';

    if (stack->active_layer >= stack->layer_count) {
        stack->active_layer = stack->layer_count - 1;
    } else if (stack->active_layer > 0 && stack->active_layer >= index) {
        stack->active_layer--;
    }
    if (stack->solo_index == index) {
        stack->solo_index = index - 1;
    } else if (stack->solo_index > index) {
        stack->solo_index--;
    }

    return 1;
}

int layer_stack_merge_up(LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count - 1) {
        return 0;
    }

    Layer *lower = &stack->layers[index];
    Layer *upper = &stack->layers[index + 1];
    if (lower->locked || upper->locked || !lower->canvas.pixels || !upper->canvas.pixels) {
        return 0;
    }

    size_t total = (size_t)stack->width * (size_t)stack->height;
    for (size_t i = 0; i < total; i++) {
        upper->canvas.pixels[i] = blend_pixel(
            lower->canvas.pixels[i],
            apply_layer_opacity(upper->canvas.pixels[i], upper->opacity_percent)
        );
    }
    upper->visible = lower->visible || upper->visible;
    upper->opacity_percent = 100;

    canvas_free(&lower->canvas);
    for (int i = index; i < stack->layer_count - 1; i++) {
        stack->layers[i] = stack->layers[i + 1];
    }

    stack->layer_count--;
    stack->layers[stack->layer_count].canvas.width = stack->width;
    stack->layers[stack->layer_count].canvas.height = stack->height;
    stack->layers[stack->layer_count].canvas.pixels = NULL;
    stack->layers[stack->layer_count].visible = 0;
    stack->layers[stack->layer_count].locked = 0;
    stack->layers[stack->layer_count].opacity_percent = 100;
    stack->layers[stack->layer_count].name[0] = '\0';

    if (stack->active_layer >= stack->layer_count) {
        stack->active_layer = stack->layer_count - 1;
    } else if (stack->active_layer > index) {
        stack->active_layer--;
    }
    if (stack->solo_index == index) {
        stack->solo_index = index;
    } else if (stack->solo_index > index) {
        stack->solo_index--;
    }

    return 1;
}

int layer_stack_flatten(LayerStack *stack, uint32_t background_color) {
    if (!stack || stack->layer_count <= 0) {
        return 0;
    }
    for (int i = 0; i < stack->layer_count; i++) {
        if (stack->layers[i].locked) {
            return 0;
        }
    }

    Canvas composite = {0};
    if (!canvas_init(&composite, stack->width, stack->height)) {
        return 0;
    }
    layer_stack_composite(stack, &composite, background_color);

    Layer *base = &stack->layers[0];
    if (!base->canvas.pixels && !ensure_layer_canvas(base, stack->width, stack->height)) {
        canvas_free(&composite);
        return 0;
    }
    memcpy(base->canvas.pixels, composite.pixels, (size_t)stack->width * (size_t)stack->height * sizeof(uint32_t));
    base->visible = 1;
    base->locked = 0;
    base->opacity_percent = 100;

    for (int i = 1; i < stack->layer_count; i++) {
        canvas_free(&stack->layers[i].canvas);
        stack->layers[i].canvas.width = stack->width;
        stack->layers[i].canvas.height = stack->height;
        stack->layers[i].canvas.pixels = NULL;
        stack->layers[i].visible = 0;
        stack->layers[i].locked = 0;
        stack->layers[i].opacity_percent = 100;
        stack->layers[i].name[0] = '\0';
    }

    stack->layer_count = 1;
    stack->active_layer = 0;
    stack->solo_index = -1;
    canvas_free(&composite);
    return 1;
}

int layer_stack_stamp_visible_into(LayerStack *stack, int index, uint32_t background_color) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }

    Canvas composite = {0};
    if (!canvas_init(&composite, stack->width, stack->height)) {
        return 0;
    }
    layer_stack_composite(stack, &composite, background_color);

    Layer *target = &stack->layers[index];
    if (target->locked || (!target->canvas.pixels && !ensure_layer_canvas(target, stack->width, stack->height))) {
        canvas_free(&composite);
        return 0;
    }
    memcpy(target->canvas.pixels, composite.pixels, (size_t)stack->width * (size_t)stack->height * sizeof(uint32_t));
    target->visible = 1;
    target->opacity_percent = 100;
    canvas_free(&composite);
    return 1;
}

int layer_stack_stamp_visible_new(LayerStack *stack, const char *name, uint32_t background_color) {
    if (!stack || stack->layer_count >= MAX_LAYERS) {
        return -1;
    }
    int index = layer_stack_add(stack, name, 0x00000000);
    if (index < 0) {
        return -1;
    }
    if (!layer_stack_stamp_visible_into(stack, index, background_color)) {
        return -1;
    }
    return index;
}

void layer_stack_composite(const LayerStack *stack, Canvas *dest, uint32_t background_color) {
    if (!stack || !dest || !dest->pixels) {
        return;
    }
    if (dest->width != stack->width || dest->height != stack->height) {
        return;
    }

    size_t total = (size_t)stack->width * (size_t)stack->height;
    for (size_t i = 0; i < total; i++) {
        uint32_t out = background_color;
        for (int layer_index = 0; layer_index < stack->layer_count; layer_index++) {
            const Layer *layer = &stack->layers[layer_index];
            int soloing = stack->solo_index >= 0;
            int solo_match = stack->solo_index == layer_index;
            if (soloing && !solo_match) {
                continue;
            }
            if ((!layer->visible && !solo_match) || !layer->canvas.pixels) {
                continue;
            }
            out = blend_pixel(out, apply_layer_opacity(layer->canvas.pixels[i], layer->opacity_percent));
        }
        dest->pixels[i] = out;
    }
}
