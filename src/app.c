#include "app.h"
#include "active_layer_ops.h"
#include "app_hotkey.h"
#include "app_input_rules.h"
#include "brush_render.h"
#include "brush_state.h"
#include "canvas.h"
#include "color_sample.h"
#include "display_canvas.h"
#include "geometry_helpers.h"
#include "image_io.h"
#include "layer_action_history.h"
#include "layer_creation.h"
#include "layer_edit_state.h"
#include "layer_selection.h"
#include "layers.h"
#include "shape_draw.h"
#include "shape_preview_state.h"
#include "snapshot_history.h"
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

typedef struct {
    SDL_Window *window;
    LayerStack *layers;
    Tool *tool;
    BrushShape *brush_shape;
    int *brush_radius;
    uint32_t *brush_color;
    int *brush_opacity;
} TitleState;

typedef struct {
    const TitleState *title_state;
    LayerStack *layers;
    Snapshot *undo_stack;
    int *undo_count;
    Snapshot *redo_stack;
    int *redo_count;
    int *needs_composite;
    int preview_active;
    const Canvas *preview_canvas;
    const Canvas *composite;
} ActionState;

typedef struct {
    SDL_Window *window;
    LayerStack *layers;
    Snapshot *undo_stack;
    int *undo_count;
    Snapshot *redo_stack;
    int *redo_count;
    Tool *tool;
    int *brush_radius;
    uint32_t *brush_color_rgb;
    uint32_t *brush_color;
    int *brush_opacity;
    BrushShape *brush_shape;
    int *last_x;
    int *last_y;
    int *drawing;
    int *needs_composite;
    int *shaping;
    int *shape_start_x;
    int *shape_start_y;
    int *preview_active;
    uint32_t *shape_base_pixels;
    uint32_t *preview_pixels;
    Canvas *preview_canvas;
    const Canvas *composite;
} MouseState;

typedef int (*LayerIndexedActionFn)(LayerStack *layers, int index);

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

typedef struct {
    uint32_t clear_color;
} LoadActiveLayerBmpContext;

static int load_active_layer_bmp_action(LayerStack *layers, void *ctx) {
    Layer *active;
    LoadActiveLayerBmpContext *load_ctx = (LoadActiveLayerBmpContext *)ctx;

    if (!layers || !load_ctx) {
        return 0;
    }
    active = layer_stack_active(layers);
    if (!active) {
        return 0;
    }
    return canvas_load_bmp(&active->canvas, "input.bmp", load_ctx->clear_color);
}

static int try_load_active_layer_bmp(LayerStack *layers,
                                     Snapshot *undo_stack, int *undo_count,
                                     Snapshot *redo_stack, int *redo_count) {
    Layer *active = layer_stack_active(layers);
    LoadActiveLayerBmpContext ctx;
    if (!active || active->locked) {
        fprintf(stderr, "%s\n", status_text_action_error(STATUS_ACTIVE_LAYER_LOCKED));
        return 0;
    }

    ctx.clear_color = active_layer_clear_color(layers, COLOR_BG);
    if (!layer_action_history_apply_custom(layers, undo_stack, undo_count, redo_stack, redo_count,
                                           MAX_HISTORY, load_active_layer_bmp_action, &ctx)) {
        fprintf(stderr, "%s\n", status_text_action_error(STATUS_LOAD_INPUT_BMP));
        return 0;
    }
    return 1;
}

static int try_flood_fill_active_layer(LayerStack *layers,
                                       Snapshot *undo_stack, int *undo_count,
                                       Snapshot *redo_stack, int *redo_count,
                                       int x, int y, uint32_t brush_color) {
    if (x < 0 || y < 0 || x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT) {
        return 0;
    }

    if (!active_layer_try_flood_fill(layers, undo_stack, undo_count, redo_stack, redo_count,
                                     x, y, brush_color, MAX_HISTORY)) {
        fprintf(stderr, "%s\n", status_text_action_error(STATUS_FILL_FAILED));
        return 0;
    }
    return 1;
}

static int try_clear_active_layer(LayerStack *layers,
                                  Snapshot *undo_stack, int *undo_count,
                                  Snapshot *redo_stack, int *redo_count) {
    return active_layer_try_clear(layers, undo_stack, undo_count, redo_stack, redo_count, COLOR_BG, MAX_HISTORY);
}

static int try_flip_horizontal_active_layer(LayerStack *layers,
                                            Snapshot *undo_stack, int *undo_count,
                                            Snapshot *redo_stack, int *redo_count) {
    return active_layer_try_flip_horizontal(layers, undo_stack, undo_count, redo_stack, redo_count, MAX_HISTORY);
}

static int try_flip_vertical_active_layer(LayerStack *layers,
                                          Snapshot *undo_stack, int *undo_count,
                                          Snapshot *redo_stack, int *redo_count) {
    return active_layer_try_flip_vertical(layers, undo_stack, undo_count, redo_stack, redo_count, MAX_HISTORY);
}

static int try_rotate_active_layer_180(LayerStack *layers,
                                       Snapshot *undo_stack, int *undo_count,
                                       Snapshot *redo_stack, int *redo_count) {
    return active_layer_try_rotate_180(layers, undo_stack, undo_count, redo_stack, redo_count, MAX_HISTORY);
}

static int try_invert_active_layer_rgb(LayerStack *layers,
                                       Snapshot *undo_stack, int *undo_count,
                                       Snapshot *redo_stack, int *redo_count) {
    return active_layer_try_invert_rgb(layers, undo_stack, undo_count, redo_stack, redo_count, MAX_HISTORY);
}

static int try_adjust_active_layer_opacity(LayerStack *layers,
                                           Snapshot *undo_stack, int *undo_count,
                                           Snapshot *redo_stack, int *redo_count,
                                           int target_opacity) {
    return active_layer_try_adjust_opacity(layers, undo_stack, undo_count, redo_stack, redo_count,
                                           target_opacity, MAX_HISTORY);
}

static int try_add_layer(LayerStack *layers,
                         Snapshot *undo_stack, int *undo_count,
                         Snapshot *redo_stack, int *redo_count) {
    char status_message[64];

    if (!layer_creation_try_add(layers, undo_stack, undo_count, redo_stack, redo_count,
                                0x00000000, MAX_HISTORY)) {
        format_status_text_max_layers(MAX_LAYERS, status_message, sizeof(status_message));
        fprintf(stderr, "%s\n", status_message);
        return 0;
    }
    return 1;
}

static int handle_add_layer_hotkey(SDL_Keycode key,
                                   int ctrl, int alt, int shift,
                                   const ActionState *action_state) {
    if (!app_is_add_layer_hotkey((int)key, ctrl, alt, shift) ||
        !action_state || !action_state->title_state || !action_state->layers ||
        !action_state->undo_stack || !action_state->undo_count ||
        !action_state->redo_stack || !action_state->redo_count || !action_state->needs_composite) {
        return 0;
    }

    if (try_add_layer(action_state->layers, action_state->undo_stack,
                      action_state->undo_count, action_state->redo_stack,
                      action_state->redo_count)) {
        *action_state->needs_composite = 1;
    }
    update_title_state(action_state->title_state);
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
    if (!layer_action_history_apply_indexed(layers, undo_stack, undo_count, redo_stack, redo_count,
                                            MAX_HISTORY, action, layers->active_layer)) {
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
    if (layer_action_history_apply_directional(layers, undo_stack, undo_count, redo_stack, redo_count,
                                               MAX_HISTORY, action, arg)) {
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
    if (layer_action_history_apply_indexed(layers, undo_stack, undo_count, redo_stack, redo_count,
                                           MAX_HISTORY, action, layers->active_layer) &&
        mark_composite) {
        *needs_composite = 1;
    }
    update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
}

static void destroy_sdl_runtime(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *texture) {
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
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

static void update_title_state(const TitleState *title_state) {
    if (!title_state || !title_state->window || !title_state->layers || !title_state->tool ||
        !title_state->brush_shape || !title_state->brush_radius ||
        !title_state->brush_color || !title_state->brush_opacity) {
        return;
    }

    update_window_title(title_state->window, title_state->layers,
                        *title_state->tool, *title_state->brush_shape,
                        *title_state->brush_radius, *title_state->brush_color,
                        *title_state->brush_opacity);
}

static int refresh_title_state_on_change(const TitleState *title_state, int changed) {
    if (changed) {
        update_title_state(title_state);
    }
    return changed;
}

static int try_nudge_active_layer_opacity(LayerStack *layers,
                                          Snapshot *undo_stack, int *undo_count,
                                          Snapshot *redo_stack, int *redo_count,
                                          int delta_percent) {
    return active_layer_try_nudge_opacity(layers, undo_stack, undo_count, redo_stack, redo_count,
                                          delta_percent, MAX_HISTORY);
}

static int try_commit_shape(LayerStack *layers,
                            Snapshot *undo_stack, int *undo_count,
                            Snapshot *redo_stack, int *redo_count,
                            Tool tool, int shape_start_x, int shape_start_y,
                            int end_x, int end_y, int brush_radius, uint32_t brush_color) {
    return active_layer_try_commit_shape(layers, undo_stack, undo_count, redo_stack, redo_count,
                                         tool, shape_start_x, shape_start_y, end_x, end_y,
                                         brush_radius, brush_color, MAX_HISTORY);
}

static int try_begin_brush_stroke(LayerStack *layers,
                                  Snapshot *undo_stack, int *undo_count,
                                  Snapshot *redo_stack, int *redo_count,
                                  Tool tool, int x, int y, int brush_radius,
                                  uint32_t brush_color, BrushShape brush_shape) {
    return active_layer_try_begin_brush_stroke(layers, undo_stack, undo_count, redo_stack, redo_count,
                                               tool, x, y, brush_radius, brush_color, brush_shape,
                                               COLOR_BG, MAX_HISTORY);
}

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
    return sample_canvas_brush_state(sample, mx, my, brush_color_rgb, brush_color, brush_opacity, tool);
}

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

static const SelectorHotkey *find_matching_selector_hotkey(SDL_Keycode key,
                                                           int ctrl, int alt, int shift) {
    size_t i;

    for (i = 0; i < sizeof(SELECTOR_HOTKEYS) / sizeof(SELECTOR_HOTKEYS[0]); i++) {
        const SelectorHotkey *hotkey = &SELECTOR_HOTKEYS[i];

        if (app_hotkey_matches(key, ctrl, alt, shift,
                               hotkey->key, hotkey->ctrl, hotkey->alt, hotkey->shift)) {
            return hotkey;
        }
    }

    return NULL;
}

static const RevealHotkey *find_matching_reveal_hotkey(SDL_Keycode key,
                                                       int ctrl, int alt, int shift) {
    size_t i;

    for (i = 0; i < sizeof(REVEAL_HOTKEYS) / sizeof(REVEAL_HOTKEYS[0]); i++) {
        const RevealHotkey *hotkey = &REVEAL_HOTKEYS[i];

        if (app_hotkey_matches(key, ctrl, alt, shift,
                               hotkey->key, hotkey->ctrl, hotkey->alt, hotkey->shift)) {
            return hotkey;
        }
    }

    return NULL;
}

static const IndexedLayerHotkey *find_matching_indexed_layer_hotkey(SDL_Keycode key,
                                                                    int ctrl, int alt, int shift) {
    size_t i;

    for (i = 0; i < sizeof(INDEXED_LAYER_HOTKEYS) / sizeof(INDEXED_LAYER_HOTKEYS[0]); i++) {
        const IndexedLayerHotkey *hotkey = &INDEXED_LAYER_HOTKEYS[i];

        if (app_hotkey_matches(key, ctrl, alt, shift,
                               hotkey->key, hotkey->ctrl, hotkey->alt, hotkey->shift)) {
            return hotkey;
        }
    }

    return NULL;
}

static const IndexedLayerSilentHotkey *find_matching_indexed_layer_silent_hotkey(SDL_Keycode key,
                                                                                  int ctrl, int alt, int shift) {
    size_t i;

    for (i = 0; i < sizeof(INDEXED_LAYER_SILENT_HOTKEYS) / sizeof(INDEXED_LAYER_SILENT_HOTKEYS[0]); i++) {
        const IndexedLayerSilentHotkey *hotkey = &INDEXED_LAYER_SILENT_HOTKEYS[i];

        if (app_hotkey_matches(key, ctrl, alt, shift,
                               hotkey->key, hotkey->ctrl, hotkey->alt, hotkey->shift)) {
            return hotkey;
        }
    }

    return NULL;
}

static int handle_brush_state_hotkey(SDL_Keycode key,
                                     uint32_t *brush_color_rgb, uint32_t *brush_color,
                                     int *brush_opacity, int *brush_radius,
                                     BrushShape *brush_shape, Tool *tool) {
    AppBrushAdjustAction adjust_action;
    AppBrushPresetAction preset_action;
    AppBrushToolAction tool_action;

    if (!brush_color_rgb || !brush_color || !brush_opacity || !brush_radius || !brush_shape || !tool) {
        return 0;
    }

    preset_action = app_brush_preset_hotkey_action((int)key);
    switch (preset_action) {
    case APP_BRUSH_PRESET_DEFAULT:
        brush_state_set_color_tool(COLOR_BRUSH, *brush_opacity, brush_color_rgb, brush_color, tool, TOOL_BRUSH);
        return 1;
    case APP_BRUSH_PRESET_ERASE:
        brush_state_set_color_tool(COLOR_ERASE, *brush_opacity, brush_color_rgb, brush_color, tool, TOOL_ERASER);
        return 1;
    case APP_BRUSH_PRESET_RED:
        brush_state_set_color_tool(COLOR_RED, *brush_opacity, brush_color_rgb, brush_color, tool, TOOL_BRUSH);
        return 1;
    case APP_BRUSH_PRESET_GREEN:
        brush_state_set_color_tool(COLOR_GREEN, *brush_opacity, brush_color_rgb, brush_color, tool, TOOL_BRUSH);
        return 1;
    case APP_BRUSH_PRESET_BLUE:
        brush_state_set_color_tool(COLOR_BLUE, *brush_opacity, brush_color_rgb, brush_color, tool, TOOL_BRUSH);
        return 1;
    case APP_BRUSH_PRESET_YELLOW:
        brush_state_set_color_tool(COLOR_YELLOW, *brush_opacity, brush_color_rgb, brush_color, tool, TOOL_BRUSH);
        return 1;
    case APP_BRUSH_PRESET_PURPLE:
        brush_state_set_color_tool(COLOR_PURPLE, *brush_opacity, brush_color_rgb, brush_color, tool, TOOL_BRUSH);
        return 1;
    case APP_BRUSH_PRESET_NONE:
    default:
        break;
    }

    tool_action = app_brush_tool_hotkey_action((int)key);
    switch (tool_action) {
    case APP_BRUSH_TOOL_LINE:
        brush_state_set_tool(TOOL_LINE, tool);
        return 1;
    case APP_BRUSH_TOOL_RECT:
        brush_state_set_tool(TOOL_RECT, tool);
        return 1;
    case APP_BRUSH_TOOL_FILLED_RECT:
        brush_state_set_tool(TOOL_FILLED_RECT, tool);
        return 1;
    case APP_BRUSH_TOOL_ELLIPSE:
        brush_state_set_tool(TOOL_ELLIPSE, tool);
        return 1;
    case APP_BRUSH_TOOL_FILLED_ELLIPSE:
        brush_state_set_tool(TOOL_FILLED_ELLIPSE, tool);
        return 1;
    case APP_BRUSH_TOOL_NONE:
    default:
        break;
    }

    adjust_action = app_brush_adjust_hotkey_action((int)key);
    if (adjust_action == APP_BRUSH_ADJUST_RADIUS_DOWN) {
        if (*brush_radius > 1) {
            brush_state_adjust_radius(-1, brush_radius);
        }
    } else if (adjust_action == APP_BRUSH_ADJUST_RADIUS_UP) {
        if (*brush_radius < 64) {
            brush_state_adjust_radius(1, brush_radius);
        }
    } else if (adjust_action == APP_BRUSH_ADJUST_SHAPE_PREV) {
        brush_state_cycle_shape_in_place(brush_shape, -1);
    } else if (adjust_action == APP_BRUSH_ADJUST_SHAPE_NEXT) {
        brush_state_cycle_shape_in_place(brush_shape, 1);
    } else if (adjust_action == APP_BRUSH_ADJUST_OPACITY_DOWN) {
        if (*brush_opacity > 1) {
            brush_state_adjust_opacity(-5, *brush_color_rgb, brush_opacity, brush_color);
        }
    } else if (adjust_action == APP_BRUSH_ADJUST_OPACITY_UP) {
        if (*brush_opacity < 100) {
            brush_state_adjust_opacity(5, *brush_color_rgb, brush_opacity, brush_color);
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
    AppActiveEditAction action;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count) {
        return 0;
    }

    action = app_active_edit_hotkey_action((int)key);
    switch (action) {
    case APP_ACTIVE_EDIT_CLEAR:
        return try_clear_active_layer(layers, undo_stack, undo_count, redo_stack, redo_count);
    case APP_ACTIVE_EDIT_FLIP_HORIZONTAL:
        return try_flip_horizontal_active_layer(layers, undo_stack, undo_count, redo_stack, redo_count);
    case APP_ACTIVE_EDIT_FLIP_VERTICAL:
        return try_flip_vertical_active_layer(layers, undo_stack, undo_count, redo_stack, redo_count);
    case APP_ACTIVE_EDIT_ROTATE_180:
        return try_rotate_active_layer_180(layers, undo_stack, undo_count, redo_stack, redo_count);
    case APP_ACTIVE_EDIT_INVERT_RGB:
        return try_invert_active_layer_rgb(layers, undo_stack, undo_count, redo_stack, redo_count);
    case APP_ACTIVE_EDIT_NONE:
    default:
        return 0;
    }
}

static int handle_mouse_position_hotkey(SDL_Keycode key,
                                        LayerStack *layers,
                                        Snapshot *undo_stack, int *undo_count,
                                        Snapshot *redo_stack, int *redo_count,
                                        const Canvas *sample,
                                        uint32_t *brush_color_rgb, uint32_t *brush_color,
                                        int *brush_opacity, Tool *tool,
                                        AppMousePositionAction *matched_action) {
    AppMousePositionAction action;
    int mx = 0;
    int my = 0;

    if (!layers || !undo_stack || !undo_count || !redo_stack || !redo_count || !sample) {
        return 0;
    }

    if (matched_action) {
        *matched_action = APP_MOUSE_POSITION_NONE;
    }
    SDL_GetMouseState(&mx, &my);
    action = app_mouse_position_hotkey_action((int)key);
    if (matched_action) {
        *matched_action = action;
    }
    switch (action) {
    case APP_MOUSE_POSITION_FILL:
        return mouse_position_fill_hotkey(layers, undo_stack, undo_count, redo_stack, redo_count,
                                          sample, mx, my, brush_color_rgb, brush_color,
                                          brush_opacity, tool);
    case APP_MOUSE_POSITION_SAMPLE:
        return mouse_position_sample_hotkey(layers, undo_stack, undo_count, redo_stack, redo_count,
                                            sample, mx, my, brush_color_rgb, brush_color,
                                            brush_opacity, tool);
    case APP_MOUSE_POSITION_NONE:
    default:
        return 0;
    }
}

static int handle_general_key_hotkey(SDL_Keycode key,
                                     const ActionState *action_state,
                                     const Canvas *sample,
                                     uint32_t *brush_color_rgb, uint32_t *brush_color,
                                     int *brush_opacity, int *brush_radius,
                                     BrushShape *brush_shape, Tool *tool,
                                     int *needs_composite) {
    AppMousePositionAction mouse_position_action = APP_MOUSE_POSITION_NONE;

    if (!action_state) {
        return 0;
    }

    if (handle_brush_state_hotkey(key, brush_color_rgb, brush_color, brush_opacity, brush_radius, brush_shape, tool)) {
        update_title_state(action_state->title_state);
        return 1;
    }

    if (handle_active_edit_hotkey(key, action_state->layers, action_state->undo_stack,
                                  action_state->undo_count, action_state->redo_stack,
                                  action_state->redo_count)) {
        if (needs_composite) {
            *needs_composite = 1;
        }
        update_title_state(action_state->title_state);
        return 1;
    }

    if (handle_mouse_position_hotkey(key, action_state->layers, action_state->undo_stack,
                                     action_state->undo_count, action_state->redo_stack,
                                     action_state->redo_count,
                                     sample, brush_color_rgb, brush_color, brush_opacity, tool,
                                     &mouse_position_action)) {
        if (needs_composite && app_mouse_position_marks_composite(mouse_position_action)) {
            *needs_composite = 1;
        }
        update_title_state(action_state->title_state);
        return 1;
    }

    update_title_state(action_state->title_state);
    return 0;
}

static int handle_selector_hotkey(SDL_Keycode key,
                                  int ctrl, int alt, int shift,
                                  const ActionState *action_state) {
    const SelectorHotkey *hotkey;

    if (!action_state || !action_state->title_state || !action_state->layers) {
        return 0;
    }

    hotkey = find_matching_selector_hotkey(key, ctrl, alt, shift);
    if (!hotkey) {
        return 0;
    }

    refresh_title_state_on_change(action_state->title_state,
                                  hotkey->action(action_state->layers, hotkey->arg) >= 0);
    return 1;
}

static int handle_reveal_hotkey(SDL_Keycode key,
                                int ctrl, int alt, int shift,
                                const ActionState *action_state) {
    const RevealHotkey *hotkey;

    if (!action_state || !action_state->title_state || !action_state->title_state->window ||
        !action_state->layers || !action_state->undo_stack || !action_state->undo_count ||
        !action_state->redo_stack || !action_state->redo_count || !action_state->needs_composite) {
        return 0;
    }

    hotkey = find_matching_reveal_hotkey(key, ctrl, alt, shift);
    if (!hotkey) {
        return 0;
    }

    run_directional_layer_action(action_state->title_state->window, action_state->layers,
                                 action_state->undo_stack, action_state->undo_count,
                                 action_state->redo_stack, action_state->redo_count,
                                 *action_state->title_state->tool, *action_state->title_state->brush_shape,
                                 *action_state->title_state->brush_radius, *action_state->title_state->brush_color,
                                 *action_state->title_state->brush_opacity,
                                 action_state->needs_composite, hotkey->action, hotkey->direction);
    return 1;
}

static int handle_layer_opacity_hotkey(SDL_Keycode key,
                                       int ctrl, int alt, int shift,
                                       const ActionState *action_state) {
    AppOpacityHotkeyAction opacity_action;
    int changed = 0;

    if (!action_state || !action_state->title_state || !action_state->layers ||
        !action_state->undo_stack || !action_state->undo_count || !action_state->redo_stack ||
        !action_state->redo_count || !action_state->needs_composite) {
        return 0;
    }

    opacity_action = app_opacity_hotkey_action(key);
    if (opacity_action == APP_OPACITY_HOTKEY_SET_MAX) {
        changed = try_adjust_active_layer_opacity(action_state->layers, action_state->undo_stack,
                                                  action_state->undo_count, action_state->redo_stack,
                                                  action_state->redo_count, 100);
    } else if (opacity_action == APP_OPACITY_HOTKEY_NUDGE_DOWN) {
        changed = try_nudge_active_layer_opacity(action_state->layers, action_state->undo_stack,
                                                 action_state->undo_count, action_state->redo_stack,
                                                 action_state->redo_count, -10);
    } else if (opacity_action == APP_OPACITY_HOTKEY_NUDGE_UP) {
        changed = try_nudge_active_layer_opacity(action_state->layers, action_state->undo_stack,
                                                 action_state->undo_count, action_state->redo_stack,
                                                 action_state->redo_count, 10);
    } else {
        return 0;
    }

    if (refresh_title_state_on_change(action_state->title_state, changed)) {
        *action_state->needs_composite = 1;
    }

    return 1;
}

static int handle_indexed_layer_hotkey(SDL_Keycode key,
                                       int ctrl, int alt, int shift,
                                       const ActionState *action_state) {
    const IndexedLayerHotkey *hotkey;

    if (!action_state || !action_state->title_state || !action_state->title_state->window ||
        !action_state->layers || !action_state->undo_stack || !action_state->undo_count ||
        !action_state->redo_stack || !action_state->redo_count || !action_state->needs_composite) {
        return 0;
    }

    hotkey = find_matching_indexed_layer_hotkey(key, ctrl, alt, shift);
    if (!hotkey) {
        return 0;
    }

    run_indexed_layer_action(action_state->title_state->window, action_state->layers,
                             action_state->undo_stack, action_state->undo_count,
                             action_state->redo_stack, action_state->redo_count,
                             *action_state->title_state->tool, *action_state->title_state->brush_shape,
                             *action_state->title_state->brush_radius, *action_state->title_state->brush_color,
                             *action_state->title_state->brush_opacity,
                             action_state->needs_composite, hotkey->action, hotkey->error_action,
                             hotkey->mark_composite);
    return 1;
}

static int handle_indexed_layer_silent_hotkey(SDL_Keycode key,
                                              int ctrl, int alt, int shift,
                                              const ActionState *action_state) {
    const IndexedLayerSilentHotkey *hotkey;

    if (!action_state || !action_state->title_state || !action_state->title_state->window ||
        !action_state->layers || !action_state->undo_stack || !action_state->undo_count ||
        !action_state->redo_stack || !action_state->redo_count || !action_state->needs_composite) {
        return 0;
    }

    hotkey = find_matching_indexed_layer_silent_hotkey(key, ctrl, alt, shift);
    if (!hotkey) {
        return 0;
    }

    run_indexed_layer_action_silent(action_state->title_state->window, action_state->layers,
                                    action_state->undo_stack, action_state->undo_count,
                                    action_state->redo_stack, action_state->redo_count,
                                    *action_state->title_state->tool, *action_state->title_state->brush_shape,
                                    *action_state->title_state->brush_radius, *action_state->title_state->brush_color,
                                    *action_state->title_state->brush_opacity,
                                    action_state->needs_composite, hotkey->action, hotkey->mark_composite);
    return 1;
}

static int handle_layer_navigation_hotkey(SDL_Keycode key,
                                          int ctrl, int alt, int shift,
                                          const ActionState *action_state) {
    AppLayerNavigationAction nav_action;
    int arg = 0;
    int changed = 0;

    if (!action_state || !action_state->title_state || !action_state->layers) {
        return 0;
    }

    nav_action = app_layer_navigation_action(key, ctrl, alt, shift, &arg);
    if (nav_action == APP_LAYER_NAV_SELECT_INDEX) {
        changed = layer_selection_try_select_index(action_state->layers, arg);
    } else if (nav_action == APP_LAYER_NAV_CYCLE_UP) {
        changed = layer_stack_cycle(action_state->layers, 1) >= 0;
    } else if (nav_action == APP_LAYER_NAV_CYCLE_DOWN) {
        changed = layer_stack_cycle(action_state->layers, -1) >= 0;
    } else {
        return 0;
    }

    refresh_title_state_on_change(action_state->title_state, changed);
    return 1;
}

static int handle_file_hotkey(SDL_Keycode key,
                              int ctrl, int alt, int shift,
                              const ActionState *action_state) {
    AppFileHotkeyAction file_action;
    const Canvas *save_canvas;

    if (!action_state || !action_state->layers || !action_state->undo_stack ||
        !action_state->undo_count || !action_state->redo_stack || !action_state->redo_count ||
        !action_state->composite || !action_state->needs_composite) {
        return 0;
    }

    file_action = app_file_hotkey_action((int)key, ctrl, alt, shift);
    if (file_action == APP_FILE_HOTKEY_SAVE) {
        save_canvas = current_display_canvas(action_state->preview_active,
                                             action_state->preview_canvas,
                                             action_state->composite);
        try_save_canvas_to_output(save_canvas);
        return 1;
    }

    if (file_action == APP_FILE_HOTKEY_LOAD_ACTIVE_LAYER) {
        if (try_load_active_layer_bmp(action_state->layers, action_state->undo_stack,
                                      action_state->undo_count, action_state->redo_stack,
                                      action_state->redo_count)) {
            *action_state->needs_composite = 1;
        }
        return 1;
    }

    return 0;
}

static int handle_left_click_down(const MouseState *mouse_state, int x, int y) {
    if (!mouse_state || !mouse_state->layers || !mouse_state->undo_stack || !mouse_state->undo_count ||
        !mouse_state->redo_stack || !mouse_state->redo_count || !mouse_state->tool ||
        !mouse_state->brush_radius || !mouse_state->brush_color || !mouse_state->brush_shape ||
        !mouse_state->drawing || !mouse_state->needs_composite || !mouse_state->shaping ||
        !mouse_state->shape_start_x || !mouse_state->shape_start_y || !mouse_state->composite) {
        return 0;
    }

    if (*mouse_state->tool == TOOL_BRUSH || *mouse_state->tool == TOOL_ERASER) {
        if (try_begin_brush_stroke(mouse_state->layers, mouse_state->undo_stack, mouse_state->undo_count,
                                   mouse_state->redo_stack, mouse_state->redo_count,
                                   *mouse_state->tool, x, y, *mouse_state->brush_radius,
                                   *mouse_state->brush_color, *mouse_state->brush_shape)) {
            *mouse_state->drawing = 1;
            *mouse_state->needs_composite = 1;
        }
        return 1;
    }

    shape_preview_begin_if_editable(mouse_state->layers, x, y, mouse_state->composite,
                                    mouse_state->shape_base_pixels, mouse_state->shaping,
                                    mouse_state->shape_start_x, mouse_state->shape_start_y);

    return 1;
}

static int handle_right_click_down(const MouseState *mouse_state, int x, int y) {
    if (!mouse_state || !mouse_state->window || !mouse_state->layers || !mouse_state->shaping ||
        !mouse_state->preview_active || !mouse_state->composite || !mouse_state->brush_color_rgb ||
        !mouse_state->brush_color || !mouse_state->brush_opacity || !mouse_state->brush_radius ||
        !mouse_state->brush_shape || !mouse_state->tool || !mouse_state->preview_canvas) {
        return 0;
    }

    if (*mouse_state->shaping) {
        shape_preview_cancel(mouse_state->shaping, mouse_state->preview_active);
        return 1;
    }

    handle_right_click_sample(mouse_state->window, mouse_state->layers,
                              current_display_canvas(*mouse_state->preview_active,
                                                     mouse_state->preview_canvas,
                                                     mouse_state->composite),
                              x, y, mouse_state->brush_color_rgb, mouse_state->brush_color,
                              mouse_state->brush_opacity, *mouse_state->brush_radius,
                              *mouse_state->brush_shape, mouse_state->tool);
    return 1;
}

static int handle_mouse_button_down(const MouseState *mouse_state, Uint8 button, int x, int y) {
    if (button == SDL_BUTTON_LEFT) {
        if (mouse_state->last_x) {
            *mouse_state->last_x = x;
        }
        if (mouse_state->last_y) {
            *mouse_state->last_y = y;
        }
        return handle_left_click_down(mouse_state, x, y);
    }

    if (button == SDL_BUTTON_RIGHT) {
        return handle_right_click_down(mouse_state, x, y);
    }

    return 0;
}

static int handle_mouse_button_up(const MouseState *mouse_state, Uint8 button, int x, int y) {
    if (button == SDL_BUTTON_LEFT) {
        if (!mouse_state || !mouse_state->layers || !mouse_state->undo_stack || !mouse_state->undo_count ||
            !mouse_state->redo_stack || !mouse_state->redo_count || !mouse_state->tool ||
            !mouse_state->brush_radius || !mouse_state->brush_color || !mouse_state->drawing ||
            !mouse_state->needs_composite || !mouse_state->shaping || !mouse_state->shape_start_x ||
            !mouse_state->shape_start_y || !mouse_state->preview_active) {
            return 0;
        }
        return handle_left_click_up(mouse_state->layers, mouse_state->undo_stack, mouse_state->undo_count,
                                    mouse_state->redo_stack, mouse_state->redo_count,
                                    *mouse_state->tool, x, y, *mouse_state->brush_radius, *mouse_state->brush_color,
                                    mouse_state->drawing, mouse_state->needs_composite,
                                    mouse_state->shaping, *mouse_state->shape_start_x, *mouse_state->shape_start_y,
                                    mouse_state->preview_active);
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
    constrain_shape_end(tool, shape_start_x, shape_start_y, end_x, end_y, shift, &end_x, &end_y);
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
    constrain_shape_end(tool, shape_start_x, shape_start_y, end_x, end_y, shift, &end_x, &end_y);
    if (try_commit_shape(layers, undo_stack, undo_count, redo_stack, redo_count,
                         tool, shape_start_x, shape_start_y, end_x, end_y,
                         brush_radius, brush_color)) {
        *needs_composite = 1;
    }
    shape_preview_cancel(shaping, preview_active);
    return 1;
}

static int handle_drawing_motion(LayerStack *layers,
                                 Tool tool, int x, int y,
                                 int brush_radius, uint32_t brush_color,
                                 BrushShape brush_shape,
                                 int *last_x, int *last_y,
                                 int *needs_composite) {
    if (!layers || !last_x || !last_y || !needs_composite) {
        return 0;
    }
    if (!active_layer_continue_brush_stroke(layers, tool, x, y, brush_radius, brush_color,
                                            brush_shape, last_x, last_y, COLOR_BG)) {
        return 0;
    }
    *needs_composite = 1;
    return 1;
}

static int handle_mouse_motion(const MouseState *mouse_state, int x, int y) {
    if (!mouse_state || !mouse_state->tool || !mouse_state->brush_radius || !mouse_state->brush_color ||
        !mouse_state->brush_shape || !mouse_state->drawing || !mouse_state->last_x || !mouse_state->last_y ||
        !mouse_state->shaping || !mouse_state->shape_start_x || !mouse_state->shape_start_y ||
        !mouse_state->preview_canvas || !mouse_state->preview_pixels || !mouse_state->preview_active ||
        !mouse_state->needs_composite) {
        return 0;
    }

    if (*mouse_state->drawing) {
        return handle_drawing_motion(mouse_state->layers, *mouse_state->tool, x, y,
                                     *mouse_state->brush_radius, *mouse_state->brush_color, *mouse_state->brush_shape,
                                     mouse_state->last_x, mouse_state->last_y, mouse_state->needs_composite);
    }

    if (*mouse_state->shaping) {
        return handle_shape_preview_motion(*mouse_state->tool, *mouse_state->shape_start_x, *mouse_state->shape_start_y, x, y,
                                           *mouse_state->brush_radius, *mouse_state->brush_color,
                                           mouse_state->shape_base_pixels, mouse_state->preview_canvas,
                                           mouse_state->preview_pixels, mouse_state->preview_active);
    }

    return 0;
}

static int handle_translation_hotkey(SDL_Keycode key,
                                     int ctrl, int alt, int shift,
                                     const ActionState *action_state) {
    int step;
    int dx = 0;
    int dy = 0;

    if (ctrl || alt || !action_state || !action_state->layers || !action_state->undo_stack ||
        !action_state->undo_count || !action_state->redo_stack || !action_state->redo_count ||
        !action_state->needs_composite) {
        return 0;
    }

    step = shift ? 10 : 1;
    if (app_key_translation_delta(key, step, &dx, &dy) &&
        active_layer_apply_translation(action_state->layers, action_state->undo_stack, action_state->undo_count,
                                       action_state->redo_stack, action_state->redo_count,
                                       dx, dy, COLOR_BG, MAX_HISTORY)) {
        *action_state->needs_composite = 1;
    }

    return 1;
}

static int handle_history_hotkey(SDL_Keycode key,
                                 int ctrl, int alt, int shift,
                                 const ActionState *action_state) {
    AppHistoryHotkeyAction history_action;
    int changed = 0;

    if (!action_state || !action_state->title_state || !action_state->layers ||
        !action_state->undo_stack || !action_state->undo_count || !action_state->redo_stack ||
        !action_state->redo_count || !action_state->needs_composite) {
        return 0;
    }

    history_action = app_history_hotkey_action((int)key, ctrl, alt, shift);
    if (history_action == APP_HISTORY_HOTKEY_UNDO) {
        changed = snapshot_restore(action_state->layers, action_state->undo_stack,
                                   action_state->undo_count, action_state->redo_stack,
                                   action_state->redo_count, MAX_HISTORY);
    } else if (history_action == APP_HISTORY_HOTKEY_REDO) {
        changed = snapshot_restore(action_state->layers, action_state->redo_stack,
                                   action_state->redo_count, action_state->undo_stack,
                                   action_state->undo_count, MAX_HISTORY);
    } else {
        return 0;
    }

    if (refresh_title_state_on_change(action_state->title_state, changed)) {
        *action_state->needs_composite = 1;
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
    if (!sample_canvas_brush_state(sample, x, y, brush_color_rgb, brush_color, brush_opacity, tool)) {
        return 0;
    }

    update_window_title(window, layers, *tool, brush_shape, brush_radius, *brush_color, *brush_opacity);
    return 1;
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
        destroy_sdl_runtime(NULL, NULL, NULL);
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        format_status_text_sdl("SDL_CreateRenderer", SDL_GetError(), status_message, sizeof(status_message));
        fprintf(stderr, "%s\n", status_message);
        destroy_sdl_runtime(window, NULL, NULL);
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
        destroy_sdl_runtime(window, renderer, NULL);
        return 1;
    }

    LayerStack layers;
    if (!layer_stack_init(&layers, CANVAS_WIDTH, CANVAS_HEIGHT, COLOR_BG)) {
        format_status_text_startup("Layer stack init", status_message, sizeof(status_message));
        fprintf(stderr, "%s\n", status_message);
        destroy_sdl_runtime(window, renderer, texture);
        return 1;
    }

    Canvas composite = {0};
    if (!canvas_init(&composite, CANVAS_WIDTH, CANVAS_HEIGHT)) {
        format_status_text_startup("Composite canvas init", status_message, sizeof(status_message));
        fprintf(stderr, "%s\n", status_message);
        layer_stack_free(&layers);
        destroy_sdl_runtime(window, renderer, texture);
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
    TitleState title_state = {window, &layers, &tool, &brush_shape, &brush_radius, &brush_color, &brush_opacity};
    ActionState action_state = {&title_state, &layers, undo_stack, &undo_count, redo_stack, &redo_count,
                                &needs_composite, 0, &preview_canvas, &composite};
    MouseState mouse_state = {window, &layers, undo_stack, &undo_count, redo_stack, &redo_count,
                              &tool, &brush_radius, &brush_color_rgb, &brush_color, &brush_opacity,
                              &brush_shape, &last_x, &last_y, &drawing, &needs_composite,
                              &shaping, &shape_start_x, &shape_start_y, &preview_active,
                              shape_base_pixels, preview_pixels, &preview_canvas, &composite};
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
                handle_mouse_button_down(&mouse_state, e.button.button, e.button.x, e.button.y);
                break;
            case SDL_MOUSEBUTTONUP:
                handle_mouse_button_up(&mouse_state, e.button.button, e.button.x, e.button.y);
                break;
            case SDL_MOUSEMOTION:
                handle_mouse_motion(&mouse_state, e.motion.x, e.motion.y);
                break;
            case SDL_KEYDOWN: {
                SDL_Keycode key = e.key.keysym.sym;
                const Uint8 *state = SDL_GetKeyboardState(NULL);
                int ctrl = state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
                int shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
                int alt = state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT];
                action_state.preview_active = preview_active;

                if (shaping && app_should_cancel_shape_on_key(key, ctrl)) {
                    shape_preview_cancel(&shaping, &preview_active);
                }

                if (key == SDLK_ESCAPE) {
                    if (shaping) {
                        shape_preview_cancel(&shaping, &preview_active);
                        break;
                    }
                    running = 0;
                    break;
                }

                if (handle_add_layer_hotkey(key, ctrl, alt, shift, &action_state)) {
                    break;
                }

                if (handle_indexed_layer_hotkey(key, ctrl, alt, shift, &action_state)) {
                    break;
                }

                if (handle_layer_opacity_hotkey(key, ctrl, alt, shift, &action_state)) {
                    break;
                }

                if (handle_file_hotkey(key, ctrl, alt, shift, &action_state)) {
                    break;
                }

                if (handle_history_hotkey(key, ctrl, alt, shift, &action_state)) {
                    break;
                }

                if (handle_indexed_layer_silent_hotkey(key, ctrl, alt, shift, &action_state)) {
                    break;
                }

                if (handle_selector_hotkey(key, ctrl, alt, shift, &action_state)) {
                    break;
                }

                if (handle_reveal_hotkey(key, ctrl, alt, shift, &action_state)) {
                    break;
                }

                if (handle_layer_navigation_hotkey(key, ctrl, alt, shift, &action_state)) {
                    break;
                }

                if (handle_translation_hotkey(key, ctrl, alt, shift, &action_state)) {
                    break;
                }

                {
                    const Canvas *sample = current_display_canvas(preview_active, &preview_canvas, &composite);
                    handle_general_key_hotkey(key, &action_state,
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

        SDL_UpdateTexture(texture, NULL, current_display_pixels(preview_active, &preview_canvas, &composite), CANVAS_WIDTH * 4);
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
    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    destroy_sdl_runtime(window, renderer, texture);
    return 0;
}
