#include "app.h"
#include "app_brush.h"
#include "app_brush_mask.h"
#include "app_canvas_click.h"
#include "app_canvas_ops.h"
#include "app_color.h"
#include "app_layer_state.h"
#include "app_layout.h"
#include "selection.h"
#include "ui_shell.h"
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

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 800
#define MAX_HISTORY 20

static int g_doc_width = APP_LAYOUT_CANVAS_WIDTH;
static int g_doc_height = APP_LAYOUT_CANVAS_HEIGHT;
static UiShellFrame g_frame;

#define CANVAS_WIDTH g_doc_width
#define CANVAS_HEIGHT g_doc_height

static const uint32_t COLOR_BG = 0xFFFFFFFF;     // white
static const uint32_t COLOR_BRUSH = 0xFF1B1F24;  // near-black

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
    int select_mode;
    int selecting;
    int select_start_x;
    int select_start_y;
    int select_cur_x;
    int select_cur_y;
    int masked_stroke;
    uint32_t *selection_backup;
    uint32_t *shape_base_pixels;
    uint32_t *preview_pixels;
    Canvas preview_canvas;
} AppRuntime;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Texture *ui_texture;
    Canvas ui_canvas;
    LayerStack layers;
    Canvas composite;
    Selection selection;
    AppRuntime runtime;
    int window_width;
    int window_height;
    double zoom;
    int pan_x;
    int pan_y;
    int panning;
    int pan_last_x;
    int pan_last_y;
    uint64_t ui_hash;
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
    int inside = 0;
    ui_shell_screen_to_doc(&g_frame, screen_x, screen_y, canvas_x, canvas_y, &inside);
    return inside;
}

static void screen_to_canvas_point_clamped(int screen_x, int screen_y, int *canvas_x, int *canvas_y) {
    ui_shell_screen_to_doc_clamped(&g_frame, screen_x, screen_y, g_doc_width, g_doc_height, canvas_x, canvas_y);
}

static void selection_edit_capture(App *app) {
    Layer *active = NULL;

    if (!app || !app->selection.active || !app->runtime.selection_backup) {
        return;
    }
    active = layer_stack_active(&app->layers);
    if (!active || !active->canvas.pixels) {
        return;
    }
    memcpy(app->runtime.selection_backup, active->canvas.pixels, (size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t));
    app->runtime.masked_stroke = 1;
}

static void selection_edit_clamp(App *app) {
    Layer *active = NULL;

    if (!app || !app->runtime.masked_stroke || !app->runtime.selection_backup) {
        return;
    }
    active = layer_stack_active(&app->layers);
    if (!active || !active->canvas.pixels) {
        return;
    }
    selection_clamp_edit(&app->selection, &active->canvas, app->runtime.selection_backup);
    app->runtime.needs_composite = 1;
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

static void draw_ant_point(SDL_Renderer *renderer, int x, int y, int phase) {
    int sx = (int)(g_frame.viewport.x + x * g_frame.scale);
    int sy = (int)(g_frame.viewport.y + y * g_frame.scale);
    int size = g_frame.scale > 1.5 ? (int)(g_frame.scale + 0.5) : 1;

    if ((((x + y) >> 2) + phase) & 1) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    }
    if (size <= 1) {
        SDL_RenderDrawPoint(renderer, sx, sy);
    } else {
        SDL_Rect dot = {sx, sy, size, size};
        SDL_RenderFillRect(renderer, &dot);
    }
}

static void draw_selection_ants(SDL_Renderer *renderer, const Selection *sel, int phase) {
    if (!renderer || !sel || !sel->active || !sel->mask) {
        return;
    }
    for (int y = 0; y < sel->height; y++) {
        const uint8_t *row = sel->mask + (size_t)y * (size_t)sel->width;
        for (int x = 0; x < sel->width; x++) {
            int edge;
            if (row[x] <= 127) {
                continue;
            }
            edge = x == 0 || y == 0 || x == sel->width - 1 || y == sel->height - 1;
            if (!edge) {
                edge = row[x - 1] <= 127 || row[x + 1] <= 127 ||
                       sel->mask[(size_t)(y - 1) * (size_t)sel->width + (size_t)x] <= 127 ||
                       sel->mask[(size_t)(y + 1) * (size_t)sel->width + (size_t)x] <= 127;
            }
            if (edge) {
                draw_ant_point(renderer, x, y, phase);
            }
        }
    }
}

static void draw_drag_ants(SDL_Renderer *renderer, int x0, int y0, int x1, int y1, int phase) {
    int left = x0 < x1 ? x0 : x1;
    int right = x0 < x1 ? x1 : x0;
    int top = y0 < y1 ? y0 : y1;
    int bottom = y0 < y1 ? y1 : y0;

    for (int x = left; x <= right; x++) {
        draw_ant_point(renderer, x, top, phase);
        draw_ant_point(renderer, x, bottom, phase);
    }
    for (int y = top; y <= bottom; y++) {
        draw_ant_point(renderer, left, y, phase);
        draw_ant_point(renderer, right, y, phase);
    }
}

static void present_canvas_texture(SDL_Renderer *renderer, SDL_Texture *texture, const Selection *selection, const AppRuntime *runtime) {
    if (!renderer || !texture) {
        return;
    }

    {
        SDL_Rect dest = {
            (int)(g_frame.viewport.x + 0.5f),
            (int)(g_frame.viewport.y + 0.5f),
            (int)(g_frame.viewport.width + 0.5f),
            (int)(g_frame.viewport.height + 0.5f),
        };
        SDL_RenderCopy(renderer, texture, NULL, &dest);
    }
    {
        int phase = (int)((SDL_GetTicks() / 160) & 1);
        draw_selection_ants(renderer, selection, phase);
        if (runtime && runtime->selecting) {
            draw_drag_ants(renderer, runtime->select_start_x, runtime->select_start_y, runtime->select_cur_x, runtime->select_cur_y, phase);
        }
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

static const char *shell_tool_labels[8] = {"M", "W", "B", "E", "L", "R", "O", "F"};

static int shell_active_tool(const AppRuntime *runtime) {
    if (runtime->select_mode == 1) return 0;
    if (runtime->select_mode == 2) return 1;
    switch (runtime->tool) {
    case TOOL_BRUSH: return 2;
    case TOOL_ERASER: return 3;
    case TOOL_LINE: return 4;
    case TOOL_RECT:
    case TOOL_FILLED_RECT: return 5;
    case TOOL_ELLIPSE:
    case TOOL_FILLED_ELLIPSE: return 6;
    default: return 7;
    }
}

static void fill_shell_state(const App *app, UiShellState *state, char *blend_buffer, size_t blend_size, char *status_buffer, size_t status_size) {
    const Layer *active = layer_stack_get(&app->layers, app->layers.active_layer);

    memset(state, 0, sizeof(*state));
    state->doc_width = app->layers.width;
    state->doc_height = app->layers.height;
    state->compact = app->runtime.compact_mode;
    state->zoom = app->zoom;
    state->pan_x = app->pan_x;
    state->pan_y = app->pan_y;
    state->tool_count = 8;
    state->active_tool = shell_active_tool(&app->runtime);
    for (int i = 0; i < 8; i++) {
        state->tool_labels[i] = shell_tool_labels[i];
    }
    state->layer_count = app->layers.layer_count;
    state->active_layer = app->layers.active_layer;
    for (int i = 0; i < app->layers.layer_count && i < UI_SHELL_MAX_LAYERS; i++) {
        snprintf(state->layer_names[i], UI_SHELL_NAME_MAX, "%s", app->layers.layers[i].name);
        state->layer_visible[i] = app->layers.layers[i].visible;
        state->layer_opacity[i] = app->layers.layers[i].opacity_percent;
    }
    snprintf(blend_buffer, blend_size, "%s  %s  size %d  %d%%",
             app_tool_label(app->runtime.tool),
             blend_mode_name(active ? (BlendMode)active->blend_mode : BLEND_NORMAL),
             app->runtime.brush_radius,
             app->runtime.brush_opacity);
    state->blend_label = blend_buffer;
    snprintf(status_buffer, status_size, "zoom %d%%", (int)(g_frame.scale * 100.0 + 0.5));
    state->status_text = status_buffer;
    state->foreground_color = 0xFF000000u | app->runtime.brush_color_rgb;
    state->background_color = COLOR_BG;
}

static uint64_t shell_state_fingerprint(const App *app, const UiShellState *state) {
    uint64_t hash = 1469598103934665603ull;
    const uint8_t *bytes = (const uint8_t *)state;

    for (size_t i = 0; i < sizeof(*state); i++) {
        hash = (hash ^ bytes[i]) * 1099511628211ull;
    }
    for (const char *p = state->blend_label; p && *p; p++) {
        hash = (hash ^ (uint8_t)*p) * 1099511628211ull;
    }
    hash = (hash ^ (uint64_t)app->window_width) * 1099511628211ull;
    hash = (hash ^ (uint64_t)app->window_height) * 1099511628211ull;
    return hash;
}

static void refresh_shell_ui(App *app, int force) {
    UiShellState state;
    char blend_buffer[96];
    char status_buffer[48];
    uint64_t hash;

    fill_shell_state(app, &state, blend_buffer, sizeof(blend_buffer), status_buffer, sizeof(status_buffer));
    hash = shell_state_fingerprint(app, &state);
    if (!force && hash == app->ui_hash) {
        return;
    }
    app->ui_hash = hash;

    ui_shell_build(app->window_width, app->window_height, &state, &g_frame);
    canvas_clear(&app->ui_canvas, 0xFF1F1F23);
    ui_draw_list_rasterize(&g_frame.draw_list, &app->ui_canvas);
    if (app->ui_texture) {
        SDL_UpdateTexture(app->ui_texture, NULL, app->ui_canvas.pixels, app->window_width * 4);
    }
}

static int recreate_window_surfaces(App *app, int width, int height) {
    if (width < 320) width = 320;
    if (height < 240) height = 240;
    app->window_width = width;
    app->window_height = height;

    if (app->ui_texture) {
        SDL_DestroyTexture(app->ui_texture);
        app->ui_texture = NULL;
    }
    canvas_free(&app->ui_canvas);
    if (!canvas_init(&app->ui_canvas, width, height)) {
        return 0;
    }
    app->ui_texture = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!app->ui_texture) {
        return 0;
    }
    refresh_shell_ui(app, 1);
    return 1;
}

static void render_app_frame(App *app) {
    if (!app || !app->renderer || !app->texture) {
        return;
    }

    if (!app->runtime.preview_active && app->runtime.needs_composite) {
        layer_stack_composite(&app->layers, &app->composite, COLOR_BG);
        app->runtime.needs_composite = 0;
    }

    refresh_shell_ui(app, 0);
    update_canvas_texture(app->texture, &app->composite, &app->runtime.preview_canvas, app->runtime.preview_active);

    SDL_SetRenderDrawColor(app->renderer, 0x1F, 0x1F, 0x23, 0xFF);
    SDL_RenderClear(app->renderer);
    if (app->ui_texture) {
        SDL_RenderCopy(app->renderer, app->ui_texture, NULL, NULL);
    }
    present_canvas_texture(app->renderer, app->texture, &app->selection, &app->runtime);
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
    free(app->runtime.selection_backup);
    selection_free(&app->selection);
    snapshot_stack_clear(app->runtime.undo_stack, &app->runtime.undo_count);
    snapshot_stack_clear(app->runtime.redo_stack, &app->runtime.redo_count);
    if (app->composite.pixels) {
        canvas_free(&app->composite);
    }
    if (app->layers.layers) {
        layer_stack_free(&app->layers);
    }
    if (app->ui_texture) {
        SDL_DestroyTexture(app->ui_texture);
    }
    canvas_free(&app->ui_canvas);
    ui_shell_shutdown();
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
        DEFAULT_WINDOW_WIDTH,
        DEFAULT_WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
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
    runtime->selection_backup = (uint32_t *)malloc((size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t));
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
    runtime->select_mode = 0;
    runtime->selecting = 0;
    runtime->select_start_x = 0;
    runtime->select_start_y = 0;
    runtime->select_cur_x = 0;
    runtime->select_cur_y = 0;
    runtime->masked_stroke = 0;
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

    if (!selection_init(&app->selection, CANVAS_WIDTH, CANVAS_HEIGHT)) {
        shutdown_app(app);
        return 0;
    }

    app->zoom = 0.0;
    app->pan_x = 0;
    app->pan_y = 0;
    app->panning = 0;
    app->ui_hash = 0;
    if (!ui_shell_init() || !recreate_window_surfaces(app, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT)) {
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
    case SDL_WINDOWEVENT:
        if (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event->window.event == SDL_WINDOWEVENT_RESIZED) {
            recreate_window_surfaces(app, event->window.data1, event->window.data2);
        }
        break;
    case SDL_MOUSEWHEEL: {
        int mx = 0;
        int my = 0;
        double before;
        double next;
        double doc_x;
        double doc_y;

        SDL_GetMouseState(&mx, &my);
        before = g_frame.scale;
        next = event->wheel.y > 0 ? before * 1.1 : before / 1.1;
        if (next < 0.05) next = 0.05;
        if (next > 16.0) next = 16.0;
        doc_x = ((double)mx - g_frame.viewport.x) / before;
        doc_y = ((double)my - g_frame.viewport.y) / before;
        app->zoom = next;
        {
            double view_w = app->layers.width * next;
            double view_h = app->layers.height * next;
            double base_x = g_frame.well.x + (g_frame.well.width - view_w) / 2.0;
            double base_y = g_frame.well.y + (g_frame.well.height - view_h) / 2.0;
            app->pan_x = (int)((double)mx - doc_x * next - base_x);
            app->pan_y = (int)((double)my - doc_y * next - base_y);
        }
        refresh_shell_ui(app, 1);
        break;
    }
    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button == SDL_BUTTON_MIDDLE) {
            app->panning = 1;
            app->pan_last_x = event->button.x;
            app->pan_last_y = event->button.y;
            break;
        }
        if (event->button.button == SDL_BUTTON_LEFT && app->runtime.select_mode) {
            int cx = 0;
            int cy = 0;
            if (screen_to_canvas_point(event->button.x, event->button.y, &cx, &cy)) {
                SDL_Keymod mod = SDL_GetModState();
                SelectionOp op = (mod & KMOD_SHIFT) ? SELECTION_ADD : (mod & KMOD_ALT) ? SELECTION_SUBTRACT : SELECTION_REPLACE;
                if (app->runtime.select_mode == 2) {
                    layer_stack_composite(&app->layers, &app->composite, COLOR_BG);
                    selection_magic_wand(&app->selection, &app->composite, cx, cy, 32, op);
                } else {
                    app->runtime.selecting = 1;
                    app->runtime.select_start_x = cx;
                    app->runtime.select_start_y = cy;
                    app->runtime.select_cur_x = cx;
                    app->runtime.select_cur_y = cy;
                }
            }
            break;
        }
        if (event->button.button == SDL_BUTTON_LEFT && app->selection.active) {
            selection_edit_capture(app);
        }
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
        if (app->runtime.masked_stroke && app->runtime.drawing) {
            selection_edit_clamp(app);
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (event->button.button == SDL_BUTTON_MIDDLE) {
            app->panning = 0;
            break;
        }
        if (app->runtime.selecting && event->button.button == SDL_BUTTON_LEFT) {
            int cx = 0;
            int cy = 0;
            SDL_Keymod mod = SDL_GetModState();
            SelectionOp op = (mod & KMOD_SHIFT) ? SELECTION_ADD : (mod & KMOD_ALT) ? SELECTION_SUBTRACT : SELECTION_REPLACE;
            screen_to_canvas_point_clamped(event->button.x, event->button.y, &cx, &cy);
            app->runtime.selecting = 0;
            if (abs(cx - app->runtime.select_start_x) < 2 && abs(cy - app->runtime.select_start_y) < 2) {
                selection_deselect(&app->selection);
            } else {
                selection_select_rect(&app->selection, app->runtime.select_start_x, app->runtime.select_start_y, cx, cy, op);
            }
            break;
        }
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
        if (app->runtime.masked_stroke) {
            selection_edit_clamp(app);
            app->runtime.masked_stroke = 0;
        }
        break;
    case SDL_MOUSEMOTION:
        if (app->panning) {
            app->pan_x += event->motion.x - app->pan_last_x;
            app->pan_y += event->motion.y - app->pan_last_y;
            app->pan_last_x = event->motion.x;
            app->pan_last_y = event->motion.y;
            if (app->zoom <= 0.0) {
                app->zoom = g_frame.scale;
            }
            refresh_shell_ui(app, 1);
            break;
        }
        if (app->runtime.selecting) {
            screen_to_canvas_point_clamped(event->motion.x, event->motion.y, &app->runtime.select_cur_x, &app->runtime.select_cur_y);
            break;
        }
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
        if (app->runtime.masked_stroke && app->runtime.drawing) {
            selection_edit_clamp(app);
        }
        break;
    case SDL_KEYDOWN: {
        SDL_Keymod mod = SDL_GetModState();
        if (event->key.keysym.sym == SDLK_m && !(mod & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT))) {
            app->runtime.select_mode = app->runtime.select_mode == 1 ? 0 : 1;
            app->runtime.selecting = 0;
            break;
        }
        if (event->key.keysym.sym == SDLK_w && !(mod & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT))) {
            app->runtime.select_mode = app->runtime.select_mode == 2 ? 0 : 2;
            app->runtime.selecting = 0;
            break;
        }
        if (event->key.keysym.sym == SDLK_ESCAPE && app->selection.active && !app->runtime.shaping) {
            selection_deselect(&app->selection);
            app->runtime.selecting = 0;
            break;
        }
        if (event->key.keysym.sym == SDLK_0 && !(mod & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT))) {
            app->zoom = 0.0;
            app->pan_x = 0;
            app->pan_y = 0;
            refresh_shell_ui(app, 1);
            break;
        }
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
        {
            int masked_key = app->selection.active && !(mod & KMOD_CTRL) &&
                             (event->key.keysym.sym == SDLK_f || event->key.keysym.sym == SDLK_c);
            if (masked_key) {
                selection_edit_capture(app);
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
            if (masked_key && app->runtime.masked_stroke) {
                selection_edit_clamp(app);
                app->runtime.masked_stroke = 0;
            }
        }
        break;
    }
    default:
        break;
    }
}

static void run_app_loop(App *app) {
    long frame_limit = 0;
    long frames = 0;
    const char *limit_env = getenv("OPENSHOP_FRAMES");

    if (!app) {
        return;
    }
    if (limit_env && limit_env[0]) {
        frame_limit = strtol(limit_env, NULL, 10);
    }

    while (app->runtime.running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            handle_app_event(app, &e);
        }

        render_app_frame(app);
        SDL_Delay(16);
        frames++;
        if (frame_limit > 0 && frames >= frame_limit) {
            app->runtime.running = 0;
        }
    }
}

int app_run(const char *input_path, int doc_width, int doc_height) {
    App app = {0};

    if (doc_width > 0 && doc_height > 0) {
        g_doc_width = doc_width;
        g_doc_height = doc_height;
    }
    if (!initialize_app(&app, input_path)) {
        return 1;
    }

    initialize_app_runtime(&app.runtime);
    update_window_title(app.window, &app.layers, app.runtime.tool, app.runtime.brush_shape, app.runtime.brush_radius, app.runtime.brush_color, app.runtime.brush_opacity);
    run_app_loop(&app);
    shutdown_app(&app);
    return 0;
}
