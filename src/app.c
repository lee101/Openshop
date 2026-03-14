#include "app.h"
#include "canvas.h"
#include "layers.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_WIDTH  1024
#define WINDOW_HEIGHT 768
#define CANVAS_WIDTH  800
#define CANVAS_HEIGHT 600
#define MAX_HISTORY   20
#define MAX_ZOOM      4

/* ── Preset palette ─────────────────────────────────────────────────── */
static const uint32_t COLOR_BG     = 0xFFFFFFFF;
static const uint32_t COLOR_BRUSH  = 0xFF1B1F24;
static const uint32_t COLOR_ERASE  = 0xFFFFFFFF;
static const uint32_t COLOR_RED    = 0xFFE53935;
static const uint32_t COLOR_GREEN  = 0xFF43A047;
static const uint32_t COLOR_BLUE   = 0xFF1E88E5;
static const uint32_t COLOR_YELLOW = 0xFFFDD835;
static const uint32_t COLOR_PURPLE = 0xFF8E24AA;

/* ── Tools ──────────────────────────────────────────────────────────── */
typedef enum {
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_LINE,
    TOOL_RECT,
    TOOL_ELLIPSE,
    TOOL_COUNT
} Tool;

static const char *tool_label(Tool t) {
    switch (t) {
    case TOOL_BRUSH:   return "Brush";
    case TOOL_ERASER:  return "Eraser";
    case TOOL_LINE:    return "Line";
    case TOOL_RECT:    return "Rect";
    case TOOL_ELLIPSE: return "Ellipse";
    default:           return "Brush";
    }
}

/* ── Snapshot (per-active-layer undo) ───────────────────────────────── */
typedef struct {
    int       width, height;
    uint32_t *pixels;
} Snapshot;

static void snapshot_free(Snapshot *s) {
    if (!s) return;
    free(s->pixels);
    s->pixels = NULL;
    s->width = s->height = 0;
}

static int snapshot_from_canvas(Snapshot *s, const Canvas *c) {
    if (!s || !c || !c->pixels) return 0;
    size_t n = (size_t)c->width * (size_t)c->height;
    uint32_t *buf = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!buf) return 0;
    memcpy(buf, c->pixels, n * sizeof(uint32_t));
    s->pixels = buf;
    s->width  = c->width;
    s->height = c->height;
    return 1;
}

static int snapshot_apply(const Snapshot *s, Canvas *c) {
    if (!s || !c || !c->pixels || !s->pixels) return 0;
    if (s->width != c->width || s->height != c->height) return 0;
    memcpy(c->pixels, s->pixels, (size_t)c->width * (size_t)c->height * sizeof(uint32_t));
    return 1;
}

static void stack_clear(Snapshot *stack, int *count) {
    if (!stack || !count) return;
    for (int i = 0; i < *count; i++) snapshot_free(&stack[i]);
    *count = 0;
}

static void push_snapshot(Canvas *c, Snapshot *stack, int *count,
                          Snapshot *redo, int *redo_count) {
    if (!c || !stack || !count) return;
    if (*count == MAX_HISTORY) {
        snapshot_free(&stack[0]);
        memmove(&stack[0], &stack[1], sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
        *count = MAX_HISTORY - 1;
    }
    Snapshot s = {0};
    if (!snapshot_from_canvas(&s, c)) return;
    stack[(*count)++] = s;
    if (redo && redo_count) stack_clear(redo, redo_count);
}

/* ── Color helpers ──────────────────────────────────────────────────── */
static uint32_t compose_color(uint32_t rgb, int opacity_pct) {
    if (opacity_pct <   1) opacity_pct =   1;
    if (opacity_pct > 100) opacity_pct = 100;
    uint32_t a = (uint32_t)((opacity_pct * 255 + 50) / 100);
    return (a << 24) | (rgb & 0x00FFFFFF);
}

/* ── Window title ───────────────────────────────────────────────────── */
static void update_title(SDL_Window *win, const LayerStack *ls, Tool tool,
                         int fill_shapes, int radius, uint32_t color,
                         int opacity, int zoom) {
    const char *lname = ls->layers[ls->active].name;
    int         lop   = ls->layers[ls->active].opacity;
    int         lvis  = ls->layers[ls->active].visible;
    char title[256];
    snprintf(title, sizeof(title),
             "Openshop | %s%s | L%d/%d:%s(%d%%%s) | %dx | sz%d | op%d%% | #%06X",
             tool_label(tool),
             (fill_shapes && (tool == TOOL_RECT || tool == TOOL_ELLIPSE)) ? "(fill)" : "",
             ls->active + 1, ls->count, lname, lop, lvis ? "" : " hidden",
             zoom, radius, opacity, color & 0x00FFFFFF);
    SDL_SetWindowTitle(win, title);
}

/* ── Shape drawing ──────────────────────────────────────────────────── */
static void draw_shape(Canvas *c, Tool tool, int fill,
                       int x0, int y0, int x1, int y1,
                       int radius, uint32_t color) {
    switch (tool) {
    case TOOL_LINE:
        canvas_draw_line(c, x0, y0, x1, y1, radius, color);
        break;
    case TOOL_RECT:
        if (fill)
            canvas_draw_rect_filled(c, x0, y0, x1, y1, color);
        else
            canvas_draw_rect_outline(c, x0, y0, x1, y1, radius, color);
        break;
    case TOOL_ELLIPSE: {
        int cx = (x0 + x1) / 2;
        int cy = (y0 + y1) / 2;
        int rx = abs(x1 - x0) / 2;
        int ry = abs(y1 - y0) / 2;
        if (fill)
            canvas_draw_ellipse_filled(c, cx, cy, rx, ry, color);
        else
            canvas_draw_ellipse_outline(c, cx, cy, rx, ry, radius, color);
        break;
    }
    default:
        break;
    }
}

static void constrain_end(Tool tool, int x0, int y0, int x1, int y1,
                          int shift, int *ox, int *oy) {
    if (!ox || !oy) return;
    *ox = x1; *oy = y1;
    if (!shift) return;
    int dx = x1 - x0, dy = y1 - y0;
    int adx = abs(dx), ady = abs(dy);
    if (tool == TOOL_LINE) {
        if (adx > ady * 2) {
            *ox = x0 + (dx >= 0 ? adx : -adx); *oy = y0;
        } else if (ady > adx * 2) {
            *ox = x0; *oy = y0 + (dy >= 0 ? ady : -ady);
        } else {
            int len = adx > ady ? adx : ady;
            *ox = x0 + (dx >= 0 ? len : -len);
            *oy = y0 + (dy >= 0 ? len : -len);
        }
    } else if (tool == TOOL_RECT || tool == TOOL_ELLIPSE) {
        int len = adx > ady ? adx : ady;
        *ox = x0 + (dx >= 0 ? len : -len);
        *oy = y0 + (dy >= 0 ? len : -len);
    }
}

/* ── Zoom / coordinate helpers ──────────────────────────────────────── */
static SDL_Rect canvas_dest_rect(int zoom) {
    int cw = CANVAS_WIDTH  * zoom;
    int ch = CANVAS_HEIGHT * zoom;
    int x  = (WINDOW_WIDTH  - cw) / 2;
    int y  = (WINDOW_HEIGHT - ch) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    return (SDL_Rect){x, y, cw, ch};
}

static void screen_to_canvas(int sx, int sy, int zoom, int *cx, int *cy) {
    SDL_Rect r = canvas_dest_rect(zoom);
    *cx = (sx - r.x) / zoom;
    *cy = (sy - r.y) / zoom;
}

static int in_canvas(int cx, int cy) {
    return cx >= 0 && cy >= 0 && cx < CANVAS_WIDTH && cy < CANVAS_HEIGHT;
}

/* ── BMP I/O ────────────────────────────────────────────────────────── */
static int load_bmp_into_canvas(Canvas *c, const char *path) {
    if (!path || !c) return 0;
    SDL_Surface *bmp = SDL_LoadBMP(path);
    if (!bmp) return 0;
    SDL_Surface *conv = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(bmp);
    if (!conv) return 0;
    int cw = conv->w < c->width  ? conv->w : c->width;
    int ch = conv->h < c->height ? conv->h : c->height;
    for (int y = 0; y < ch; y++) {
        uint8_t *row = (uint8_t *)conv->pixels + y * conv->pitch;
        memcpy(c->pixels + y * c->width, row, (size_t)cw * sizeof(uint32_t));
    }
    SDL_FreeSurface(conv);
    return 1;
}

static int save_canvas_to_bmp(const Canvas *c, const char *path) {
    if (!c || !c->pixels || !path) return 0;
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(
        (void *)c->pixels, c->width, c->height, 32,
        c->width * 4, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) return 0;
    int ok = SDL_SaveBMP(surf, path) == 0;
    SDL_FreeSurface(surf);
    return ok;
}

/* ── Entry point ────────────────────────────────────────────────────── */
int app_run(const char *input_path) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Openshop", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        CANVAS_WIDTH, CANVAS_HEIGHT);
    if (!texture) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* Layer stack + composite canvas for display */
    LayerStack layers;
    if (!layers_init(&layers, CANVAS_WIDTH, CANVAS_HEIGHT)) {
        fprintf(stderr, "layers_init failed\n");
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Canvas composite;
    if (!canvas_init(&composite, CANVAS_WIDTH, CANVAS_HEIGHT)) {
        fprintf(stderr, "canvas_init (composite) failed\n");
        layers_free(&layers);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    canvas_clear(&composite, COLOR_BG);

    if (input_path && input_path[0]) {
        Canvas *bg = layers_active_canvas(&layers);
        if (bg) load_bmp_into_canvas(bg, input_path);
    }

    /* ── Paint state ── */
    int      running       = 1;
    int      drawing       = 0;
    int      last_cx       = 0, last_cy = 0;
    int      brush_radius  = 6;
    int      brush_opacity = 100;
    uint32_t brush_rgb     = COLOR_BRUSH & 0x00FFFFFF;
    uint32_t bg_rgb        = 0x00FFFFFF; /* background color for X-swap */
    uint32_t brush_color   = compose_color(brush_rgb, brush_opacity);
    Tool     tool          = TOOL_BRUSH;
    int      fill_shapes   = 0; /* G toggles filled rect/ellipse */
    int      zoom          = 1;

    /* Undo / redo */
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count = 0, redo_count = 0;
    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));

    /* Shape drag / preview */
    Snapshot  shape_base = {0};
    int       shaping    = 0;
    int       shape_sx   = 0, shape_sy = 0; /* canvas coords at drag start */
    uint32_t *preview_px = NULL;
    Canvas    preview_cv = {0};
    int       preview_on = 0;

    update_title(window, &layers, tool, fill_shapes,
                 brush_radius, brush_color, brush_opacity, zoom);

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {

            /* ── Quit ── */
            case SDL_QUIT:
                running = 0;
                break;

            /* ── Mouse button down ── */
            case SDL_MOUSEBUTTONDOWN: {
                int cx, cy;
                screen_to_canvas(ev.button.x, ev.button.y, zoom, &cx, &cy);

                if (ev.button.button == SDL_BUTTON_LEFT) {
                    Canvas *ac = layers_active_canvas(&layers);
                    if (!ac) break;
                    push_snapshot(ac, undo_stack, &undo_count, redo_stack, &redo_count);
                    last_cx = cx; last_cy = cy;
                    if (tool == TOOL_BRUSH || tool == TOOL_ERASER) {
                        drawing = 1;
                        if (in_canvas(cx, cy))
                            canvas_draw_circle(ac, cx, cy, brush_radius, brush_color);
                    } else {
                        shaping    = 1;
                        shape_sx   = cx;
                        shape_sy   = cy;
                        snapshot_free(&shape_base);
                        snapshot_from_canvas(&shape_base, ac);
                    }
                } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                    /* Right-click: eyedropper */
                    Canvas *ac = layers_active_canvas(&layers);
                    if (!ac || !in_canvas(cx, cy)) break;
                    brush_color   = canvas_get_pixel(ac, cx, cy);
                    brush_rgb     = brush_color & 0x00FFFFFF;
                    int sa        = (int)((brush_color >> 24) & 0xFF);
                    brush_opacity = (sa * 100 + 127) / 255;
                    if (brush_opacity < 1) brush_opacity = 1;
                    brush_color   = compose_color(brush_rgb, brush_opacity);
                    tool          = TOOL_BRUSH;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                }
                break;
            }

            /* ── Mouse button up ── */
            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    drawing = 0;
                    if (shaping) {
                        const Uint8 *ks = SDL_GetKeyboardState(NULL);
                        int shift = ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT];
                        int ex, ey;
                        screen_to_canvas(ev.button.x, ev.button.y, zoom, &ex, &ey);
                        constrain_end(tool, shape_sx, shape_sy, ex, ey, shift, &ex, &ey);
                        Canvas *ac = layers_active_canvas(&layers);
                        if (ac)
                            draw_shape(ac, tool, fill_shapes,
                                       shape_sx, shape_sy, ex, ey,
                                       brush_radius, brush_color);
                        shaping    = 0;
                        preview_on = 0;
                    }
                }
                break;

            /* ── Mouse motion ── */
            case SDL_MOUSEMOTION: {
                int cx, cy;
                screen_to_canvas(ev.motion.x, ev.motion.y, zoom, &cx, &cy);

                if (drawing) {
                    Canvas *ac = layers_active_canvas(&layers);
                    if (ac && in_canvas(cx, cy)) {
                        canvas_draw_line(ac, last_cx, last_cy, cx, cy,
                                         brush_radius, brush_color);
                        last_cx = cx; last_cy = cy;
                    }
                } else if (shaping && shape_base.pixels) {
                    if (!in_canvas(cx, cy)) break;
                    const Uint8 *ks = SDL_GetKeyboardState(NULL);
                    int shift = ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT];
                    int ex = cx, ey = cy;
                    constrain_end(tool, shape_sx, shape_sy, ex, ey, shift, &ex, &ey);

                    /* Allocate preview buffer on first use */
                    if (!preview_px) {
                        preview_px = (uint32_t *)malloc(
                            (size_t)CANVAS_WIDTH * CANVAS_HEIGHT * sizeof(uint32_t));
                        if (!preview_px) break;
                        preview_cv.width  = CANVAS_WIDTH;
                        preview_cv.height = CANVAS_HEIGHT;
                        preview_cv.pixels = preview_px;
                    }

                    /* Build preview: composite layers with shape_base in active slot,
                       then draw the preview shape on top of the result. */
                    Canvas *ac = layers_active_canvas(&layers);
                    if (ac && ac->pixels) {
                        size_t sz = (size_t)ac->width * (size_t)ac->height
                                    * sizeof(uint32_t);
                        uint32_t *saved = (uint32_t *)malloc(sz);
                        if (saved) {
                            memcpy(saved, ac->pixels, sz);
                            memcpy(ac->pixels, shape_base.pixels, sz);
                            layers_composite(&layers, &preview_cv);
                            memcpy(ac->pixels, saved, sz);
                            free(saved);
                        } else {
                            layers_composite(&layers, &preview_cv);
                        }
                    } else {
                        layers_composite(&layers, &preview_cv);
                    }
                    draw_shape(&preview_cv, tool, fill_shapes,
                               shape_sx, shape_sy, ex, ey,
                               brush_radius, brush_color);
                    preview_on = 1;
                }
                break;
            }

            /* ── Keyboard ── */
            case SDL_KEYDOWN: {
                SDL_Keycode  key   = ev.key.keysym.sym;
                const Uint8 *ks    = SDL_GetKeyboardState(NULL);
                int ctrl  = ks[SDL_SCANCODE_LCTRL]  || ks[SDL_SCANCODE_RCTRL];
                int shift = ks[SDL_SCANCODE_LSHIFT]  || ks[SDL_SCANCODE_RSHIFT];
                Canvas      *ac    = layers_active_canvas(&layers);

                /* ── App ── */
                if (key == SDLK_ESCAPE) {
                    running = 0;

                /* ── Zoom: Ctrl+= / Ctrl+- / Ctrl+0 ── */
                } else if (ctrl && !shift && key == SDLK_EQUALS) {
                    if (zoom < MAX_ZOOM) zoom *= 2;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (ctrl && !shift && key == SDLK_MINUS) {
                    if (zoom > 1) zoom /= 2;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (ctrl && key == SDLK_0) {
                    zoom = 1;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);

                /* ── Layer management ── */
                /* Ctrl+Shift+N: new layer */
                } else if (ctrl && shift && key == SDLK_n) {
                    layers_add(&layers);
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                /* Ctrl+Shift+D: delete active layer */
                } else if (ctrl && shift && key == SDLK_d) {
                    layers_delete(&layers, layers.active);
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                /* PageUp/PageDown: select layer above/below */
                } else if (!ctrl && key == SDLK_PAGEUP) {
                    if (layers.active < layers.count - 1) layers.active++;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (!ctrl && key == SDLK_PAGEDOWN) {
                    if (layers.active > 0) layers.active--;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                /* Ctrl+PageUp/Down: reorder layer */
                } else if (ctrl && key == SDLK_PAGEUP) {
                    layers_move_up(&layers, layers.active);
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (ctrl && key == SDLK_PAGEDOWN) {
                    layers_move_down(&layers, layers.active);
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                /* Ctrl+Shift+H: toggle layer visibility */
                } else if (ctrl && shift && key == SDLK_h) {
                    layers.layers[layers.active].visible ^= 1;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                /* Ctrl+Shift+[ / ]: layer opacity -10 / +10 */
                } else if (ctrl && shift && key == SDLK_LEFTBRACKET) {
                    layers.layers[layers.active].opacity -= 10;
                    if (layers.layers[layers.active].opacity < 0)
                        layers.layers[layers.active].opacity = 0;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (ctrl && shift && key == SDLK_RIGHTBRACKET) {
                    layers.layers[layers.active].opacity += 10;
                    if (layers.layers[layers.active].opacity > 100)
                        layers.layers[layers.active].opacity = 100;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);

                /* ── Tools ── */
                } else if (!ctrl && key == SDLK_b) {
                    brush_rgb   = COLOR_BRUSH & 0x00FFFFFF;
                    brush_color = compose_color(brush_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (!ctrl && key == SDLK_e) {
                    /* Eraser: transparent on non-background layers, white on bg */
                    if (layers.active == 0) {
                        brush_rgb   = COLOR_ERASE & 0x00FFFFFF;
                        brush_color = compose_color(brush_rgb, brush_opacity);
                    } else {
                        brush_rgb   = COLOR_ERASE & 0x00FFFFFF;
                        brush_color = compose_color(brush_rgb, brush_opacity);
                    }
                    tool = TOOL_ERASER;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (!ctrl && key == SDLK_l) {
                    tool = TOOL_LINE;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (!ctrl && key == SDLK_r) {
                    tool = TOOL_RECT;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (!ctrl && key == SDLK_o && !shift) {
                    tool = TOOL_ELLIPSE;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                /* G: toggle filled shapes */
                } else if (!ctrl && key == SDLK_g) {
                    fill_shapes ^= 1;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                /* Tab: cycle tools */
                } else if (!ctrl && key == SDLK_TAB) {
                    tool = (Tool)((tool + 1) % TOOL_COUNT);
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);

                /* ── Brush size: [ / ] ── */
                } else if (!ctrl && !shift && key == SDLK_LEFTBRACKET) {
                    if (brush_radius > 1) brush_radius--;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (!ctrl && !shift && key == SDLK_RIGHTBRACKET) {
                    if (brush_radius < 64) brush_radius++;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);

                /* ── Brush opacity: - / = ── */
                } else if (key == SDLK_MINUS && !ctrl) {
                    brush_opacity -= 5;
                    if (brush_opacity <   1) brush_opacity =   1;
                    brush_color = compose_color(brush_rgb, brush_opacity);
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if ((key == SDLK_EQUALS || key == SDLK_KP_PLUS) && !ctrl) {
                    brush_opacity += 5;
                    if (brush_opacity > 100) brush_opacity = 100;
                    brush_color = compose_color(brush_rgb, brush_opacity);
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);

                /* ── Color presets 1-6 ── */
                } else if (key == SDLK_1) {
                    brush_rgb = COLOR_BRUSH & 0x00FFFFFF;
                    brush_color = compose_color(brush_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (key == SDLK_2) {
                    brush_rgb = COLOR_RED & 0x00FFFFFF;
                    brush_color = compose_color(brush_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (key == SDLK_3) {
                    brush_rgb = COLOR_GREEN & 0x00FFFFFF;
                    brush_color = compose_color(brush_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (key == SDLK_4) {
                    brush_rgb = COLOR_BLUE & 0x00FFFFFF;
                    brush_color = compose_color(brush_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (key == SDLK_5) {
                    brush_rgb = COLOR_YELLOW & 0x00FFFFFF;
                    brush_color = compose_color(brush_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);
                } else if (key == SDLK_6) {
                    brush_rgb = COLOR_PURPLE & 0x00FFFFFF;
                    brush_color = compose_color(brush_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);

                /* D: reset to default colors (black fg, white bg) */
                } else if (!ctrl && key == SDLK_d) {
                    brush_rgb = COLOR_BRUSH & 0x00FFFFFF;
                    bg_rgb    = 0x00FFFFFF;
                    brush_color = compose_color(brush_rgb, brush_opacity);
                    tool = TOOL_BRUSH;
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);

                /* X: swap foreground / background colors */
                } else if (!ctrl && key == SDLK_x) {
                    uint32_t tmp = brush_rgb;
                    brush_rgb    = bg_rgb;
                    bg_rgb       = tmp;
                    brush_color  = compose_color(brush_rgb, brush_opacity);
                    update_title(window, &layers, tool, fill_shapes,
                                 brush_radius, brush_color, brush_opacity, zoom);

                /* ── Canvas ops ── */
                /* C: clear active layer */
                } else if (!ctrl && key == SDLK_c) {
                    if (ac) {
                        push_snapshot(ac, undo_stack, &undo_count, redo_stack, &redo_count);
                        /* Background layer clears to white; other layers to transparent */
                        uint32_t clear_col = (layers.active == 0) ? COLOR_BG : 0x00000000U;
                        canvas_clear(ac, clear_col);
                    }

                /* Ctrl+S: save composite as BMP */
                } else if (ctrl && key == SDLK_s) {
                    layers_composite(&layers, &composite);
                    if (!save_canvas_to_bmp(&composite, "output.bmp"))
                        fprintf(stderr, "Save failed\n");
                    else
                        printf("Saved output.bmp\n");

                /* Ctrl+O: load BMP into active layer */
                } else if (ctrl && !shift && key == SDLK_o) {
                    if (ac) {
                        push_snapshot(ac, undo_stack, &undo_count, redo_stack, &redo_count);
                        if (!load_bmp_into_canvas(ac, "input.bmp"))
                            fprintf(stderr, "Load input.bmp failed\n");
                    }

                /* Ctrl+Z: undo */
                } else if (ctrl && key == SDLK_z) {
                    if (undo_count > 0 && ac) {
                        Snapshot cur = {0};
                        if (snapshot_from_canvas(&cur, ac)) {
                            if (redo_count == MAX_HISTORY) {
                                snapshot_free(&redo_stack[0]);
                                memmove(&redo_stack[0], &redo_stack[1],
                                        sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
                                redo_count = MAX_HISTORY - 1;
                            }
                            redo_stack[redo_count++] = cur;
                        }
                        Snapshot prev = undo_stack[--undo_count];
                        snapshot_apply(&prev, ac);
                        snapshot_free(&prev);
                    }

                /* Ctrl+Y: redo */
                } else if (ctrl && key == SDLK_y) {
                    if (redo_count > 0 && ac) {
                        Snapshot cur = {0};
                        if (snapshot_from_canvas(&cur, ac)) {
                            if (undo_count == MAX_HISTORY) {
                                snapshot_free(&undo_stack[0]);
                                memmove(&undo_stack[0], &undo_stack[1],
                                        sizeof(Snapshot) * (size_t)(MAX_HISTORY - 1));
                                undo_count = MAX_HISTORY - 1;
                            }
                            undo_stack[undo_count++] = cur;
                        }
                        Snapshot next = redo_stack[--redo_count];
                        snapshot_apply(&next, ac);
                        snapshot_free(&next);
                    }

                /* F: flood fill at cursor */
                } else if (!ctrl && key == SDLK_f) {
                    int mx, my, cx2, cy2;
                    SDL_GetMouseState(&mx, &my);
                    screen_to_canvas(mx, my, zoom, &cx2, &cy2);
                    if (in_canvas(cx2, cy2) && ac) {
                        push_snapshot(ac, undo_stack, &undo_count, redo_stack, &redo_count);
                        if (!canvas_flood_fill(ac, cx2, cy2, brush_color))
                            fprintf(stderr, "Fill failed\n");
                    }

                /* I: eyedropper at cursor */
                } else if (!ctrl && key == SDLK_i) {
                    int mx, my, cx2, cy2;
                    SDL_GetMouseState(&mx, &my);
                    screen_to_canvas(mx, my, zoom, &cx2, &cy2);
                    if (in_canvas(cx2, cy2) && ac) {
                        brush_color   = canvas_get_pixel(ac, cx2, cy2);
                        brush_rgb     = brush_color & 0x00FFFFFF;
                        int sa        = (int)((brush_color >> 24) & 0xFF);
                        brush_opacity = (sa * 100 + 127) / 255;
                        if (brush_opacity < 1) brush_opacity = 1;
                        brush_color   = compose_color(brush_rgb, brush_opacity);
                        tool          = TOOL_BRUSH;
                        update_title(window, &layers, tool, fill_shapes,
                                     brush_radius, brush_color, brush_opacity, zoom);
                    }
                }
                break;
            } /* SDL_KEYDOWN */

            default:
                break;
            } /* switch ev.type */
        } /* SDL_PollEvent */

        /* ── Render ── */
        const uint32_t *render_src;
        if (preview_on && preview_px) {
            render_src = preview_px;
        } else {
            layers_composite(&layers, &composite);
            render_src = composite.pixels;
        }

        SDL_UpdateTexture(texture, NULL, render_src, CANVAS_WIDTH * 4);
        SDL_SetRenderDrawColor(renderer, 30, 30, 34, 255);
        SDL_RenderClear(renderer);

        SDL_Rect dest = canvas_dest_rect(zoom);
        SDL_RenderCopy(renderer, texture, NULL, &dest);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    } /* while running */

    /* ── Cleanup ── */
    layers_free(&layers);
    canvas_free(&composite);
    stack_clear(undo_stack, &undo_count);
    stack_clear(redo_stack, &redo_count);
    snapshot_free(&shape_base);
    free(preview_px);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
