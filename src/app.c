#include "app.h"
#include "canvas.h"
#include "image_io.h"
#include "layers.h"
#include "status_text.h"
#include "title_hints.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define CANVAS_WIDTH 800
#define CANVAS_HEIGHT 600
#define MAX_HISTORY 20

static const uint32_t COLOR_BG = 0xFFFFFFFF;     // white
static const uint32_t COLOR_BRUSH = 0xFF1B1F24;  // near-black
static const uint32_t COLOR_ERASE = 0xFFFFFFFF;  // background fallback
static const uint32_t COLOR_RED = 0xFFE53935;
static const uint32_t COLOR_GREEN = 0xFF43A047;
static const uint32_t COLOR_BLUE = 0xFF1E88E5;
static const uint32_t COLOR_YELLOW = 0xFFFDD835;
static const uint32_t COLOR_PURPLE = 0xFF8E24AA;
static const int CHECKER_SIZE = 16;

typedef enum {
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_LINE,
    TOOL_RECT,
    TOOL_FILLED_RECT,
    TOOL_ELLIPSE,
    TOOL_FILLED_ELLIPSE
} Tool;

typedef enum {
    BRUSH_SHAPE_ROUND = 0,
    BRUSH_SHAPE_SQUARE,
    BRUSH_SHAPE_DIAMOND,
    BRUSH_SHAPE_COUNT
} BrushShape;

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
} Snapshot;

typedef int (*LayerIndexedActionFn)(LayerStack *layers, int index);

static void snapshot_free(Snapshot *s) {
    if (!s) {
        return;
    }
    free(s->pixels);
    s->pixels = NULL;
    s->width = 0;
    s->height = 0;
    s->layer_count = 0;
    s->active_layer = 0;
}

static int snapshot_from_layers(Snapshot *s, const LayerStack *stack) {
    if (!s || !stack) {
        return 0;
    }

    memset(s, 0, sizeof(*s));
    s->width = stack->width;
    s->height = stack->height;
    s->layer_count = stack->layer_count;
    s->active_layer = stack->active_layer;
    s->solo_index = stack->solo_index;

    size_t per_layer = (size_t)stack->width * (size_t)stack->height;
    size_t total_pixels = per_layer * (size_t)stack->layer_count;
    if (total_pixels > 0) {
        s->pixels = (uint32_t *)malloc(total_pixels * sizeof(uint32_t));
        if (!s->pixels) {
            return 0;
        }
    }

    for (int layer_index = 0; layer_index < stack->layer_count; layer_index++) {
        const Layer *layer = &stack->layers[layer_index];
        s->visibility[layer_index] = (uint8_t)layer->visible;
        s->locked[layer_index] = (uint8_t)layer->locked;
        s->opacity_percent[layer_index] = (uint8_t)layer->opacity_percent;
        strncpy(s->names[layer_index], layer->name, LAYER_NAME_MAX - 1);
        s->names[layer_index][LAYER_NAME_MAX - 1] = '\0';
        if (!s->pixels) {
            continue;
        }
        uint32_t *dst = s->pixels + per_layer * (size_t)layer_index;
        if (layer->canvas.pixels) {
            memcpy(dst, layer->canvas.pixels, per_layer * sizeof(uint32_t));
        } else {
            memset(dst, 0, per_layer * sizeof(uint32_t));
        }
    }

    return 1;
}

static int snapshot_apply(const Snapshot *s, LayerStack *stack) {
    if (!s || !stack || !s->pixels) {
        return 0;
    }
    if (s->width != stack->width || s->height != stack->height) {
        return 0;
    }
    if (s->layer_count <= 0 || s->layer_count > MAX_LAYERS) {
        return 0;
    }

    while (stack->layer_count < s->layer_count) {
        if (layer_stack_add(stack, NULL, 0x00000000) < 0) {
            return 0;
        }
    }
    stack->layer_count = s->layer_count;

    size_t per_layer = (size_t)stack->width * (size_t)stack->height;
    for (int layer_index = 0; layer_index < stack->layer_count; layer_index++) {
        Layer *layer = &stack->layers[layer_index];
        if (!layer->canvas.pixels && !layer_stack_clear_layer(stack, layer_index, 0x00000000)) {
            return 0;
        }
        memcpy(layer->canvas.pixels, s->pixels + per_layer * (size_t)layer_index, per_layer * sizeof(uint32_t));
        layer->visible = s->visibility[layer_index] ? 1 : 0;
        layer->locked = s->locked[layer_index] ? 1 : 0;
        layer->opacity_percent = s->opacity_percent[layer_index];
        strncpy(layer->name, s->names[layer_index], LAYER_NAME_MAX - 1);
        layer->name[LAYER_NAME_MAX - 1] = '\0';
    }

    stack->active_layer = s->active_layer;
    if (stack->active_layer < 0) {
        stack->active_layer = 0;
    }
    if (stack->active_layer >= stack->layer_count) {
        stack->active_layer = stack->layer_count - 1;
    }
    stack->solo_index = s->solo_index;
    if (stack->solo_index >= stack->layer_count) {
        stack->solo_index = -1;
    }
    return 1;
}

static void stack_clear(Snapshot *stack, int *count) {
    if (!stack || !count) {
        return;
    }
    for (int i = 0; i < *count; i++) {
        snapshot_free(&stack[i]);
    }
    *count = 0;
}

static void push_snapshot(const LayerStack *layers, Snapshot *stack, int *count, Snapshot *redo, int *redo_count) {
    if (!layers || !stack || !count) {
        return;
    }
    if (*count == MAX_HISTORY) {
        snapshot_free(&stack[0]);
        memmove(&stack[0], &stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
        *count = MAX_HISTORY - 1;
    }

    Snapshot s = {0};
    if (!snapshot_from_layers(&s, layers)) {
        snapshot_free(&s);
        return;
    }
    stack[(*count)++] = s;
    if (redo && redo_count) {
        stack_clear(redo, redo_count);
    }
}

static uint32_t compose_brush_color(uint32_t rgb_color, int opacity_percent) {
    if (opacity_percent < 1) {
        opacity_percent = 1;
    } else if (opacity_percent > 100) {
        opacity_percent = 100;
    }
    uint32_t alpha = (uint32_t)((opacity_percent * 255 + 50) / 100);
    return (alpha << 24) | (rgb_color & 0x00FFFFFF);
}

static const char *tool_label(Tool tool) {
    switch (tool) {
    case TOOL_BRUSH:
        return "Brush";
    case TOOL_ERASER:
        return "Eraser";
    case TOOL_LINE:
        return "Line";
    case TOOL_RECT:
        return "Rectangle";
    case TOOL_FILLED_RECT:
        return "Filled Rectangle";
    case TOOL_ELLIPSE:
        return "Ellipse";
    case TOOL_FILLED_ELLIPSE:
        return "Filled Ellipse";
    default:
        return "Brush";
    }
}

static const char *brush_shape_label(BrushShape shape) {
    switch (shape) {
    case BRUSH_SHAPE_ROUND:
        return "Round";
    case BRUSH_SHAPE_SQUARE:
        return "Square";
    case BRUSH_SHAPE_DIAMOND:
        return "Diamond";
    default:
        return "Round";
    }
}

static BrushShape cycle_brush_shape(BrushShape shape, int direction) {
    int idx = (int)shape + direction;
    if (idx < 0) {
        idx = BRUSH_SHAPE_COUNT - 1;
    } else if (idx >= BRUSH_SHAPE_COUNT) {
        idx = 0;
    }
    return (BrushShape)idx;
}

static void update_window_title(SDL_Window *window, const LayerStack *layers, Tool tool, BrushShape brush_shape, int radius, uint32_t color, int opacity_percent) {
    char title[384];

    if (!window || !layers) {
        return;
    }
    format_window_title(layers, tool_label(tool), brush_shape_label(brush_shape), radius, color, opacity_percent,
                        title, sizeof(title));
    SDL_SetWindowTitle(window, title);
}

static int action_insert_layer_above(LayerStack *layers, int index) {
    return layer_stack_insert(layers, index + 1, NULL, 0x00000000) >= 0;
}

static int action_insert_layer_below(LayerStack *layers, int index) {
    return layer_stack_insert(layers, index, NULL, 0x00000000) >= 0;
}

static int action_toggle_layer_lock(LayerStack *layers, int index) {
    return layer_stack_toggle_lock(layers, index);
}

static int action_lock_and_advance(LayerStack *layers, int index) {
    return layer_stack_lock_and_advance(layers, index);
}

static int action_lock_and_retreat(LayerStack *layers, int index) {
    return layer_stack_lock_and_retreat(layers, index);
}

static int action_unlock_all_layers(LayerStack *layers, int index) {
    (void)index;
    return layer_stack_unlock_all(layers);
}

static int action_show_unlocked_only(LayerStack *layers, int index) {
    return layer_stack_show_unlocked_only(layers, index);
}

static int action_show_locked_only(LayerStack *layers, int index) {
    return layer_stack_show_locked_only(layers, index);
}

static int action_show_hidden_locked_only(LayerStack *layers, int index) {
    return layer_stack_show_hidden_locked_only(layers, index);
}

static int action_show_hidden_unlocked_only(LayerStack *layers, int index) {
    return layer_stack_show_hidden_unlocked_only(layers, index);
}

static int action_show_all_layers(LayerStack *layers, int index) {
    (void)index;
    return layer_stack_show_all(layers);
}

static int action_show_active_layer(LayerStack *layers, int index) {
    return layer_stack_show(layers, index);
}

static int action_isolate_active_layer(LayerStack *layers, int index) {
    return layer_stack_isolate(layers, index);
}

static int action_invert_active_layer_visibility(LayerStack *layers, int index) {
    return layer_stack_invert_visibility(layers, index);
}

static int action_show_hidden_only(LayerStack *layers, int index) {
    return layer_stack_show_hidden_only(layers, index);
}

static int action_flatten_layers(LayerStack *layers, int index) {
    (void)index;
    return layer_stack_flatten(layers, COLOR_BG);
}

static int action_stamp_visible_into_active(LayerStack *layers, int index) {
    return layer_stack_stamp_visible_into(layers, index, COLOR_BG);
}

static int action_stamp_visible_new_layer(LayerStack *layers, int index) {
    (void)index;
    return layer_stack_stamp_visible_new(layers, "Visible Stamp", COLOR_BG) >= 0;
}

static int action_duplicate_active_layer(LayerStack *layers, int index) {
    return layer_stack_duplicate(layers, index, NULL) >= 0;
}

static int action_move_layer_down(LayerStack *layers, int index) {
    return layer_stack_move(layers, index, -1);
}

static int action_move_layer_up(LayerStack *layers, int index) {
    return layer_stack_move(layers, index, 1);
}

static void try_save_canvas_to_output(const Canvas *save_canvas) {
    char status_message[128];

    if (!canvas_save_bmp(save_canvas, "output.bmp")) {
        format_status_text_file_save("output.bmp", status_message, sizeof(status_message));
        fprintf(stderr, "%s\n", status_message);
    }
}

static int try_load_active_layer_bmp(LayerStack *layers,
                                     Snapshot *undo_stack, int *undo_count,
                                     Snapshot *redo_stack, int *redo_count) {
    Layer *active = layer_stack_active(layers);
    if (!active || active->locked) {
        fprintf(stderr, "%s\n", status_text_action_error(STATUS_ACTIVE_LAYER_LOCKED));
        return 0;
    }

    push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    if (!canvas_load_bmp(&active->canvas, "input.bmp", active_layer_clear_color(layers))) {
        fprintf(stderr, "%s\n", status_text_action_error(STATUS_LOAD_INPUT_BMP));
        return 0;
    }
    return 1;
}

static int try_flood_fill_active_layer(LayerStack *layers,
                                       Snapshot *undo_stack, int *undo_count,
                                       Snapshot *redo_stack, int *redo_count,
                                       int x, int y, uint32_t brush_color) {
    Layer *active;

    if (x < 0 || y < 0 || x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!active || active->locked) {
        fprintf(stderr, "%s\n", status_text_action_error(STATUS_FILL_FAILED));
        return 0;
    }

    push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    if (!canvas_flood_fill(&active->canvas, x, y, brush_color)) {
        fprintf(stderr, "%s\n", status_text_action_error(STATUS_FILL_FAILED));
        return 0;
    }
    return 1;
}

static int try_clear_active_layer(LayerStack *layers,
                                  Snapshot *undo_stack, int *undo_count,
                                  Snapshot *redo_stack, int *redo_count) {
    if (active_layer_editable(layers)) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    }
    return layer_stack_clear_layer(layers, layers->active_layer, active_layer_clear_color(layers));
}

static int try_flip_horizontal_active_layer(LayerStack *layers,
                                            Snapshot *undo_stack, int *undo_count,
                                            Snapshot *redo_stack, int *redo_count) {
    return apply_canvas_transform(layers, undo_stack, undo_count, redo_stack, redo_count, canvas_flip_horizontal);
}

static int try_flip_vertical_active_layer(LayerStack *layers,
                                          Snapshot *undo_stack, int *undo_count,
                                          Snapshot *redo_stack, int *redo_count) {
    return apply_canvas_transform(layers, undo_stack, undo_count, redo_stack, redo_count, canvas_flip_vertical);
}

static int try_rotate_active_layer_180(LayerStack *layers,
                                       Snapshot *undo_stack, int *undo_count,
                                       Snapshot *redo_stack, int *redo_count) {
    return apply_canvas_transform(layers, undo_stack, undo_count, redo_stack, redo_count, canvas_rotate_180);
}

static int try_invert_active_layer_rgb(LayerStack *layers,
                                       Snapshot *undo_stack, int *undo_count,
                                       Snapshot *redo_stack, int *redo_count) {
    return apply_canvas_transform(layers, undo_stack, undo_count, redo_stack, redo_count, canvas_invert_rgb);
}

static int try_adjust_active_layer_opacity(LayerStack *layers,
                                           Snapshot *undo_stack, int *undo_count,
                                           Snapshot *redo_stack, int *redo_count,
                                           int target_opacity) {
    Layer *active = layer_stack_active(layers);

    if (!active || active->opacity_percent == target_opacity) {
        return 0;
    }

    push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    return layer_stack_set_opacity(layers, layers->active_layer, target_opacity);
}

static int try_add_layer(LayerStack *layers,
                         Snapshot *undo_stack, int *undo_count,
                         Snapshot *redo_stack, int *redo_count) {
    char status_message[64];

    push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    if (layer_stack_add(layers, NULL, 0x00000000) < 0) {
        format_status_text_max_layers(MAX_LAYERS, status_message, sizeof(status_message));
        fprintf(stderr, "%s\n", status_message);
        return 0;
    }
    return 1;
}

static int handle_add_layer_hotkey(SDL_Keycode key,
                                   int ctrl, int alt, int shift,
                                   SDL_Window *window,
                                   LayerStack *layers,
                                   Snapshot *undo_stack, int *undo_count,
                                   Snapshot *redo_stack, int *redo_count,
                                   Tool tool, BrushShape brush_shape,
                                   int brush_radius, uint32_t brush_color,
                                   int brush_opacity, int *needs_composite) {
    if (!(ctrl && shift) || alt || key != SDLK_n ||
        !window || !layers || !undo_stack || !undo_count || !redo_stack || !redo_count || !needs_composite) {
        return 0;
    }

    if (try_add_layer(layers, undo_stack, undo_count, redo_stack, redo_count)) {
        *needs_composite = 1;
    }
    update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
    return 1;
}

typedef int (*LayerDirectionalActionFn)(LayerStack *layers, int arg);

static void run_indexed_layer_action(SDL_Window *window, LayerStack *layers,
                                     Snapshot *undo_stack, int *undo_count,
                                     Snapshot *redo_stack, int *redo_count,
                                     Tool tool, BrushShape brush_shape,
                                     int brush_radius, uint32_t brush_color, int brush_opacity,
                                     int *needs_composite, LayerIndexedActionFn action,
                                     StatusTextAction error_action, int mark_composite) {
    push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    if (!action(layers, layers->active_layer)) {
        fprintf(stderr, "%s\n", status_text_action_error(error_action));
    } else if (mark_composite) {
        *needs_composite = 1;
    }
    update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
}

static void run_directional_layer_action(SDL_Window *window, LayerStack *layers,
                                         Snapshot *undo_stack, int *undo_count,
                                         Snapshot *redo_stack, int *redo_count,
                                         Tool tool, BrushShape brush_shape,
                                         int brush_radius, uint32_t brush_color, int brush_opacity,
                                         int *needs_composite, LayerDirectionalActionFn action, int arg) {
    push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    if (action(layers, arg)) {
        *needs_composite = 1;
    }
    update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
}

static void run_indexed_layer_action_silent(SDL_Window *window, LayerStack *layers,
                                            Snapshot *undo_stack, int *undo_count,
                                            Snapshot *redo_stack, int *redo_count,
                                            Tool tool, BrushShape brush_shape,
                                            int brush_radius, uint32_t brush_color, int brush_opacity,
                                            int *needs_composite, LayerIndexedActionFn action,
                                            int mark_composite) {
    push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    if (action(layers, layers->active_layer) && mark_composite) {
        *needs_composite = 1;
    }
    update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
}

static int key_translation_delta(SDL_Keycode key, int step, int *dx, int *dy) {
    if (!dx || !dy) {
        return 0;
    }

    *dx = 0;
    *dy = 0;
    if (key == SDLK_UP) {
        *dy = -step;
    } else if (key == SDLK_DOWN) {
        *dy = step;
    } else if (key == SDLK_LEFT) {
        *dx = -step;
    } else if (key == SDLK_RIGHT) {
        *dx = step;
    } else {
        return 0;
    }
    return 1;
}

static int refresh_title_on_change(SDL_Window *window, const LayerStack *layers,
                                   Tool tool, BrushShape brush_shape,
                                   int brush_radius, uint32_t brush_color, int brush_opacity,
                                   int changed) {
    if (changed) {
        update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
    }
    return changed;
}

static int try_select_layer_index(LayerStack *layers, int target) {
    if (!layers || target < 0 || target >= layers->layer_count) {
        return 0;
    }
    layers->active_layer = target;
    return 1;
}

static int try_nudge_active_layer_opacity(LayerStack *layers,
                                          Snapshot *undo_stack, int *undo_count,
                                          Snapshot *redo_stack, int *redo_count,
                                          int delta_percent) {
    Layer *active;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!active) {
        return 0;
    }

    return try_adjust_active_layer_opacity(layers, undo_stack, undo_count, redo_stack, redo_count,
                                           active->opacity_percent + delta_percent);
}

static int try_restore_snapshot(LayerStack *layers,
                                Snapshot *source_stack, int *source_count,
                                Snapshot *target_stack, int *target_count) {
    Snapshot current = {0};
    Snapshot restored;

    if (!layers || !source_stack || !source_count || !target_stack || !target_count || *source_count <= 0) {
        return 0;
    }

    if (snapshot_from_layers(&current, layers)) {
        if (*target_count == MAX_HISTORY) {
            snapshot_free(&target_stack[0]);
            memmove(&target_stack[0], &target_stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
            *target_count = MAX_HISTORY - 1;
        }
        target_stack[(*target_count)++] = current;
    }

    restored = source_stack[--(*source_count)];
    snapshot_apply(&restored, layers);
    snapshot_free(&restored);
    return 1;
}

static int try_commit_shape(LayerStack *layers,
                            Snapshot *undo_stack, int *undo_count,
                            Snapshot *redo_stack, int *redo_count,
                            Tool tool, int shape_start_x, int shape_start_y,
                            int end_x, int end_y, int brush_radius, uint32_t brush_color) {
    Layer *active = layer_stack_active(layers);

    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }

    push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    draw_shape(&active->canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
    return 1;
}

static int try_sample_canvas_color(const Canvas *sample, int x, int y,
                                   uint32_t *brush_color_rgb, uint32_t *brush_color,
                                   int *brush_opacity, Tool *tool) {
    uint32_t sampled_color;
    int sampled_alpha;

    if (!sample || !brush_color_rgb || !brush_color || !brush_opacity || !tool) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= sample->width || y >= sample->height) {
        return 0;
    }

    sampled_color = canvas_get_pixel(sample, x, y);
    *brush_color_rgb = sampled_color & 0x00FFFFFF;
    sampled_alpha = (int)((sampled_color >> 24) & 0xFF);
    *brush_opacity = (sampled_alpha * 100 + 127) / 255;
    if (*brush_opacity < 1) {
        *brush_opacity = 1;
    }
    *brush_color = compose_brush_color(*brush_color_rgb, *brush_opacity);
    *tool = TOOL_BRUSH;
    return 1;
}

static int try_begin_brush_stroke(LayerStack *layers,
                                  Snapshot *undo_stack, int *undo_count,
                                  Snapshot *redo_stack, int *redo_count,
                                  Tool tool, int x, int y, int brush_radius,
                                  uint32_t brush_color, BrushShape brush_shape) {
    Layer *active = layer_stack_active(layers);

    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }

    push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    if (tool == TOOL_ERASER) {
        erase_stamp(&active->canvas, x, y, brush_radius, active_layer_clear_color(layers), brush_shape);
    } else {
        stamp_brush(&active->canvas, x, y, brush_radius, brush_color, brush_shape);
    }
    return 1;
}

static void set_brush_color_tool(uint32_t color_rgb, int brush_opacity,
                                 uint32_t *brush_color_rgb, uint32_t *brush_color,
                                 Tool *tool, Tool next_tool) {
    if (!brush_color_rgb || !brush_color || !tool) {
        return;
    }
    *brush_color_rgb = color_rgb & 0x00FFFFFF;
    *brush_color = compose_brush_color(*brush_color_rgb, brush_opacity);
    *tool = next_tool;
}

static void adjust_brush_opacity(int delta, uint32_t brush_color_rgb,
                                 int *brush_opacity, uint32_t *brush_color) {
    if (!brush_opacity || !brush_color) {
        return;
    }
    *brush_opacity += delta;
    if (*brush_opacity < 1) {
        *brush_opacity = 1;
    } else if (*brush_opacity > 100) {
        *brush_opacity = 100;
    }
    *brush_color = compose_brush_color(brush_color_rgb, *brush_opacity);
}

static void set_tool(Tool next_tool, Tool *tool) {
    if (tool) {
        *tool = next_tool;
    }
}

static void adjust_brush_radius(int delta, int *brush_radius) {
    if (!brush_radius) {
        return;
    }
    *brush_radius += delta;
    if (*brush_radius < 1) {
        *brush_radius = 1;
    } else if (*brush_radius > 64) {
        *brush_radius = 64;
    }
}

static void cycle_brush_shape_in_place(BrushShape *brush_shape, int direction) {
    if (brush_shape) {
        *brush_shape = cycle_brush_shape(*brush_shape, direction);
    }
}

typedef struct {
    SDL_Keycode key;
    uint32_t color_rgb;
    Tool tool;
} BrushColorHotkey;

typedef struct {
    SDL_Keycode key;
    Tool tool;
} ToolHotkey;

typedef int (*ActiveEditHotkeyFn)(LayerStack *layers,
                                  Snapshot *undo_stack, int *undo_count,
                                  Snapshot *redo_stack, int *redo_count);

typedef struct {
    SDL_Keycode key;
    ActiveEditHotkeyFn action;
} ActiveEditHotkey;

typedef int (*MousePositionHotkeyFn)(LayerStack *layers,
                                     Snapshot *undo_stack, int *undo_count,
                                     Snapshot *redo_stack, int *redo_count,
                                     const Canvas *sample,
                                     int mx, int my,
                                     uint32_t *brush_color_rgb, uint32_t *brush_color,
                                     int *brush_opacity, Tool *tool);

typedef struct {
    SDL_Keycode key;
    MousePositionHotkeyFn action;
} MousePositionHotkey;

typedef int (*SelectorHotkeyFn)(LayerStack *layers, int arg);

typedef struct {
    SDL_Keycode key;
    int ctrl;
    int alt;
    int shift;
    SelectorHotkeyFn action;
    int arg;
} SelectorHotkey;

typedef int (*DirectionalLayerAction)(LayerStack *layers, int direction);

typedef struct {
    SDL_Keycode key;
    int ctrl;
    int alt;
    int shift;
    DirectionalLayerAction action;
    int direction;
} RevealHotkey;

typedef struct {
    SDL_Keycode key;
    int ctrl;
    int alt;
    int shift;
    LayerIndexedActionFn action;
    StatusTextAction error_action;
    int mark_composite;
} IndexedLayerHotkey;

typedef struct {
    SDL_Keycode key;
    int ctrl;
    int alt;
    int shift;
    LayerIndexedActionFn action;
    int mark_composite;
} IndexedLayerSilentHotkey;

static const BrushColorHotkey BRUSH_COLOR_HOTKEYS[] = {
    {SDLK_b, COLOR_BRUSH, TOOL_BRUSH},
    {SDLK_e, COLOR_ERASE, TOOL_ERASER},
    {SDLK_1, COLOR_BRUSH, TOOL_BRUSH},
    {SDLK_2, COLOR_RED, TOOL_BRUSH},
    {SDLK_3, COLOR_GREEN, TOOL_BRUSH},
    {SDLK_4, COLOR_BLUE, TOOL_BRUSH},
    {SDLK_5, COLOR_YELLOW, TOOL_BRUSH},
    {SDLK_6, COLOR_PURPLE, TOOL_BRUSH},
};

static const ToolHotkey TOOL_HOTKEYS[] = {
    {SDLK_l, TOOL_LINE},
    {SDLK_r, TOOL_RECT},
    {SDLK_t, TOOL_FILLED_RECT},
    {SDLK_o, TOOL_ELLIPSE},
    {SDLK_p, TOOL_FILLED_ELLIPSE},
};

static const ActiveEditHotkey ACTIVE_EDIT_HOTKEYS[] = {
    {SDLK_c, try_clear_active_layer},
    {SDLK_h, try_flip_horizontal_active_layer},
    {SDLK_v, try_flip_vertical_active_layer},
    {SDLK_j, try_rotate_active_layer_180},
    {SDLK_x, try_invert_active_layer_rgb},
};

static int mouse_position_fill_hotkey(LayerStack *layers,
                                      Snapshot *undo_stack, int *undo_count,
                                      Snapshot *redo_stack, int *redo_count,
                                      const Canvas *sample,
                                      int mx, int my,
                                      uint32_t *brush_color_rgb, uint32_t *brush_color,
                                      int *brush_opacity, Tool *tool) {
    (void)sample;
    (void)brush_color_rgb;
    (void)brush_opacity;
    (void)tool;
    return try_flood_fill_active_layer(layers, undo_stack, undo_count, redo_stack, redo_count, mx, my, *brush_color);
}

static int mouse_position_sample_hotkey(LayerStack *layers,
                                        Snapshot *undo_stack, int *undo_count,
                                        Snapshot *redo_stack, int *redo_count,
                                        const Canvas *sample,
                                        int mx, int my,
                                        uint32_t *brush_color_rgb, uint32_t *brush_color,
                                        int *brush_opacity, Tool *tool) {
    (void)layers;
    (void)undo_stack;
    (void)undo_count;
    (void)redo_stack;
    (void)redo_count;
    return try_sample_canvas_color(sample, mx, my, brush_color_rgb, brush_color, brush_opacity, tool);
}

static const MousePositionHotkey MOUSE_POSITION_HOTKEYS[] = {
    {SDLK_f, mouse_position_fill_hotkey},
    {SDLK_i, mouse_position_sample_hotkey},
};

static int select_bottom_visible_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_bottom_visible(layers);
}

static int select_top_visible_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_top_visible(layers);
}

static int select_bottom_hidden_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_bottom_hidden(layers);
}

static int select_top_hidden_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_top_hidden(layers);
}

static int select_bottom_locked_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_bottom_locked(layers);
}

static int select_top_locked_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_top_locked(layers);
}

static int select_bottom_unlocked_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_bottom_unlocked(layers);
}

static int select_top_unlocked_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_top_unlocked(layers);
}

static int select_bottom_hidden_unlocked_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_bottom_hidden_unlocked(layers);
}

static int select_top_hidden_unlocked_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_top_hidden_unlocked(layers);
}

static int select_bottom_hidden_locked_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_bottom_hidden_locked(layers);
}

static int select_top_hidden_locked_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_top_hidden_locked(layers);
}

static int select_bottom_editable_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_bottom_editable(layers);
}

static int select_top_editable_hotkey(LayerStack *layers, int arg) {
    (void)arg;
    return layer_stack_select_top_editable(layers);
}

static const SelectorHotkey SELECTOR_HOTKEYS[] = {
    {SDLK_PAGEUP, 0, 0, 1, layer_stack_cycle_visible, 1},
    {SDLK_PAGEDOWN, 0, 0, 1, layer_stack_cycle_visible, -1},
    {SDLK_PAGEUP, 1, 0, 0, layer_stack_cycle_hidden, 1},
    {SDLK_PAGEDOWN, 1, 0, 0, layer_stack_cycle_hidden, -1},
    {SDLK_HOME, 1, 0, 0, select_bottom_visible_hotkey, 0},
    {SDLK_END, 1, 0, 0, select_top_visible_hotkey, 0},
    {SDLK_HOME, 1, 0, 1, select_bottom_hidden_hotkey, 0},
    {SDLK_END, 1, 0, 1, select_top_hidden_hotkey, 0},
    {SDLK_PAGEUP, 0, 1, 0, layer_stack_cycle_locked, 1},
    {SDLK_PAGEDOWN, 0, 1, 0, layer_stack_cycle_locked, -1},
    {SDLK_HOME, 0, 1, 0, select_bottom_locked_hotkey, 0},
    {SDLK_END, 0, 1, 0, select_top_locked_hotkey, 0},
    {SDLK_PAGEUP, 1, 1, 0, layer_stack_cycle_unlocked, 1},
    {SDLK_PAGEDOWN, 1, 1, 0, layer_stack_cycle_unlocked, -1},
    {SDLK_HOME, 1, 1, 0, select_bottom_unlocked_hotkey, 0},
    {SDLK_END, 1, 1, 0, select_top_unlocked_hotkey, 0},
    {SDLK_RIGHTBRACKET, 1, 1, 0, layer_stack_cycle_hidden_unlocked, 1},
    {SDLK_LEFTBRACKET, 1, 1, 0, layer_stack_cycle_hidden_unlocked, -1},
    {SDLK_COMMA, 1, 1, 0, select_bottom_hidden_unlocked_hotkey, 0},
    {SDLK_PERIOD, 1, 1, 0, select_top_hidden_unlocked_hotkey, 0},
    {SDLK_RIGHTBRACKET, 1, 1, 1, layer_stack_cycle_hidden_locked, 1},
    {SDLK_LEFTBRACKET, 1, 1, 1, layer_stack_cycle_hidden_locked, -1},
    {SDLK_COMMA, 1, 1, 1, select_bottom_hidden_locked_hotkey, 0},
    {SDLK_PERIOD, 1, 1, 1, select_top_hidden_locked_hotkey, 0},
    {SDLK_PAGEUP, 0, 1, 1, layer_stack_cycle_editable, 1},
    {SDLK_PAGEDOWN, 0, 1, 1, layer_stack_cycle_editable, -1},
    {SDLK_HOME, 0, 1, 1, select_bottom_editable_hotkey, 0},
    {SDLK_END, 0, 1, 1, select_top_editable_hotkey, 0},
};

static const RevealHotkey REVEAL_HOTKEYS[] = {
    {SDLK_PAGEUP, 1, 1, 1, layer_stack_reveal_editable, 1},
    {SDLK_PAGEDOWN, 1, 1, 1, layer_stack_reveal_editable, -1},
    {SDLK_HOME, 1, 1, 1, layer_stack_reveal_hidden_editable, 0},
    {SDLK_END, 1, 1, 1, layer_stack_reveal_hidden_editable, 1},
    {SDLK_COMMA, 1, 0, 1, layer_stack_reveal_hidden_locked, 0},
    {SDLK_PERIOD, 1, 0, 1, layer_stack_reveal_hidden_locked, 1},
    {SDLK_SEMICOLON, 1, 1, 0, layer_stack_reveal_hidden_unlocked, 0},
    {SDLK_QUOTE, 1, 1, 0, layer_stack_reveal_hidden_unlocked, 1},
    {SDLK_PAGEUP, 1, 0, 1, layer_stack_reveal_hidden, 1},
    {SDLK_PAGEDOWN, 1, 0, 1, layer_stack_reveal_hidden, -1},
};

static const IndexedLayerHotkey INDEXED_LAYER_HOTKEYS[] = {
    {SDLK_n, 1, 0, 0, action_insert_layer_above, STATUS_INSERT_LAYER_ABOVE, 1},
    {SDLK_COMMA, 1, 0, 0, action_insert_layer_below, STATUS_INSERT_LAYER_BELOW, 1},
    {SDLK_l, 1, 0, 1, action_toggle_layer_lock, STATUS_LOCK_TOGGLE, 0},
    {SDLK_l, 0, 1, 0, action_lock_and_advance, STATUS_LOCK_AND_ADVANCE, 0},
    {SDLK_l, 0, 1, 1, action_lock_and_retreat, STATUS_LOCK_AND_RETREAT, 0},
    {SDLK_u, 0, 1, 0, action_unlock_all_layers, STATUS_UNLOCK_ALL, 0},
    {SDLK_u, 1, 1, 0, action_show_unlocked_only, STATUS_SHOW_UNLOCKED_ONLY, 0},
    {SDLK_l, 1, 1, 0, action_show_locked_only, STATUS_SHOW_LOCKED_ONLY, 0},
    {SDLK_i, 1, 1, 1, action_show_hidden_locked_only, STATUS_SHOW_HIDDEN_LOCKED_ONLY, 0},
    {SDLK_u, 1, 1, 1, action_show_hidden_unlocked_only, STATUS_SHOW_HIDDEN_UNLOCKED_ONLY, 0},
    {SDLK_m, 1, 0, 1, action_flatten_layers, STATUS_FLATTEN_LOCKED, 1},
    {SDLK_e, 1, 0, 1, action_stamp_visible_into_active, STATUS_STAMP_VISIBLE_INTO_LOCKED, 1},
    {SDLK_g, 1, 0, 1, action_stamp_visible_new_layer, STATUS_STAMP_VISIBLE_NEW, 1},
    {SDLK_d, 1, 0, 0, action_duplicate_active_layer, STATUS_DUPLICATE_LAYER, 1},
    {SDLK_LEFTBRACKET, 1, 0, 0, action_move_layer_down, STATUS_MOVE_LAYER_BOTTOM, 1},
    {SDLK_RIGHTBRACKET, 1, 0, 0, action_move_layer_up, STATUS_MOVE_LAYER_TOP, 1},
    {SDLK_v, 1, 0, 1, layer_stack_toggle_visibility, STATUS_HIDE_FINAL_VISIBLE, 1},
    {SDLK_h, 1, 0, 1, layer_stack_hide_and_advance, STATUS_HIDE_FINAL_VISIBLE, 1},
    {SDLK_j, 1, 0, 1, layer_stack_hide_and_retreat, STATUS_HIDE_FINAL_VISIBLE, 1},
    {SDLK_SLASH, 1, 0, 0, layer_stack_toggle_solo, STATUS_TOGGLE_SOLO, 1},
    {SDLK_DELETE, 0, 0, 0, layer_stack_delete, STATUS_DELETE_FINAL_OR_LOCKED, 1},
    {SDLK_BACKSPACE, 0, 0, 0, layer_stack_delete, STATUS_DELETE_FINAL_OR_LOCKED, 1},
    {SDLK_m, 1, 0, 0, layer_stack_merge_down, STATUS_MERGE_DOWN_BLOCKED, 1},
    {SDLK_u, 1, 0, 0, layer_stack_merge_up, STATUS_MERGE_UP_BLOCKED, 1},
};

static const IndexedLayerSilentHotkey INDEXED_LAYER_SILENT_HOTKEYS[] = {
    {SDLK_a, 1, 0, 0, action_show_all_layers, 1},
    {SDLK_r, 1, 0, 1, action_show_active_layer, 1},
    {SDLK_SLASH, 1, 0, 1, action_isolate_active_layer, 1},
    {SDLK_i, 1, 0, 1, action_invert_active_layer_visibility, 1},
    {SDLK_i, 1, 1, 0, action_show_hidden_only, 1},
};

static int handle_brush_state_hotkey(SDL_Keycode key,
                                     uint32_t *brush_color_rgb, uint32_t *brush_color,
                                     int *brush_opacity, int *brush_radius,
                                     BrushShape *brush_shape, Tool *tool) {
    size_t i;

    if (!brush_color_rgb || !brush_color || !brush_opacity || !brush_radius || !brush_shape || !tool) {
        return 0;
    }

    for (i = 0; i < sizeof(BRUSH_COLOR_HOTKEYS) / sizeof(BRUSH_COLOR_HOTKEYS[0]); i++) {
        if (BRUSH_COLOR_HOTKEYS[i].key == key) {
            set_brush_color_tool(BRUSH_COLOR_HOTKEYS[i].color_rgb, *brush_opacity,
                                 brush_color_rgb, brush_color, tool, BRUSH_COLOR_HOTKEYS[i].tool);
            return 1;
        }
    }

    for (i = 0; i < sizeof(TOOL_HOTKEYS) / sizeof(TOOL_HOTKEYS[0]); i++) {
        if (TOOL_HOTKEYS[i].key == key) {
            set_tool(TOOL_HOTKEYS[i].tool, tool);
            return 1;
        }
    }

    if (key == SDLK_LEFTBRACKET) {
        if (*brush_radius > 1) {
            adjust_brush_radius(-1, brush_radius);
        }
    } else if (key == SDLK_RIGHTBRACKET) {
        if (*brush_radius < 64) {
            adjust_brush_radius(1, brush_radius);
        }
    } else if (key == SDLK_COMMA) {
        cycle_brush_shape_in_place(brush_shape, -1);
    } else if (key == SDLK_PERIOD) {
        cycle_brush_shape_in_place(brush_shape, 1);
    } else if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
        if (*brush_opacity > 1) {
            adjust_brush_opacity(-5, *brush_color_rgb, brush_opacity, brush_color);
        }
    } else if (key == SDLK_EQUALS || key == SDLK_KP_PLUS) {
        if (*brush_opacity < 100) {
            adjust_brush_opacity(5, *brush_color_rgb, brush_opacity, brush_color);
        }
    } else {
        return 0;
    }

    return 1;
}

static int handle_active_edit_hotkey(SDL_Keycode key,
                                     LayerStack *layers,
                                     Snapshot *undo_stack, int *undo_count,
                                     Snapshot *redo_stack, int *redo_count) {
    size_t i;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count) {
        return 0;
    }

    for (i = 0; i < sizeof(ACTIVE_EDIT_HOTKEYS) / sizeof(ACTIVE_EDIT_HOTKEYS[0]); i++) {
        if (ACTIVE_EDIT_HOTKEYS[i].key == key) {
            return ACTIVE_EDIT_HOTKEYS[i].action(layers, undo_stack, undo_count, redo_stack, redo_count);
        }
    }

    return 0;
}

static int handle_mouse_position_hotkey(SDL_Keycode key,
                                        LayerStack *layers,
                                        Snapshot *undo_stack, int *undo_count,
                                        Snapshot *redo_stack, int *redo_count,
                                        const Canvas *sample,
                                        uint32_t *brush_color_rgb, uint32_t *brush_color,
                                        int *brush_opacity, Tool *tool) {
    size_t i;
    int mx = 0;
    int my = 0;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || !sample) {
        return 0;
    }

    SDL_GetMouseState(&mx, &my);
    for (i = 0; i < sizeof(MOUSE_POSITION_HOTKEYS) / sizeof(MOUSE_POSITION_HOTKEYS[0]); i++) {
        if (MOUSE_POSITION_HOTKEYS[i].key == key) {
            return MOUSE_POSITION_HOTKEYS[i].action(layers, undo_stack, undo_count, redo_stack, redo_count,
                                                    sample, mx, my, brush_color_rgb, brush_color,
                                                    brush_opacity, tool);
        }
    }

    return 0;
}

static int handle_general_key_hotkey(SDL_Keycode key,
                                     SDL_Window *window,
                                     LayerStack *layers,
                                     Snapshot *undo_stack, int *undo_count,
                                     Snapshot *redo_stack, int *redo_count,
                                     const Canvas *sample,
                                     uint32_t *brush_color_rgb, uint32_t *brush_color,
                                     int *brush_opacity, int *brush_radius,
                                     BrushShape *brush_shape, Tool *tool,
                                     int *needs_composite) {
    if (handle_brush_state_hotkey(key, brush_color_rgb, brush_color, brush_opacity, brush_radius, brush_shape, tool)) {
        update_window_title(window, layers, *tool, *brush_shape, *brush_radius, *brush_color, *brush_opacity);
        return 1;
    }

    if (handle_active_edit_hotkey(key, layers, undo_stack, undo_count, redo_stack, redo_count)) {
        if (needs_composite) {
            *needs_composite = 1;
        }
        update_window_title(window, layers, *tool, *brush_shape, *brush_radius, *brush_color, *brush_opacity);
        return 1;
    }

    if (handle_mouse_position_hotkey(key, layers, undo_stack, undo_count, redo_stack, redo_count,
                                     sample, brush_color_rgb, brush_color, brush_opacity, tool)) {
        if (needs_composite && key == SDLK_f) {
            *needs_composite = 1;
        }
        update_window_title(window, layers, *tool, *brush_shape, *brush_radius, *brush_color, *brush_opacity);
        return 1;
    }

    update_window_title(window, layers, *tool, *brush_shape, *brush_radius, *brush_color, *brush_opacity);
    return 0;
}

static int handle_selector_hotkey(SDL_Keycode key,
                                  int ctrl, int alt, int shift,
                                  SDL_Window *window,
                                  LayerStack *layers,
                                  Tool tool, BrushShape brush_shape,
                                  int brush_radius, uint32_t brush_color,
                                  int brush_opacity) {
    size_t i;

    if (!window || !layers) {
        return 0;
    }

    for (i = 0; i < sizeof(SELECTOR_HOTKEYS) / sizeof(SELECTOR_HOTKEYS[0]); i++) {
        const SelectorHotkey *hotkey = &SELECTOR_HOTKEYS[i];

        if (hotkey->key == key &&
            hotkey->ctrl == ctrl &&
            hotkey->alt == alt &&
            hotkey->shift == shift) {
            refresh_title_on_change(window, layers, tool, brush_shape, brush_radius,
                                    brush_color, brush_opacity,
                                    hotkey->action(layers, hotkey->arg) >= 0);
            return 1;
        }
    }

    return 0;
}

static int handle_reveal_hotkey(SDL_Keycode key,
                                int ctrl, int alt, int shift,
                                SDL_Window *window,
                                LayerStack *layers,
                                Snapshot *undo_stack, int *undo_count,
                                Snapshot *redo_stack, int *redo_count,
                                Tool tool, BrushShape brush_shape,
                                int brush_radius, uint32_t brush_color,
                                int brush_opacity, int *needs_composite) {
    size_t i;

    if (!window || !layers || !undo_stack || !undo_count || !redo_stack || !redo_count || !needs_composite) {
        return 0;
    }

    for (i = 0; i < sizeof(REVEAL_HOTKEYS) / sizeof(REVEAL_HOTKEYS[0]); i++) {
        const RevealHotkey *hotkey = &REVEAL_HOTKEYS[i];

        if (hotkey->key == key &&
            hotkey->ctrl == ctrl &&
            hotkey->alt == alt &&
            hotkey->shift == shift) {
            run_directional_layer_action(window, layers, undo_stack, undo_count, redo_stack, redo_count,
                                         tool, brush_shape, brush_radius, brush_color, brush_opacity,
                                         needs_composite, hotkey->action, hotkey->direction);
            return 1;
        }
    }

    return 0;
}

static int handle_layer_opacity_hotkey(SDL_Keycode key,
                                       int ctrl, int alt, int shift,
                                       SDL_Window *window,
                                       LayerStack *layers,
                                       Snapshot *undo_stack, int *undo_count,
                                       Snapshot *redo_stack, int *redo_count,
                                       Tool tool, BrushShape brush_shape,
                                       int brush_radius, uint32_t brush_color,
                                       int brush_opacity, int *needs_composite) {
    int changed = 0;

    if (!ctrl || alt || shift || !window || !layers || !undo_stack || !undo_count ||
        !redo_stack || !redo_count || !needs_composite) {
        return 0;
    }

    if (key == SDLK_0) {
        changed = try_adjust_active_layer_opacity(layers, undo_stack, undo_count, redo_stack, redo_count, 100);
    } else if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
        changed = try_nudge_active_layer_opacity(layers, undo_stack, undo_count, redo_stack, redo_count, -10);
    } else if (key == SDLK_EQUALS || key == SDLK_KP_PLUS) {
        changed = try_nudge_active_layer_opacity(layers, undo_stack, undo_count, redo_stack, redo_count, 10);
    } else {
        return 0;
    }

    if (refresh_title_on_change(window, layers, tool, brush_shape, brush_radius, brush_color,
                                brush_opacity, changed)) {
        *needs_composite = 1;
    }

    return 1;
}

static int handle_indexed_layer_hotkey(SDL_Keycode key,
                                       int ctrl, int alt, int shift,
                                       SDL_Window *window,
                                       LayerStack *layers,
                                       Snapshot *undo_stack, int *undo_count,
                                       Snapshot *redo_stack, int *redo_count,
                                       Tool tool, BrushShape brush_shape,
                                       int brush_radius, uint32_t brush_color,
                                       int brush_opacity, int *needs_composite) {
    size_t i;

    if (!window || !layers || !undo_stack || !undo_count || !redo_stack || !redo_count || !needs_composite) {
        return 0;
    }

    for (i = 0; i < sizeof(INDEXED_LAYER_HOTKEYS) / sizeof(INDEXED_LAYER_HOTKEYS[0]); i++) {
        const IndexedLayerHotkey *hotkey = &INDEXED_LAYER_HOTKEYS[i];

        if (hotkey->key == key &&
            hotkey->ctrl == ctrl &&
            hotkey->alt == alt &&
            hotkey->shift == shift) {
            run_indexed_layer_action(window, layers, undo_stack, undo_count, redo_stack, redo_count,
                                     tool, brush_shape, brush_radius, brush_color, brush_opacity,
                                     needs_composite, hotkey->action, hotkey->error_action,
                                     hotkey->mark_composite);
            return 1;
        }
    }

    return 0;
}

static int handle_indexed_layer_silent_hotkey(SDL_Keycode key,
                                              int ctrl, int alt, int shift,
                                              SDL_Window *window,
                                              LayerStack *layers,
                                              Snapshot *undo_stack, int *undo_count,
                                              Snapshot *redo_stack, int *redo_count,
                                              Tool tool, BrushShape brush_shape,
                                              int brush_radius, uint32_t brush_color,
                                              int brush_opacity, int *needs_composite) {
    size_t i;

    if (!window || !layers || !undo_stack || !undo_count || !redo_stack || !redo_count || !needs_composite) {
        return 0;
    }

    for (i = 0; i < sizeof(INDEXED_LAYER_SILENT_HOTKEYS) / sizeof(INDEXED_LAYER_SILENT_HOTKEYS[0]); i++) {
        const IndexedLayerSilentHotkey *hotkey = &INDEXED_LAYER_SILENT_HOTKEYS[i];

        if (hotkey->key == key &&
            hotkey->ctrl == ctrl &&
            hotkey->alt == alt &&
            hotkey->shift == shift) {
            run_indexed_layer_action_silent(window, layers, undo_stack, undo_count, redo_stack, redo_count,
                                            tool, brush_shape, brush_radius, brush_color, brush_opacity,
                                            needs_composite, hotkey->action, hotkey->mark_composite);
            return 1;
        }
    }

    return 0;
}

static int handle_layer_navigation_hotkey(SDL_Keycode key,
                                          int ctrl, int alt, int shift,
                                          SDL_Window *window,
                                          LayerStack *layers,
                                          Tool tool, BrushShape brush_shape,
                                          int brush_radius, uint32_t brush_color,
                                          int brush_opacity) {
    int changed = 0;

    if (!window || !layers || alt || shift) {
        return 0;
    }

    if (ctrl && key >= SDLK_1 && key <= SDLK_8) {
        changed = try_select_layer_index(layers, (int)(key - SDLK_1));
    } else if (!ctrl && key == SDLK_PAGEUP) {
        changed = layer_stack_cycle(layers, 1) >= 0;
    } else if (!ctrl && key == SDLK_PAGEDOWN) {
        changed = layer_stack_cycle(layers, -1) >= 0;
    } else {
        return 0;
    }

    refresh_title_on_change(window, layers, tool, brush_shape, brush_radius, brush_color,
                            brush_opacity, changed);
    return 1;
}

static int handle_file_hotkey(SDL_Keycode key,
                              int ctrl, int alt, int shift,
                              int preview_active,
                              const Canvas *preview_canvas,
                              const Canvas *composite,
                              LayerStack *layers,
                              Snapshot *undo_stack, int *undo_count,
                              Snapshot *redo_stack, int *redo_count,
                              int *needs_composite) {
    const Canvas *save_canvas;

    if (!ctrl || alt || shift || !layers || !undo_stack || !undo_count || !redo_stack || !redo_count ||
        !composite || !needs_composite) {
        return 0;
    }

    if (key == SDLK_s) {
        save_canvas = (preview_active && preview_canvas && preview_canvas->pixels) ? preview_canvas : composite;
        try_save_canvas_to_output(save_canvas);
        return 1;
    }

    if (key == SDLK_o) {
        if (try_load_active_layer_bmp(layers, undo_stack, undo_count, redo_stack, redo_count)) {
            *needs_composite = 1;
        }
        return 1;
    }

    return 0;
}

static int handle_left_click_down(LayerStack *layers,
                                  Snapshot *undo_stack, int *undo_count,
                                  Snapshot *redo_stack, int *redo_count,
                                  Tool tool, int x, int y,
                                  int brush_radius, uint32_t brush_color,
                                  BrushShape brush_shape,
                                  int *drawing, int *needs_composite,
                                  int *shaping, int *shape_start_x, int *shape_start_y,
                                  uint32_t *shape_base_pixels,
                                  const Canvas *composite) {
    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count ||
        !drawing || !needs_composite || !shaping || !shape_start_x || !shape_start_y || !composite) {
        return 0;
    }

    if (tool == TOOL_BRUSH || tool == TOOL_ERASER) {
        if (try_begin_brush_stroke(layers, undo_stack, undo_count, redo_stack, redo_count,
                                   tool, x, y, brush_radius, brush_color, brush_shape)) {
            *drawing = 1;
            *needs_composite = 1;
        }
        return 1;
    }

    if (active_layer_editable(layers)) {
        *shaping = 1;
        *shape_start_x = x;
        *shape_start_y = y;
        if (shape_base_pixels) {
            memcpy(shape_base_pixels, composite->pixels,
                   (size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t));
        }
    }

    return 1;
}

static int handle_right_click_down(SDL_Window *window,
                                   const LayerStack *layers,
                                   int x, int y,
                                   int *shaping, int *preview_active,
                                   const Canvas *preview_canvas,
                                   const Canvas *composite,
                                   uint32_t *brush_color_rgb, uint32_t *brush_color,
                                   int *brush_opacity, int brush_radius,
                                   BrushShape brush_shape, Tool *tool) {
    const Canvas *sample;

    if (!window || !layers || !shaping || !preview_active || !composite ||
        !brush_color_rgb || !brush_color || !brush_opacity || !tool) {
        return 0;
    }

    if (*shaping) {
        cancel_shape_preview(shaping, preview_active);
        return 1;
    }

    sample = (preview_canvas && preview_canvas->pixels) ? preview_canvas : composite;
    handle_right_click_sample(window, layers, sample, x, y, brush_color_rgb, brush_color,
                              brush_opacity, brush_radius, brush_shape, tool);
    return 1;
}

static int handle_mouse_button_down(SDL_Window *window,
                                    LayerStack *layers,
                                    Snapshot *undo_stack, int *undo_count,
                                    Snapshot *redo_stack, int *redo_count,
                                    Uint8 button, int x, int y,
                                    Tool tool, int brush_radius, uint32_t brush_color,
                                    BrushShape brush_shape,
                                    int *last_x, int *last_y,
                                    int *drawing, int *needs_composite,
                                    int *shaping, int *shape_start_x, int *shape_start_y,
                                    int *preview_active,
                                    uint32_t *shape_base_pixels,
                                    const Canvas *preview_canvas,
                                    const Canvas *composite,
                                    uint32_t *brush_color_rgb, uint32_t *brush_color,
                                    int *brush_opacity, Tool *tool_out) {
    if (button == SDL_BUTTON_LEFT) {
        if (last_x) {
            *last_x = x;
        }
        if (last_y) {
            *last_y = y;
        }
        return handle_left_click_down(layers, undo_stack, undo_count, redo_stack, redo_count,
                                      tool, x, y, brush_radius, brush_color, brush_shape,
                                      drawing, needs_composite,
                                      shaping, shape_start_x, shape_start_y,
                                      shape_base_pixels, composite);
    }

    if (button == SDL_BUTTON_RIGHT) {
        return handle_right_click_down(window, layers, x, y,
                                       shaping, preview_active,
                                       preview_canvas, composite,
                                       brush_color_rgb, brush_color,
                                       brush_opacity, brush_radius, brush_shape, tool_out);
    }

    return 0;
}

static int handle_shape_preview_motion(Tool tool,
                                       int shape_start_x, int shape_start_y,
                                       int x, int y,
                                       int brush_radius, uint32_t brush_color,
                                       uint32_t *shape_base_pixels,
                                       Canvas *preview_canvas,
                                       uint32_t *preview_pixels,
                                       int *preview_active) {
    const Uint8 *state;
    int shift;
    int end_x = x;
    int end_y = y;

    if (!shape_base_pixels || !preview_canvas || !preview_canvas->pixels || !preview_pixels || !preview_active) {
        return 0;
    }

    if (x < 0 || y < 0 || x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT) {
        return 0;
    }

    state = SDL_GetKeyboardState(NULL);
    shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    constrain_end(tool, shape_start_x, shape_start_y, end_x, end_y, shift, &end_x, &end_y);
    memcpy(preview_pixels, shape_base_pixels,
           (size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t));
    draw_shape(preview_canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
    *preview_active = 1;
    return 1;
}

static int handle_left_click_up(LayerStack *layers,
                                Snapshot *undo_stack, int *undo_count,
                                Snapshot *redo_stack, int *redo_count,
                                Tool tool, int x, int y,
                                int brush_radius, uint32_t brush_color,
                                int *drawing, int *needs_composite,
                                int *shaping, int shape_start_x, int shape_start_y,
                                int *preview_active) {
    const Uint8 *state;
    int shift;
    int end_x = x;
    int end_y = y;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count ||
        !drawing || !needs_composite || !shaping || !preview_active) {
        return 0;
    }

    *drawing = 0;
    if (!*shaping) {
        return 1;
    }

    state = SDL_GetKeyboardState(NULL);
    shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    constrain_end(tool, shape_start_x, shape_start_y, end_x, end_y, shift, &end_x, &end_y);
    if (try_commit_shape(layers, undo_stack, undo_count, redo_stack, redo_count,
                         tool, shape_start_x, shape_start_y, end_x, end_y,
                         brush_radius, brush_color)) {
        *needs_composite = 1;
    }
    cancel_shape_preview(shaping, preview_active);
    return 1;
}

static int handle_drawing_motion(LayerStack *layers,
                                 Tool tool, int x, int y,
                                 int brush_radius, uint32_t brush_color,
                                 BrushShape brush_shape,
                                 int *last_x, int *last_y,
                                 int *needs_composite) {
    Layer *active;

    if (!layers || !last_x || !last_y || !needs_composite) {
        return 0;
    }

    if (x < 0 || y < 0 || x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }

    if (tool == TOOL_ERASER) {
        erase_line(&active->canvas, *last_x, *last_y, x, y, brush_radius, active_layer_clear_color(layers), brush_shape);
    } else {
        draw_brush_line(&active->canvas, *last_x, *last_y, x, y, brush_radius, brush_color, brush_shape);
    }
    *last_x = x;
    *last_y = y;
    *needs_composite = 1;
    return 1;
}

static int handle_mouse_motion(LayerStack *layers,
                               Tool tool, int x, int y,
                               int brush_radius, uint32_t brush_color,
                               BrushShape brush_shape,
                               int drawing, int *last_x, int *last_y,
                               int shaping, int shape_start_x, int shape_start_y,
                               uint32_t *shape_base_pixels,
                               Canvas *preview_canvas,
                               uint32_t *preview_pixels,
                               int *preview_active,
                               int *needs_composite) {
    if (drawing) {
        return handle_drawing_motion(layers, tool, x, y, brush_radius, brush_color, brush_shape,
                                     last_x, last_y, needs_composite);
    }

    if (shaping) {
        return handle_shape_preview_motion(tool, shape_start_x, shape_start_y, x, y,
                                           brush_radius, brush_color,
                                           shape_base_pixels, preview_canvas,
                                           preview_pixels, preview_active);
    }

    return 0;
}

static int handle_translation_hotkey(SDL_Keycode key,
                                     int ctrl, int alt, int shift,
                                     LayerStack *layers,
                                     Snapshot *undo_stack, int *undo_count,
                                     Snapshot *redo_stack, int *redo_count,
                                     int *needs_composite) {
    int step;
    int dx = 0;
    int dy = 0;

    if (ctrl || alt || !layers || !undo_stack || !undo_count || !redo_stack || !redo_count || !needs_composite) {
        return 0;
    }

    if (key != SDLK_UP && key != SDLK_DOWN && key != SDLK_LEFT && key != SDLK_RIGHT) {
        return 0;
    }

    step = shift ? 10 : 1;
    if (key_translation_delta(key, step, &dx, &dy) &&
        apply_canvas_translation(layers, undo_stack, undo_count, redo_stack, redo_count, dx, dy)) {
        *needs_composite = 1;
    }

    return 1;
}

static int handle_history_hotkey(SDL_Keycode key,
                                 int ctrl, int alt, int shift,
                                 SDL_Window *window,
                                 LayerStack *layers,
                                 Snapshot *undo_stack, int *undo_count,
                                 Snapshot *redo_stack, int *redo_count,
                                 Tool tool, BrushShape brush_shape,
                                 int brush_radius, uint32_t brush_color,
                                 int brush_opacity, int *needs_composite) {
    int changed = 0;

    if (!ctrl || alt || shift || !window || !layers || !undo_stack || !undo_count ||
        !redo_stack || !redo_count || !needs_composite) {
        return 0;
    }

    if (key == SDLK_z) {
        changed = try_restore_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    } else if (key == SDLK_y) {
        changed = try_restore_snapshot(layers, redo_stack, redo_count, undo_stack, undo_count);
    } else {
        return 0;
    }

    if (refresh_title_on_change(window, layers, tool, brush_shape, brush_radius, brush_color,
                                brush_opacity, changed)) {
        *needs_composite = 1;
    }

    return 1;
}

static int handle_right_click_sample(SDL_Window *window,
                                     const LayerStack *layers,
                                     const Canvas *sample,
                                     int x, int y,
                                     uint32_t *brush_color_rgb, uint32_t *brush_color,
                                     int *brush_opacity, int brush_radius,
                                     BrushShape brush_shape, Tool *tool) {
    if (!try_sample_canvas_color(sample, x, y, brush_color_rgb, brush_color, brush_opacity, tool)) {
        return 0;
    }

    update_window_title(window, layers, *tool, brush_shape, brush_radius, *brush_color, *brush_opacity);
    return 1;
}

static int brush_mask_contains(BrushShape shape, int x, int y, int radius) {
    switch (shape) {
    case BRUSH_SHAPE_ROUND:
        return x * x + y * y <= radius * radius;
    case BRUSH_SHAPE_SQUARE:
        return abs(x) <= radius && abs(y) <= radius;
    case BRUSH_SHAPE_DIAMOND:
        return abs(x) + abs(y) <= radius;
    default:
        return 0;
    }
}

static void stamp_brush(Canvas *c, int cx, int cy, int radius, uint32_t color, BrushShape shape) {
    if (!c || !c->pixels || radius <= 0) {
        return;
    }
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (!brush_mask_contains(shape, dx, dy, radius)) {
                continue;
            }
            canvas_set_pixel(c, cx + dx, cy + dy, color);
        }
    }
}

static void draw_shape(Canvas *c, Tool tool, int x0, int y0, int x1, int y1, int radius, uint32_t color) {
    switch (tool) {
    case TOOL_LINE:
        canvas_draw_line(c, x0, y0, x1, y1, radius, color);
        break;
    case TOOL_RECT:
        canvas_draw_rect_outline(c, x0, y0, x1, y1, radius, color);
        break;
    case TOOL_FILLED_RECT:
        canvas_draw_rect_filled(c, x0, y0, x1, y1, color);
        break;
    case TOOL_ELLIPSE: {
        int cx = (x0 + x1) / 2;
        int cy = (y0 + y1) / 2;
        int rx = abs(x1 - x0) / 2;
        int ry = abs(y1 - y0) / 2;
        canvas_draw_ellipse_outline(c, cx, cy, rx, ry, radius, color);
        break;
    }
    case TOOL_FILLED_ELLIPSE: {
        int cx = (x0 + x1) / 2;
        int cy = (y0 + y1) / 2;
        int rx = abs(x1 - x0) / 2;
        int ry = abs(y1 - y0) / 2;
        canvas_draw_ellipse_filled(c, cx, cy, rx, ry, color);
        break;
    }
    default:
        break;
    }
}

static void constrain_end(Tool tool, int x0, int y0, int x1, int y1, int shift, int *out_x, int *out_y) {
    if (!out_x || !out_y) {
        return;
    }
    *out_x = x1;
    *out_y = y1;
    if (!shift) {
        return;
    }

    int dx = x1 - x0;
    int dy = y1 - y0;
    int adx = abs(dx);
    int ady = abs(dy);

    if (tool == TOOL_LINE) {
        if (adx > ady * 2) {
            *out_x = x0 + (dx >= 0 ? adx : -adx);
            *out_y = y0;
        } else if (ady > adx * 2) {
            *out_x = x0;
            *out_y = y0 + (dy >= 0 ? ady : -ady);
        } else {
            int len = adx > ady ? adx : ady;
            *out_x = x0 + (dx >= 0 ? len : -len);
            *out_y = y0 + (dy >= 0 ? len : -len);
        }
    } else if (tool == TOOL_RECT || tool == TOOL_FILLED_RECT || tool == TOOL_ELLIPSE || tool == TOOL_FILLED_ELLIPSE) {
        int len = adx > ady ? adx : ady;
        *out_x = x0 + (dx >= 0 ? len : -len);
        *out_y = y0 + (dy >= 0 ? len : -len);
    }
}

static void cancel_shape_preview(int *shaping, int *preview_active) {
    if (shaping) {
        *shaping = 0;
    }
    if (preview_active) {
        *preview_active = 0;
    }
}

static int should_cancel_shape_on_key(SDL_Keycode key, int ctrl) {
    if (key == SDLK_ESCAPE || key == SDLK_LSHIFT || key == SDLK_RSHIFT) {
        return 0;
    }
    if (ctrl && (key == SDLK_s || key == SDLK_o || key == SDLK_z || key == SDLK_y || key == SDLK_n || key == SDLK_u || key == SDLK_v || key == SDLK_m || key == SDLK_d || key == SDLK_e || key == SDLK_g || key == SDLK_h || key == SDLK_l || key == SDLK_a || key == SDLK_r || key == SDLK_0 || key == SDLK_COMMA || key == SDLK_PERIOD || key == SDLK_SEMICOLON || key == SDLK_QUOTE || key == SDLK_LEFTBRACKET || key == SDLK_RIGHTBRACKET || key == SDLK_MINUS || key == SDLK_KP_MINUS || key == SDLK_EQUALS || key == SDLK_KP_PLUS || key == SDLK_SLASH || key == SDLK_1 || key == SDLK_2 || key == SDLK_3 || key == SDLK_4 || key == SDLK_5 || key == SDLK_6 || key == SDLK_7 || key == SDLK_8)) {
        return 1;
    }
    switch (key) {
    case SDLK_b:
    case SDLK_e:
    case SDLK_l:
    case SDLK_r:
    case SDLK_t:
    case SDLK_o:
    case SDLK_p:
    case SDLK_LEFTBRACKET:
    case SDLK_RIGHTBRACKET:
    case SDLK_COMMA:
    case SDLK_PERIOD:
    case SDLK_SEMICOLON:
    case SDLK_QUOTE:
    case SDLK_MINUS:
    case SDLK_KP_MINUS:
    case SDLK_EQUALS:
    case SDLK_KP_PLUS:
    case SDLK_1:
    case SDLK_2:
    case SDLK_3:
    case SDLK_4:
    case SDLK_5:
    case SDLK_6:
    case SDLK_c:
    case SDLK_h:
    case SDLK_v:
    case SDLK_j:
    case SDLK_x:
    case SDLK_f:
    case SDLK_i:
    case SDLK_UP:
    case SDLK_DOWN:
    case SDLK_LEFT:
    case SDLK_RIGHT:
    case SDLK_PAGEUP:
    case SDLK_PAGEDOWN:
    case SDLK_DELETE:
    case SDLK_BACKSPACE:
        return 1;
    default:
        return 0;
    }
}

static uint32_t active_layer_clear_color(const LayerStack *layers) {
    if (!layers) {
        return COLOR_BG;
    }
    return (layers->active_layer == 0) ? COLOR_BG : 0x00000000;
}

static int active_layer_editable(const LayerStack *layers) {
    const Layer *active = layers ? layer_stack_get(layers, layers->active_layer) : NULL;
    return active && !active->locked && active->canvas.pixels;
}

static int apply_canvas_transform(
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    void (*transform)(Canvas *)
) {
    if (!layers || !transform) {
        return 0;
    }
    Layer *active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }
    push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    transform(&active->canvas);
    return 1;
}

static int apply_canvas_translation(
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int dx,
    int dy
) {
    if (!layers || (dx == 0 && dy == 0)) {
        return 0;
    }
    Layer *active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }
    push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
    canvas_translate(&active->canvas, dx, dy, active_layer_clear_color(layers));
    return 1;
}

static void erase_stamp(Canvas *c, int cx, int cy, int radius, uint32_t clear_color, BrushShape shape) {
    if (!c || !c->pixels || radius <= 0) {
        return;
    }
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (!brush_mask_contains(shape, dx, dy, radius)) {
                continue;
            }
            canvas_set_pixel_raw(c, cx + dx, cy + dy, clear_color);
        }
    }
}

static void erase_line(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t clear_color, BrushShape shape) {
    if (!c || !c->pixels) {
        return;
    }
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        erase_stamp(c, x0, y0, radius, clear_color, shape);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_brush_line(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t color, BrushShape shape) {
    if (!c || !c->pixels) {
        return;
    }
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        stamp_brush(c, x0, y0, radius, color, shape);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

int app_run(const char *input_path) {
    char status_message[128];
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        format_status_text_sdl("SDL_Init", SDL_GetError(), status_message, sizeof(status_message));
        fprintf(stderr, "%s\n", status_message);
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Openshop - Minimal Paint",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        format_status_text_sdl("SDL_CreateWindow", SDL_GetError(), status_message, sizeof(status_message));
        fprintf(stderr, "%s\n", status_message);
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        format_status_text_sdl("SDL_CreateRenderer", SDL_GetError(), status_message, sizeof(status_message));
        fprintf(stderr, "%s\n", status_message);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        CANVAS_WIDTH,
        CANVAS_HEIGHT
    );
    if (!texture) {
        format_status_text_sdl("SDL_CreateTexture", SDL_GetError(), status_message, sizeof(status_message));
        fprintf(stderr, "%s\n", status_message);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    LayerStack layers;
    if (!layer_stack_init(&layers, CANVAS_WIDTH, CANVAS_HEIGHT, COLOR_BG)) {
        format_status_text_startup("Layer stack init", status_message, sizeof(status_message));
        fprintf(stderr, "%s\n", status_message);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Canvas composite = {0};
    if (!canvas_init(&composite, CANVAS_WIDTH, CANVAS_HEIGHT)) {
        format_status_text_startup("Composite canvas init", status_message, sizeof(status_message));
        fprintf(stderr, "%s\n", status_message);
        layer_stack_free(&layers);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (input_path && input_path[0]) {
        Layer *active = layer_stack_active(&layers);
        if (active && !canvas_load_bmp(&active->canvas, input_path, COLOR_BG)) {
            format_status_text_file_load(input_path, status_message, sizeof(status_message));
            fprintf(stderr, "%s\n", status_message);
        }
    }
    layer_stack_composite(&layers, &composite, COLOR_BG);

    int running = 1;
    int drawing = 0;
    int last_x = 0;
    int last_y = 0;
    int brush_radius = 6;
    int brush_opacity = 100;
    uint32_t brush_color_rgb = COLOR_BRUSH & 0x00FFFFFF;
    uint32_t brush_color = compose_brush_color(brush_color_rgb, brush_opacity);
    BrushShape brush_shape = BRUSH_SHAPE_ROUND;
    Tool tool = TOOL_BRUSH;
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count = 0;
    int redo_count = 0;
    int shaping = 0;
    int shape_start_x = 0;
    int shape_start_y = 0;
    int preview_active = 0;
    int needs_composite = 0;
    uint32_t *shape_base_pixels = (uint32_t *)malloc((size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t));
    uint32_t *preview_pixels = (uint32_t *)malloc((size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t));
    Canvas preview_canvas = {CANVAS_WIDTH, CANVAS_HEIGHT, preview_pixels};
    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));
    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_MOUSEBUTTONDOWN:
                handle_mouse_button_down(window, &layers, undo_stack, &undo_count, redo_stack, &redo_count,
                                         e.button.button, e.button.x, e.button.y,
                                         tool, brush_radius, brush_color, brush_shape,
                                         &last_x, &last_y,
                                         &drawing, &needs_composite,
                                         &shaping, &shape_start_x, &shape_start_y,
                                         &preview_active,
                                         shape_base_pixels, &preview_canvas, &composite,
                                         &brush_color_rgb, &brush_color, &brush_opacity, &tool);
                break;
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    handle_left_click_up(&layers, undo_stack, &undo_count, redo_stack, &redo_count,
                                         tool, e.button.x, e.button.y, brush_radius, brush_color,
                                         &drawing, &needs_composite,
                                         &shaping, shape_start_x, shape_start_y, &preview_active);
                }
                break;
            case SDL_MOUSEMOTION:
                handle_mouse_motion(&layers, tool, e.motion.x, e.motion.y,
                                    brush_radius, brush_color, brush_shape,
                                    drawing, &last_x, &last_y,
                                    shaping, shape_start_x, shape_start_y,
                                    shape_base_pixels, &preview_canvas,
                                    preview_pixels, &preview_active,
                                    &needs_composite);
                break;
            case SDL_KEYDOWN: {
                SDL_Keycode key = e.key.keysym.sym;
                const Uint8 *state = SDL_GetKeyboardState(NULL);
                int ctrl = state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
                int shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
                int alt = state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT];

                if (shaping && should_cancel_shape_on_key(key, ctrl)) {
                    cancel_shape_preview(&shaping, &preview_active);
                }

                if (key == SDLK_ESCAPE) {
                    if (shaping) {
                        cancel_shape_preview(&shaping, &preview_active);
                        break;
                    }
                    running = 0;
                    break;
                }

                if (handle_add_layer_hotkey(key, ctrl, alt, shift, window, &layers,
                                            undo_stack, &undo_count, redo_stack, &redo_count,
                                            tool, brush_shape, brush_radius, brush_color,
                                            brush_opacity, &needs_composite)) {
                    break;
                }

                if (handle_indexed_layer_hotkey(key, ctrl, alt, shift, window, &layers,
                                                undo_stack, &undo_count, redo_stack, &redo_count,
                                                tool, brush_shape, brush_radius, brush_color,
                                                brush_opacity, &needs_composite)) {
                    break;
                }

                if (handle_layer_opacity_hotkey(key, ctrl, alt, shift, window, &layers,
                                                undo_stack, &undo_count, redo_stack, &redo_count,
                                                tool, brush_shape, brush_radius, brush_color,
                                                brush_opacity, &needs_composite)) {
                    break;
                }

                if (handle_file_hotkey(key, ctrl, alt, shift, preview_active, &preview_canvas, &composite,
                                       &layers, undo_stack, &undo_count, redo_stack, &redo_count,
                                       &needs_composite)) {
                    break;
                }

                if (handle_history_hotkey(key, ctrl, alt, shift, window, &layers,
                                          undo_stack, &undo_count, redo_stack, &redo_count,
                                          tool, brush_shape, brush_radius, brush_color,
                                          brush_opacity, &needs_composite)) {
                    break;
                }

                if (handle_indexed_layer_silent_hotkey(key, ctrl, alt, shift, window, &layers,
                                                       undo_stack, &undo_count, redo_stack, &redo_count,
                                                       tool, brush_shape, brush_radius, brush_color,
                                                       brush_opacity, &needs_composite)) {
                    break;
                }

                if (handle_selector_hotkey(key, ctrl, alt, shift, window, &layers, tool, brush_shape,
                                           brush_radius, brush_color, brush_opacity)) {
                    break;
                }

                if (handle_reveal_hotkey(key, ctrl, alt, shift, window, &layers,
                                         undo_stack, &undo_count, redo_stack, &redo_count,
                                         tool, brush_shape, brush_radius, brush_color,
                                         brush_opacity, &needs_composite)) {
                    break;
                }

                if (handle_layer_navigation_hotkey(key, ctrl, alt, shift, window, &layers,
                                                   tool, brush_shape, brush_radius, brush_color,
                                                   brush_opacity)) {
                    break;
                }

                if (handle_translation_hotkey(key, ctrl, alt, shift, &layers,
                                              undo_stack, &undo_count, redo_stack, &redo_count,
                                              &needs_composite)) {
                    break;
                }

                {
                    const Canvas *sample = (preview_active && preview_canvas.pixels) ? &preview_canvas : &composite;
                    handle_general_key_hotkey(key, window, &layers, undo_stack, &undo_count, redo_stack, &redo_count,
                                              sample, &brush_color_rgb, &brush_color, &brush_opacity,
                                              &brush_radius, &brush_shape, &tool, &needs_composite);
                }
                break;
            }
            default:
                break;
            }
        }

        if (!preview_active && needs_composite) {
            layer_stack_composite(&layers, &composite, COLOR_BG);
            needs_composite = 0;
        }

        if (preview_active && preview_canvas.pixels) {
            SDL_UpdateTexture(texture, NULL, preview_canvas.pixels, CANVAS_WIDTH * 4);
        } else {
            SDL_UpdateTexture(texture, NULL, composite.pixels, CANVAS_WIDTH * 4);
        }
        SDL_SetRenderDrawColor(renderer, 30, 30, 34, 255);
        SDL_RenderClear(renderer);

        for (int y = 0; y < CANVAS_HEIGHT; y += CHECKER_SIZE) {
            for (int x = 0; x < CANVAS_WIDTH; x += CHECKER_SIZE) {
                int even = ((x / CHECKER_SIZE) + (y / CHECKER_SIZE)) % 2 == 0;
                if (even) {
                    SDL_SetRenderDrawColor(renderer, 232, 232, 236, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 206, 206, 212, 255);
                }
                SDL_Rect cell = {x, y, CHECKER_SIZE, CHECKER_SIZE};
                SDL_RenderFillRect(renderer, &cell);
            }
        }

        SDL_Rect dest = {0, 0, CANVAS_WIDTH, CANVAS_HEIGHT};
        SDL_RenderCopy(renderer, texture, NULL, &dest);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    free(shape_base_pixels);
    free(preview_pixels);
    canvas_free(&composite);
    layer_stack_free(&layers);
    stack_clear(undo_stack, &undo_count);
    stack_clear(redo_stack, &redo_count);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
