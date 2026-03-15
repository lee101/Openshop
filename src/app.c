#include "app.h"
#include "canvas.h"
#include "layers.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define CANVAS_WIDTH 800
#define CANVAS_HEIGHT 600
#define MAX_HISTORY 12

static const uint32_t COLOR_BG = 0xFFFFFFFF;    // white
static const uint32_t COLOR_BRUSH = 0xFF1B1F24; // near-black
static const uint32_t COLOR_RED = 0xFFE53935;
static const uint32_t COLOR_GREEN = 0xFF43A047;
static const uint32_t COLOR_BLUE = 0xFF1E88E5;
static const uint32_t COLOR_YELLOW = 0xFFFDD835;
static const uint32_t COLOR_PURPLE = 0xFF8E24AA;

typedef enum {
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_LINE,
    TOOL_RECT,
    TOOL_ELLIPSE
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
    uint8_t visibility[MAX_LAYERS];
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

    const size_t per_layer = (size_t)stack->width * (size_t)stack->height;
    const size_t total_pixels = per_layer * (size_t)stack->layer_count;
    if (total_pixels > 0) {
        s->pixels = (uint32_t *)malloc(total_pixels * sizeof(uint32_t));
        if (!s->pixels) {
            return 0;
        }
    }

    for (int i = 0; i < MAX_LAYERS; i++) {
        s->visibility[i] = 0;
        s->names[i][0] = '\0';
    }

    for (int layer_index = 0; layer_index < stack->layer_count; layer_index++) {
        const Layer *layer = &stack->layers[layer_index];
        s->visibility[layer_index] = (uint8_t)layer->visible;
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

    const size_t per_layer = (size_t)stack->width * (size_t)stack->height;
    for (int i = 0; i < stack->layer_count; i++) {
        Layer *layer = &stack->layers[i];
        if (!layer->canvas.pixels && !layer_stack_clear_layer(stack, i, 0x00000000)) {
            return 0;
        }
        memcpy(layer->canvas.pixels, s->pixels + per_layer * (size_t)i, per_layer * sizeof(uint32_t));
        layer->visible = s->visibility[i] ? 1 : 0;
        strncpy(layer->name, s->names[i], LAYER_NAME_MAX - 1);
        layer->name[LAYER_NAME_MAX - 1] = '\0';
    }

    stack->active_layer = s->active_layer;
    if (stack->active_layer < 0) {
        stack->active_layer = 0;
    }
    if (stack->active_layer >= stack->layer_count) {
        stack->active_layer = stack->layer_count - 1;
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
    case TOOL_ELLIPSE:
        return "Ellipse";
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

static void update_window_title(SDL_Window *window, const LayerStack *layers, Tool tool, BrushShape shape, int radius, uint32_t color, int opacity_percent) {
    if (!window || !layers) {
        return;
    }
    const Layer *active = layer_stack_get(layers, layers->active_layer);
    const char *layer_name = active && active->name[0] ? active->name : "Layer";
    char title[256];
    snprintf(
        title,
        sizeof(title),
        "Openshop - %s (%s) | size %d | opacity %d%% | layer %d/%d %s [%s] | #%08X",
        tool_label(tool),
        brush_shape_label(shape),
        radius,
        opacity_percent,
        layers->active_layer + 1,
        layers->layer_count,
        layer_name,
        (active && active->visible) ? "shown" : "hidden",
        color
    );
    SDL_SetWindowTitle(window, title);
}

static void draw_shape(Canvas *c, Tool tool, int x0, int y0, int x1, int y1, int radius, uint32_t color) {
    switch (tool) {
    case TOOL_LINE:
        canvas_draw_line(c, x0, y0, x1, y1, radius, color);
        break;
    case TOOL_RECT:
        canvas_draw_rect_outline(c, x0, y0, x1, y1, radius, color);
        break;
    case TOOL_ELLIPSE: {
        int cx = (x0 + x1) / 2;
        int cy = (y0 + y1) / 2;
        int rx = abs(x1 - x0) / 2;
        int ry = abs(y1 - y0) / 2;
        canvas_draw_ellipse_outline(c, cx, cy, rx, ry, radius, color);
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
    } else if (tool == TOOL_RECT || tool == TOOL_ELLIPSE) {
        int len = adx > ady ? adx : ady;
        *out_x = x0 + (dx >= 0 ? len : -len);
        *out_y = y0 + (dy >= 0 ? len : -len);
    }
}

static int load_bmp_into_layer(Layer *layer, const char *path) {
    if (!layer || !path || !layer->canvas.pixels) {
        return 0;
    }
    SDL_Surface *bmp = SDL_LoadBMP(path);
    if (!bmp) {
        return 0;
    }
    SDL_Surface *converted = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(bmp);
    if (!converted) {
        return 0;
    }
    int w = converted->w;
    int h = converted->h;
    int copy_w = w < layer->canvas.width ? w : layer->canvas.width;
    int copy_h = h < layer->canvas.height ? h : layer->canvas.height;
    for (int y = 0; y < copy_h; y++) {
        uint8_t *row = (uint8_t *)converted->pixels + y * converted->pitch;
        memcpy(layer->canvas.pixels + y * layer->canvas.width, row, (size_t)copy_w * sizeof(uint32_t));
    }
    SDL_FreeSurface(converted);
    return 1;
}

static int save_canvas_to_bmp(const Canvas *c, const char *path) {
    if (!c || !c->pixels || !path) {
        return 0;
    }
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        (void *)c->pixels,
        c->width,
        c->height,
        32,
        c->width * 4,
        SDL_PIXELFORMAT_ARGB8888
    );
    if (!surface) {
        return 0;
    }
    int ok = SDL_SaveBMP(surface, path) == 0;
    SDL_FreeSurface(surface);
    return ok;
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

static uint32_t apply_erase(uint32_t current, uint8_t erase_alpha) {
    uint8_t da = (uint8_t)((current >> 24) & 0xFF);
    if (da == 0) {
        return 0;
    }
    int next_alpha = (int)da - (int)erase_alpha;
    if (next_alpha <= 0) {
        return 0;
    }
    return (current & 0x00FFFFFF) | ((uint32_t)next_alpha << 24);
}

static void stamp_brush(Canvas *c, int cx, int cy, int radius, uint32_t color, BrushShape shape, int erase_mode, int opacity_percent) {
    if (!c || !c->pixels || radius <= 0) {
        return;
    }
    uint8_t erase_alpha = (uint8_t)((opacity_percent * 255 + 50) / 100);
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (!brush_mask_contains(shape, dx, dy, radius)) {
                continue;
            }
            int px = cx + dx;
            int py = cy + dy;
            if (px < 0 || py < 0 || px >= c->width || py >= c->height) {
                continue;
            }
            if (erase_mode) {
                uint32_t current = canvas_get_pixel(c, px, py);
                uint32_t updated = apply_erase(current, erase_alpha);
                canvas_set_pixel_raw(c, px, py, updated);
            } else {
                canvas_set_pixel(c, px, py, color);
            }
        }
    }
}

static void draw_brush_line(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t color, BrushShape shape, int erase_mode, int opacity_percent) {
    if (!c) {
        return;
    }
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        stamp_brush(c, x0, y0, radius, color, shape, erase_mode, opacity_percent);
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
        if (active && active->canvas.pixels) {
            if (!load_bmp_into_layer(active, input_path)) {
                fprintf(stderr, "Failed to load %s\n", input_path);
            }
        }
    }
    layer_stack_composite(&layers, &composite, COLOR_BG);

    int running = 1;
    int drawing = 0;
    int shaping = 0;
    int last_x = 0;
    int last_y = 0;
    int shape_start_x = 0;
    int shape_start_y = 0;
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
    uint32_t *preview_pixels = NULL;
    uint32_t *shape_preview_base = NULL;
    Canvas preview_canvas = {CANVAS_WIDTH, CANVAS_HEIGHT, NULL};
    int preview_active = 0;
    int needs_composite = 0;
    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));
    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);

    const size_t pixel_bytes = (size_t)CANVAS_WIDTH * (size_t)CANVAS_HEIGHT * sizeof(uint32_t);

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    push_snapshot(&layers, undo_stack, &undo_count, redo_stack, &redo_count);
                    last_x = e.button.x;
                    last_y = e.button.y;
                    if (tool == TOOL_BRUSH || tool == TOOL_ERASER) {
                        drawing = 1;
                        Layer *active = layer_stack_active(&layers);
                        if (active && active->canvas.pixels) {
                            stamp_brush(&active->canvas, last_x, last_y, brush_radius, brush_color, brush_shape, tool == TOOL_ERASER, brush_opacity);
                            needs_composite = 1;
                        }
                    } else {
                        shaping = 1;
                        shape_start_x = last_x;
                        shape_start_y = last_y;
                        if (!shape_preview_base) {
                            shape_preview_base = (uint32_t *)malloc(pixel_bytes);
                        }
                        if (shape_preview_base) {
                            memcpy(shape_preview_base, composite.pixels, pixel_bytes);
                            preview_active = 1;
                        }
                    }
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    int x = e.button.x;
                    int y = e.button.y;
                    if (x >= 0 && y >= 0 && x < CANVAS_WIDTH && y < CANVAS_HEIGHT) {
                        const uint32_t sampled = canvas_get_pixel(preview_active && preview_canvas.pixels ? &preview_canvas : &composite, x, y);
                        brush_color_rgb = sampled & 0x00FFFFFF;
                        int sampled_alpha = (int)((sampled >> 24) & 0xFF);
                        brush_opacity = (sampled_alpha * 100 + 127) / 255;
                        if (brush_opacity < 1) {
                            brush_opacity = 1;
                        }
                        brush_color = compose_brush_color(brush_color_rgb, brush_opacity);
                        tool = TOOL_BRUSH;
                        update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                    }
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    if (drawing) {
                        drawing = 0;
                    }
                    if (shaping) {
                        const Uint8 *state = SDL_GetKeyboardState(NULL);
                        int shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
                        int end_x = e.button.x;
                        int end_y = e.button.y;
                        constrain_end(tool, shape_start_x, shape_start_y, end_x, end_y, shift, &end_x, &end_y);
                        Layer *active = layer_stack_active(&layers);
                        if (active && active->canvas.pixels) {
                            draw_shape(&active->canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
                            needs_composite = 1;
                        }
                        shaping = 0;
                        preview_active = 0;
                    }
                }
                break;
            case SDL_MOUSEMOTION:
                if (drawing) {
                    int x = e.motion.x;
                    int y = e.motion.y;
                    if (x >= 0 && y >= 0 && x < CANVAS_WIDTH && y < CANVAS_HEIGHT) {
                        Layer *active = layer_stack_active(&layers);
                        if (active && active->canvas.pixels) {
                            draw_brush_line(&active->canvas, last_x, last_y, x, y, brush_radius, brush_color, brush_shape, tool == TOOL_ERASER, brush_opacity);
                            last_x = x;
                            last_y = y;
                            needs_composite = 1;
                        }
                    }
                } else if (shaping && shape_preview_base) {
                    int x = e.motion.x;
                    int y = e.motion.y;
                    if (x < 0 || y < 0 || x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT) {
                        break;
                    }
                    const Uint8 *state = SDL_GetKeyboardState(NULL);
                    int shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
                    int end_x = x;
                    int end_y = y;
                    constrain_end(tool, shape_start_x, shape_start_y, end_x, end_y, shift, &end_x, &end_y);
                    if (!preview_pixels) {
                        preview_pixels = (uint32_t *)malloc(pixel_bytes);
                        if (preview_pixels) {
                            preview_canvas.pixels = preview_pixels;
                        }
                    }
                    if (preview_pixels) {
                        memcpy(preview_pixels, shape_preview_base, pixel_bytes);
                        draw_shape(&preview_canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
                        preview_active = 1;
                    }
                }
                break;
            case SDL_KEYDOWN: {
                SDL_Keycode key = e.key.keysym.sym;
                const Uint8 *state = SDL_GetKeyboardState(NULL);
                int ctrl = state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
                int shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];

                if (key == SDLK_ESCAPE) {
                    running = 0;
                    break;
                }

                if (ctrl && shift && key == SDLK_n) {
                    push_snapshot(&layers, undo_stack, &undo_count, redo_stack, &redo_count);
                    if (layer_stack_add(&layers, NULL, 0x00000000) < 0) {
                        fprintf(stderr, "Max layers reached (%d)\n", MAX_LAYERS);
                    } else {
                        Layer *active = layer_stack_active(&layers);
                        if (active && active->canvas.pixels) {
                            canvas_clear(&active->canvas, 0x00000000);
                        }
                        needs_composite = 1;
                    }
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                    break;
                }

                if (ctrl && shift && key == SDLK_v) {
                    push_snapshot(&layers, undo_stack, &undo_count, redo_stack, &redo_count);
                    if (!layer_stack_toggle_visibility(&layers, layers.active_layer)) {
                        fprintf(stderr, "Cannot hide the final visible layer\n");
                    } else {
                        needs_composite = 1;
                    }
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                    break;
                }

                if (ctrl && key == SDLK_s) {
                    if (!save_canvas_to_bmp(preview_active && preview_canvas.pixels ? &preview_canvas : &composite, "output.bmp")) {
                        fprintf(stderr, "Failed to save output.bmp\n");
                    }
                    break;
                }

                if (ctrl && key == SDLK_o) {
                    push_snapshot(&layers, undo_stack, &undo_count, redo_stack, &redo_count);
                    Layer *active = layer_stack_active(&layers);
                    if (!active || !load_bmp_into_layer(active, "input.bmp")) {
                        fprintf(stderr, "Failed to load input.bmp\n");
                    } else {
                        needs_composite = 1;
                    }
                    break;
                }

                if (ctrl && key == SDLK_z) {
                    if (undo_count > 0) {
                        Snapshot current = {0};
                        if (snapshot_from_layers(&current, &layers)) {
                            if (redo_count == MAX_HISTORY) {
                                snapshot_free(&redo_stack[0]);
                                memmove(&redo_stack[0], &redo_stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
                                redo_count = MAX_HISTORY - 1;
                            }
                            redo_stack[redo_count++] = current;
                        }
                        Snapshot prev = undo_stack[--undo_count];
                        if (!snapshot_apply(&prev, &layers)) {
                            fprintf(stderr, "Undo snapshot apply failed\n");
                        }
                        snapshot_free(&prev);
                        needs_composite = 1;
                        update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                    }
                    break;
                }

                if (ctrl && key == SDLK_y) {
                    if (redo_count > 0) {
                        Snapshot current = {0};
                        if (snapshot_from_layers(&current, &layers)) {
                            if (undo_count == MAX_HISTORY) {
                                snapshot_free(&undo_stack[0]);
                                memmove(&undo_stack[0], &undo_stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
                                undo_count = MAX_HISTORY - 1;
                            }
                            undo_stack[undo_count++] = current;
                        }
                        Snapshot next = redo_stack[--redo_count];
                        if (!snapshot_apply(&next, &layers)) {
                            fprintf(stderr, "Redo snapshot apply failed\n");
                        }
                        snapshot_free(&next);
                        needs_composite = 1;
                        update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                    }
                    break;
                }

                // Layer navigation without modifiers
                if (key == SDLK_PAGEUP) {
                    if (layer_stack_cycle(&layers, 1) >= 0) {
                        update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                    }
                    break;
                }

                if (key == SDLK_PAGEDOWN) {
                    if (layer_stack_cycle(&layers, -1) >= 0) {
                        update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                    }
                    break;
                }

                if (key == SDLK_b) {
                    brush_color_rgb = COLOR_BRUSH & 0x00FFFFFF;
                    brush_color = compose_brush_color(brush_color_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                } else if (key == SDLK_e) {
                    tool = TOOL_ERASER;
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                } else if (key == SDLK_l) {
                    tool = TOOL_LINE;
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                } else if (!ctrl && key == SDLK_r) {
                    tool = TOOL_RECT;
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                } else if (!ctrl && key == SDLK_o) {
                    tool = TOOL_ELLIPSE;
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                } else if (key == SDLK_LEFTBRACKET) {
                    if (brush_radius > 1) {
                        brush_radius -= 1;
                        update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                    }
                } else if (key == SDLK_RIGHTBRACKET) {
                    if (brush_radius < 64) {
                        brush_radius += 1;
                        update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                    }
                } else if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
                    if (brush_opacity > 1) {
                        brush_opacity -= 5;
                        if (brush_opacity < 1) {
                            brush_opacity = 1;
                        }
                        brush_color = compose_brush_color(brush_color_rgb, brush_opacity);
                        update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                    }
                } else if (key == SDLK_EQUALS || key == SDLK_KP_PLUS) {
                    if (brush_opacity < 100) {
                        brush_opacity += 5;
                        if (brush_opacity > 100) {
                            brush_opacity = 100;
                        }
                        brush_color = compose_brush_color(brush_color_rgb, brush_opacity);
                        update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                    }
                } else if (key == SDLK_1) {
                    brush_color_rgb = COLOR_BRUSH & 0x00FFFFFF;
                    brush_color = compose_brush_color(brush_color_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                } else if (key == SDLK_2) {
                    brush_color_rgb = COLOR_RED & 0x00FFFFFF;
                    brush_color = compose_brush_color(brush_color_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                } else if (key == SDLK_3) {
                    brush_color_rgb = COLOR_GREEN & 0x00FFFFFF;
                    brush_color = compose_brush_color(brush_color_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                } else if (key == SDLK_4) {
                    brush_color_rgb = COLOR_BLUE & 0x00FFFFFF;
                    brush_color = compose_brush_color(brush_color_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                } else if (key == SDLK_5) {
                    brush_color_rgb = COLOR_YELLOW & 0x00FFFFFF;
                    brush_color = compose_brush_color(brush_color_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                } else if (key == SDLK_6) {
                    brush_color_rgb = COLOR_PURPLE & 0x00FFFFFF;
                    brush_color = compose_brush_color(brush_color_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                } else if (key == SDLK_c) {
                    push_snapshot(&layers, undo_stack, &undo_count, redo_stack, &redo_count);
                    uint32_t clear_color = (layers.active_layer == 0) ? COLOR_BG : 0x00000000;
                    if (layer_stack_clear_layer(&layers, layers.active_layer, clear_color)) {
                        needs_composite = 1;
                    }
                } else if (key == SDLK_f) {
                    int mx = 0;
                    int my = 0;
                    SDL_GetMouseState(&mx, &my);
                    if (mx >= 0 && my >= 0 && mx < CANVAS_WIDTH && my < CANVAS_HEIGHT) {
                        push_snapshot(&layers, undo_stack, &undo_count, redo_stack, &redo_count);
                        Layer *active = layer_stack_active(&layers);
                        if (!active || !canvas_flood_fill(&active->canvas, mx, my, brush_color)) {
                            fprintf(stderr, "Fill failed\n");
                        } else {
                            needs_composite = 1;
                        }
                    }
                } else if (key == SDLK_i) {
                    int mx = 0;
                    int my = 0;
                    SDL_GetMouseState(&mx, &my);
                    if (mx >= 0 && my >= 0 && mx < CANVAS_WIDTH && my < CANVAS_HEIGHT) {
                        const uint32_t sampled = canvas_get_pixel(preview_active && preview_canvas.pixels ? &preview_canvas : &composite, mx, my);
                        brush_color_rgb = sampled & 0x00FFFFFF;
                        int sampled_alpha = (int)((sampled >> 24) & 0xFF);
                        brush_opacity = (sampled_alpha * 100 + 127) / 255;
                        if (brush_opacity < 1) {
                            brush_opacity = 1;
                        }
                        brush_color = compose_brush_color(brush_color_rgb, brush_opacity);
                        tool = TOOL_BRUSH;
                        update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                    }
                } else if (key == SDLK_PERIOD || key == SDLK_GREATER) {
                    brush_shape = cycle_brush_shape(brush_shape, 1);
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
                } else if (key == SDLK_COMMA || key == SDLK_LESS) {
                    brush_shape = cycle_brush_shape(brush_shape, -1);
                    update_window_title(window, &layers, tool, brush_shape, brush_radius, brush_color, brush_opacity);
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

        if (preview_active && preview_pixels) {
            SDL_UpdateTexture(texture, NULL, preview_pixels, CANVAS_WIDTH * 4);
        } else {
            SDL_UpdateTexture(texture, NULL, composite.pixels, CANVAS_WIDTH * 4);
        }
        SDL_SetRenderDrawColor(renderer, 30, 30, 34, 255);
        SDL_RenderClear(renderer);

        SDL_Rect dest = {0, 0, CANVAS_WIDTH, CANVAS_HEIGHT};
        SDL_RenderCopy(renderer, texture, NULL, &dest);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    free(shape_preview_base);
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
