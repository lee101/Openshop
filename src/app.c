#include "app.h"
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

static int should_cancel_shape_on_key(SDL_Keycode key, int ctrl) {
    if (key == SDLK_ESCAPE || key == SDLK_LSHIFT || key == SDLK_RSHIFT) {
        return 0;
    }
    if (ctrl && (key == SDLK_s || key == SDLK_o || key == SDLK_z || key == SDLK_y || key == SDLK_n || key == SDLK_u || key == SDLK_v || key == SDLK_m || key == SDLK_d || key == SDLK_e || key == SDLK_g || key == SDLK_h || key == SDLK_l || key == SDLK_k || key == SDLK_a || key == SDLK_r || key == SDLK_i || key == SDLK_0 || key == SDLK_COMMA || key == SDLK_LEFTBRACKET || key == SDLK_RIGHTBRACKET || key == SDLK_MINUS || key == SDLK_KP_MINUS || key == SDLK_EQUALS || key == SDLK_KP_PLUS || key == SDLK_SLASH || key == SDLK_HOME || key == SDLK_END || key == SDLK_1 || key == SDLK_2 || key == SDLK_3 || key == SDLK_4 || key == SDLK_5 || key == SDLK_6 || key == SDLK_7 || key == SDLK_8)) {
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
    case SDLK_HOME:
    case SDLK_END:
    case SDLK_DELETE:
    case SDLK_BACKSPACE:
        return 1;
    default:
        return 0;
    }
}

static int handle_layer_navigation_shortcut(
    SDL_Keycode key,
    int ctrl,
    int shift,
    int alt,
    LayerStack *layers,
    SDL_Window *window,
    Tool tool,
    BrushShape brush_shape,
    int brush_radius,
    uint32_t brush_color,
    int brush_opacity
) {
    int handled = 1;
    int changed = 0;

    if (ctrl && shift && key >= SDLK_1 && key <= SDLK_8) {
        changed = layer_stack_select_nth_editable_visible(layers, (int)(key - SDLK_1)) >= 0;
    } else if (ctrl && alt && key >= SDLK_1 && key <= SDLK_8) {
        changed = layer_stack_select_nth_visible(layers, (int)(key - SDLK_1)) >= 0;
    } else if (ctrl && key >= SDLK_1 && key <= SDLK_8) {
        int target = (int)(key - SDLK_1);
        if (target < layers->layer_count) {
            layers->active_layer = target;
            changed = 1;
        }
    } else if (key == SDLK_PAGEUP) {
        int cycled = (ctrl && shift) ? layer_stack_cycle_editable_visible(layers, 1)
            : (ctrl ? layer_stack_cycle_unlocked(layers, 1)
            : (shift ? layer_stack_cycle_visible(layers, 1) : layer_stack_cycle(layers, 1)));
        changed = cycled >= 0;
    } else if (key == SDLK_PAGEDOWN) {
        int cycled = (ctrl && shift) ? layer_stack_cycle_editable_visible(layers, -1)
            : (ctrl ? layer_stack_cycle_unlocked(layers, -1)
            : (shift ? layer_stack_cycle_visible(layers, -1) : layer_stack_cycle(layers, -1)));
        changed = cycled >= 0;
    } else if (!ctrl && key == SDLK_HOME) {
        int selected = shift ? layer_stack_select_edge_visible(layers, -1) : layer_stack_select_edge(layers, -1);
        changed = selected >= 0;
    } else if (!ctrl && key == SDLK_END) {
        int selected = shift ? layer_stack_select_edge_visible(layers, 1) : layer_stack_select_edge(layers, 1);
        changed = selected >= 0;
    } else if (ctrl && shift && key == SDLK_HOME) {
        changed = layer_stack_select_edge_editable_visible(layers, -1) >= 0;
    } else if (ctrl && shift && key == SDLK_END) {
        changed = layer_stack_select_edge_editable_visible(layers, 1) >= 0;
    } else {
        handled = 0;
    }

    if (handled && changed) {
        update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
    }
    return handled;
}

static int handle_layer_stack_shortcut(
    SDL_Keycode key,
    int ctrl,
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
    int handled = 1;

    if (ctrl && shift && key == SDLK_n) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (layer_stack_add(layers, NULL, 0x00000000) < 0) {
            fprintf(stderr, "Max layers reached (%d)\n", MAX_LAYERS);
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && key == SDLK_n) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (layer_stack_insert(layers, layers->active_layer + 1, NULL, 0x00000000) < 0) {
            fprintf(stderr, "Could not insert a layer above the active layer\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && key == SDLK_COMMA) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (layer_stack_insert(layers, layers->active_layer, NULL, 0x00000000) < 0) {
            fprintf(stderr, "Could not insert a layer below the active layer\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && shift && key == SDLK_l) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_toggle_lock(layers, layers->active_layer)) {
            fprintf(stderr, "Could not toggle layer lock\n");
        }
    } else if (ctrl && shift && key == SDLK_k) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_toggle_lock_others(layers, layers->active_layer)) {
            fprintf(stderr, "Could not toggle locks on the other layers\n");
        }
    } else if (ctrl && shift && key == SDLK_i) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_toggle_visibility_others(layers, layers->active_layer)) {
            fprintf(stderr, "Could not toggle visibility on the other layers\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && shift && key == SDLK_u) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_unlock_all(layers)) {
            fprintf(stderr, "Could not unlock layers\n");
        }
    } else if (ctrl && shift && key == SDLK_m) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_flatten(layers, COLOR_BG)) {
            fprintf(stderr, "Flatten failed (check for locked layers)\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && shift && key == SDLK_e) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_stamp_visible_into(layers, layers->active_layer, COLOR_BG)) {
            fprintf(stderr, "Stamp visible failed (active layer may be locked)\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && shift && key == SDLK_g) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (layer_stack_stamp_visible_new(layers, "Visible Stamp", COLOR_BG) < 0) {
            fprintf(stderr, "Could not stamp visible image into a new layer\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && shift && key == SDLK_d) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (layer_stack_duplicate_below(layers, layers->active_layer, NULL) < 0) {
            fprintf(stderr, "Could not duplicate layer below\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && key == SDLK_d) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (layer_stack_duplicate(layers, layers->active_layer, NULL) < 0) {
            fprintf(stderr, "Could not duplicate layer\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && key == SDLK_LEFTBRACKET) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_move(layers, layers->active_layer, -1)) {
            fprintf(stderr, "Layer is already at the bottom\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && key == SDLK_RIGHTBRACKET) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_move(layers, layers->active_layer, 1)) {
            fprintf(stderr, "Layer is already at the top\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && key == SDLK_HOME) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_move_to(layers, layers->active_layer, 0)) {
            fprintf(stderr, "Layer is already at the bottom\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && key == SDLK_END) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_move_to(layers, layers->active_layer, layers->layer_count - 1)) {
            fprintf(stderr, "Layer is already at the top\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && (key == SDLK_MINUS || key == SDLK_KP_MINUS)) {
        Layer *active = layer_stack_active(layers);
        if (active) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_set_opacity(layers, layers->active_layer, active->opacity_percent - 10);
            *needs_composite = 1;
        }
    } else if (ctrl && (key == SDLK_EQUALS || key == SDLK_KP_PLUS)) {
        Layer *active = layer_stack_active(layers);
        if (active) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_set_opacity(layers, layers->active_layer, active->opacity_percent + 10);
            *needs_composite = 1;
        }
    } else if (ctrl && shift && key == SDLK_v) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_toggle_visibility(layers, layers->active_layer)) {
            fprintf(stderr, "Cannot hide the final visible layer\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && shift && key == SDLK_h) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_hide_and_advance(layers, layers->active_layer)) {
            fprintf(stderr, "Cannot hide the final visible layer\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && key == SDLK_SLASH) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_toggle_solo(layers, layers->active_layer)) {
            fprintf(stderr, "Could not toggle solo mode\n");
        } else {
            *needs_composite = 1;
        }
    } else if (key == SDLK_DELETE || key == SDLK_BACKSPACE) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_delete(layers, layers->active_layer)) {
            fprintf(stderr, "Cannot delete the final or a locked layer\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && key == SDLK_m) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_merge_down(layers, layers->active_layer)) {
            fprintf(stderr, "No lower layer to merge into, or one of the layers is locked\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && key == SDLK_u) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (!layer_stack_merge_up(layers, layers->active_layer)) {
            fprintf(stderr, "No upper layer to merge into, or one of the layers is locked\n");
        } else {
            *needs_composite = 1;
        }
    } else if (ctrl && key == SDLK_0) {
        Layer *active = layer_stack_active(layers);
        if (active && active->opacity_percent != 100) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_set_opacity(layers, layers->active_layer, 100);
            *needs_composite = 1;
        }
    } else if (ctrl && key == SDLK_a) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (layer_stack_show_all(layers)) {
            *needs_composite = 1;
        }
    } else if (ctrl && shift && key == SDLK_r) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (layer_stack_show(layers, layers->active_layer)) {
            *needs_composite = 1;
        }
    } else {
        handled = 0;
    }

    if (handled) {
        update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
    }
    return handled;
}

static void push_current_snapshot(
    const LayerStack *layers,
    Snapshot *stack,
    int *count
) {
    if (!layers || !stack || !count) {
        return;
    }

    Snapshot current = {0};
    if (!snapshot_from_layers(&current, layers)) {
        snapshot_free(&current);
        return;
    }
    if (*count == MAX_HISTORY) {
        snapshot_free(&stack[0]);
        memmove(&stack[0], &stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
        *count = MAX_HISTORY - 1;
    }
    stack[(*count)++] = current;
}

static int restore_from_history(
    LayerStack *layers,
    Snapshot *from_stack,
    int *from_count,
    Snapshot *to_stack,
    int *to_count
) {
    if (!layers || !from_stack || !from_count || !to_stack || !to_count || *from_count <= 0) {
        return 0;
    }

    push_current_snapshot(layers, to_stack, to_count);
    Snapshot restored = from_stack[--(*from_count)];
    snapshot_apply(&restored, layers);
    snapshot_free(&restored);
    return 1;
}

static int handle_document_shortcut(
    SDL_Keycode key,
    int ctrl,
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
    int brush_opacity,
    const Canvas *preview_canvas,
    int preview_active,
    const Canvas *composite
) {
    int handled = 1;

    if (ctrl && key == SDLK_s) {
        const Canvas *save_canvas = (preview_active && preview_canvas && preview_canvas->pixels) ? preview_canvas : composite;
        if (!canvas_save_bmp(save_canvas, "output.bmp")) {
            fprintf(stderr, "Failed to save output.bmp\n");
        }
    } else if (ctrl && key == SDLK_o) {
        Layer *active = layer_stack_active(layers);
        if (!active || active->locked) {
            fprintf(stderr, "Active layer is locked\n");
        } else {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            if (!canvas_load_bmp(&active->canvas, "input.bmp", active_layer_clear_color(layers))) {
                fprintf(stderr, "Failed to load input.bmp\n");
            } else {
                *needs_composite = 1;
            }
        }
    } else if (ctrl && key == SDLK_z) {
        if (restore_from_history(layers, undo_stack, undo_count, redo_stack, redo_count)) {
            *needs_composite = 1;
            update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
        }
    } else if (ctrl && key == SDLK_y) {
        if (restore_from_history(layers, redo_stack, redo_count, undo_stack, undo_count)) {
            *needs_composite = 1;
            update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
        }
    } else if (ctrl && key == SDLK_0) {
        Layer *active = layer_stack_active(layers);
        if (active && active->opacity_percent != 100) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            layer_stack_set_opacity(layers, layers->active_layer, 100);
            *needs_composite = 1;
        }
        update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
    } else if (ctrl && key == SDLK_a) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (layer_stack_show_all(layers)) {
            *needs_composite = 1;
        }
        update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
    } else if (ctrl && shift && key == SDLK_r) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        if (layer_stack_show(layers, layers->active_layer)) {
            *needs_composite = 1;
        }
        update_window_title(window, layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
    } else {
        handled = 0;
    }

    return handled;
}

static int handle_tool_shortcut(
    SDL_Keycode key,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite,
    Tool *tool,
    BrushShape *brush_shape,
    int *brush_radius,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity,
    const Canvas *preview_canvas,
    int preview_active,
    const Canvas *composite
) {
    int handled = 1;

    if (key == SDLK_b) {
        *brush_color_rgb = COLOR_BRUSH & 0x00FFFFFF;
        *brush_color = compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (key == SDLK_e) {
        *brush_color_rgb = COLOR_ERASE & 0x00FFFFFF;
        *brush_color = compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_ERASER;
    } else if (key == SDLK_l) {
        *tool = TOOL_LINE;
    } else if (key == SDLK_r) {
        *tool = TOOL_RECT;
    } else if (key == SDLK_t) {
        *tool = TOOL_FILLED_RECT;
    } else if (key == SDLK_o) {
        *tool = TOOL_ELLIPSE;
    } else if (key == SDLK_p) {
        *tool = TOOL_FILLED_ELLIPSE;
    } else if (key == SDLK_LEFTBRACKET) {
        if (*brush_radius > 1) {
            *brush_radius -= 1;
        }
    } else if (key == SDLK_RIGHTBRACKET) {
        if (*brush_radius < 64) {
            *brush_radius += 1;
        }
    } else if (key == SDLK_COMMA) {
        *brush_shape = cycle_brush_shape(*brush_shape, -1);
    } else if (key == SDLK_PERIOD) {
        *brush_shape = cycle_brush_shape(*brush_shape, 1);
    } else if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
        if (*brush_opacity > 1) {
            *brush_opacity -= 5;
            if (*brush_opacity < 1) {
                *brush_opacity = 1;
            }
            *brush_color = compose_brush_color(*brush_color_rgb, *brush_opacity);
        }
    } else if (key == SDLK_EQUALS || key == SDLK_KP_PLUS) {
        if (*brush_opacity < 100) {
            *brush_opacity += 5;
            if (*brush_opacity > 100) {
                *brush_opacity = 100;
            }
            *brush_color = compose_brush_color(*brush_color_rgb, *brush_opacity);
        }
    } else if (key >= SDLK_1 && key <= SDLK_6) {
        switch (key) {
        case SDLK_1:
            *brush_color_rgb = COLOR_BRUSH & 0x00FFFFFF;
            break;
        case SDLK_2:
            *brush_color_rgb = COLOR_RED & 0x00FFFFFF;
            break;
        case SDLK_3:
            *brush_color_rgb = COLOR_GREEN & 0x00FFFFFF;
            break;
        case SDLK_4:
            *brush_color_rgb = COLOR_BLUE & 0x00FFFFFF;
            break;
        case SDLK_5:
            *brush_color_rgb = COLOR_YELLOW & 0x00FFFFFF;
            break;
        case SDLK_6:
            *brush_color_rgb = COLOR_PURPLE & 0x00FFFFFF;
            break;
        default:
            break;
        }
        *brush_color = compose_brush_color(*brush_color_rgb, *brush_opacity);
        *tool = TOOL_BRUSH;
    } else if (key == SDLK_c) {
        if (active_layer_editable(layers)) {
            push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        }
        if (layer_stack_clear_layer(layers, layers->active_layer, active_layer_clear_color(layers))) {
            *needs_composite = 1;
        }
    } else if (key == SDLK_h) {
        if (apply_canvas_transform(layers, undo_stack, undo_count, redo_stack, redo_count, canvas_flip_horizontal)) {
            *needs_composite = 1;
        }
    } else if (key == SDLK_v) {
        if (apply_canvas_transform(layers, undo_stack, undo_count, redo_stack, redo_count, canvas_flip_vertical)) {
            *needs_composite = 1;
        }
    } else if (key == SDLK_j) {
        if (apply_canvas_transform(layers, undo_stack, undo_count, redo_stack, redo_count, canvas_rotate_180)) {
            *needs_composite = 1;
        }
    } else if (key == SDLK_x) {
        if (apply_canvas_transform(layers, undo_stack, undo_count, redo_stack, redo_count, canvas_invert_rgb)) {
            *needs_composite = 1;
        }
    } else if (key == SDLK_f) {
        int mx = 0;
        int my = 0;
        SDL_GetMouseState(&mx, &my);
        if (mx >= 0 && my >= 0 && mx < CANVAS_WIDTH && my < CANVAS_HEIGHT) {
            Layer *active = layer_stack_active(layers);
            if (active && !active->locked) {
                push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
            }
            if (!active || active->locked || !canvas_flood_fill(&active->canvas, mx, my, *brush_color)) {
                fprintf(stderr, "Fill failed\n");
            } else {
                *needs_composite = 1;
            }
        }
    } else if (key == SDLK_i) {
        int mx = 0;
        int my = 0;
        SDL_GetMouseState(&mx, &my);
        if (mx >= 0 && my >= 0 && mx < CANVAS_WIDTH && my < CANVAS_HEIGHT) {
            const Canvas *sample = (preview_active && preview_canvas && preview_canvas->pixels) ? preview_canvas : composite;
            *brush_color = canvas_get_pixel(sample, mx, my);
            *brush_color_rgb = *brush_color & 0x00FFFFFF;
            *brush_opacity = (int)((((*brush_color >> 24) & 0xFF) * 100 + 127) / 255);
            if (*brush_opacity < 1) {
                *brush_opacity = 1;
            }
            *brush_color = compose_brush_color(*brush_color_rgb, *brush_opacity);
            *tool = TOOL_BRUSH;
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
    if (shaping && preview_active && *shaping && should_cancel_shape_on_key(key, ctrl)) {
        cancel_shape_preview(shaping, preview_active);
    }

    if (key != SDLK_ESCAPE) {
        return 0;
    }
    if (shaping && preview_active && *shaping) {
        cancel_shape_preview(shaping, preview_active);
    } else if (running) {
        *running = 0;
    }
    return 1;
}

static int handle_translation_shortcut(
    SDL_Keycode key,
    int shift,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    int dx = 0;
    int dy = 0;
    int step = shift ? 10 : 1;

    if (key == SDLK_UP) {
        dy = -step;
    } else if (key == SDLK_DOWN) {
        dy = step;
    } else if (key == SDLK_LEFT) {
        dx = -step;
    } else if (key == SDLK_RIGHT) {
        dx = step;
    } else {
        return 0;
    }

    if (apply_canvas_translation(layers, undo_stack, undo_count, redo_stack, redo_count, dx, dy)) {
        *needs_composite = 1;
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
    int *drawing,
    int *last_x,
    int *last_y,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y,
    uint32_t *shape_base_pixels,
    const Canvas *composite,
    int *preview_active,
    Tool *tool,
    BrushShape brush_shape,
    int brush_radius,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity,
    const Canvas *preview_canvas,
    int *needs_composite,
    SDL_Window *window
) {
    if (!button || !drawing || !last_x || !last_y || !layers || !tool || !brush_color || !brush_color_rgb ||
        !brush_opacity || !needs_composite || !window) {
        return;
    }

    if (button->button == SDL_BUTTON_LEFT) {
        *last_x = button->x;
        *last_y = button->y;
        if (*tool == TOOL_BRUSH || *tool == TOOL_ERASER) {
            Layer *active = layer_stack_active(layers);
            if (active && !active->locked && active->canvas.pixels) {
                push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
                *drawing = 1;
                if (*tool == TOOL_ERASER) {
                    erase_stamp(&active->canvas, *last_x, *last_y, brush_radius, active_layer_clear_color(layers), brush_shape);
                } else {
                    stamp_brush(&active->canvas, *last_x, *last_y, brush_radius, *brush_color, brush_shape);
                }
                *needs_composite = 1;
            }
        } else if (active_layer_editable(layers)) {
            *shaping = 1;
            *shape_start_x = *last_x;
            *shape_start_y = *last_y;
            if (shape_base_pixels && composite && composite->pixels) {
                memcpy(
                    shape_base_pixels,
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
    if (shaping && preview_active && *shaping) {
        cancel_shape_preview(shaping, preview_active);
        return;
    }
    if (button->x < 0 || button->y < 0 || button->x >= CANVAS_WIDTH || button->y >= CANVAS_HEIGHT) {
        return;
    }
    const Canvas *sample = (preview_active && *preview_active && preview_canvas && preview_canvas->pixels) ? preview_canvas : composite;
    sample_canvas_color(sample, button->x, button->y, tool, brush_color, brush_color_rgb, brush_opacity);
    update_window_title(window, layers, *tool, brush_shape, brush_radius, *brush_color, *brush_opacity);
}

static void handle_mouse_up(
    const SDL_MouseButtonEvent *button,
    int *drawing,
    int *shaping,
    int *preview_active,
    Tool tool,
    int shape_start_x,
    int shape_start_y,
    int brush_radius,
    uint32_t brush_color,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite
) {
    if (!button || !drawing || !shaping || !preview_active || !layers || !needs_composite) {
        return;
    }
    if (button->button != SDL_BUTTON_LEFT) {
        return;
    }

    *drawing = 0;
    if (!*shaping) {
        return;
    }

    const Uint8 *state = SDL_GetKeyboardState(NULL);
    int shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    int end_x = button->x;
    int end_y = button->y;
    constrain_end(tool, shape_start_x, shape_start_y, end_x, end_y, shift, &end_x, &end_y);
    Layer *active = layer_stack_active(layers);
    if (active && !active->locked && active->canvas.pixels) {
        push_snapshot(layers, undo_stack, undo_count, redo_stack, redo_count);
        draw_shape(&active->canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
        *needs_composite = 1;
    }
    cancel_shape_preview(shaping, preview_active);
}

static void handle_mouse_motion(
    const SDL_MouseMotionEvent *motion,
    int drawing,
    int *last_x,
    int *last_y,
    LayerStack *layers,
    Tool tool,
    BrushShape brush_shape,
    int brush_radius,
    uint32_t brush_color,
    int *needs_composite,
    int shaping,
    uint32_t *shape_base_pixels,
    Canvas *preview_canvas,
    int shape_start_x,
    int shape_start_y,
    int *preview_active
) {
    if (!motion || !last_x || !last_y || !layers || !needs_composite || !preview_active) {
        return;
    }

    if (drawing) {
        int x = motion->x;
        int y = motion->y;
        if (x >= 0 && y >= 0 && x < CANVAS_WIDTH && y < CANVAS_HEIGHT) {
            Layer *active = layer_stack_active(layers);
            if (active && !active->locked && active->canvas.pixels) {
                if (tool == TOOL_ERASER) {
                    erase_line(&active->canvas, *last_x, *last_y, x, y, brush_radius, active_layer_clear_color(layers), brush_shape);
                } else {
                    draw_brush_line(&active->canvas, *last_x, *last_y, x, y, brush_radius, brush_color, brush_shape);
                }
                *last_x = x;
                *last_y = y;
                *needs_composite = 1;
            }
        }
        return;
    }

    if (!shaping || !shape_base_pixels || !preview_canvas || !preview_canvas->pixels) {
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
    constrain_end(tool, shape_start_x, shape_start_y, end_x, end_y, shift, &end_x, &end_y);
    memcpy(
        preview_canvas->pixels,
        shape_base_pixels,
        (size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t)
    );
    draw_shape(preview_canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
    *preview_active = 1;
}

static void handle_keydown_shortcut(
    SDL_Keycode key,
    int ctrl,
    int shift,
    int alt,
    int *shaping,
    int *preview_active,
    int *running,
    LayerStack *layers,
    Snapshot *undo_stack,
    int *undo_count,
    Snapshot *redo_stack,
    int *redo_count,
    int *needs_composite,
    SDL_Window *window,
    Tool *tool,
    BrushShape *brush_shape,
    int *brush_radius,
    uint32_t *brush_color,
    uint32_t *brush_color_rgb,
    int *brush_opacity,
    const Canvas *preview_canvas,
    const Canvas *composite
) {
    if (handle_session_shortcut(key, ctrl, shaping, preview_active, running)) {
        return;
    }

    if (handle_layer_stack_shortcut(
            key,
            ctrl,
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
        return;
    }

    if (handle_document_shortcut(
            key,
            ctrl,
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
            *brush_opacity,
            preview_canvas,
            *preview_active,
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
            *tool,
            *brush_shape,
            *brush_radius,
            *brush_color,
            *brush_opacity)) {
        return;
    }

    if (handle_translation_shortcut(
            key,
            shift,
            layers,
            undo_stack,
            undo_count,
            redo_stack,
            redo_count,
            needs_composite)) {
        return;
    }

    handle_tool_shortcut(
        key,
        layers,
        undo_stack,
        undo_count,
        redo_stack,
        redo_count,
        needs_composite,
        tool,
        brush_shape,
        brush_radius,
        brush_color,
        brush_color_rgb,
        brush_opacity,
        preview_canvas,
        *preview_active,
        composite
    );

    update_window_title(window, layers, *tool, *brush_shape, *brush_radius, *brush_color, *brush_opacity);
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
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    LayerStack layers;
    if (!layer_stack_init(&layers, CANVAS_WIDTH, CANVAS_HEIGHT, COLOR_BG)) {
        fprintf(stderr, "Layer stack init failed\n");
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Canvas composite = {0};
    if (!canvas_init(&composite, CANVAS_WIDTH, CANVAS_HEIGHT)) {
        fprintf(stderr, "Composite canvas init failed\n");
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
            fprintf(stderr, "Failed to load %s\n", input_path);
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
                handle_mouse_down(
                    &e.button,
                    &drawing,
                    &last_x,
                    &last_y,
                    &layers,
                    undo_stack,
                    &undo_count,
                    redo_stack,
                    &redo_count,
                    &shaping,
                    &shape_start_x,
                    &shape_start_y,
                    shape_base_pixels,
                    &composite,
                    &preview_active,
                    &tool,
                    brush_shape,
                    brush_radius,
                    &brush_color,
                    &brush_color_rgb,
                    &brush_opacity,
                    &preview_canvas,
                    &needs_composite,
                    window
                );
                break;
            case SDL_MOUSEBUTTONUP:
                handle_mouse_up(
                    &e.button,
                    &drawing,
                    &shaping,
                    &preview_active,
                    tool,
                    shape_start_x,
                    shape_start_y,
                    brush_radius,
                    brush_color,
                    &layers,
                    undo_stack,
                    &undo_count,
                    redo_stack,
                    &redo_count,
                    &needs_composite
                );
                break;
            case SDL_MOUSEMOTION:
                handle_mouse_motion(
                    &e.motion,
                    drawing,
                    &last_x,
                    &last_y,
                    &layers,
                    tool,
                    brush_shape,
                    brush_radius,
                    brush_color,
                    &needs_composite,
                    shaping,
                    shape_base_pixels,
                    &preview_canvas,
                    shape_start_x,
                    shape_start_y,
                    &preview_active
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
                    &shaping,
                    &preview_active,
                    &running,
                    &layers,
                    undo_stack,
                    &undo_count,
                    redo_stack,
                    &redo_count,
                    &needs_composite,
                    window,
                    &tool,
                    &brush_shape,
                    &brush_radius,
                    &brush_color,
                    &brush_color_rgb,
                    &brush_opacity,
                    &preview_canvas,
                    &composite
                );
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
