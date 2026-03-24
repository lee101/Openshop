#include "app.h"
#include "app_brush.h"
#include "app_brush_mask.h"
#include "app_canvas_ops.h"
#include "app_color.h"
#include "app_layer_state.h"
#include "app_preview.h"
#include "app_sampled_color.h"
#include "app_shape.h"
#include "app_title.h"
#include "brush_shortcuts.h"
#include "canvas.h"
#include "canvas_shortcuts.h"
#include "direct_layer_shortcuts.h"
#include "file_shortcuts.h"
#include "history_shortcuts.h"
#include "history_state.h"
#include "image_io.h"
#include "layer_name_shortcuts.h"
#include "layers.h"
#include "merge_shortcuts.h"
#include "paint_shortcuts.h"
#include "view_shortcuts.h"

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
} AppRuntime;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    LayerStack layers;
    Canvas composite;
    AppRuntime runtime;
} App;

static int handle_history_navigation_shortcut(
    SDL_Keycode key,
    int ctrl,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    HistoryShortcutAction action;

    if (!ctrl || !layers || !undo_stack || !undo_count || !redo_stack || !redo_count) {
        return 0;
    }

    action = history_shortcut_action(ctrl, (int)key);

    if (action == HISTORY_SHORTCUT_UNDO) {
        if (snapshot_undo(layers, undo_stack, undo_count, MAX_HISTORY, redo_stack, redo_count)) {
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (action == HISTORY_SHORTCUT_REDO) {
        if (snapshot_redo(layers, undo_stack, undo_count, MAX_HISTORY, redo_stack, redo_count)) {
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    return 0;
}

static int handle_canvas_sample_shortcut(
    CanvasShortcutAction canvas_action,
    LayerStack *layers,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_active,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    Tool *tool,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity,
    int *needs_composite
) {
    Layer *active = NULL;
    const Canvas *sample = NULL;
    int mx = 0;
    int my = 0;

    if (!layers || !tool || !brush_color || !brush_color_rgb || !brush_opacity) {
        return 0;
    }
    if (canvas_action != CANVAS_SHORTCUT_FILL && canvas_action != CANVAS_SHORTCUT_EYEDROPPER) {
        return 0;
    }

    SDL_GetMouseState(&mx, &my);
    if (mx < 0 || my < 0 || mx >= CANVAS_WIDTH || my >= CANVAS_HEIGHT) {
        return 1;
    }

    if (canvas_action == CANVAS_SHORTCUT_FILL) {
        active = layer_stack_active(layers);
        if (active && !active->locked) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        }
        if (!active || active->locked || !canvas_flood_fill(&active->canvas, mx, my, *brush_color)) {
            fprintf(stderr, "Fill failed\n");
        } else if (needs_composite) {
            *needs_composite = 1;
        }
        return 1;
    }

    sample = (preview_active && preview_canvas && preview_canvas->pixels) ? preview_canvas : composite;
    app_apply_sampled_brush_color(canvas_get_pixel(sample, mx, my), tool, brush_color, brush_color_rgb, brush_opacity);
    return 1;
}

static int handle_canvas_mutation_shortcut(
    CanvasShortcutAction canvas_action,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    int changed = 0;

    if (!layers) {
        return 0;
    }

    if (canvas_action == CANVAS_SHORTCUT_CLEAR) {
        if (app_layer_editable(layer_stack_get(layers, layers->active_layer))) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        }
        changed = layer_stack_clear_layer(layers, layers->active_layer, app_active_layer_clear_color(layers->active_layer));
    } else if (canvas_action == CANVAS_SHORTCUT_FLIP_HORIZONTAL) {
        changed = app_apply_canvas_transform(layers, undo_stack, undo_count, MAX_HISTORY, redo_stack, redo_count, canvas_flip_horizontal);
    } else if (canvas_action == CANVAS_SHORTCUT_FLIP_VERTICAL) {
        changed = app_apply_canvas_transform(layers, undo_stack, undo_count, MAX_HISTORY, redo_stack, redo_count, canvas_flip_vertical);
    } else if (canvas_action == CANVAS_SHORTCUT_ROTATE_180) {
        changed = app_apply_canvas_transform(layers, undo_stack, undo_count, MAX_HISTORY, redo_stack, redo_count, canvas_rotate_180);
    } else if (canvas_action == CANVAS_SHORTCUT_INVERT_RGB) {
        changed = app_apply_canvas_transform(layers, undo_stack, undo_count, MAX_HISTORY, redo_stack, redo_count, canvas_invert_rgb);
    } else {
        return 0;
    }

    if (changed && needs_composite) {
        *needs_composite = 1;
    }
    return 1;
}

static void update_window_title(SDL_Window *window, const LayerStack *layers, Tool tool, BrushShape brush_shape, int radius, uint32_t color, int opacity_percent) {
    const Layer *active = NULL;
    const char *layer_name = "Layer";
    int visible_layers = 0;
    char title[256];

    if (!window || !layers) {
        return;
    }

    active = layer_stack_get(layers, layers->active_layer);
    if (active && active->name[0]) {
        layer_name = active->name;
    }
    visible_layers = layer_stack_visible_count(layers);

    app_title_format(
        title,
        sizeof(title),
        app_tool_label(tool),
        app_brush_shape_label(brush_shape),
        radius,
        opacity_percent,
        layers->active_layer,
        layers->layer_count,
        layer_name,
        active && active->visible,
        active && active->locked,
        active ? active->opacity_percent : 100,
        layers->solo_index == layers->active_layer,
        visible_layers,
        color
    );
    SDL_SetWindowTitle(window, title);
}

static int refresh_after_shortcut(
    int handled,
    SDL_Window *window,
    const LayerStack *layers,
    Tool tool,
    BrushShape brush_shape,
    int brush_radius,
    uint32_t brush_color,
    int brush_opacity
) {
    if (!handled) {
        return 0;
    }

    update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
    return 1;
}

static void stamp_brush(Canvas *c, int cx, int cy, int radius, uint32_t color, BrushShape shape) {
    if (!c || !c->pixels || radius <= 0) {
        return;
    }
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (!app_brush_mask_contains(shape, dx, dy, radius)) {
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

static int should_cancel_shape_on_key(SDL_Keycode key, int ctrl) {
    if (key == SDLK_ESCAPE || key == SDLK_LSHIFT || key == SDLK_RSHIFT) {
        return 0;
    }
    if (ctrl && (key == SDLK_s || key == SDLK_o || key == SDLK_z || key == SDLK_y || key == SDLK_n || key == SDLK_u || key == SDLK_v || key == SDLK_m || key == SDLK_d || key == SDLK_e || key == SDLK_g || key == SDLK_h || key == SDLK_l || key == SDLK_a || key == SDLK_r || key == SDLK_0 || key == SDLK_COMMA || key == SDLK_LEFTBRACKET || key == SDLK_RIGHTBRACKET || key == SDLK_MINUS || key == SDLK_KP_MINUS || key == SDLK_EQUALS || key == SDLK_KP_PLUS || key == SDLK_SLASH || key == SDLK_1 || key == SDLK_2 || key == SDLK_3 || key == SDLK_4 || key == SDLK_5 || key == SDLK_6 || key == SDLK_7 || key == SDLK_8)) {
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
    case SDLK_F2:
    case SDLK_DELETE:
    case SDLK_BACKSPACE:
        return 1;
    default:
        return 0;
    }
}

static int handle_direct_layer_shortcut(
    SDL_Keycode key,
    int ctrl,
    int alt,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    DirectLayerShortcutAction action;
    int target = 0;
    const Layer *target_layer = NULL;

    if (!ctrl || key < SDLK_1 || key > SDLK_8 || !layers) {
        return 0;
    }

    target = (int)(key - SDLK_1);
    if (target >= layers->layer_count) {
        return 1;
    }

    action = direct_layer_shortcut_action_from_modifiers(ctrl, alt, shift);

    if (action == DIRECT_LAYER_SHORTCUT_TOGGLE_LOCK) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_toggle_lock(layers, target)) {
            fprintf(stderr, "Could not toggle layer lock\n");
        }
        return 1;
    }

    if (action == DIRECT_LAYER_SHORTCUT_TOGGLE_VISIBILITY) {
        target_layer = layer_stack_get(layers, target);
        if (target_layer && (!target_layer->visible || layer_stack_visible_count(layers) > 1)) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (!layer_stack_toggle_visibility(layers, target)) {
                fprintf(stderr, "Cannot hide the final visible layer\n");
            } else if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (action == DIRECT_LAYER_SHORTCUT_SOLO) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        layers->active_layer = target;
        if (layer_stack_toggle_solo(layers, target) && needs_composite) {
            *needs_composite = 1;
        }
        return 1;
    }

    layers->active_layer = target;
    return 1;
}

static int handle_layer_name_shortcut(
    SDL_Keycode key,
    int ctrl,
    int alt,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count
) {
    int (*can_reset)(const LayerStack *) = NULL;
    int (*reset)(LayerStack *) = NULL;
    LayerNameResetShortcut shortcut;

    if (!layers || key != SDLK_F2) {
        return 0;
    }

    shortcut = layer_name_reset_shortcut_from_modifiers(ctrl, alt, shift);
    if (shortcut == LAYER_NAME_RESET_SHORTCUT_NON_BACKGROUND_LOCKED) {
        can_reset = layer_stack_can_reset_non_background_locked_names;
        reset = layer_stack_reset_non_background_locked_names;
    } else if (shortcut == LAYER_NAME_RESET_SHORTCUT_NON_BACKGROUND_VISIBLE) {
        can_reset = layer_stack_can_reset_non_background_visible_names;
        reset = layer_stack_reset_non_background_visible_names;
    } else if (shortcut == LAYER_NAME_RESET_SHORTCUT_LOCKED) {
        can_reset = layer_stack_can_reset_locked_names;
        reset = layer_stack_reset_locked_names;
    } else if (shortcut == LAYER_NAME_RESET_SHORTCUT_NON_BACKGROUND_UNLOCKED) {
        can_reset = layer_stack_can_reset_non_background_unlocked_names;
        reset = layer_stack_reset_non_background_unlocked_names;
    } else if (shortcut == LAYER_NAME_RESET_SHORTCUT_VISIBLE) {
        can_reset = layer_stack_can_reset_visible_names;
        reset = layer_stack_reset_visible_names;
    } else if (shortcut == LAYER_NAME_RESET_SHORTCUT_UNLOCKED) {
        can_reset = layer_stack_can_reset_unlocked_names;
        reset = layer_stack_reset_unlocked_names;
    } else if (shortcut == LAYER_NAME_RESET_SHORTCUT_ALL) {
        can_reset = layer_stack_can_reset_all_names;
        reset = layer_stack_reset_all_names;
    }

    if (can_reset && reset) {
        if (can_reset(layers)) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            reset(layers);
        }
        return 1;
    }

    if (layer_stack_can_reset_name(layers, layers->active_layer)) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        layer_stack_reset_name(layers, layers->active_layer);
    }
    return 1;
}

static int handle_active_layer_state_shortcut(
    SDL_Keycode key,
    int ctrl,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    const Layer *active = NULL;

    if (!layers || !ctrl) {
        return 0;
    }

    if (shift && key == SDLK_l) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_toggle_lock(layers, layers->active_layer)) {
            fprintf(stderr, "Could not toggle layer lock\n");
        }
        return 1;
    }

    active = layer_stack_get(layers, layers->active_layer);
    if (shift && key == SDLK_v) {
        if (active && (!active->visible || layer_stack_visible_count(layers) > 1)) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (!layer_stack_toggle_visibility(layers, layers->active_layer)) {
                fprintf(stderr, "Cannot hide the final visible layer\n");
            } else if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (shift && key == SDLK_h) {
        if (active && active->visible && layer_stack_visible_count(layers) > 1) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (!layer_stack_hide_and_advance(layers, layers->active_layer)) {
                fprintf(stderr, "Cannot hide the final visible layer\n");
            } else if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (key == SDLK_SLASH) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_toggle_solo(layers, layers->active_layer)) {
            fprintf(stderr, "Could not toggle solo mode\n");
        } else if (needs_composite) {
            *needs_composite = 1;
        }
        return 1;
    }

    return 0;
}

static int handle_active_layer_structure_shortcut(
    SDL_Keycode key,
    int ctrl,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    Layer *active = NULL;

    if (!layers || !ctrl) {
        return 0;
    }

    if (key == SDLK_d) {
        if (!layer_stack_can_duplicate(layers, layers->active_layer)) {
            fprintf(stderr, "Could not duplicate layer\n");
        } else {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_duplicate(layers, layers->active_layer, NULL);
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (shift && key == SDLK_LEFTBRACKET) {
        if (layers->active_layer > 0) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (!layer_stack_move_to_edge(layers, layers->active_layer, -1)) {
                fprintf(stderr, "Layer is already at the bottom\n");
            } else if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (shift && key == SDLK_RIGHTBRACKET) {
        if (layers->active_layer + 1 < layers->layer_count) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (!layer_stack_move_to_edge(layers, layers->active_layer, 1)) {
                fprintf(stderr, "Layer is already at the top\n");
            } else if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (key == SDLK_LEFTBRACKET) {
        if (layers->active_layer > 0) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (!layer_stack_move(layers, layers->active_layer, -1)) {
                fprintf(stderr, "Layer is already at the bottom\n");
            } else if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (key == SDLK_RIGHTBRACKET) {
        if (layers->active_layer + 1 < layers->layer_count) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (!layer_stack_move(layers, layers->active_layer, 1)) {
                fprintf(stderr, "Layer is already at the top\n");
            } else if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    active = layer_stack_active(layers);
    if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
        if (active && active->opacity_percent > 0) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (layer_stack_set_opacity(layers, layers->active_layer, active->opacity_percent - 10) && needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (key == SDLK_EQUALS || key == SDLK_KP_PLUS) {
        if (active && active->opacity_percent < 100) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (layer_stack_set_opacity(layers, layers->active_layer, active->opacity_percent + 10) && needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    return 0;
}

static int handle_active_layer_mutation_shortcut(
    SDL_Keycode key,
    int ctrl,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    if (!layers) {
        return 0;
    }

    if (ctrl && shift && key == SDLK_n) {
        if (!layer_stack_can_insert(layers)) {
            fprintf(stderr, "Max layers reached (%d)\n", MAX_LAYERS);
        } else {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_add(layers, NULL, 0x00000000);
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (ctrl && key == SDLK_n) {
        if (!layer_stack_can_insert(layers)) {
            fprintf(stderr, "Could not insert a layer above the active layer\n");
        } else {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_insert(layers, layers->active_layer + 1, NULL, 0x00000000);
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (ctrl && key == SDLK_COMMA) {
        if (!layer_stack_can_insert(layers)) {
            fprintf(stderr, "Could not insert a layer below the active layer\n");
        } else {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_insert(layers, layers->active_layer, NULL, 0x00000000);
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (ctrl && shift && key == SDLK_m) {
        if (!layer_stack_can_flatten(layers)) {
            fprintf(stderr, "Flatten failed (check for locked layers)\n");
        } else {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_flatten(layers, COLOR_BG);
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (ctrl && shift && key == SDLK_e) {
        if (layer_stack_stamp_visible_would_change(layers, layers->active_layer, COLOR_BG)) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (!layer_stack_stamp_visible_into(layers, layers->active_layer, COLOR_BG)) {
                fprintf(stderr, "Stamp visible failed (active layer may be locked)\n");
            } else if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (ctrl && shift && key == SDLK_g) {
        if (!layer_stack_can_insert(layers)) {
            fprintf(stderr, "Could not stamp visible image into a new layer\n");
        } else {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_stamp_visible_new(layers, "Visible Stamp", COLOR_BG);
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (key == SDLK_DELETE || key == SDLK_BACKSPACE) {
        if (!layer_stack_can_delete(layers, layers->active_layer)) {
            fprintf(stderr, "Cannot delete the final or a locked layer\n");
        } else {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_delete(layers, layers->active_layer);
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    return 0;
}

static int handle_file_shortcut(
    SDL_Keycode key,
    int ctrl,
    LayerStack *layers,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_active,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    FileShortcutAction action;
    Layer *active = NULL;
    const Canvas *save_canvas = NULL;

    if (!ctrl || !layers) {
        return 0;
    }

    action = file_shortcut_action(ctrl, (int)key);

    if (action == FILE_SHORTCUT_SAVE) {
        save_canvas = (preview_active && preview_canvas && preview_canvas->pixels) ? preview_canvas : composite;
        if (!canvas_save_bmp(save_canvas, "output.bmp")) {
            fprintf(stderr, "Failed to save output.bmp\n");
        }
        return 1;
    }

    if (action == FILE_SHORTCUT_LOAD) {
        active = layer_stack_active(layers);
        if (!active || active->locked) {
            fprintf(stderr, "Active layer is locked\n");
            return 1;
        }
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!canvas_load_bmp(&active->canvas, "input.bmp", app_active_layer_clear_color(layers->active_layer))) {
            fprintf(stderr, "Failed to load input.bmp\n");
        } else if (needs_composite) {
            *needs_composite = 1;
        }
        return 1;
    }

    return 0;
}

static int handle_merge_shortcut(
    SDL_Keycode key,
    int ctrl,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    MergeShortcutAction action;

    if (!ctrl || !layers) {
        return 0;
    }

    action = merge_shortcut_action(ctrl, (int)key);

    if (action == MERGE_SHORTCUT_DOWN) {
        if (!layer_stack_can_merge_down(layers, layers->active_layer)) {
            fprintf(stderr, "No lower layer to merge into, or one of the layers is locked\n");
        } else {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_merge_down(layers, layers->active_layer);
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (action == MERGE_SHORTCUT_UP) {
        if (!layer_stack_can_merge_up(layers, layers->active_layer)) {
            fprintf(stderr, "No upper layer to merge into, or one of the layers is locked\n");
        } else {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_merge_up(layers, layers->active_layer);
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    return 0;
}

static int handle_layer_visibility_shortcut(
    SDL_Keycode key,
    int ctrl,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    if (!layers || !ctrl) {
        return 0;
    }

    if (key == SDLK_a) {
        if (layers->solo_index >= 0 || layer_stack_visible_count(layers) != layers->layer_count) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (layer_stack_show_all(layers) && needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    if (shift && key == SDLK_r) {
        const Layer *active = layer_stack_get(layers, layers->active_layer);
        if (active && !active->visible) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (layer_stack_show(layers, layers->active_layer) && needs_composite) {
                *needs_composite = 1;
            }
        }
        return 1;
    }

    return 0;
}

static int handle_layer_opacity_reset_shortcut(
    SDL_Keycode key,
    int ctrl,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    Layer *active = NULL;

    if (!layers || !ctrl || key != SDLK_0) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (active && active->opacity_percent != 100) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        layer_stack_set_opacity(layers, layers->active_layer, 100);
        if (needs_composite) {
            *needs_composite = 1;
        }
    }
    return 1;
}

static int handle_brush_and_paint_shortcut(
    PaintShortcutAction paint_action,
    BrushShortcutAction brush_action,
    Tool *tool,
    BrushShape *brush_shape,
    int *brush_radius,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity
) {
    if (!tool || !brush_shape || !brush_radius || !brush_color || !brush_color_rgb || !brush_opacity) {
        return 0;
    }

    if (paint_action == PAINT_SHORTCUT_TOOL_BRUSH) {
        *brush_color_rgb = COLOR_BRUSH & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (paint_action == PAINT_SHORTCUT_TOOL_ERASER) {
        *brush_color_rgb = COLOR_ERASE & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_ERASER;
    } else if (paint_action == PAINT_SHORTCUT_TOOL_LINE) {
        *tool = TOOL_LINE;
    } else if (paint_action == PAINT_SHORTCUT_TOOL_RECT) {
        *tool = TOOL_RECT;
    } else if (paint_action == PAINT_SHORTCUT_TOOL_FILLED_RECT) {
        *tool = TOOL_FILLED_RECT;
    } else if (paint_action == PAINT_SHORTCUT_TOOL_ELLIPSE) {
        *tool = TOOL_ELLIPSE;
    } else if (paint_action == PAINT_SHORTCUT_TOOL_FILLED_ELLIPSE) {
        *tool = TOOL_FILLED_ELLIPSE;
    } else if (brush_action == BRUSH_SHORTCUT_RADIUS_DOWN) {
        if (*brush_radius > 1) {
            *brush_radius -= 1;
        }
    } else if (brush_action == BRUSH_SHORTCUT_RADIUS_UP) {
        if (*brush_radius < 64) {
            *brush_radius += 1;
        }
    } else if (brush_action == BRUSH_SHORTCUT_SHAPE_PREV) {
        *brush_shape = app_cycle_brush_shape(*brush_shape, -1);
    } else if (brush_action == BRUSH_SHORTCUT_SHAPE_NEXT) {
        *brush_shape = app_cycle_brush_shape(*brush_shape, 1);
    } else if (brush_action == BRUSH_SHORTCUT_OPACITY_DOWN) {
        if (*brush_opacity > 1) {
            *brush_opacity -= 5;
            if (*brush_opacity < 1) {
                *brush_opacity = 1;
            }
            *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        }
    } else if (brush_action == BRUSH_SHORTCUT_OPACITY_UP) {
        if (*brush_opacity < 100) {
            *brush_opacity += 5;
            if (*brush_opacity > 100) {
                *brush_opacity = 100;
            }
            *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        }
    } else if (paint_action == PAINT_SHORTCUT_COLOR_BRUSH) {
        *brush_color_rgb = COLOR_BRUSH & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (paint_action == PAINT_SHORTCUT_COLOR_RED) {
        *brush_color_rgb = COLOR_RED & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (paint_action == PAINT_SHORTCUT_COLOR_GREEN) {
        *brush_color_rgb = COLOR_GREEN & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (paint_action == PAINT_SHORTCUT_COLOR_BLUE) {
        *brush_color_rgb = COLOR_BLUE & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (paint_action == PAINT_SHORTCUT_COLOR_YELLOW) {
        *brush_color_rgb = COLOR_YELLOW & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (paint_action == PAINT_SHORTCUT_COLOR_PURPLE) {
        *brush_color_rgb = COLOR_PURPLE & 0x00FFFFFF;
        *brush_color = app_compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else {
        return 0;
    }

    return 1;
}

static int handle_view_and_canvas_shortcut(
    SDL_Keycode key,
    int shift,
    LayerStack *layers,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_active,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    Tool *tool,
    BrushShape *brush_shape,
    int *brush_radius,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity,
    int *needs_composite
) {
    BrushShortcutAction brush_action;
    CanvasShortcutAction canvas_action;
    PaintShortcutAction paint_action;
    ViewShortcutKey view_key = VIEW_SHORTCUT_KEY_NONE;
    ViewShortcutResult view_result;

    if (!layers || !tool || !brush_shape || !brush_radius || !brush_color || !brush_color_rgb || !brush_opacity) {
        return 0;
    }

    switch (key) {
    case SDLK_PAGEUP:
        view_key = VIEW_SHORTCUT_KEY_PAGEUP;
        break;
    case SDLK_PAGEDOWN:
        view_key = VIEW_SHORTCUT_KEY_PAGEDOWN;
        break;
    case SDLK_UP:
        view_key = VIEW_SHORTCUT_KEY_UP;
        break;
    case SDLK_DOWN:
        view_key = VIEW_SHORTCUT_KEY_DOWN;
        break;
    case SDLK_LEFT:
        view_key = VIEW_SHORTCUT_KEY_LEFT;
        break;
    case SDLK_RIGHT:
        view_key = VIEW_SHORTCUT_KEY_RIGHT;
        break;
    default:
        break;
    }

    view_result = view_shortcut_result(view_key, shift);
    if (view_result.action == VIEW_SHORTCUT_CYCLE) {
        return layer_stack_cycle(layers, view_result.cycle_direction) >= 0;
    }

    if (view_result.action == VIEW_SHORTCUT_TRANSLATE) {
        if (app_apply_canvas_translation(
                layers,
                undo_stack,
                undo_count,
                MAX_HISTORY,
                redo_stack,
                redo_count,
                view_result.dx,
                view_result.dy
            ) && needs_composite) {
            *needs_composite = 1;
        }
        return 1;
    }

    paint_action = paint_shortcut_action((int)key);
    brush_action = brush_shortcut_action((int)key);
    canvas_action = canvas_shortcut_action((int)key);

    if (handle_brush_and_paint_shortcut(
            paint_action,
            brush_action,
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            brush_color_rgb,
            brush_opacity
        )) {
    } else if (handle_canvas_mutation_shortcut(
                   canvas_action,
                   layers,
                   undo_stack,
                   undo_count,
                   redo_stack,
                   redo_count,
                   needs_composite
               )) {
    } else if (handle_canvas_sample_shortcut(
                   canvas_action,
                   layers,
                   composite,
                   preview_canvas,
                   preview_active,
                   undo_stack,
                   undo_count,
                   redo_stack,
                   redo_count,
                   tool,
                   brush_color,
                   brush_color_rgb,
                   brush_opacity,
                   needs_composite
               )) {
    } else {
        return 0;
    }

    return 1;
}

static void erase_stamp(Canvas *c, int cx, int cy, int radius, uint32_t clear_color, BrushShape shape) {
    if (!c || !c->pixels || radius <= 0) {
        return;
    }
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (!app_brush_mask_contains(shape, dx, dy, radius)) {
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

static void handle_canvas_motion(
    int x,
    int y,
    int *drawing,
    int *last_x,
    int *last_y,
    int *shaping,
    int shape_start_x,
    int shape_start_y,
    LayerStack *layers,
    Tool tool,
    BrushShape brush_shape,
    int brush_radius,
    uint32_t brush_color,
    uint32_t *shape_base_pixels,
    uint32_t *preview_pixels,
    Canvas *preview_canvas,
    int *preview_active,
    int *needs_composite
) {
    if (!drawing || !last_x || !last_y || !shaping || !layers || !preview_active) {
        return;
    }

    if (*drawing) {
        Layer *active = NULL;

        if (x < 0 || y < 0 || x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT) {
            return;
        }

        active = layer_stack_active(layers);
        if (active && !active->locked && active->canvas.pixels) {
            if (tool == TOOL_ERASER) {
                erase_line(&active->canvas, *last_x, *last_y, x, y, brush_radius, app_active_layer_clear_color(layers->active_layer), brush_shape);
            } else {
                draw_brush_line(&active->canvas, *last_x, *last_y, x, y, brush_radius, brush_color, brush_shape);
            }
            *last_x = x;
            *last_y = y;
            if (needs_composite) {
                *needs_composite = 1;
            }
        }
        return;
    }

    if (*shaping) {
        const Uint8 *state = NULL;
        int shift = 0;
        int end_x = x;
        int end_y = y;

        if (!shape_base_pixels || !preview_canvas || !preview_canvas->pixels || !preview_pixels) {
            return;
        }
        if (x < 0 || y < 0 || x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT) {
            return;
        }

        state = SDL_GetKeyboardState(NULL);
        shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
        app_constrain_shape_end(tool, shape_start_x, shape_start_y, end_x, end_y, shift, &end_x, &end_y);
        memcpy(
            preview_pixels,
            shape_base_pixels,
            (size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t)
        );
        draw_shape(preview_canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
        *preview_active = 1;
    }
}

static void begin_shape_preview(
    int start_x,
    int start_y,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y,
    uint32_t *shape_base_pixels,
    const Canvas *composite
) {
    if (!shaping || !shape_start_x || !shape_start_y) {
        return;
    }

    *shaping = 1;
    *shape_start_x = start_x;
    *shape_start_y = start_y;
    if (shape_base_pixels && composite) {
        memcpy(
            shape_base_pixels,
            composite->pixels,
            (size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t)
        );
    }
}

static void finalize_shape_preview(
    SDL_MouseButtonEvent button,
    LayerStack *layers,
    int *shaping,
    int *preview_active,
    int shape_start_x,
    int shape_start_y,
    Tool tool,
    int brush_radius,
    uint32_t brush_color,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    const Uint8 *state = NULL;
    int shift = 0;
    int end_x = button.x;
    int end_y = button.y;
    Layer *active = NULL;

    if (!layers || !shaping || !preview_active || !*shaping) {
        return;
    }

    state = SDL_GetKeyboardState(NULL);
    shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    app_constrain_shape_end(tool, shape_start_x, shape_start_y, end_x, end_y, shift, &end_x, &end_y);
    active = layer_stack_active(layers);
    if (active && !active->locked && active->canvas.pixels) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        draw_shape(&active->canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
        if (needs_composite) {
            *needs_composite = 1;
        }
    }
    app_cancel_shape_preview(shaping, preview_active);
}

static void handle_mouse_button_down(
    SDL_MouseButtonEvent button,
    LayerStack *layers,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int *drawing,
    int *last_x,
    int *last_y,
    Tool *tool,
    BrushShape brush_shape,
    int brush_radius,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y,
    uint32_t *shape_base_pixels,
    int *preview_active,
    Canvas *preview_canvas_mut,
    int *needs_composite,
    SDL_Window *window
) {
    if (!layers || !drawing || !last_x || !last_y || !tool || !brush_color || !brush_color_rgb || !brush_opacity ||
        !shaping || !shape_start_x || !shape_start_y || !preview_active || !preview_canvas_mut) {
        return;
    }

    if (button.button == SDL_BUTTON_LEFT) {
        *last_x = button.x;
        *last_y = button.y;
        if (*tool == TOOL_BRUSH || *tool == TOOL_ERASER) {
            Layer *active = layer_stack_active(layers);
            if (active && !active->locked && active->canvas.pixels) {
                push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
                *drawing = 1;
                if (*tool == TOOL_ERASER) {
                    erase_stamp(&active->canvas, *last_x, *last_y, brush_radius, app_active_layer_clear_color(layers->active_layer), brush_shape);
                } else {
                    stamp_brush(&active->canvas, *last_x, *last_y, brush_radius, *brush_color, brush_shape);
                }
                if (needs_composite) {
                    *needs_composite = 1;
                }
            }
        } else if (app_layer_editable(layer_stack_get(layers, layers->active_layer))) {
            begin_shape_preview(*last_x, *last_y, shaping, shape_start_x, shape_start_y, shape_base_pixels, composite);
        }
        return;
    }

    if (button.button == SDL_BUTTON_RIGHT) {
        const Canvas *sample = NULL;

        if (*shaping) {
            app_cancel_shape_preview(shaping, preview_active);
            return;
        }
        if (button.x < 0 || button.y < 0 || button.x >= CANVAS_WIDTH || button.y >= CANVAS_HEIGHT) {
            return;
        }

        sample = (*preview_active && preview_canvas_mut->pixels) ? preview_canvas_mut : composite;
        app_apply_sampled_brush_color(canvas_get_pixel(sample, button.x, button.y), tool, brush_color, brush_color_rgb, brush_opacity);
        update_window_title(window, layers, *tool, brush_shape, brush_radius, *brush_color, *brush_opacity);
    }
}

static void handle_mouse_button_up(
    SDL_MouseButtonEvent button,
    LayerStack *layers,
    int *drawing,
    int *shaping,
    int *preview_active,
    int shape_start_x,
    int shape_start_y,
    Tool tool,
    int brush_radius,
    uint32_t brush_color,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    if (!layers || !drawing || !shaping || !preview_active || button.button != SDL_BUTTON_LEFT) {
        return;
    }

    *drawing = 0;
    finalize_shape_preview(
        button,
        layers,
        shaping,
        preview_active,
        shape_start_x,
        shape_start_y,
        tool,
        brush_radius,
        brush_color,
        undo_stack,
        undo_count,
        redo_stack,
        redo_count,
        needs_composite
    );
}

static void handle_key_down(
    SDL_Keycode key,
    LayerStack *layers,
    Canvas *composite,
    Canvas *preview_canvas,
    int preview_active,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    Tool *tool,
    BrushShape *brush_shape,
    int *brush_radius,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity,
    int *shaping,
    int *preview_active_flag,
    int *running,
    int *needs_composite,
    SDL_Window *window
) {
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    int ctrl = state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
    int alt = state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT];
    int shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];

    if (!layers || !composite || !preview_canvas || !undo_count || !redo_count || !tool || !brush_shape ||
        !brush_radius || !brush_color || !brush_color_rgb || !brush_opacity || !shaping || !preview_active_flag ||
        !running || !needs_composite || !window) {
        return;
    }

    if (*shaping && should_cancel_shape_on_key(key, ctrl)) {
        app_cancel_shape_preview(shaping, preview_active_flag);
    }

    if (key == SDLK_ESCAPE) {
        if (*shaping) {
            app_cancel_shape_preview(shaping, preview_active_flag);
            return;
        }
        *running = 0;
        return;
    }

    if (refresh_after_shortcut(
            handle_active_layer_mutation_shortcut(
                key,
                ctrl,
                shift,
                layers,
                undo_stack,
                undo_count,
                redo_stack,
                redo_count,
                needs_composite),
            window,
            layers,
            *tool,
            *brush_shape,
            *brush_radius,
            *brush_color,
            *brush_opacity)) {
        return;
    }

    if (refresh_after_shortcut(
            handle_active_layer_state_shortcut(
                key,
                ctrl,
                shift,
                layers,
                undo_stack,
                undo_count,
                redo_stack,
                redo_count,
                needs_composite),
            window,
            layers,
            *tool,
            *brush_shape,
            *brush_radius,
            *brush_color,
            *brush_opacity)) {
        return;
    }

    if (refresh_after_shortcut(
            handle_active_layer_structure_shortcut(
                key,
                ctrl,
                shift,
                layers,
                undo_stack,
                undo_count,
                redo_stack,
                redo_count,
                needs_composite),
            window,
            layers,
            *tool,
            *brush_shape,
            *brush_radius,
            *brush_color,
            *brush_opacity)) {
        return;
    }

    if (handle_file_shortcut(
            key,
            ctrl,
            layers,
            composite,
            preview_canvas,
            preview_active,
            undo_stack,
            undo_count,
            redo_stack,
            redo_count,
            needs_composite)) {
        return;
    }

    if (refresh_after_shortcut(
            handle_merge_shortcut(
                key,
                ctrl,
                layers,
                undo_stack,
                undo_count,
                redo_stack,
                redo_count,
                needs_composite),
            window,
            layers,
            *tool,
            *brush_shape,
            *brush_radius,
            *brush_color,
            *brush_opacity)) {
        return;
    }

    if (refresh_after_shortcut(
            handle_history_navigation_shortcut(
                key,
                ctrl,
                layers,
                undo_stack,
                undo_count,
                redo_stack,
                redo_count,
                needs_composite),
            window,
            layers,
            *tool,
            *brush_shape,
            *brush_radius,
            *brush_color,
            *brush_opacity)) {
        return;
    }

    if (refresh_after_shortcut(
            handle_direct_layer_shortcut(
                key,
                ctrl,
                alt,
                shift,
                layers,
                undo_stack,
                undo_count,
                redo_stack,
                redo_count,
                needs_composite),
            window,
            layers,
            *tool,
            *brush_shape,
            *brush_radius,
            *brush_color,
            *brush_opacity)) {
        return;
    }

    if (refresh_after_shortcut(
            handle_layer_opacity_reset_shortcut(
                key,
                ctrl,
                layers,
                undo_stack,
                undo_count,
                redo_stack,
                redo_count,
                needs_composite),
            window,
            layers,
            *tool,
            *brush_shape,
            *brush_radius,
            *brush_color,
            *brush_opacity)) {
        return;
    }

    if (refresh_after_shortcut(
            handle_layer_name_shortcut(
                key,
                ctrl,
                alt,
                shift,
                layers,
                undo_stack,
                undo_count,
                redo_stack,
                redo_count),
            window,
            layers,
            *tool,
            *brush_shape,
            *brush_radius,
            *brush_color,
            *brush_opacity)) {
        return;
    }

    if (refresh_after_shortcut(
            handle_layer_visibility_shortcut(
                key,
                ctrl,
                shift,
                layers,
                undo_stack,
                undo_count,
                redo_stack,
                redo_count,
                needs_composite),
            window,
            layers,
            *tool,
            *brush_shape,
            *brush_radius,
            *brush_color,
            *brush_opacity)) {
        return;
    }

    if (refresh_after_shortcut(
            handle_view_and_canvas_shortcut(
                key,
                shift,
                layers,
                composite,
                preview_canvas,
                preview_active,
                undo_stack,
                undo_count,
                redo_stack,
                redo_count,
                tool,
                brush_shape,
                brush_radius,
                brush_color,
                brush_color_rgb,
                brush_opacity,
                needs_composite),
            window,
            layers,
            *tool,
            *brush_shape,
            *brush_radius,
            *brush_color,
            *brush_opacity)) {
        return;
    }

    update_window_title(window, layers, *tool, *brush_shape, *brush_radius, *brush_color, *brush_opacity);
}

static void draw_checkerboard_background(SDL_Renderer *renderer) {
    if (!renderer) {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 30, 30, 34, 255);
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
}

static void render_frame_background(SDL_Renderer *renderer) {
    if (!renderer) {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 30, 30, 34, 255);
    SDL_RenderClear(renderer);
    draw_checkerboard_background(renderer);
}

static void present_canvas_texture(SDL_Renderer *renderer, SDL_Texture *texture) {
    if (!renderer || !texture) {
        return;
    }

    {
        SDL_Rect dest = {0, 0, CANVAS_WIDTH, CANVAS_HEIGHT};
        SDL_RenderCopy(renderer, texture, NULL, &dest);
    }
    SDL_RenderPresent(renderer);
}

static void update_canvas_texture(
    SDL_Texture *texture,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_active
) {
    const uint32_t *pixels = NULL;

    if (!texture || !composite) {
        return;
    }

    if (preview_active && preview_canvas && preview_canvas->pixels) {
        pixels = preview_canvas->pixels;
    } else {
        pixels = composite->pixels;
    }

    if (!pixels) {
        return;
    }

    SDL_UpdateTexture(texture, NULL, pixels, CANVAS_WIDTH * 4);
}

static void render_app_frame(
    SDL_Renderer *renderer,
    SDL_Texture *texture,
    LayerStack *layers,
    Canvas *composite,
    Canvas *preview_canvas,
    int preview_active,
    int *needs_composite
) {
    if (!renderer || !texture || !layers || !composite || !preview_canvas || !needs_composite) {
        return;
    }

    if (!preview_active && *needs_composite) {
        layer_stack_composite(layers, composite, COLOR_BG);
        *needs_composite = 0;
    }

    update_canvas_texture(texture, composite, preview_canvas, preview_active);
    render_frame_background(renderer);
    present_canvas_texture(renderer, texture);
}

static void destroy_app_graphics(
    SDL_Texture *texture,
    SDL_Renderer *renderer,
    SDL_Window *window
) {
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
}

static void shutdown_app(App *app) {
    if (!app) {
        SDL_Quit();
        return;
    }

    free(app->runtime.shape_base_pixels);
    free(app->runtime.preview_pixels);
    snapshot_stack_clear(app->runtime.undo_stack, &app->runtime.undo_count);
    snapshot_stack_clear(app->runtime.redo_stack, &app->runtime.redo_count);
    if (app->composite.pixels) {
        canvas_free(&app->composite);
    }
    if (app->layers.layers) {
        layer_stack_free(&app->layers);
    }
    destroy_app_graphics(app->texture, app->renderer, app->window);
    SDL_Quit();
}

static int initialize_app_graphics(
    SDL_Window **window_out,
    SDL_Renderer **renderer_out,
    SDL_Texture **texture_out
) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;

    if (!window_out || !renderer_out || !texture_out) {
        return 0;
    }

    *window_out = NULL;
    *renderer_out = NULL;
    *texture_out = NULL;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }

    window = SDL_CreateWindow(
        "Openshop - Minimal Paint",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        destroy_app_graphics(NULL, NULL, window);
        SDL_Quit();
        return 0;
    }

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        CANVAS_WIDTH,
        CANVAS_HEIGHT
    );
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        destroy_app_graphics(NULL, renderer, window);
        SDL_Quit();
        return 0;
    }

    *window_out = window;
    *renderer_out = renderer;
    *texture_out = texture;
    return 1;
}

static int initialize_app_document(
    LayerStack *layers_out,
    Canvas *composite_out,
    const char *input_path
) {
    if (!layers_out || !composite_out) {
        return 0;
    }

    *layers_out = (LayerStack){0};
    *composite_out = (Canvas){0};

    if (!layer_stack_init(layers_out, CANVAS_WIDTH, CANVAS_HEIGHT, COLOR_BG)) {
        fprintf(stderr, "Layer stack init failed\n");
        return 0;
    }

    if (!canvas_init(composite_out, CANVAS_WIDTH, CANVAS_HEIGHT)) {
        fprintf(stderr, "Composite canvas init failed\n");
        layer_stack_free(layers_out);
        *layers_out = (LayerStack){0};
        return 0;
    }

    if (input_path && input_path[0]) {
        Layer *active = layer_stack_active(layers_out);
        if (active && !canvas_load_bmp(&active->canvas, input_path, COLOR_BG)) {
            fprintf(stderr, "Failed to load %s\n", input_path);
        }
    }
    layer_stack_composite(layers_out, composite_out, COLOR_BG);
    return 1;
}

static void initialize_app_runtime(AppRuntime *runtime) {
    if (!runtime) {
        return;
    }

    runtime->shape_base_pixels = (uint32_t *)malloc((size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t));
    runtime->preview_pixels = (uint32_t *)malloc((size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t));
    runtime->running = 1;
    runtime->drawing = 0;
    runtime->last_x = 0;
    runtime->last_y = 0;
    runtime->brush_radius = 6;
    runtime->brush_opacity = 100;
    runtime->brush_color_rgb = COLOR_BRUSH & 0x00FFFFFF;
    runtime->brush_color = app_compose_brush_color(runtime->brush_color_rgb, runtime->brush_opacity);
    runtime->brush_shape = BRUSH_SHAPE_ROUND;
    runtime->tool = TOOL_BRUSH;
    runtime->undo_count = 0;
    runtime->redo_count = 0;
    runtime->shaping = 0;
    runtime->shape_start_x = 0;
    runtime->shape_start_y = 0;
    runtime->preview_active = 0;
    runtime->needs_composite = 0;
    memset(runtime->undo_stack, 0, sizeof(runtime->undo_stack));
    memset(runtime->redo_stack, 0, sizeof(runtime->redo_stack));
    runtime->preview_canvas.width = CANVAS_WIDTH;
    runtime->preview_canvas.height = CANVAS_HEIGHT;
    runtime->preview_canvas.pixels = runtime->preview_pixels;
}

static int initialize_app(App *app, const char *input_path) {
    if (!app) {
        return 0;
    }

    *app = (App){0};

    if (!initialize_app_graphics(&app->window, &app->renderer, &app->texture)) {
        return 0;
    }

    if (!initialize_app_document(&app->layers, &app->composite, input_path)) {
        shutdown_app(app);
        return 0;
    }

    return 1;
}

static void handle_app_event(App *app, const SDL_Event *event) {
    if (!app || !event) {
        return;
    }

    switch (event->type) {
    case SDL_QUIT:
        app->runtime.running = 0;
        break;
    case SDL_MOUSEBUTTONDOWN:
        handle_mouse_button_down(
            event->button,
            &app->layers,
            &app->composite,
            &app->runtime.preview_canvas,
            &app->runtime.drawing,
            &app->runtime.last_x,
            &app->runtime.last_y,
            &app->runtime.tool,
            app->runtime.brush_shape,
            app->runtime.brush_radius,
            &app->runtime.brush_color,
            &app->runtime.brush_color_rgb,
            &app->runtime.brush_opacity,
            app->runtime.undo_stack,
            &app->runtime.undo_count,
            app->runtime.redo_stack,
            &app->runtime.redo_count,
            &app->runtime.shaping,
            &app->runtime.shape_start_x,
            &app->runtime.shape_start_y,
            app->runtime.shape_base_pixels,
            &app->runtime.preview_active,
            &app->runtime.preview_canvas,
            &app->runtime.needs_composite,
            app->window
        );
        break;
    case SDL_MOUSEBUTTONUP:
        handle_mouse_button_up(
            event->button,
            &app->layers,
            &app->runtime.drawing,
            &app->runtime.shaping,
            &app->runtime.preview_active,
            app->runtime.shape_start_x,
            app->runtime.shape_start_y,
            app->runtime.tool,
            app->runtime.brush_radius,
            app->runtime.brush_color,
            app->runtime.undo_stack,
            &app->runtime.undo_count,
            app->runtime.redo_stack,
            &app->runtime.redo_count,
            &app->runtime.needs_composite
        );
        break;
    case SDL_MOUSEMOTION:
        handle_canvas_motion(
            event->motion.x,
            event->motion.y,
            &app->runtime.drawing,
            &app->runtime.last_x,
            &app->runtime.last_y,
            &app->runtime.shaping,
            app->runtime.shape_start_x,
            app->runtime.shape_start_y,
            &app->layers,
            app->runtime.tool,
            app->runtime.brush_shape,
            app->runtime.brush_radius,
            app->runtime.brush_color,
            app->runtime.shape_base_pixels,
            app->runtime.preview_pixels,
            &app->runtime.preview_canvas,
            &app->runtime.preview_active,
            &app->runtime.needs_composite
        );
        break;
    case SDL_KEYDOWN:
        handle_key_down(
            event->key.keysym.sym,
            &app->layers,
            &app->composite,
            &app->runtime.preview_canvas,
            app->runtime.preview_active,
            app->runtime.undo_stack,
            &app->runtime.undo_count,
            app->runtime.redo_stack,
            &app->runtime.redo_count,
            &app->runtime.tool,
            &app->runtime.brush_shape,
            &app->runtime.brush_radius,
            &app->runtime.brush_color,
            &app->runtime.brush_color_rgb,
            &app->runtime.brush_opacity,
            &app->runtime.shaping,
            &app->runtime.preview_active,
            &app->runtime.running,
            &app->runtime.needs_composite,
            app->window
        );
        break;
    default:
        break;
    }
}

static void run_app_loop(App *app) {
    if (!app) {
        return;
    }

    while (app->runtime.running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            handle_app_event(app, &e);
        }

        render_app_frame(
            app->renderer,
            app->texture,
            &app->layers,
            &app->composite,
            &app->runtime.preview_canvas,
            app->runtime.preview_active,
            &app->runtime.needs_composite
        );
        SDL_Delay(16);
    }
}

int app_run(const char *input_path) {
    App app = {0};

    if (!initialize_app(&app, input_path)) {
        return 1;
    }

    initialize_app_runtime(&app.runtime);
    update_window_title(app.window, &app.layers, app.runtime.tool, app.runtime.brush_shape, app.runtime.brush_radius, app.runtime.brush_color, app.runtime.brush_opacity);
    run_app_loop(&app);
    shutdown_app(&app);
    return 0;
}
