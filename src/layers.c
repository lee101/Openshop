#include "layers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int layer_name_exists(const LayerStack *stack, const char *name, int skip_index) {
    if (!stack || !name || !name[0]) {
        return 0;
    }
    for (int i = 0; i < stack->layer_count; i++) {
        if (i == skip_index) {
            continue;
        }
        if (strcmp(stack->layers[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void layer_name_copy_unique(char *dest, size_t dest_size, const LayerStack *stack, int skip_index, const char *preferred, const char *fallback_prefix) {
    char candidate[LAYER_NAME_MAX];
    const char *base = (preferred && preferred[0]) ? preferred : fallback_prefix;

    if (!dest || dest_size == 0) {
        return;
    }

    snprintf(candidate, sizeof(candidate), "%s", base && base[0] ? base : "Layer");
    if (!layer_name_exists(stack, candidate, skip_index)) {
        snprintf(dest, dest_size, "%s", candidate);
        return;
    }

    for (int suffix = 2;; suffix++) {
        snprintf(candidate, sizeof(candidate), "%s %d", base && base[0] ? base : "Layer", suffix);
        if (!layer_name_exists(stack, candidate, skip_index)) {
            snprintf(dest, dest_size, "%s", candidate);
            return;
        }
    }
}

static void layer_duplicate_name_base(char *dest, size_t dest_size, const char *source_name) {
    const char *suffix = NULL;
    size_t base_len = 0;

    if (!dest || dest_size == 0) {
        return;
    }
    if (!source_name || !source_name[0]) {
        snprintf(dest, dest_size, "Layer Copy");
        return;
    }

    suffix = strstr(source_name, " Copy");
    if (suffix && suffix[5] == '\0') {
        snprintf(dest, dest_size, "%s", source_name);
        return;
    }
    if (suffix && suffix[5] == ' ') {
        const char *digits = suffix + 6;
        if (*digits) {
            int all_digits = 1;
            for (const char *p = digits; *p; p++) {
                if (*p < '0' || *p > '9') {
                    all_digits = 0;
                    break;
                }
            }
            if (all_digits) {
                base_len = (size_t)(suffix - source_name + 5);
                if (base_len >= dest_size) {
                    base_len = dest_size - 1;
                }
                memcpy(dest, source_name, base_len);
                dest[base_len] = '\0';
                return;
            }
        }
    }

    snprintf(dest, dest_size, "%s Copy", source_name);
}

static void layer_default_name(char *dest, size_t dest_size, const LayerStack *stack, int skip_index) {
    const char *base = (skip_index == 0) ? "Background" : "Layer";
    layer_name_copy_unique(dest, dest_size, stack, skip_index, NULL, base);
}

static int layer_index_matches_reset_scope(const LayerStack *stack, int index, int require_visible, int require_unlocked, int require_locked, int skip_background) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }
    if (skip_background && index == 0) {
        return 0;
    }
    if (require_visible && !stack->layers[index].visible) {
        return 0;
    }
    if (require_unlocked && stack->layers[index].locked) {
        return 0;
    }
    if (require_locked && !stack->layers[index].locked) {
        return 0;
    }
    return 1;
}

static int layer_stack_can_reset_names_in_scope(const LayerStack *stack, int require_visible, int require_unlocked, int require_locked, int skip_background) {
    char next_name[LAYER_NAME_MAX];

    if (!stack) {
        return 0;
    }

    for (int i = 0; i < stack->layer_count; i++) {
        if (!layer_index_matches_reset_scope(stack, i, require_visible, require_unlocked, require_locked, skip_background)) {
            continue;
        }
        layer_default_name(next_name, sizeof(next_name), stack, i);
        if (strcmp(stack->layers[i].name, next_name) != 0) {
            return 1;
        }
    }

    return 0;
}

static int layer_stack_reset_names_in_scope(LayerStack *stack, int require_visible, int require_unlocked, int require_locked, int skip_background) {
    char next_name[LAYER_NAME_MAX];
    int changed = 0;

    if (!stack) {
        return 0;
    }

    for (int i = 0; i < stack->layer_count; i++) {
        if (!layer_index_matches_reset_scope(stack, i, require_visible, require_unlocked, require_locked, skip_background)) {
            continue;
        }
        layer_default_name(next_name, sizeof(next_name), stack, i);
        if (strcmp(stack->layers[i].name, next_name) == 0) {
            continue;
        }
        memcpy(stack->layers[i].name, next_name, sizeof(next_name));
        changed = 1;
    }

    return changed;
}

static int layer_name_differs_from_default(const LayerStack *stack, int index) {
    char next_name[LAYER_NAME_MAX];

    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }

    layer_default_name(next_name, sizeof(next_name), stack, index);
    return strcmp(stack->layers[index].name, next_name) != 0;
}

static int layer_reset_name_to_default(LayerStack *stack, int index) {
    char next_name[LAYER_NAME_MAX];

    if (!layer_name_differs_from_default(stack, index)) {
        return 0;
    }

    layer_default_name(next_name, sizeof(next_name), stack, index);
    memcpy(stack->layers[index].name, next_name, sizeof(next_name));
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

int layer_stack_can_insert(const LayerStack *stack) {
    return stack && stack->layer_count < MAX_LAYERS;
}

int layer_stack_insert(LayerStack *stack, int index, const char *name, uint32_t clear_color) {
    if (!layer_stack_can_insert(stack)) {
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
        layer_name_copy_unique(layer->name, sizeof(layer->name), stack, index, name, "Layer");
    } else {
        layer_default_name(layer->name, sizeof(layer->name), stack, index);
    }

    stack->layer_count++;
    stack->active_layer = index;
    if (stack->solo_index >= index) {
        stack->solo_index++;
    }
    return index;
}

int layer_stack_cycle(LayerStack *stack, int direction) {
    if (!stack || stack->layer_count <= 0 || direction == 0) {
        return -1;
    }
    if (stack->layer_count == 1) {
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

int layer_stack_rename(LayerStack *stack, int index, const char *name) {
    char next_name[LAYER_NAME_MAX];

    if (!stack || index < 0 || index >= stack->layer_count || !name || !name[0]) {
        return 0;
    }

    strncpy(next_name, name, LAYER_NAME_MAX - 1);
    next_name[LAYER_NAME_MAX - 1] = '\0';
    if (strcmp(stack->layers[index].name, next_name) == 0) {
        return 0;
    }

    memcpy(stack->layers[index].name, next_name, sizeof(next_name));
    return 1;
}

int layer_stack_can_reset_name(const LayerStack *stack, int index) {
    return layer_name_differs_from_default(stack, index);
}

int layer_stack_reset_name(LayerStack *stack, int index) {
    return layer_reset_name_to_default(stack, index);
}

int layer_stack_can_reset_all_names(const LayerStack *stack) {
    return layer_stack_can_reset_names_in_scope(stack, 0, 0, 0, 0);
}

int layer_stack_reset_all_names(LayerStack *stack) {
    return layer_stack_reset_names_in_scope(stack, 0, 0, 0, 0);
}

int layer_stack_can_reset_unlocked_names(const LayerStack *stack) {
    return layer_stack_can_reset_names_in_scope(stack, 0, 1, 0, 0);
}

int layer_stack_reset_unlocked_names(LayerStack *stack) {
    return layer_stack_reset_names_in_scope(stack, 0, 1, 0, 0);
}

int layer_stack_can_reset_visible_names(const LayerStack *stack) {
    return layer_stack_can_reset_names_in_scope(stack, 1, 0, 0, 0);
}

int layer_stack_reset_visible_names(LayerStack *stack) {
    return layer_stack_reset_names_in_scope(stack, 1, 0, 0, 0);
}

int layer_stack_can_reset_non_background_unlocked_names(const LayerStack *stack) {
    return layer_stack_can_reset_names_in_scope(stack, 0, 1, 0, 1);
}

int layer_stack_reset_non_background_unlocked_names(LayerStack *stack) {
    return layer_stack_reset_names_in_scope(stack, 0, 1, 0, 1);
}

int layer_stack_can_reset_locked_names(const LayerStack *stack) {
    return layer_stack_can_reset_names_in_scope(stack, 0, 0, 1, 0);
}

int layer_stack_reset_locked_names(LayerStack *stack) {
    return layer_stack_reset_names_in_scope(stack, 0, 0, 1, 0);
}

int layer_stack_can_reset_non_background_visible_names(const LayerStack *stack) {
    return layer_stack_can_reset_names_in_scope(stack, 1, 0, 0, 1);
}

int layer_stack_reset_non_background_visible_names(LayerStack *stack) {
    return layer_stack_reset_names_in_scope(stack, 1, 0, 0, 1);
}

int layer_stack_can_reset_non_background_locked_names(const LayerStack *stack) {
    return layer_stack_can_reset_names_in_scope(stack, 0, 0, 1, 1);
}

int layer_stack_reset_non_background_locked_names(LayerStack *stack) {
    return layer_stack_reset_names_in_scope(stack, 0, 0, 1, 1);
}

int layer_stack_toggle_lock(LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }
    stack->layers[index].locked = !stack->layers[index].locked;
    return 1;
}

int layer_stack_show_all(LayerStack *stack) {
    if (!stack) {
        return 0;
    }
    if (stack->solo_index == -1 && layer_stack_visible_count(stack) == stack->layer_count) {
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
    if (stack->layers[index].visible) {
        return 0;
    }
    stack->layers[index].visible = 1;
    return 1;
}

int layer_stack_hide_and_advance(LayerStack *stack, int index) {
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
        int next = (index + offset) % stack->layer_count;
        if (stack->layers[next].visible) {
            stack->active_layer = next;
            return 1;
        }
    }
    return 1;
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
    if (stack->layers[index].opacity_percent == opacity_percent) {
        return 0;
    }
    stack->layers[index].opacity_percent = opacity_percent;
    return 1;
}

int layer_stack_can_delete(const LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count || stack->layer_count == 1) {
        return 0;
    }
    return !stack->layers[index].locked;
}

int layer_stack_delete(LayerStack *stack, int index) {
    if (!layer_stack_can_delete(stack, index)) {
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

int layer_stack_can_duplicate(const LayerStack *stack, int index) {
    if (!layer_stack_can_insert(stack) || index < 0 || index >= stack->layer_count) {
        return 0;
    }
    return stack->layers[index].canvas.pixels != NULL;
}

int layer_stack_duplicate(LayerStack *stack, int index, const char *name) {
    if (!layer_stack_can_duplicate(stack, index)) {
        return -1;
    }
    Layer *source = &stack->layers[index];

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
        layer_name_copy_unique(dup->name, sizeof(dup->name), stack, insert_at, name, "Layer");
    } else {
        char fallback[LAYER_NAME_MAX];
        layer_duplicate_name_base(fallback, sizeof(fallback), source->name[0] ? source->name : "Layer");
        layer_name_copy_unique(dup->name, sizeof(dup->name), stack, insert_at, fallback, "Layer Copy");
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

int layer_stack_move_to_edge(LayerStack *stack, int index, int direction) {
    if (!stack || index < 0 || index >= stack->layer_count || direction == 0) {
        return 0;
    }

    int target = direction < 0 ? 0 : stack->layer_count - 1;
    if (index == target) {
        return 0;
    }

    Layer moved = stack->layers[index];
    if (direction < 0) {
        memmove(&stack->layers[1], &stack->layers[0], (size_t)index * sizeof(Layer));
    } else {
        memmove(&stack->layers[index], &stack->layers[index + 1], (size_t)(stack->layer_count - index - 1) * sizeof(Layer));
    }
    stack->layers[target] = moved;
    stack->active_layer = target;

    if (stack->solo_index == index) {
        stack->solo_index = target;
    } else if (direction < 0 && stack->solo_index >= 0 && stack->solo_index < index) {
        stack->solo_index++;
    } else if (direction > 0 && stack->solo_index > index) {
        stack->solo_index--;
    }

    return 1;
}

int layer_stack_can_merge_down(const LayerStack *stack, int index) {
    if (!stack || index <= 0 || index >= stack->layer_count) {
        return 0;
    }

    const Layer *lower = &stack->layers[index - 1];
    const Layer *upper = &stack->layers[index];
    return !lower->locked && !upper->locked && lower->canvas.pixels && upper->canvas.pixels;
}

int layer_stack_merge_down(LayerStack *stack, int index) {
    if (!layer_stack_can_merge_down(stack, index)) {
        return 0;
    }
    Layer *lower = &stack->layers[index - 1];
    Layer *upper = &stack->layers[index];

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

int layer_stack_can_merge_up(const LayerStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->layer_count - 1) {
        return 0;
    }

    const Layer *lower = &stack->layers[index];
    const Layer *upper = &stack->layers[index + 1];
    return !lower->locked && !upper->locked && lower->canvas.pixels && upper->canvas.pixels;
}

int layer_stack_merge_up(LayerStack *stack, int index) {
    if (!layer_stack_can_merge_up(stack, index)) {
        return 0;
    }
    Layer *lower = &stack->layers[index];
    Layer *upper = &stack->layers[index + 1];

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

int layer_stack_can_flatten(const LayerStack *stack) {
    if (!stack || stack->layer_count <= 0) {
        return 0;
    }
    for (int i = 0; i < stack->layer_count; i++) {
        if (stack->layers[i].locked) {
            return 0;
        }
    }
    return 1;
}

int layer_stack_flatten(LayerStack *stack, uint32_t background_color) {
    if (!layer_stack_can_flatten(stack)) {
        return 0;
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

int layer_stack_stamp_visible_would_change(const LayerStack *stack, int index, uint32_t background_color) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }

    const Layer *target = &stack->layers[index];
    if (target->locked) {
        return 0;
    }

    Canvas composite = {0};
    if (!canvas_init(&composite, stack->width, stack->height)) {
        return 0;
    }
    layer_stack_composite(stack, &composite, background_color);

    size_t total = (size_t)stack->width * (size_t)stack->height;
    int would_change = !target->visible || target->opacity_percent != 100;
    for (size_t i = 0; i < total && !would_change; i++) {
        uint32_t current = target->canvas.pixels ? target->canvas.pixels[i] : 0;
        if (current != composite.pixels[i]) {
            would_change = 1;
        }
    }

    canvas_free(&composite);
    return would_change;
}

int layer_stack_stamp_visible_into(LayerStack *stack, int index, uint32_t background_color) {
    if (!stack || index < 0 || index >= stack->layer_count) {
        return 0;
    }

    Layer *target = &stack->layers[index];
    if (target->locked) {
        return 0;
    }

    Canvas composite = {0};
    if (!canvas_init(&composite, stack->width, stack->height)) {
        return 0;
    }
    layer_stack_composite(stack, &composite, background_color);

    size_t total = (size_t)stack->width * (size_t)stack->height;
    int would_change = !target->visible || target->opacity_percent != 100;
    for (size_t i = 0; i < total && !would_change; i++) {
        uint32_t current = target->canvas.pixels ? target->canvas.pixels[i] : 0;
        if (current != composite.pixels[i]) {
            would_change = 1;
        }
    }
    if (!would_change) {
        canvas_free(&composite);
        return 0;
    }

    if (!target->canvas.pixels && !ensure_layer_canvas(target, stack->width, stack->height)) {
        canvas_free(&composite);
        return 0;
    }
    memcpy(target->canvas.pixels, composite.pixels, total * sizeof(uint32_t));
    target->visible = 1;
    target->opacity_percent = 100;
    canvas_free(&composite);
    return 1;
}

int layer_stack_stamp_visible_new(LayerStack *stack, const char *name, uint32_t background_color) {
    if (!stack || stack->layer_count >= MAX_LAYERS) {
        return -1;
    }

    Canvas composite = {0};
    if (!canvas_init(&composite, stack->width, stack->height)) {
        return -1;
    }
    layer_stack_composite(stack, &composite, background_color);

    int index = layer_stack_add(stack, name, 0x00000000);
    if (index < 0) {
        canvas_free(&composite);
        return -1;
    }

    Layer *target = &stack->layers[index];
    memcpy(target->canvas.pixels, composite.pixels, (size_t)stack->width * (size_t)stack->height * sizeof(uint32_t));
    target->visible = 1;
    target->opacity_percent = 100;
    canvas_free(&composite);
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
