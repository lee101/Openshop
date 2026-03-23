#include "app.h"
#include "app_document.h"
#include "app_history.h"
#include "app_layer_stack.h"
#include "app_navigation.h"
#include "app_session.h"
#include "app_tool.h"
#include "app_translation.h"
#include "canvas.h"
#include "image_io.h"
#include "layers.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define CANVAS_WIDTH 800
#define CANVAS_HEIGHT 600

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

typedef struct AppRuntime AppRuntime;

static void push_runtime_snapshot(const LayerStack *layers, AppRuntime *runtime);
static int restore_runtime_history(LayerStack *layers, AppRuntime *runtime, int redo_to_undo);
static int apply_runtime_canvas_transform(LayerStack *layers, AppRuntime *runtime, void (*transform)(Canvas *));
static int apply_runtime_canvas_translation(LayerStack *layers, AppRuntime *runtime, int dx, int dy);
static int save_document_canvas(const Canvas *canvas, const char *path, void *userdata);
static int load_document_canvas(Canvas *canvas, const char *path, uint32_t clear_color, void *userdata);
static int restore_document_history(LayerStack *layers, int redo_to_undo, void *userdata);
static void push_document_snapshot(const LayerStack *layers, void *userdata);

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
    if (!window || !layers) {
        return;
    }
    const Layer *active = layer_stack_get(layers, layers->active_layer);
    const char *layer_name = active && active->name[0] ? active->name : "Layer";
    int visible_layers = layer_stack_visible_count(layers);
    char title[256];
    snprintf(
        title,
        sizeof(title),
        "Openshop - %s (%s) | size %d | brush %d%% | layer %d/%d %s [%s%s %d%%]%s | visible %d/%d | #%08X",
        tool_label(tool),
        brush_shape_label(brush_shape),
        radius,
        opacity_percent,
        layers->active_layer + 1,
        layers->layer_count,
        layer_name,
        active && active->visible ? "visible" : "hidden",
        active && active->locked ? ", locked" : "",
        active ? active->opacity_percent : 100,
        (layers->solo_index == layers->active_layer) ? " [solo]" : "",
        visible_layers,
        layers->layer_count,
        color
    );
    SDL_SetWindowTitle(window, title);
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

static int handle_layer_navigation_shortcut(
    SDL_Keycode key,
    int ctrl,
    int shift,
    int alt,
    LayerStack *layers,
    SDL_Window *window,
    AppRuntime *runtime
) {
    AppNavigationCommand command = app_navigation_command_for_key((int)key, ctrl, shift, alt);
    int handled = command.handled;
    int changed = 0;

    switch (command.action) {
    case APP_NAV_SELECT_NTH_UNLOCKED:
        changed = layer_stack_select_nth_unlocked(layers, command.argument) >= 0;
        break;
    case APP_NAV_SELECT_NTH_EDITABLE_VISIBLE:
        changed = layer_stack_select_nth_editable_visible(layers, command.argument) >= 0;
        break;
    case APP_NAV_SELECT_NTH_VISIBLE:
        changed = layer_stack_select_nth_visible(layers, command.argument) >= 0;
        break;
    case APP_NAV_SELECT_NTH_DIRECT: {
        int target = command.argument;
        if (target < layers->layer_count) {
            layers->active_layer = target;
            changed = 1;
        }
        break;
    }
    case APP_NAV_CYCLE_EDITABLE_VISIBLE:
        changed = layer_stack_cycle_editable_visible(layers, command.argument) >= 0;
        break;
    case APP_NAV_CYCLE_UNLOCKED:
        changed = layer_stack_cycle_unlocked(layers, command.argument) >= 0;
        break;
    case APP_NAV_CYCLE_VISIBLE:
        changed = layer_stack_cycle_visible(layers, command.argument) >= 0;
        break;
    case APP_NAV_CYCLE_ALL:
        changed = layer_stack_cycle(layers, command.argument) >= 0;
        break;
    case APP_NAV_EDGE_VISIBLE:
        changed = layer_stack_select_edge_visible(layers, command.argument) >= 0;
        break;
    case APP_NAV_EDGE_ALL:
        changed = layer_stack_select_edge(layers, command.argument) >= 0;
        break;
    case APP_NAV_EDGE_UNLOCKED:
        changed = layer_stack_select_edge_unlocked(layers, command.argument) >= 0;
        break;
    case APP_NAV_EDGE_EDITABLE_VISIBLE:
        changed = layer_stack_select_edge_editable_visible(layers, command.argument) >= 0;
        break;
    case APP_NAV_NONE:
    default:
        break;
    }

    if (handled && changed) {
        update_window_title_for_runtime(window, layers, runtime);
    }
    return handled;
}

static int handle_layer_stack_shortcut(
    SDL_Keycode key,
    int ctrl,
    int shift,
    LayerStack *layers,
    SDL_Window *window,
    AppRuntime *runtime
) {
    AppLayerStackCommand command = app_layer_stack_command_for_key((int)key, ctrl, shift);
    int handled = command.handled;

    switch (command.action) {
    case APP_LAYER_STACK_ADD_TOP:
        push_runtime_snapshot(layers, runtime);
        if (layer_stack_add(layers, NULL, 0x00000000) < 0) {
            fprintf(stderr, "Max layers reached (%d)\n", MAX_LAYERS);
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_INSERT_ABOVE:
        push_runtime_snapshot(layers, runtime);
        if (layer_stack_insert(layers, layers->active_layer + 1, NULL, 0x00000000) < 0) {
            fprintf(stderr, "Could not insert a layer above the active layer\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_INSERT_BELOW:
        push_runtime_snapshot(layers, runtime);
        if (layer_stack_insert(layers, layers->active_layer, NULL, 0x00000000) < 0) {
            fprintf(stderr, "Could not insert a layer below the active layer\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_TOGGLE_LOCK:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_toggle_lock(layers, layers->active_layer)) {
            fprintf(stderr, "Could not toggle layer lock\n");
        }
        break;
    case APP_LAYER_STACK_TOGGLE_LOCK_OTHERS:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_toggle_lock_others(layers, layers->active_layer)) {
            fprintf(stderr, "Could not toggle locks on the other layers\n");
        }
        break;
    case APP_LAYER_STACK_TOGGLE_VISIBILITY_OTHERS:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_toggle_visibility_others(layers, layers->active_layer)) {
            fprintf(stderr, "Could not toggle visibility on the other layers\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_UNLOCK_ALL:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_unlock_all(layers)) {
            fprintf(stderr, "Could not unlock layers\n");
        }
        break;
    case APP_LAYER_STACK_FLATTEN:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_flatten(layers, COLOR_BG)) {
            fprintf(stderr, "Flatten failed (check for locked layers)\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_STAMP_VISIBLE_INTO:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_stamp_visible_into(layers, layers->active_layer, COLOR_BG)) {
            fprintf(stderr, "Stamp visible failed (active layer may be locked)\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_STAMP_VISIBLE_NEW:
        push_runtime_snapshot(layers, runtime);
        if (layer_stack_stamp_visible_new(layers, "Visible Stamp", COLOR_BG) < 0) {
            fprintf(stderr, "Could not stamp visible image into a new layer\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_DUPLICATE_BELOW:
        push_runtime_snapshot(layers, runtime);
        if (layer_stack_duplicate_below(layers, layers->active_layer, NULL) < 0) {
            fprintf(stderr, "Could not duplicate layer below\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_DUPLICATE:
        push_runtime_snapshot(layers, runtime);
        if (layer_stack_duplicate(layers, layers->active_layer, NULL) < 0) {
            fprintf(stderr, "Could not duplicate layer\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_MOVE_RELATIVE:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_move(layers, layers->active_layer, command.argument)) {
            fprintf(stderr, command.argument < 0 ? "Layer is already at the bottom\n" : "Layer is already at the top\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_MOVE_TO_EDGE:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_move_to(
                layers,
                layers->active_layer,
                command.argument == 0 ? 0 : layers->layer_count - 1)) {
            fprintf(stderr, command.argument == 0 ? "Layer is already at the bottom\n" : "Layer is already at the top\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_ADJUST_OPACITY: {
        Layer *active = layer_stack_active(layers);
        if (active) {
            push_runtime_snapshot(layers, runtime);
            layer_stack_set_opacity(layers, layers->active_layer, active->opacity_percent + command.argument);
            runtime->needs_composite = 1;
        }
        break;
    }
    case APP_LAYER_STACK_TOGGLE_VISIBILITY:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_toggle_visibility(layers, layers->active_layer)) {
            fprintf(stderr, "Cannot hide the final visible layer\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_HIDE_AND_ADVANCE:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_hide_and_advance(layers, layers->active_layer)) {
            fprintf(stderr, "Cannot hide the final visible layer\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_TOGGLE_SOLO:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_toggle_solo(layers, layers->active_layer)) {
            fprintf(stderr, "Could not toggle solo mode\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_DELETE:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_delete(layers, layers->active_layer)) {
            fprintf(stderr, "Cannot delete the final or a locked layer\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_MERGE_DOWN:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_merge_down(layers, layers->active_layer)) {
            fprintf(stderr, "No lower layer to merge into, or one of the layers is locked\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_MERGE_UP:
        push_runtime_snapshot(layers, runtime);
        if (!layer_stack_merge_up(layers, layers->active_layer)) {
            fprintf(stderr, "No upper layer to merge into, or one of the layers is locked\n");
        } else {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_RESET_OPACITY: {
        Layer *active = layer_stack_active(layers);
        if (active && active->opacity_percent != 100) {
            push_runtime_snapshot(layers, runtime);
            layer_stack_set_opacity(layers, layers->active_layer, 100);
            runtime->needs_composite = 1;
        }
        break;
    }
    case APP_LAYER_STACK_SHOW_ALL:
        push_runtime_snapshot(layers, runtime);
        if (layer_stack_show_all(layers)) {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_SHOW_ACTIVE:
        push_runtime_snapshot(layers, runtime);
        if (layer_stack_show(layers, layers->active_layer)) {
            runtime->needs_composite = 1;
        }
        break;
    case APP_LAYER_STACK_NONE:
    default:
        break;
    }

    if (handled) {
        update_window_title_for_runtime(window, layers, runtime);
    }
    return handled;
}

static int restore_from_history(
    LayerStack *layers,
    Snapshot *from_stack,
    int *from_count,
    Snapshot *to_stack,
    int *to_count
) {
    return snapshot_restore(layers, from_stack, from_count, to_stack, to_count);
}

static int handle_document_shortcut(
    SDL_Keycode key,
    int ctrl,
    int shift,
    LayerStack *layers,
    SDL_Window *window,
    AppRuntime *runtime,
    const Canvas *preview_canvas,
    const Canvas *composite
) {
    int handled = 1;
    AppDocumentState state = {
        .preview_active = runtime ? runtime->preview_active : 0,
        .needs_composite = runtime ? runtime->needs_composite : 0,
    };
    AppDocumentCallbacks callbacks = {
        .save_canvas = save_document_canvas,
        .load_canvas = load_document_canvas,
        .restore_history = restore_document_history,
        .push_snapshot = push_document_snapshot,
        .userdata = runtime,
    };

    if (ctrl && key == SDLK_s) {
        if (!app_document_apply(
                APP_DOCUMENT_ACTION_SAVE,
                layers,
                &state,
                preview_canvas,
                composite,
                active_layer_clear_color(layers),
                &callbacks)) {
            fprintf(stderr, "Failed to save output.bmp\n");
        }
    } else if (ctrl && key == SDLK_o) {
        Layer *active = layer_stack_active(layers);
        if (!active || active->locked) {
            fprintf(stderr, "Active layer is locked\n");
        } else if (!app_document_apply(
                       APP_DOCUMENT_ACTION_LOAD,
                       layers,
                       &state,
                       preview_canvas,
                       composite,
                       active_layer_clear_color(layers),
                       &callbacks)) {
            fprintf(stderr, "Failed to load input.bmp\n");
        }
    } else if (ctrl && key == SDLK_z) {
        if (app_document_apply(
                APP_DOCUMENT_ACTION_UNDO,
                layers,
                &state,
                preview_canvas,
                composite,
                active_layer_clear_color(layers),
                &callbacks)) {
            update_window_title_for_runtime(window, layers, runtime);
        }
    } else if (ctrl && key == SDLK_y) {
        if (app_document_apply(
                APP_DOCUMENT_ACTION_REDO,
                layers,
                &state,
                preview_canvas,
                composite,
                active_layer_clear_color(layers),
                &callbacks)) {
            update_window_title_for_runtime(window, layers, runtime);
        }
    } else if (ctrl && key == SDLK_0) {
        app_document_apply(
            APP_DOCUMENT_ACTION_RESET_OPACITY,
            layers,
            &state,
            preview_canvas,
            composite,
            active_layer_clear_color(layers),
            &callbacks
        );
        update_window_title_for_runtime(window, layers, runtime);
    } else if (ctrl && key == SDLK_a) {
        app_document_apply(
            APP_DOCUMENT_ACTION_SHOW_ALL,
            layers,
            &state,
            preview_canvas,
            composite,
            active_layer_clear_color(layers),
            &callbacks
        );
        update_window_title_for_runtime(window, layers, runtime);
    } else if (ctrl && shift && key == SDLK_r) {
        app_document_apply(
            APP_DOCUMENT_ACTION_SHOW_ACTIVE,
            layers,
            &state,
            preview_canvas,
            composite,
            active_layer_clear_color(layers),
            &callbacks
        );
        update_window_title_for_runtime(window, layers, runtime);
    } else {
        handled = 0;
    }

    if (runtime) {
        runtime->needs_composite = state.needs_composite;
    }

    return handled;
}

static int handle_tool_shortcut(
    SDL_Keycode key,
    LayerStack *layers,
    AppRuntime *runtime,
    const Canvas *preview_canvas,
    const Canvas *composite
) {
    int handled = 1;
    AppToolCommand tool_command = app_tool_command_for_key(
        (int)key,
        (int)runtime->tool,
        (int)runtime->brush_shape,
        runtime->brush_radius,
        runtime->brush_opacity,
        runtime->brush_color_rgb
    );

    if (tool_command.handled) {
        runtime->tool = (Tool)tool_command.tool;
        runtime->brush_shape = (BrushShape)tool_command.brush_shape;
        runtime->brush_radius = tool_command.brush_radius;
        runtime->brush_opacity = tool_command.brush_opacity;
        runtime->brush_color_rgb = tool_command.brush_color_rgb;
        runtime->brush_color = tool_command.brush_color;
    } else if (key == SDLK_c) {
        if (active_layer_editable(layers)) {
            push_runtime_snapshot(layers, runtime);
        }
        if (layer_stack_clear_layer(layers, layers->active_layer, active_layer_clear_color(layers))) {
            runtime->needs_composite = 1;
        }
    } else if (key == SDLK_h) {
        if (apply_runtime_canvas_transform(layers, runtime, canvas_flip_horizontal)) {
            runtime->needs_composite = 1;
        }
    } else if (key == SDLK_v) {
        if (apply_runtime_canvas_transform(layers, runtime, canvas_flip_vertical)) {
            runtime->needs_composite = 1;
        }
    } else if (key == SDLK_j) {
        if (apply_runtime_canvas_transform(layers, runtime, canvas_rotate_180)) {
            runtime->needs_composite = 1;
        }
    } else if (key == SDLK_x) {
        if (apply_runtime_canvas_transform(layers, runtime, canvas_invert_rgb)) {
            runtime->needs_composite = 1;
        }
    } else if (key == SDLK_f) {
        int mx = 0;
        int my = 0;
        SDL_GetMouseState(&mx, &my);
        if (mx >= 0 && my >= 0 && mx < CANVAS_WIDTH && my < CANVAS_HEIGHT) {
            Layer *active = layer_stack_active(layers);
            if (active && !active->locked) {
                push_runtime_snapshot(layers, runtime);
            }
            if (!active || active->locked || !canvas_flood_fill(&active->canvas, mx, my, runtime->brush_color)) {
                fprintf(stderr, "Fill failed\n");
            } else {
                runtime->needs_composite = 1;
            }
        }
    } else if (key == SDLK_i) {
        int mx = 0;
        int my = 0;
        SDL_GetMouseState(&mx, &my);
        if (mx >= 0 && my >= 0 && mx < CANVAS_WIDTH && my < CANVAS_HEIGHT) {
            const Canvas *sample =
                (runtime->preview_active && preview_canvas && preview_canvas->pixels) ? preview_canvas : composite;
            runtime->brush_color = canvas_get_pixel(sample, mx, my);
            runtime->brush_color_rgb = runtime->brush_color & 0x00FFFFFF;
            runtime->brush_opacity = (int)((((runtime->brush_color >> 24) & 0xFF) * 100 + 127) / 255);
            if (runtime->brush_opacity < 1) {
                runtime->brush_opacity = 1;
            }
            runtime->brush_color = compose_brush_color(runtime->brush_color_rgb, runtime->brush_opacity);
            runtime->tool = TOOL_BRUSH;
        }
    } else {
        handled = 0;
    }

    return handled;
}

static int handle_session_shortcut(
    SDL_Keycode key,
    int ctrl,
    int *shaping,
    int *preview_active,
    int *running
) {
    AppSessionCommand command = app_session_command_for_key((int)key, ctrl, shaping ? *shaping : 0);

    if (shaping && preview_active && command.cancel_shape) {
        cancel_shape_preview(shaping, preview_active);
    }

    if (running && command.stop_running) {
        *running = 0;
    }
    return command.handled;
}

static int handle_translation_shortcut(
    SDL_Keycode key,
    int shift,
    LayerStack *layers,
    AppRuntime *runtime
) {
    AppTranslationCommand command = app_translation_command_for_key((int)key, shift);
    if (!command.handled) {
        return 0;
    }

    if (apply_runtime_canvas_translation(layers, runtime, command.dx, command.dy)) {
        runtime->needs_composite = 1;
    }
    return 1;
}

static void sample_canvas_color(
    const Canvas *sample,
    int x,
    int y,
    Tool *tool,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity
) {
    if (!sample || !brush_color || !brush_color_rgb || !brush_opacity || !tool) {
        return;
    }
    *brush_color = canvas_get_pixel(sample, x, y);
    *brush_color_rgb = *brush_color & 0x00FFFFFF;
    *brush_opacity = (int)((((*brush_color >> 24) & 0xFF) * 100 + 127) / 255);
    if (*brush_opacity < 1) {
        *brush_opacity = 1;
    }
    *brush_color = compose_brush_color(*brush_color_rgb, *brush_opacity);
    *tool = TOOL_BRUSH;
}

static void handle_mouse_down(
    const SDL_MouseButtonEvent *button,
    AppRuntime *runtime,
    LayerStack *layers,
    const Canvas *composite,
    const Canvas *preview_canvas,
    SDL_Window *window
) {
    if (!button || !runtime || !layers || !window) {
        return;
    }

    if (button->button == SDL_BUTTON_LEFT) {
        runtime->last_x = button->x;
        runtime->last_y = button->y;
        if (runtime->tool == TOOL_BRUSH || runtime->tool == TOOL_ERASER) {
            Layer *active = layer_stack_active(layers);
            if (active && !active->locked && active->canvas.pixels) {
                push_runtime_snapshot(layers, runtime);
                runtime->drawing = 1;
                if (runtime->tool == TOOL_ERASER) {
                    erase_stamp(
                        &active->canvas,
                        runtime->last_x,
                        runtime->last_y,
                        runtime->brush_radius,
                        active_layer_clear_color(layers),
                        runtime->brush_shape
                    );
                } else {
                    stamp_brush(
                        &active->canvas,
                        runtime->last_x,
                        runtime->last_y,
                        runtime->brush_radius,
                        runtime->brush_color,
                        runtime->brush_shape
                    );
                }
                runtime->needs_composite = 1;
            }
        } else if (active_layer_editable(layers)) {
            runtime->shaping = 1;
            runtime->shape_start_x = runtime->last_x;
            runtime->shape_start_y = runtime->last_y;
            if (runtime->shape_base_pixels && composite && composite->pixels) {
                memcpy(
                    runtime->shape_base_pixels,
                    composite->pixels,
                    (size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t)
                );
            }
        }
        return;
    }

    if (button->button != SDL_BUTTON_RIGHT) {
        return;
    }
    if (runtime->shaping) {
        cancel_shape_preview(&runtime->shaping, &runtime->preview_active);
        return;
    }
    if (button->x < 0 || button->y < 0 || button->x >= CANVAS_WIDTH || button->y >= CANVAS_HEIGHT) {
        return;
    }
    const Canvas *sample =
        (runtime->preview_active && preview_canvas && preview_canvas->pixels) ? preview_canvas : composite;
    sample_canvas_color(
        sample,
        button->x,
        button->y,
        &runtime->tool,
        &runtime->brush_color,
        &runtime->brush_color_rgb,
        &runtime->brush_opacity
    );
    update_window_title_for_runtime(window, layers, runtime);
}

static void handle_mouse_up(
    const SDL_MouseButtonEvent *button,
    AppRuntime *runtime,
    LayerStack *layers,
) {
    if (!button || !runtime || !layers) {
        return;
    }
    if (button->button != SDL_BUTTON_LEFT) {
        return;
    }

    runtime->drawing = 0;
    if (!runtime->shaping) {
        return;
    }

    const Uint8 *state = SDL_GetKeyboardState(NULL);
    int shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    int end_x = button->x;
    int end_y = button->y;
    constrain_end(
        runtime->tool,
        runtime->shape_start_x,
        runtime->shape_start_y,
        end_x,
        end_y,
        shift,
        &end_x,
        &end_y
    );
    Layer *active = layer_stack_active(layers);
    if (active && !active->locked && active->canvas.pixels) {
        push_runtime_snapshot(layers, runtime);
        draw_shape(
            &active->canvas,
            runtime->tool,
            runtime->shape_start_x,
            runtime->shape_start_y,
            end_x,
            end_y,
            runtime->brush_radius,
            runtime->brush_color
        );
        runtime->needs_composite = 1;
    }
    cancel_shape_preview(&runtime->shaping, &runtime->preview_active);
}

static void handle_mouse_motion(
    const SDL_MouseMotionEvent *motion,
    AppRuntime *runtime,
    LayerStack *layers,
) {
    if (!motion || !runtime || !layers) {
        return;
    }

    if (runtime->drawing) {
        int x = motion->x;
        int y = motion->y;
        if (x >= 0 && y >= 0 && x < CANVAS_WIDTH && y < CANVAS_HEIGHT) {
            Layer *active = layer_stack_active(layers);
            if (active && !active->locked && active->canvas.pixels) {
                if (runtime->tool == TOOL_ERASER) {
                    erase_line(
                        &active->canvas,
                        runtime->last_x,
                        runtime->last_y,
                        x,
                        y,
                        runtime->brush_radius,
                        active_layer_clear_color(layers),
                        runtime->brush_shape
                    );
                } else {
                    draw_brush_line(
                        &active->canvas,
                        runtime->last_x,
                        runtime->last_y,
                        x,
                        y,
                        runtime->brush_radius,
                        runtime->brush_color,
                        runtime->brush_shape
                    );
                }
                runtime->last_x = x;
                runtime->last_y = y;
                runtime->needs_composite = 1;
            }
        }
        return;
    }

    if (!runtime->shaping || !runtime->shape_base_pixels || !runtime->preview_canvas.pixels) {
        return;
    }

    int x = motion->x;
    int y = motion->y;
    if (x < 0 || y < 0 || x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT) {
        return;
    }
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    int shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    int end_x = x;
    int end_y = y;
    constrain_end(
        runtime->tool,
        runtime->shape_start_x,
        runtime->shape_start_y,
        end_x,
        end_y,
        shift,
        &end_x,
        &end_y
    );
    memcpy(
        runtime->preview_canvas.pixels,
        runtime->shape_base_pixels,
        (size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t)
    );
    draw_shape(
        &runtime->preview_canvas,
        runtime->tool,
        runtime->shape_start_x,
        runtime->shape_start_y,
        end_x,
        end_y,
        runtime->brush_radius,
        runtime->brush_color
    );
    runtime->preview_active = 1;
}

static void handle_keydown_shortcut(
    SDL_Keycode key,
    int ctrl,
    int shift,
    int alt,
    AppRuntime *runtime,
    LayerStack *layers,
    SDL_Window *window,
    const Canvas *preview_canvas,
    const Canvas *composite
) {
    if (!runtime) {
        return;
    }

    if (handle_session_shortcut(key, ctrl, &runtime->shaping, &runtime->preview_active, &runtime->running)) {
        return;
    }

    if (handle_layer_stack_shortcut(
            key,
            ctrl,
            shift,
            layers,
            window,
            runtime)) {
        return;
    }

    if (handle_document_shortcut(
            key,
            ctrl,
            shift,
            layers,
            window,
            runtime,
            preview_canvas,
            composite)) {
        return;
    }

    if (handle_layer_navigation_shortcut(
            key,
            ctrl,
            shift,
            alt,
            layers,
            window,
            runtime)) {
        return;
    }

    if (handle_translation_shortcut(
            key,
            shift,
            layers,
            runtime)) {
        return;
    }

    handle_tool_shortcut(
        key,
        layers,
        runtime,
        preview_canvas,
        composite
    );

    update_window_title_for_runtime(window, layers, runtime);
}

static void process_app_events(
    AppRuntime *runtime,
    LayerStack *layers,
    Canvas *composite,
    SDL_Window *window
) {
    if (!runtime) {
        return;
    }

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            runtime->running = 0;
            break;
        case SDL_MOUSEBUTTONDOWN:
            handle_mouse_down(
                &e.button,
                runtime,
                layers,
                composite,
                &runtime->preview_canvas,
                window
            );
            break;
        case SDL_MOUSEBUTTONUP:
            handle_mouse_up(
                &e.button,
                runtime,
                layers,
            );
            break;
        case SDL_MOUSEMOTION:
            handle_mouse_motion(
                &e.motion,
                runtime,
                layers,
            );
            break;
        case SDL_KEYDOWN: {
            SDL_Keycode key = e.key.keysym.sym;
            const Uint8 *state = SDL_GetKeyboardState(NULL);
            int ctrl = state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
            int shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
            int alt = state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT];
            handle_keydown_shortcut(
                key,
                ctrl,
                shift,
                alt,
                runtime,
                layers,
                window,
                &runtime->preview_canvas,
                composite
            );
            break;
        }
        default:
            break;
        }
    }
}

static void render_frame(
    SDL_Renderer *renderer,
    SDL_Texture *texture,
    AppRuntime *runtime,
    LayerStack *layers,
    Canvas *composite
) {
    if (!renderer || !texture || !runtime || !layers || !composite) {
        return;
    }

    if (!runtime->preview_active && runtime->needs_composite) {
        layer_stack_composite(layers, composite, COLOR_BG);
        runtime->needs_composite = 0;
    }

    if (runtime->preview_active && runtime->preview_canvas.pixels) {
        SDL_UpdateTexture(texture, NULL, runtime->preview_canvas.pixels, CANVAS_WIDTH * 4);
    } else {
        SDL_UpdateTexture(texture, NULL, composite->pixels, CANVAS_WIDTH * 4);
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
}

static void cleanup_app_resources(
    uint32_t *shape_base_pixels,
    uint32_t *preview_pixels,
    Canvas *composite,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    SDL_Texture *texture,
    SDL_Renderer *renderer,
    SDL_Window *window
) {
    free(shape_base_pixels);
    free(preview_pixels);
    if (composite) {
        canvas_free(composite);
    }
    if (layers) {
        layer_stack_free(layers);
    }
    if (undo_stack && undo_count) {
        snapshot_stack_clear(undo_stack, undo_count);
    }
    if (redo_stack && redo_count) {
        snapshot_stack_clear(redo_stack, redo_count);
    }
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

static int init_app_resources(
    const char *input_path,
    SDL_Window **window,
    SDL_Renderer **renderer,
    SDL_Texture **texture,
    LayerStack *layers,
    Canvas *composite
) {
    if (!window || !renderer || !texture || !layers || !composite) {
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }

    *window = SDL_CreateWindow(
        "Openshop - Minimal Paint",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!*window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (!*renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(*window);
        *window = NULL;
        SDL_Quit();
        return 0;
    }

    *texture = SDL_CreateTexture(
        *renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        CANVAS_WIDTH,
        CANVAS_HEIGHT
    );
    if (!*texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(*renderer);
        SDL_DestroyWindow(*window);
        *renderer = NULL;
        *window = NULL;
        SDL_Quit();
        return 0;
    }

    if (!layer_stack_init(layers, CANVAS_WIDTH, CANVAS_HEIGHT, COLOR_BG)) {
        fprintf(stderr, "Layer stack init failed\n");
        SDL_DestroyTexture(*texture);
        SDL_DestroyRenderer(*renderer);
        SDL_DestroyWindow(*window);
        *texture = NULL;
        *renderer = NULL;
        *window = NULL;
        SDL_Quit();
        return 0;
    }

    if (!canvas_init(composite, CANVAS_WIDTH, CANVAS_HEIGHT)) {
        fprintf(stderr, "Composite canvas init failed\n");
        layer_stack_free(layers);
        SDL_DestroyTexture(*texture);
        SDL_DestroyRenderer(*renderer);
        SDL_DestroyWindow(*window);
        *texture = NULL;
        *renderer = NULL;
        *window = NULL;
        SDL_Quit();
        return 0;
    }

    if (input_path && input_path[0]) {
        Layer *active = layer_stack_active(layers);
        if (active && !canvas_load_bmp(&active->canvas, input_path, COLOR_BG)) {
            fprintf(stderr, "Failed to load %s\n", input_path);
        }
    }
    layer_stack_composite(layers, composite, COLOR_BG);
    return 1;
}

static int init_app_session(
    uint32_t **shape_base_pixels,
    uint32_t **preview_pixels,
    Canvas *preview_canvas,
    Snapshot *undo_stack,
    Snapshot *redo_stack
) {
    if (!shape_base_pixels || !preview_pixels || !preview_canvas || !undo_stack || !redo_stack) {
        return 0;
    }

    *shape_base_pixels = (uint32_t *)malloc((size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t));
    *preview_pixels = (uint32_t *)malloc((size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t));
    if (!*shape_base_pixels || !*preview_pixels) {
        fprintf(stderr, "Scratch buffer allocation failed\n");
        free(*shape_base_pixels);
        free(*preview_pixels);
        *shape_base_pixels = NULL;
        *preview_pixels = NULL;
        preview_canvas->width = 0;
        preview_canvas->height = 0;
        preview_canvas->pixels = NULL;
        return 0;
    }

    preview_canvas->width = CANVAS_WIDTH;
    preview_canvas->height = CANVAS_HEIGHT;
    preview_canvas->pixels = *preview_pixels;
    memset(undo_stack, 0, sizeof(Snapshot) * MAX_HISTORY);
    memset(redo_stack, 0, sizeof(Snapshot) * MAX_HISTORY);
    return 1;
}

struct AppRuntime {
    int running;
    int drawing;
    int last_x;
    int last_y;
    int brush_radius;
    int brush_opacity;
    uint32_t brush_color_rgb;
    uint32_t brush_color;
    BrushShape brush_shape;
    Tool tool;
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count;
    int redo_count;
    int shaping;
    int shape_start_x;
    int shape_start_y;
    int preview_active;
    int needs_composite;
    uint32_t *shape_base_pixels;
    uint32_t *preview_pixels;
    Canvas preview_canvas;
};

static void update_window_title_for_runtime(SDL_Window *window, const LayerStack *layers, const AppRuntime *runtime) {
    if (!runtime) {
        return;
    }
    update_window_title(
        window,
        layers,
        runtime->tool,
        runtime->brush_shape,
        runtime->brush_radius,
        runtime->brush_color,
        runtime->brush_opacity
    );
}

static void push_runtime_snapshot(const LayerStack *layers, AppRuntime *runtime) {
    if (!runtime) {
        return;
    }
    snapshot_push(layers, runtime->undo_stack, &runtime->undo_count, runtime->redo_stack, &runtime->redo_count);
}

static int save_document_canvas(const Canvas *canvas, const char *path, void *userdata) {
    (void)userdata;
    return canvas_save_bmp(canvas, path);
}

static int load_document_canvas(Canvas *canvas, const char *path, uint32_t clear_color, void *userdata) {
    (void)userdata;
    return canvas_load_bmp(canvas, path, clear_color);
}

static int restore_document_history(LayerStack *layers, int redo_to_undo, void *userdata) {
    AppRuntime *runtime = (AppRuntime *)userdata;
    return restore_runtime_history(layers, runtime, redo_to_undo);
}

static void push_document_snapshot(const LayerStack *layers, void *userdata) {
    AppRuntime *runtime = (AppRuntime *)userdata;
    push_runtime_snapshot(layers, runtime);
}

static int restore_runtime_history(LayerStack *layers, AppRuntime *runtime, int redo_to_undo) {
    if (!runtime) {
        return 0;
    }
    if (redo_to_undo) {
        return restore_from_history(layers, runtime->redo_stack, &runtime->redo_count, runtime->undo_stack, &runtime->undo_count);
    }
    return restore_from_history(layers, runtime->undo_stack, &runtime->undo_count, runtime->redo_stack, &runtime->redo_count);
}

static int apply_runtime_canvas_transform(LayerStack *layers, AppRuntime *runtime, void (*transform)(Canvas *)) {
    if (!layers || !runtime || !transform) {
        return 0;
    }
    Layer *active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }
    push_runtime_snapshot(layers, runtime);
    transform(&active->canvas);
    return 1;
}

static int apply_runtime_canvas_translation(LayerStack *layers, AppRuntime *runtime, int dx, int dy) {
    if (!layers || !runtime || (dx == 0 && dy == 0)) {
        return 0;
    }
    Layer *active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels) {
        return 0;
    }
    push_runtime_snapshot(layers, runtime);
    canvas_translate(&active->canvas, dx, dy, active_layer_clear_color(layers));
    return 1;
}

static void cleanup_app_runtime(
    AppRuntime *runtime,
    Canvas *composite,
    LayerStack *layers,
    SDL_Texture *texture,
    SDL_Renderer *renderer,
    SDL_Window *window
) {
    cleanup_app_resources(
        runtime ? runtime->shape_base_pixels : NULL,
        runtime ? runtime->preview_pixels : NULL,
        composite,
        layers,
        runtime ? runtime->undo_stack : NULL,
        runtime ? &runtime->undo_count : NULL,
        runtime ? runtime->redo_stack : NULL,
        runtime ? &runtime->redo_count : NULL,
        texture,
        renderer,
        window
    );
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
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    AppRuntime runtime = {
        .running = 1,
        .brush_radius = 6,
        .brush_opacity = 100,
        .brush_color_rgb = COLOR_BRUSH & 0x00FFFFFF,
        .brush_shape = BRUSH_SHAPE_ROUND,
        .tool = TOOL_BRUSH,
    };
    LayerStack layers;
    Canvas composite = {0};
    memset(&layers, 0, sizeof(layers));
    if (!init_app_resources(input_path, &window, &renderer, &texture, &layers, &composite)) {
        return 1;
    }
    runtime.brush_color = compose_brush_color(runtime.brush_color_rgb, runtime.brush_opacity);

    if (!init_app_session(
            &runtime.shape_base_pixels,
            &runtime.preview_pixels,
            &runtime.preview_canvas,
            runtime.undo_stack,
            runtime.redo_stack)) {
        cleanup_app_runtime(&runtime, &composite, &layers, texture, renderer, window);
        return 1;
    }
    update_window_title_for_runtime(window, &layers, &runtime);

    while (runtime.running) {
        process_app_events(
            &runtime,
            &layers,
            &composite,
            window
        );
        render_frame(renderer, texture, &runtime, &layers, &composite);
        SDL_Delay(16);
    }

    cleanup_app_runtime(&runtime, &composite, &layers, texture, renderer, window);
    return 0;
}
