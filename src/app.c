#include "app.h"
#include "app_brush.h"
#include "app_brush_mask.h"
#include "app_canvas_click.h"
#include "app_canvas_ops.h"
#include "app_color.h"
#include "app_layer_state.h"
#include "app_layout.h"
#include "app_preview.h"
#include "app_runtime_shortcuts.h"
#include "app_sampled_color.h"
#include "app_shape.h"
#include "app_shape_cancel.h"
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

#define WINDOW_WIDTH APP_LAYOUT_WINDOW_WIDTH
#define WINDOW_HEIGHT APP_LAYOUT_WINDOW_HEIGHT
#define CANVAS_WIDTH APP_LAYOUT_CANVAS_WIDTH
#define CANVAS_HEIGHT APP_LAYOUT_CANVAS_HEIGHT
#define CANVAS_ORIGIN_X APP_LAYOUT_CANVAS_X
#define CANVAS_ORIGIN_Y APP_LAYOUT_CANVAS_Y
#define RIGHT_PANEL_X APP_LAYOUT_RIGHT_PANEL_X
#define RIGHT_PANEL_WIDTH APP_LAYOUT_RIGHT_PANEL_WIDTH
#define BOTTOM_PANEL_Y APP_LAYOUT_BOTTOM_PANEL_Y
#define MAX_HISTORY 20

static const uint32_t COLOR_BG = 0xFFFFFFFF;     // white
static const uint32_t COLOR_BRUSH = 0xFF1B1F24;  // near-black
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
    int compact_mode;
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

static void push_snapshot(const LayerStack *layers, Snapshot *undo_stack, int *undo_count, Snapshot *redo_stack, int *redo_count);
static AppShapeCancelKey app_shape_cancel_key_from_sdl(SDL_Keycode key);
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
);
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
);
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
);
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
);
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
);
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
);
static int handle_merge_shortcut(
    SDL_Keycode key,
    int ctrl,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
);
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
);

static void push_snapshot(const LayerStack *layers, Snapshot *undo_stack, int *undo_count, Snapshot *redo_stack, int *redo_count) {
    snapshot_push(layers, undo_stack, undo_count, MAX_HISTORY, redo_stack, redo_count);
}

static int screen_to_canvas_point(int screen_x, int screen_y, int *canvas_x, int *canvas_y) {
    AppLayout layout = app_layout_default();
    return app_layout_screen_to_canvas(&layout, screen_x, screen_y, canvas_x, canvas_y);
}

static void screen_to_canvas_point_clamped(int screen_x, int screen_y, int *canvas_x, int *canvas_y) {
    AppLayout layout = app_layout_default();
    app_layout_screen_to_canvas_clamped(&layout, screen_x, screen_y, canvas_x, canvas_y);
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
    int mx = 0;
    int my = 0;
    int canvas_x = 0;
    int canvas_y = 0;

    SDL_GetMouseState(&mx, &my);
    if (!screen_to_canvas_point(mx, my, &canvas_x, &canvas_y)) {
        return 0;
    }

    return app_handle_canvas_sample_shortcut_at(
        canvas_action,
        layers,
        composite,
        preview_canvas,
        preview_active,
        canvas_x,
        canvas_y,
        CANVAS_WIDTH,
        CANVAS_HEIGHT,
        undo_stack,
        undo_count,
        MAX_HISTORY,
        redo_stack,
        redo_count,
        tool,
        brush_color,
        brush_color_rgb,
        brush_opacity,
        needs_composite
    );
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
    if (active && active->blend_mode != BLEND_NORMAL) {
        size_t len = strlen(title);
        snprintf(title + len, sizeof(title) - len, " | blend %s", blend_mode_name((BlendMode)active->blend_mode));
    }
    SDL_SetWindowTitle(window, title);
}

static void update_window_title_from_brush_state(
    SDL_Window *window,
    const LayerStack *layers,
    const Tool *tool,
    const BrushShape *brush_shape,
    const int *brush_radius,
    const uint32_t *brush_color,
    const int *brush_opacity
) {
    if (!tool || !brush_shape || !brush_radius || !brush_color || !brush_opacity) {
        return;
    }

    update_window_title(
        window,
        layers,
        *tool,
        *brush_shape,
        *brush_radius,
        *brush_color,
        *brush_opacity
    );
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

static int handle_key_down_layer_shortcuts(
    SDL_Keycode key,
    int ctrl,
    int alt,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite,
    SDL_Window *window,
    Tool tool,
    BrushShape brush_shape,
    int brush_radius,
    uint32_t brush_color,
    int brush_opacity
) {
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
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            brush_opacity)) {
        return 1;
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
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            brush_opacity)) {
        return 1;
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
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            brush_opacity)) {
        return 1;
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
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            brush_opacity)) {
        return 1;
    }

    if (refresh_after_shortcut(
            app_handle_history_navigation_shortcut(
                (int)key,
                ctrl,
                layers,
                undo_stack,
                undo_count,
                MAX_HISTORY,
                redo_stack,
                redo_count,
                needs_composite),
            window,
            layers,
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            brush_opacity)) {
        return 1;
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
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            brush_opacity)) {
        return 1;
    }

    if (refresh_after_shortcut(
            app_handle_layer_opacity_reset_shortcut(
                (int)key,
                ctrl,
                layers,
                undo_stack,
                undo_count,
                MAX_HISTORY,
                redo_stack,
                redo_count,
                needs_composite),
            window,
            layers,
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            brush_opacity)) {
        return 1;
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
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            brush_opacity)) {
        return 1;
    }

    if (refresh_after_shortcut(
            app_handle_layer_visibility_shortcut(
                (int)key,
                ctrl,
                shift,
                layers,
                undo_stack,
                undo_count,
                MAX_HISTORY,
                redo_stack,
                redo_count,
                needs_composite),
            window,
            layers,
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            brush_opacity)) {
        return 1;
    }

    return 0;
}

static int handle_key_down_file_and_view_shortcuts(
    SDL_Keycode key,
    int ctrl,
    int shift,
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
    int *needs_composite,
    SDL_Window *window
) {
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
        return 1;
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
        return 1;
    }

    return 0;
}

static int handle_key_down_shortcuts(
    SDL_Keycode key,
    int ctrl,
    int alt,
    int shift,
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
    int *needs_composite,
    SDL_Window *window
) {
    if (handle_key_down_layer_shortcuts(
            key,
            ctrl,
            alt,
            shift,
            layers,
            undo_stack,
            undo_count,
            redo_stack,
            redo_count,
            needs_composite,
            window,
            *tool,
            *brush_shape,
            *brush_radius,
            *brush_color,
            *brush_opacity)) {
        return 1;
    }

    if (handle_key_down_file_and_view_shortcuts(
            key,
            ctrl,
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
            needs_composite,
            window)) {
        return 1;
    }

    return 0;
}

static int handle_key_down_runtime(
    SDL_Keycode key,
    int ctrl,
    int alt,
    int shift,
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
    AppPreviewKeyResult preview_key_result = app_handle_shape_preview_key(
        app_shape_cancel_key_from_sdl(key),
        ctrl,
        shaping,
        preview_active_flag,
        running
    );

    if (preview_key_result == APP_PREVIEW_KEY_RESULT_HANDLED || key == SDLK_ESCAPE) {
        return 1;
    }

    return handle_key_down_shortcuts(
        key,
        ctrl,
        alt,
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
        needs_composite,
        window
    );
}

static AppShapeCancelKey app_shape_cancel_key_from_sdl(SDL_Keycode key) {
    switch (key) {
    case SDLK_ESCAPE: return APP_SHAPE_CANCEL_KEY_ESCAPE;
    case SDLK_LSHIFT: return APP_SHAPE_CANCEL_KEY_LSHIFT;
    case SDLK_RSHIFT: return APP_SHAPE_CANCEL_KEY_RSHIFT;
    case SDLK_s: return APP_SHAPE_CANCEL_KEY_S;
    case SDLK_o: return APP_SHAPE_CANCEL_KEY_O;
    case SDLK_z: return APP_SHAPE_CANCEL_KEY_Z;
    case SDLK_y: return APP_SHAPE_CANCEL_KEY_Y;
    case SDLK_n: return APP_SHAPE_CANCEL_KEY_N;
    case SDLK_u: return APP_SHAPE_CANCEL_KEY_U;
    case SDLK_v: return APP_SHAPE_CANCEL_KEY_V;
    case SDLK_m: return APP_SHAPE_CANCEL_KEY_M;
    case SDLK_d: return APP_SHAPE_CANCEL_KEY_D;
    case SDLK_e: return APP_SHAPE_CANCEL_KEY_E;
    case SDLK_g: return APP_SHAPE_CANCEL_KEY_G;
    case SDLK_h: return APP_SHAPE_CANCEL_KEY_H;
    case SDLK_l: return APP_SHAPE_CANCEL_KEY_L;
    case SDLK_a: return APP_SHAPE_CANCEL_KEY_A;
    case SDLK_r: return APP_SHAPE_CANCEL_KEY_R;
    case SDLK_0: return APP_SHAPE_CANCEL_KEY_0;
    case SDLK_COMMA: return APP_SHAPE_CANCEL_KEY_COMMA;
    case SDLK_LEFTBRACKET: return APP_SHAPE_CANCEL_KEY_LEFTBRACKET;
    case SDLK_RIGHTBRACKET: return APP_SHAPE_CANCEL_KEY_RIGHTBRACKET;
    case SDLK_MINUS: return APP_SHAPE_CANCEL_KEY_MINUS;
    case SDLK_KP_MINUS: return APP_SHAPE_CANCEL_KEY_KP_MINUS;
    case SDLK_EQUALS: return APP_SHAPE_CANCEL_KEY_EQUALS;
    case SDLK_KP_PLUS: return APP_SHAPE_CANCEL_KEY_KP_PLUS;
    case SDLK_SLASH: return APP_SHAPE_CANCEL_KEY_SLASH;
    case SDLK_1: return APP_SHAPE_CANCEL_KEY_1;
    case SDLK_2: return APP_SHAPE_CANCEL_KEY_2;
    case SDLK_3: return APP_SHAPE_CANCEL_KEY_3;
    case SDLK_4: return APP_SHAPE_CANCEL_KEY_4;
    case SDLK_5: return APP_SHAPE_CANCEL_KEY_5;
    case SDLK_6: return APP_SHAPE_CANCEL_KEY_6;
    case SDLK_7: return APP_SHAPE_CANCEL_KEY_7;
    case SDLK_8: return APP_SHAPE_CANCEL_KEY_8;
    case SDLK_b: return APP_SHAPE_CANCEL_KEY_B;
    case SDLK_t: return APP_SHAPE_CANCEL_KEY_T;
    case SDLK_p: return APP_SHAPE_CANCEL_KEY_P;
    case SDLK_PERIOD: return APP_SHAPE_CANCEL_KEY_PERIOD;
    case SDLK_c: return APP_SHAPE_CANCEL_KEY_C;
    case SDLK_j: return APP_SHAPE_CANCEL_KEY_J;
    case SDLK_x: return APP_SHAPE_CANCEL_KEY_X;
    case SDLK_f: return APP_SHAPE_CANCEL_KEY_F;
    case SDLK_i: return APP_SHAPE_CANCEL_KEY_I;
    case SDLK_UP: return APP_SHAPE_CANCEL_KEY_UP;
    case SDLK_DOWN: return APP_SHAPE_CANCEL_KEY_DOWN;
    case SDLK_LEFT: return APP_SHAPE_CANCEL_KEY_LEFT;
    case SDLK_RIGHT: return APP_SHAPE_CANCEL_KEY_RIGHT;
    case SDLK_PAGEUP: return APP_SHAPE_CANCEL_KEY_PAGEUP;
    case SDLK_PAGEDOWN: return APP_SHAPE_CANCEL_KEY_PAGEDOWN;
    case SDLK_F2: return APP_SHAPE_CANCEL_KEY_F2;
    case SDLK_DELETE: return APP_SHAPE_CANCEL_KEY_DELETE;
    case SDLK_BACKSPACE: return APP_SHAPE_CANCEL_KEY_BACKSPACE;
    default: return APP_SHAPE_CANCEL_KEY_OTHER;
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
    int runtime_key = 0;

    if (key == SDLK_l) {
        runtime_key = 'l';
    } else if (key == SDLK_v) {
        runtime_key = 'v';
    } else if (key == SDLK_h) {
        runtime_key = 'h';
    } else if (key == SDLK_SLASH) {
        runtime_key = '/';
    }

    return app_handle_active_layer_state_shortcut(
        runtime_key,
        ctrl,
        shift,
        layers,
        undo_stack,
        undo_count,
        MAX_HISTORY,
        redo_stack,
        redo_count,
        needs_composite
    );
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
    if (!layers || !ctrl) {
        return 0;
    }

    if (app_handle_active_layer_duplicate_shortcut(
            key == SDLK_d ? 'd' : 0,
            ctrl,
            layers,
            undo_stack,
            undo_count,
            MAX_HISTORY,
            redo_stack,
            redo_count,
            needs_composite
        )) {
        return 1;
    }

    if (app_handle_active_layer_reorder_shortcut(
            key == SDLK_LEFTBRACKET ? '[' :
            key == SDLK_RIGHTBRACKET ? ']' :
            0,
            shift,
            layers,
            undo_stack,
            undo_count,
            MAX_HISTORY,
            redo_stack,
            redo_count,
            needs_composite
        )) {
        return 1;
    }

    if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
        app_handle_active_layer_opacity_step(
            layers,
            -10,
            undo_stack,
            undo_count,
            MAX_HISTORY,
            redo_stack,
            redo_count,
            needs_composite
        );
        return 1;
    }

    if (key == SDLK_EQUALS || key == SDLK_KP_PLUS) {
        app_handle_active_layer_opacity_step(
            layers,
            10,
            undo_stack,
            undo_count,
            MAX_HISTORY,
            redo_stack,
            redo_count,
            needs_composite
        );
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

    if (app_handle_active_layer_add_shortcut(
            key == SDLK_n ? 'n' : 0,
            ctrl,
            shift,
            layers,
            undo_stack,
            undo_count,
            MAX_HISTORY,
            redo_stack,
            redo_count,
            needs_composite
        )) {
        return 1;
    }

    if (ctrl && key == SDLK_n) {
        app_handle_active_layer_insert_shortcut(
            'n',
            ctrl,
            1,
            layers,
            undo_stack,
            undo_count,
            MAX_HISTORY,
            redo_stack,
            redo_count,
            needs_composite
        );
        return 1;
    }

    if (ctrl && key == SDLK_COMMA) {
        app_handle_active_layer_insert_shortcut(
            ',',
            ctrl,
            0,
            layers,
            undo_stack,
            undo_count,
            MAX_HISTORY,
            redo_stack,
            redo_count,
            needs_composite
        );
        return 1;
    }

    if (app_handle_active_layer_composite_shortcut(
            key == SDLK_m ? 'm' :
            key == SDLK_e ? 'e' :
            key == SDLK_g ? 'g' :
            0,
            ctrl,
            shift,
            layers,
            undo_stack,
            undo_count,
            MAX_HISTORY,
            redo_stack,
            redo_count,
            needs_composite
        )) {
        return 1;
    }

    if (key == SDLK_DELETE || key == SDLK_BACKSPACE) {
        app_handle_active_layer_delete_shortcut(
            key == SDLK_DELETE ? 127 : '\b',
            layers,
            undo_stack,
            undo_count,
            MAX_HISTORY,
            redo_stack,
            redo_count,
            needs_composite
        );
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
        save_canvas = app_preview_canvas_or_composite(composite, preview_canvas, preview_active);
        if (!canvas_save_bmp(save_canvas, "output.bmp")) {
            fprintf(stderr, "Failed to save output.bmp\n");
        }
        return 1;
    }

    if (action == FILE_SHORTCUT_LOAD) {
        active = app_active_editable_layer(layers);
        if (!active) {
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

static ViewShortcutKey view_shortcut_key_from_sdl(SDL_Keycode key) {
    switch (key) {
    case SDLK_PAGEUP:
        return VIEW_SHORTCUT_KEY_PAGEUP;
    case SDLK_PAGEDOWN:
        return VIEW_SHORTCUT_KEY_PAGEDOWN;
    case SDLK_UP:
        return VIEW_SHORTCUT_KEY_UP;
    case SDLK_DOWN:
        return VIEW_SHORTCUT_KEY_DOWN;
    case SDLK_LEFT:
        return VIEW_SHORTCUT_KEY_LEFT;
    case SDLK_RIGHT:
        return VIEW_SHORTCUT_KEY_RIGHT;
    default:
        return VIEW_SHORTCUT_KEY_NONE;
    }
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
    ViewShortcutResult view_result;

    if (!layers || !tool || !brush_shape || !brush_radius || !brush_color || !brush_color_rgb || !brush_opacity) {
        return 0;
    }

    view_result = view_shortcut_result(view_shortcut_key_from_sdl(key), shift);
    if (app_handle_view_shortcut(
            view_result,
            layers,
            undo_stack,
            undo_count,
            MAX_HISTORY,
            redo_stack,
            redo_count,
            needs_composite)) {
        return 1;
    }

    paint_action = paint_shortcut_action((int)key);
    brush_action = brush_shortcut_action((int)key);
    canvas_action = canvas_shortcut_action((int)key);

    if (app_handle_brush_and_paint_shortcut(
            paint_action,
            brush_action,
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            brush_color_rgb,
            brush_opacity
        )) {
    } else if (app_handle_canvas_mutation_shortcut(
                   canvas_action,
                   layers,
                   undo_stack,
                   undo_count,
                   MAX_HISTORY,
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

static void sdl_shortcut_modifiers(int *ctrl, int *alt, int *shift) {
    const Uint8 *state = SDL_GetKeyboardState(NULL);

    if (!state) {
        if (ctrl) {
            *ctrl = 0;
        }
        if (alt) {
            *alt = 0;
        }
        if (shift) {
            *shift = 0;
        }
        return;
    }

    if (ctrl) {
        *ctrl = state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
    }
    if (alt) {
        *alt = state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT];
    }
    if (shift) {
        *shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
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
    int shift = 0;
    int canvas_x = 0;
    int canvas_y = 0;

    if (!screen_to_canvas_point(x, y, &canvas_x, &canvas_y)) {
        if (!drawing || !*drawing) {
            return;
        }
        screen_to_canvas_point_clamped(x, y, &canvas_x, &canvas_y);
    }

    sdl_shortcut_modifiers(NULL, NULL, &shift);
    app_handle_canvas_motion(
        canvas_x,
        canvas_y,
        drawing,
        last_x,
        last_y,
        shaping,
        shape_start_x,
        shape_start_y,
        shift,
        layers,
        tool,
        brush_shape,
        brush_radius,
        brush_color,
        shape_base_pixels,
        preview_pixels,
        preview_canvas,
        preview_active,
        needs_composite,
        (size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT
    );
}

static void handle_mouse_button_down(
    SDL_MouseButtonEvent button,
    LayerStack *layers,
    const Canvas *composite,
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
    int canvas_x = 0;
    int canvas_y = 0;

    if (!layers || !drawing || !last_x || !last_y || !tool || !brush_color || !brush_color_rgb || !brush_opacity ||
        !shaping || !shape_start_x || !shape_start_y || !preview_active || !preview_canvas_mut) {
        return;
    }

    if (!screen_to_canvas_point(button.x, button.y, &canvas_x, &canvas_y)) {
        return;
    }

    if (button.button == SDL_BUTTON_LEFT) {
        app_handle_left_canvas_press(
            layers,
            canvas_x,
            canvas_y,
            last_x,
            last_y,
            *tool,
            brush_shape,
            brush_radius,
            *brush_color,
            composite,
            undo_stack,
            undo_count,
            MAX_HISTORY,
            redo_stack,
            redo_count,
            drawing,
            shaping,
            shape_start_x,
            shape_start_y,
            shape_base_pixels,
            needs_composite
        );
        return;
    }

    if (button.button == SDL_BUTTON_RIGHT) {
        if (app_canvas_click_result_refreshes_title(app_handle_right_canvas_press(
            shaping,
            preview_active,
            composite,
            preview_canvas_mut,
            *preview_active,
            canvas_x,
            canvas_y,
            tool,
            brush_color,
            brush_color_rgb,
            brush_opacity
        ))) {
            update_window_title_from_brush_state(
                window,
                layers,
                tool,
                &brush_shape,
                &brush_radius,
                brush_color,
                brush_opacity
            );
        }
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
    int shift = 0;
    int canvas_x = 0;
    int canvas_y = 0;

    if (!layers || !drawing || !shaping || !preview_active || button.button != SDL_BUTTON_LEFT) {
        return;
    }

    if (!screen_to_canvas_point(button.x, button.y, &canvas_x, &canvas_y)) {
        if (!*drawing && !*shaping) {
            return;
        }
        screen_to_canvas_point_clamped(button.x, button.y, &canvas_x, &canvas_y);
    }

    sdl_shortcut_modifiers(NULL, NULL, &shift);
    app_handle_left_canvas_release(
        drawing,
        layers,
        shaping,
        preview_active,
        shape_start_x,
        shape_start_y,
        canvas_x,
        canvas_y,
        shift,
        tool,
        brush_radius,
        brush_color,
        undo_stack,
        undo_count,
        MAX_HISTORY,
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
    int ctrl = 0;
    int alt = 0;
    int shift = 0;

    if (!layers || !composite || !preview_canvas || !undo_count || !redo_count || !tool || !brush_shape ||
        !brush_radius || !brush_color || !brush_color_rgb || !brush_opacity || !shaping || !preview_active_flag ||
        !running || !needs_composite || !window) {
        return;
    }

    sdl_shortcut_modifiers(&ctrl, &alt, &shift);

    if (handle_key_down_runtime(
            key,
            ctrl,
            alt,
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
            shaping,
            preview_active_flag,
            running,
            needs_composite,
            window)) {
        return;
    }

    update_window_title_from_brush_state(
        window,
        layers,
        tool,
        brush_shape,
        brush_radius,
        brush_color,
        brush_opacity
    );
}

static void set_render_color(SDL_Renderer *renderer, uint32_t argb) {
    SDL_SetRenderDrawColor(
        renderer,
        (Uint8)((argb >> 16) & 0xFF),
        (Uint8)((argb >> 8) & 0xFF),
        (Uint8)(argb & 0xFF),
        (Uint8)((argb >> 24) & 0xFF)
    );
}

static void fill_rect(SDL_Renderer *renderer, int x, int y, int w, int h, uint32_t argb) {
    SDL_Rect rect = {x, y, w, h};
    set_render_color(renderer, argb);
    SDL_RenderFillRect(renderer, &rect);
}

static void stroke_rect(SDL_Renderer *renderer, int x, int y, int w, int h, uint32_t argb) {
    SDL_Rect rect = {x, y, w, h};
    set_render_color(renderer, argb);
    SDL_RenderDrawRect(renderer, &rect);
}

static void draw_checkerboard_background(SDL_Renderer *renderer) {
    if (!renderer) {
        return;
    }

    fill_rect(renderer, CANVAS_ORIGIN_X - 12, CANVAS_ORIGIN_Y - 12, CANVAS_WIDTH + 24, CANVAS_HEIGHT + 24, 0xFF111113);
    for (int y = 0; y < CANVAS_HEIGHT; y += CHECKER_SIZE) {
        for (int x = 0; x < CANVAS_WIDTH; x += CHECKER_SIZE) {
            int even = ((x / CHECKER_SIZE) + (y / CHECKER_SIZE)) % 2 == 0;
            if (even) {
                set_render_color(renderer, 0xFFE8E8EC);
            } else {
                set_render_color(renderer, 0xFFCECED4);
            }
            SDL_Rect cell = {CANVAS_ORIGIN_X + x, CANVAS_ORIGIN_Y + y, CHECKER_SIZE, CHECKER_SIZE};
            SDL_RenderFillRect(renderer, &cell);
        }
    }
}

static void draw_toolbar_button(SDL_Renderer *renderer, int index, int active) {
    AppLayout layout = app_layout_default();
    AppRect button = app_layout_toolbar_button(&layout, index);
    int x = button.x;
    int y = button.y;
    uint32_t border = active ? 0xFF5DADE2 : 0xFF4A4A50;
    uint32_t glyph = active ? 0xFFE7F3FF : 0xFFC9CDD3;

    fill_rect(renderer, x, y, 36, 34, active ? 0xFF2D4D68 : 0xFF242428);
    stroke_rect(renderer, x, y, 36, 34, border);

    if (index == 0) {
        SDL_Point points[3] = {{x + 12, y + 9}, {x + 25, y + 17}, {x + 13, y + 25}};
        set_render_color(renderer, glyph);
        SDL_RenderDrawLines(renderer, points, 3);
    } else if (index == 1) {
        fill_rect(renderer, x + 11, y + 9, 14, 14, glyph);
        fill_rect(renderer, x + 21, y + 21, 5, 5, glyph);
    } else if (index == 2) {
        fill_rect(renderer, x + 10, y + 22, 18, 4, glyph);
        fill_rect(renderer, x + 18, y + 8, 6, 16, glyph);
    } else if (index == 3) {
        stroke_rect(renderer, x + 9, y + 9, 19, 16, glyph);
    } else if (index == 4) {
        SDL_RenderDrawLine(renderer, x + 9, y + 25, x + 28, y + 9);
    } else if (index == 5) {
        stroke_rect(renderer, x + 9, y + 10, 19, 14, glyph);
        SDL_RenderDrawLine(renderer, x + 9, y + 10, x + 28, y + 24);
    } else {
        fill_rect(renderer, x + 9, y + 9, 9, 9, 0xFFE53935);
        fill_rect(renderer, x + 18, y + 9, 9, 9, 0xFF1E88E5);
        fill_rect(renderer, x + 9, y + 18, 9, 9, 0xFFFDD835);
        fill_rect(renderer, x + 18, y + 18, 9, 9, 0xFF43A047);
    }
}

static void draw_photoshop_shell(SDL_Renderer *renderer, int compact) {
    if (!renderer) {
        return;
    }

    fill_rect(renderer, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0xFF1F1F23);

    if (compact) {
        stroke_rect(renderer, CANVAS_ORIGIN_X - 1, CANVAS_ORIGIN_Y - 1, CANVAS_WIDTH + 2, CANVAS_HEIGHT + 2, 0xFF0B0B0D);
        return;
    }

    fill_rect(renderer, 0, 0, WINDOW_WIDTH, 24, 0xFF2B2B2F);
    for (int i = 0; i < 7; i++) {
        fill_rect(renderer, 18 + i * 58, 8, 34 + (i % 3) * 8, 5, 0xFFBFC3C9);
    }

    fill_rect(renderer, 0, 24, WINDOW_WIDTH, 28, 0xFF333338);
    fill_rect(renderer, 78, 34, 108, 8, 0xFF56565D);
    fill_rect(renderer, 206, 34, 64, 8, 0xFF56565D);
    fill_rect(renderer, 292, 34, 88, 8, 0xFF56565D);
    fill_rect(renderer, 404, 34, 52, 8, 0xFF56565D);

    fill_rect(renderer, 0, 52, 64, WINDOW_HEIGHT - 52, 0xFF2A2A2F);
    for (int i = 0; i < 8; i++) {
        draw_toolbar_button(renderer, i, i == 1);
    }
    fill_rect(renderer, 15, 432, 16, 16, 0xFF1B1F24);
    fill_rect(renderer, 31, 448, 16, 16, 0xFFFDD835);
    stroke_rect(renderer, 14, 431, 18, 18, 0xFF0C0C0D);
    stroke_rect(renderer, 30, 447, 18, 18, 0xFF0C0C0D);

    fill_rect(renderer, RIGHT_PANEL_X, 52, RIGHT_PANEL_WIDTH, WINDOW_HEIGHT - 64, 0xFF2B2B30);
    fill_rect(renderer, RIGHT_PANEL_X + 10, 68, RIGHT_PANEL_WIDTH - 20, 18, 0xFF3A3A40);
    fill_rect(renderer, RIGHT_PANEL_X + 10, 96, 28, 28, 0xFFE53935);
    fill_rect(renderer, RIGHT_PANEL_X + 41, 96, 28, 28, 0xFF43A047);
    fill_rect(renderer, RIGHT_PANEL_X + 72, 96, 28, 28, 0xFF1E88E5);

    fill_rect(renderer, RIGHT_PANEL_X + 10, 150, RIGHT_PANEL_WIDTH - 20, 18, 0xFF3A3A40);
    for (int i = 0; i < 4; i++) {
        int y = 182 + i * 42;
        fill_rect(renderer, RIGHT_PANEL_X + 14, y, 32, 24, i == 1 ? 0xFF5DADE2 : 0xFF5B6573);
        fill_rect(renderer, RIGHT_PANEL_X + 54, y + 5, 48, 6, i == 1 ? 0xFFE8EDF5 : 0xFF8B929C);
        fill_rect(renderer, RIGHT_PANEL_X + 54, y + 17, 36, 5, 0xFF626A73);
        stroke_rect(renderer, RIGHT_PANEL_X + 10, y - 5, RIGHT_PANEL_WIDTH - 20, 34, i == 1 ? 0xFF5DADE2 : 0xFF484850);
    }

    fill_rect(renderer, RIGHT_PANEL_X + 10, 392, RIGHT_PANEL_WIDTH - 20, 18, 0xFF3A3A40);
    fill_rect(renderer, RIGHT_PANEL_X + 16, 428, 92, 6, 0xFF5DADE2);
    fill_rect(renderer, RIGHT_PANEL_X + 16, 460, 68, 6, 0xFFFDD835);
    fill_rect(renderer, RIGHT_PANEL_X + 16, 492, 82, 6, 0xFF8E24AA);

    fill_rect(renderer, 64, BOTTOM_PANEL_Y, 800, 72, 0xFF252529);
    fill_rect(renderer, 82, BOTTOM_PANEL_Y + 18, 170, 10, 0xFF7B7F87);
    fill_rect(renderer, 292, BOTTOM_PANEL_Y + 18, 96, 10, 0xFF7B7F87);
    fill_rect(renderer, 430, BOTTOM_PANEL_Y + 18, 148, 10, 0xFF7B7F87);
    fill_rect(renderer, 82, BOTTOM_PANEL_Y + 45, 310, 8, 0xFF3F4147);

    stroke_rect(renderer, CANVAS_ORIGIN_X - 1, CANVAS_ORIGIN_Y - 1, CANVAS_WIDTH + 2, CANVAS_HEIGHT + 2, 0xFF0B0B0D);
    stroke_rect(renderer, CANVAS_ORIGIN_X - 12, CANVAS_ORIGIN_Y - 12, CANVAS_WIDTH + 24, CANVAS_HEIGHT + 24, 0xFF333338);
}

static void render_frame_background(SDL_Renderer *renderer, int compact) {
    if (!renderer) {
        return;
    }

    draw_photoshop_shell(renderer, compact);
    draw_checkerboard_background(renderer);
}

static void present_canvas_texture(SDL_Renderer *renderer, SDL_Texture *texture) {
    if (!renderer || !texture) {
        return;
    }

    {
        SDL_Rect dest = {CANVAS_ORIGIN_X, CANVAS_ORIGIN_Y, CANVAS_WIDTH, CANVAS_HEIGHT};
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

    pixels = app_preview_canvas_or_composite(composite, preview_canvas, preview_active)->pixels;

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
    int *needs_composite,
    int compact_mode
) {
    if (!renderer || !texture || !layers || !composite || !preview_canvas || !needs_composite) {
        return;
    }

    if (!preview_active && *needs_composite) {
        layer_stack_composite(layers, composite, COLOR_BG);
        *needs_composite = 0;
    }

    update_canvas_texture(texture, composite, preview_canvas, preview_active);
    render_frame_background(renderer, compact_mode);
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
    runtime->compact_mode = 0;
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
    case SDL_KEYDOWN: {
        SDL_Keymod mod = SDL_GetModState();
        if (event->key.keysym.sym == SDLK_TAB && !(mod & (KMOD_CTRL | KMOD_ALT))) {
            app->runtime.compact_mode = !app->runtime.compact_mode;
            break;
        }
        if ((mod & KMOD_SHIFT) && !(mod & (KMOD_CTRL | KMOD_ALT)) &&
            (event->key.keysym.sym == SDLK_EQUALS || event->key.keysym.sym == SDLK_MINUS ||
             event->key.keysym.sym == SDLK_KP_PLUS || event->key.keysym.sym == SDLK_KP_MINUS)) {
            int direction = (event->key.keysym.sym == SDLK_EQUALS || event->key.keysym.sym == SDLK_KP_PLUS) ? 1 : -1;
            if (layer_stack_cycle_blend_mode(&app->layers, app->layers.active_layer, direction)) {
                app->runtime.needs_composite = 1;
                update_window_title(
                    app->window,
                    &app->layers,
                    app->runtime.tool,
                    app->runtime.brush_shape,
                    app->runtime.brush_radius,
                    app->runtime.brush_color,
                    app->runtime.brush_opacity
                );
            }
            break;
        }
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
    }
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
            &app->runtime.needs_composite,
            app->runtime.compact_mode
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
