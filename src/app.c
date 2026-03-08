#include "app.h"
#include "canvas.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define CANVAS_WIDTH 800
#define CANVAS_HEIGHT 600
#define MAX_HISTORY 20

static const uint32_t COLOR_BG = 0xFFFFFFFF;   // white
static const uint32_t COLOR_BRUSH = 0xFF1B1F24; // near-black
static const uint32_t COLOR_ERASE = 0xFFFFFFFF; // white
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

typedef struct {
    int width;
    int height;
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
}

static int snapshot_from_canvas(Snapshot *s, const Canvas *c) {
    if (!s || !c || !c->pixels) {
        return 0;
    }
    size_t size = (size_t)c->width * (size_t)c->height;
    uint32_t *copy = (uint32_t *)malloc(size * sizeof(uint32_t));
    if (!copy) {
        return 0;
    }
    memcpy(copy, c->pixels, size * sizeof(uint32_t));
    s->pixels = copy;
    s->width = c->width;
    s->height = c->height;
    return 1;
}

static int snapshot_apply(const Snapshot *s, Canvas *c) {
    if (!s || !c || !c->pixels || !s->pixels) {
        return 0;
    }
    if (s->width != c->width || s->height != c->height) {
        return 0;
    }
    size_t size = (size_t)c->width * (size_t)c->height;
    memcpy(c->pixels, s->pixels, size * sizeof(uint32_t));
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

static void push_snapshot(Canvas *canvas, Snapshot *stack, int *count, Snapshot *redo, int *redo_count) {
    if (!canvas || !stack || !count) {
        return;
    }
    if (*count == MAX_HISTORY) {
        snapshot_free(&stack[0]);
        memmove(&stack[0], &stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
        *count = MAX_HISTORY - 1;
    }
    Snapshot s = {0};
    if (!snapshot_from_canvas(&s, canvas)) {
        return;
    }
    stack[(*count)++] = s;
    if (redo && redo_count) {
        stack_clear(redo, redo_count);
    }
}

static void update_window_title(SDL_Window *window, const char *tool, int radius, uint32_t color) {
    char title[128];
    snprintf(title, sizeof(title), "Openshop - %s | size %d | #%08X", tool, radius, color);
    SDL_SetWindowTitle(window, title);
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

static int load_bmp_into_canvas(Canvas *c, const char *path) {
    if (!path || !c) {
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
    int copy_w = w < c->width ? w : c->width;
    int copy_h = h < c->height ? h : c->height;
    for (int y = 0; y < copy_h; y++) {
        uint8_t *row = (uint8_t *)converted->pixels + y * converted->pitch;
        memcpy(c->pixels + y * c->width, row, (size_t)copy_w * sizeof(uint32_t));
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

    Canvas canvas;
    if (!canvas_init(&canvas, CANVAS_WIDTH, CANVAS_HEIGHT)) {
        fprintf(stderr, "Canvas init failed\n");
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    canvas_clear(&canvas, COLOR_BG);
    if (input_path && input_path[0]) {
        load_bmp_into_canvas(&canvas, input_path);
    }

    int running = 1;
    int drawing = 0;
    int last_x = 0;
    int last_y = 0;
    int brush_radius = 6;
    uint32_t brush_color = COLOR_BRUSH;
    Tool tool = TOOL_BRUSH;
    const char *tool_name = tool_label(tool);
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count = 0;
    int redo_count = 0;
    Snapshot shape_base = {0};
    int shaping = 0;
    int shape_start_x = 0;
    int shape_start_y = 0;
    uint32_t *preview_pixels = NULL;
    Canvas preview_canvas = {0};
    int preview_active = 0;
    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));
    update_window_title(window, tool_name, brush_radius, brush_color);

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    push_snapshot(&canvas, undo_stack, &undo_count, redo_stack, &redo_count);
                    last_x = e.button.x;
                    last_y = e.button.y;
                    if (tool == TOOL_BRUSH || tool == TOOL_ERASER) {
                        drawing = 1;
                        canvas_draw_circle(&canvas, last_x, last_y, brush_radius, brush_color);
                    } else {
                        shaping = 1;
                        shape_start_x = last_x;
                        shape_start_y = last_y;
                        snapshot_free(&shape_base);
                        snapshot_from_canvas(&shape_base, &canvas);
                    }
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    int x = e.button.x;
                    int y = e.button.y;
                    if (x >= 0 && y >= 0 && x < CANVAS_WIDTH && y < CANVAS_HEIGHT) {
                        brush_color = canvas_get_pixel(&canvas, x, y);
                        tool = TOOL_BRUSH;
                        tool_name = tool_label(tool);
                        update_window_title(window, tool_name, brush_radius, brush_color);
                    }
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    drawing = 0;
                    if (shaping) {
                        const Uint8 *state = SDL_GetKeyboardState(NULL);
                        int shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
                        int end_x = e.button.x;
                        int end_y = e.button.y;
                        constrain_end(tool, shape_start_x, shape_start_y, end_x, end_y, shift, &end_x, &end_y);
                        draw_shape(&canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
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
                        canvas_draw_line(&canvas, last_x, last_y, x, y, brush_radius, brush_color);
                        last_x = x;
                        last_y = y;
                    }
                } else if (shaping) {
                    if (!shape_base.pixels) {
                        break;
                    }
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
                        preview_pixels = (uint32_t *)malloc((size_t)canvas.width * (size_t)canvas.height * sizeof(uint32_t));
                        if (!preview_pixels) {
                            break;
                        }
                        preview_canvas.width = canvas.width;
                        preview_canvas.height = canvas.height;
                        preview_canvas.pixels = preview_pixels;
                    }
                    memcpy(preview_pixels, shape_base.pixels, (size_t)canvas.width * (size_t)canvas.height * sizeof(uint32_t));
                    draw_shape(&preview_canvas, tool, shape_start_x, shape_start_y, end_x, end_y, brush_radius, brush_color);
                    preview_active = 1;
                }
                break;
            case SDL_KEYDOWN: {
                SDL_Keycode key = e.key.keysym.sym;
                const Uint8 *state = SDL_GetKeyboardState(NULL);
                int ctrl = state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];

                if (key == SDLK_ESCAPE) {
                    running = 0;
                } else if (key == SDLK_b) {
                    brush_color = COLOR_BRUSH;
                    tool = TOOL_BRUSH;
                    tool_name = tool_label(tool);
                    update_window_title(window, tool_name, brush_radius, brush_color);
                } else if (key == SDLK_e) {
                    brush_color = COLOR_ERASE;
                    tool = TOOL_ERASER;
                    tool_name = tool_label(tool);
                    update_window_title(window, tool_name, brush_radius, brush_color);
                } else if (key == SDLK_l) {
                    tool = TOOL_LINE;
                    tool_name = tool_label(tool);
                    update_window_title(window, tool_name, brush_radius, brush_color);
                } else if (key == SDLK_r) {
                    tool = TOOL_RECT;
                    tool_name = tool_label(tool);
                    update_window_title(window, tool_name, brush_radius, brush_color);
                } else if (key == SDLK_o) {
                    tool = TOOL_ELLIPSE;
                    tool_name = tool_label(tool);
                    update_window_title(window, tool_name, brush_radius, brush_color);
                } else if (key == SDLK_LEFTBRACKET) {
                    if (brush_radius > 1) {
                        brush_radius -= 1;
                        update_window_title(window, tool_name, brush_radius, brush_color);
                    }
                } else if (key == SDLK_RIGHTBRACKET) {
                    if (brush_radius < 64) {
                        brush_radius += 1;
                        update_window_title(window, tool_name, brush_radius, brush_color);
                    }
                } else if (key == SDLK_1) {
                    brush_color = COLOR_BRUSH;
                    tool = TOOL_BRUSH;
                    tool_name = tool_label(tool);
                    update_window_title(window, tool_name, brush_radius, brush_color);
                } else if (key == SDLK_2) {
                    brush_color = COLOR_RED;
                    tool = TOOL_BRUSH;
                    tool_name = tool_label(tool);
                    update_window_title(window, tool_name, brush_radius, brush_color);
                } else if (key == SDLK_3) {
                    brush_color = COLOR_GREEN;
                    tool = TOOL_BRUSH;
                    tool_name = tool_label(tool);
                    update_window_title(window, tool_name, brush_radius, brush_color);
                } else if (key == SDLK_4) {
                    brush_color = COLOR_BLUE;
                    tool = TOOL_BRUSH;
                    tool_name = tool_label(tool);
                    update_window_title(window, tool_name, brush_radius, brush_color);
                } else if (key == SDLK_5) {
                    brush_color = COLOR_YELLOW;
                    tool = TOOL_BRUSH;
                    tool_name = tool_label(tool);
                    update_window_title(window, tool_name, brush_radius, brush_color);
                } else if (key == SDLK_6) {
                    brush_color = COLOR_PURPLE;
                    tool = TOOL_BRUSH;
                    tool_name = tool_label(tool);
                    update_window_title(window, tool_name, brush_radius, brush_color);
                } else if (key == SDLK_c) {
                    push_snapshot(&canvas, undo_stack, &undo_count, redo_stack, &redo_count);
                    canvas_clear(&canvas, COLOR_BG);
                } else if (ctrl && key == SDLK_s) {
                    if (!save_canvas_to_bmp(&canvas, "output.bmp")) {
                        fprintf(stderr, "Failed to save output.bmp\n");
                    }
                } else if (ctrl && key == SDLK_o) {
                    push_snapshot(&canvas, undo_stack, &undo_count, redo_stack, &redo_count);
                    if (!load_bmp_into_canvas(&canvas, "input.bmp")) {
                        fprintf(stderr, "Failed to load input.bmp\n");
                    }
                } else if (ctrl && key == SDLK_z) {
                    if (undo_count > 0) {
                        Snapshot current = {0};
                        if (snapshot_from_canvas(&current, &canvas)) {
                            if (redo_count == MAX_HISTORY) {
                                snapshot_free(&redo_stack[0]);
                                memmove(&redo_stack[0], &redo_stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
                                redo_count = MAX_HISTORY - 1;
                            }
                            redo_stack[redo_count++] = current;
                        }
                        Snapshot prev = undo_stack[--undo_count];
                        snapshot_apply(&prev, &canvas);
                        snapshot_free(&prev);
                    }
                } else if (ctrl && key == SDLK_y) {
                    if (redo_count > 0) {
                        Snapshot current = {0};
                        if (snapshot_from_canvas(&current, &canvas)) {
                            if (undo_count == MAX_HISTORY) {
                                snapshot_free(&undo_stack[0]);
                                memmove(&undo_stack[0], &undo_stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
                                undo_count = MAX_HISTORY - 1;
                            }
                            undo_stack[undo_count++] = current;
                        }
                        Snapshot next = redo_stack[--redo_count];
                        snapshot_apply(&next, &canvas);
                        snapshot_free(&next);
                    }
                } else if (key == SDLK_f) {
                    int mx = 0;
                    int my = 0;
                    SDL_GetMouseState(&mx, &my);
                    if (mx >= 0 && my >= 0 && mx < CANVAS_WIDTH && my < CANVAS_HEIGHT) {
                        push_snapshot(&canvas, undo_stack, &undo_count, redo_stack, &redo_count);
                        if (!canvas_flood_fill(&canvas, mx, my, brush_color)) {
                            fprintf(stderr, "Fill failed\n");
                        }
                    }
                } else if (key == SDLK_i) {
                    int mx = 0;
                    int my = 0;
                    SDL_GetMouseState(&mx, &my);
                    if (mx >= 0 && my >= 0 && mx < CANVAS_WIDTH && my < CANVAS_HEIGHT) {
                        brush_color = canvas_get_pixel(&canvas, mx, my);
                        tool = TOOL_BRUSH;
                        tool_name = tool_label(tool);
                        update_window_title(window, tool_name, brush_radius, brush_color);
                    }
                }
                break;
            }
            default:
                break;
            }
        }

        if (preview_active && preview_pixels) {
            SDL_UpdateTexture(texture, NULL, preview_pixels, CANVAS_WIDTH * 4);
        } else {
            SDL_UpdateTexture(texture, NULL, canvas.pixels, CANVAS_WIDTH * 4);
        }
        SDL_SetRenderDrawColor(renderer, 30, 30, 34, 255);
        SDL_RenderClear(renderer);

        SDL_Rect dest = {0, 0, CANVAS_WIDTH, CANVAS_HEIGHT};
        SDL_RenderCopy(renderer, texture, NULL, &dest);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    canvas_free(&canvas);
    stack_clear(undo_stack, &undo_count);
    stack_clear(redo_stack, &redo_count);
    snapshot_free(&shape_base);
    free(preview_pixels);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
