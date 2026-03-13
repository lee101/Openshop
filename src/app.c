#include "app.h"
#include "canvas.h"
#include "filters.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_WIDTH  1024
#define WINDOW_HEIGHT  768
#define CANVAS_WIDTH   800
#define CANVAS_HEIGHT  600
#define MAX_HISTORY     20

/* ---------------------------------------------------------------
 * Palette
 * --------------------------------------------------------------- */
static const uint32_t COLOR_BG     = 0xFFFFFFFF;
static const uint32_t COLOR_BLACK  = 0xFF1B1F24;
static const uint32_t COLOR_WHITE  = 0xFFFFFFFF;
static const uint32_t COLOR_RED    = 0xFFE53935;
static const uint32_t COLOR_GREEN  = 0xFF43A047;
static const uint32_t COLOR_BLUE   = 0xFF1E88E5;
static const uint32_t COLOR_YELLOW = 0xFFFDD835;
static const uint32_t COLOR_PURPLE = 0xFF8E24AA;
static const uint32_t COLOR_ORANGE = 0xFFFB8C00;
static const uint32_t COLOR_CYAN   = 0xFF00ACC1;

/* ---------------------------------------------------------------
 * Tool enum
 * --------------------------------------------------------------- */
typedef enum {
    TOOL_BRUSH,      /* hard circle brush                   B */
    TOOL_SOFT,       /* gaussian-falloff soft brush         Q */
    TOOL_SPRAY,      /* airbrush scatter                    A */
    TOOL_ERASER,     /* hard erase (draws white)            E */
    TOOL_LINE,       /* line shape                          L */
    TOOL_RECT,       /* rectangle outline                   R */
    TOOL_RECT_FILL,  /* filled rectangle                    T */
    TOOL_ELLIPSE,    /* ellipse outline                     O */
    TOOL_ELLIPSE_FILL/* filled ellipse                      P */
} Tool;

static const char *tool_label(Tool t) {
    switch (t) {
    case TOOL_BRUSH:        return "Brush";
    case TOOL_SOFT:         return "Soft";
    case TOOL_SPRAY:        return "Spray";
    case TOOL_ERASER:       return "Eraser";
    case TOOL_LINE:         return "Line";
    case TOOL_RECT:         return "Rect";
    case TOOL_RECT_FILL:    return "RectFill";
    case TOOL_ELLIPSE:      return "Ellipse";
    case TOOL_ELLIPSE_FILL: return "EllipseFill";
    default:                return "Brush";
    }
}

static int tool_is_freehand(Tool t) {
    return t == TOOL_BRUSH || t == TOOL_SOFT || t == TOOL_SPRAY || t == TOOL_ERASER;
}

static int tool_is_shape(Tool t) {
    return !tool_is_freehand(t);
}

/* ---------------------------------------------------------------
 * Undo / redo snapshot
 * --------------------------------------------------------------- */
typedef struct {
    int       width;
    int       height;
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
    uint32_t *copy = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!copy) return 0;
    memcpy(copy, c->pixels, n * sizeof(uint32_t));
    s->pixels = copy;
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

static void push_snapshot(Canvas *canvas, Snapshot *stack, int *count,
                          Snapshot *redo, int *redo_count) {
    if (!canvas || !stack || !count) return;
    if (*count == MAX_HISTORY) {
        snapshot_free(&stack[0]);
        memmove(&stack[0], &stack[1], sizeof(Snapshot) * (MAX_HISTORY - 1));
        *count = MAX_HISTORY - 1;
    }
    Snapshot s = {0};
    if (!snapshot_from_canvas(&s, canvas)) return;
    stack[(*count)++] = s;
    if (redo && redo_count) stack_clear(redo, redo_count);
}

/* ---------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------- */

static uint32_t compose_color(uint32_t rgb, int opacity_pct) {
    if (opacity_pct <   1) opacity_pct =   1;
    if (opacity_pct > 100) opacity_pct = 100;
    uint32_t a = (uint32_t)((opacity_pct * 255 + 50) / 100);
    return (a << 24) | (rgb & 0x00FFFFFF);
}

static void update_title(SDL_Window *w, const char *tool,
                         int radius, uint32_t color, int opacity) {
    char buf[160];
    snprintf(buf, sizeof(buf),
             "Openshop | %s | sz %d | op %d%% | #%06X  "
             "[ B soft=Q spray=A E L R T(fill) O P(fill) "
             "| Ctrl: G=blur H=sharpen M=emboss N=edge "
             "I=invert D=gray V=vignette ]",
             tool, radius, opacity, color & 0xFFFFFF);
    SDL_SetWindowTitle(w, buf);
}

static void constrain_end(Tool tool, int x0, int y0,
                          int x1, int y1, int shift,
                          int *ox, int *oy) {
    *ox = x1; *oy = y1;
    if (!shift) return;
    int dx = x1 - x0, dy = y1 - y0;
    int adx = abs(dx), ady = abs(dy);
    if (tool == TOOL_LINE) {
        if      (adx > ady * 2) { *ox = x0 + (dx >= 0 ? adx : -adx); *oy = y0; }
        else if (ady > adx * 2) { *ox = x0;                           *oy = y0 + (dy >= 0 ? ady : -ady); }
        else { int l = adx > ady ? adx : ady;
               *ox = x0 + (dx >= 0 ? l : -l);
               *oy = y0 + (dy >= 0 ? l : -l); }
    } else {
        int len = adx > ady ? adx : ady;
        *ox = x0 + (dx >= 0 ? len : -len);
        *oy = y0 + (dy >= 0 ? len : -len);
    }
}

static void draw_shape(Canvas *c, Tool tool,
                       int x0, int y0, int x1, int y1,
                       int radius, uint32_t color) {
    switch (tool) {
    case TOOL_LINE:
        canvas_draw_line(c, x0, y0, x1, y1, radius, color);
        break;
    case TOOL_RECT:
        canvas_draw_rect_outline(c, x0, y0, x1, y1, radius, color);
        break;
    case TOOL_RECT_FILL:
        canvas_fill_rect(c, x0, y0, x1, y1, color);
        break;
    case TOOL_ELLIPSE: {
        int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        int rx = abs(x1 - x0) / 2, ry = abs(y1 - y0) / 2;
        canvas_draw_ellipse_outline(c, cx, cy, rx, ry, radius, color);
        break; }
    case TOOL_ELLIPSE_FILL: {
        int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        int rx = abs(x1 - x0) / 2, ry = abs(y1 - y0) / 2;
        canvas_fill_ellipse(c, cx, cy, rx, ry, color);
        break; }
    default: break;
    }
}

static void stroke_at(Canvas *c, Tool tool,
                      int lx, int ly, int x, int y,
                      int radius, uint32_t color) {
    switch (tool) {
    case TOOL_BRUSH:
    case TOOL_ERASER:
        canvas_draw_line(c, lx, ly, x, y, radius, color);
        break;
    case TOOL_SOFT:
        canvas_draw_soft_line(c, lx, ly, x, y, radius, color);
        break;
    case TOOL_SPRAY:
        /* spray is position-based — no line interpolation needed */
        canvas_draw_spray(c, x, y, radius, color, radius * 4);
        break;
    default: break;
    }
}

static int load_bmp(Canvas *c, const char *path) {
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

static int save_bmp(const Canvas *c, const char *path) {
    if (!c || !c->pixels || !path) return 0;
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(
        (void *)c->pixels, c->width, c->height, 32,
        c->width * 4, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return 0;
    int ok = SDL_SaveBMP(s, path) == 0;
    SDL_FreeSurface(s);
    return ok;
}

/* ---------------------------------------------------------------
 * Main application loop
 * --------------------------------------------------------------- */

int app_run(const char *input_path) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Openshop", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit(); return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        /* fallback: software renderer */
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, CANVAS_WIDTH, CANVAS_HEIGHT);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
        SDL_Quit(); return 1;
    }

    Canvas canvas;
    if (!canvas_init(&canvas, CANVAS_WIDTH, CANVAS_HEIGHT)) {
        fprintf(stderr, "canvas_init failed\n");
        SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
    canvas_clear(&canvas, COLOR_BG);
    if (input_path && input_path[0]) load_bmp(&canvas, input_path);

    /* ---- state ---- */
    int      running      = 1;
    int      drawing      = 0;
    int      last_x       = 0, last_y = 0;
    int      brush_radius = 6;
    int      brush_opacity= 100;
    uint32_t brush_rgb    = COLOR_BLACK & 0x00FFFFFF;
    uint32_t brush_color  = compose_color(brush_rgb, brush_opacity);
    Tool     tool         = TOOL_BRUSH;

    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int      undo_count = 0, redo_count = 0;
    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));

    /* shape preview */
    Snapshot  shape_base   = {0};
    int       shaping      = 0;
    int       shape_sx     = 0, shape_sy = 0;
    uint32_t *preview_buf  = NULL;
    Canvas    preview_cv   = {0};
    int       preview_on   = 0;

    /* fps counter */
    Uint32 fps_last  = SDL_GetTicks();
    int    fps_frames= 0;
    int    fps_shown = 0;

    update_title(window, tool_label(tool), brush_radius, brush_color, brush_opacity);

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {

            /* ---- quit ---- */
            case SDL_QUIT: running = 0; break;

            /* ---- mouse button ---- */
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    push_snapshot(&canvas, undo_stack, &undo_count,
                                  redo_stack, &redo_count);
                    last_x = e.button.x;
                    last_y = e.button.y;
                    if (tool_is_freehand(tool)) {
                        drawing = 1;
                        if (tool == TOOL_SPRAY)
                            canvas_draw_spray(&canvas, last_x, last_y,
                                              brush_radius, brush_color,
                                              brush_radius * 4);
                        else if (tool == TOOL_SOFT)
                            canvas_draw_soft_circle(&canvas, last_x, last_y,
                                                    brush_radius, brush_color);
                        else
                            canvas_draw_circle(&canvas, last_x, last_y,
                                               brush_radius, brush_color);
                    } else {
                        shaping  = 1;
                        shape_sx = last_x;
                        shape_sy = last_y;
                        snapshot_free(&shape_base);
                        snapshot_from_canvas(&shape_base, &canvas);
                    }
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    /* eyedropper */
                    int mx = e.button.x, my = e.button.y;
                    if (mx >= 0 && my >= 0 && mx < CANVAS_WIDTH && my < CANVAS_HEIGHT) {
                        uint32_t picked = canvas_get_pixel(&canvas, mx, my);
                        brush_rgb = picked & 0x00FFFFFF;
                        int pa = (int)((picked >> 24) & 0xFF);
                        brush_opacity = (pa * 100 + 127) / 255;
                        if (brush_opacity < 1) brush_opacity = 1;
                        brush_color = compose_color(brush_rgb, brush_opacity);
                        tool = TOOL_BRUSH;
                        update_title(window, tool_label(tool),
                                     brush_radius, brush_color, brush_opacity);
                    }
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    drawing = 0;
                    if (shaping) {
                        const Uint8 *ks = SDL_GetKeyboardState(NULL);
                        int shift = ks[SDL_SCANCODE_LSHIFT]||ks[SDL_SCANCODE_RSHIFT];
                        int ex = e.button.x, ey = e.button.y;
                        constrain_end(tool, shape_sx, shape_sy,
                                      ex, ey, shift, &ex, &ey);
                        draw_shape(&canvas, tool,
                                   shape_sx, shape_sy, ex, ey,
                                   brush_radius, brush_color);
                        shaping = preview_on = 0;
                    }
                }
                break;

            /* ---- mouse motion ---- */
            case SDL_MOUSEMOTION:
                if (drawing) {
                    int x = e.motion.x, y = e.motion.y;
                    if (x >= 0 && y >= 0 && x < CANVAS_WIDTH && y < CANVAS_HEIGHT) {
                        stroke_at(&canvas, tool,
                                  last_x, last_y, x, y,
                                  brush_radius, brush_color);
                        last_x = x; last_y = y;
                    }
                } else if (shaping && shape_base.pixels) {
                    int x = e.motion.x, y = e.motion.y;
                    if (x < 0 || y < 0 || x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT)
                        break;
                    const Uint8 *ks = SDL_GetKeyboardState(NULL);
                    int shift = ks[SDL_SCANCODE_LSHIFT]||ks[SDL_SCANCODE_RSHIFT];
                    int ex = x, ey = y;
                    constrain_end(tool, shape_sx, shape_sy,
                                  ex, ey, shift, &ex, &ey);
                    if (!preview_buf) {
                        preview_buf = (uint32_t *)malloc(
                            (size_t)canvas.width * (size_t)canvas.height
                            * sizeof(uint32_t));
                        if (!preview_buf) break;
                        preview_cv.width  = canvas.width;
                        preview_cv.height = canvas.height;
                        preview_cv.pixels = preview_buf;
                    }
                    memcpy(preview_buf, shape_base.pixels,
                           (size_t)canvas.width * (size_t)canvas.height
                           * sizeof(uint32_t));
                    draw_shape(&preview_cv, tool,
                               shape_sx, shape_sy, ex, ey,
                               brush_radius, brush_color);
                    preview_on = 1;
                }
                break;

            /* ---- keyboard ---- */
            case SDL_KEYDOWN: {
                SDL_Keycode key = e.key.keysym.sym;
                const Uint8 *ks = SDL_GetKeyboardState(NULL);
                int ctrl  = ks[SDL_SCANCODE_LCTRL] || ks[SDL_SCANCODE_RCTRL];

                /* --- quit --- */
                if (key == SDLK_ESCAPE) { running = 0; break; }

                /* --- tools --- */
                if (!ctrl) {
                    switch (key) {
                    case SDLK_b:
                        brush_rgb = COLOR_BLACK & 0x00FFFFFF;
                        brush_color = compose_color(brush_rgb, brush_opacity);
                        tool = TOOL_BRUSH; break;
                    case SDLK_q:
                        tool = TOOL_SOFT; break;
                    case SDLK_a:
                        tool = TOOL_SPRAY; break;
                    case SDLK_e:
                        brush_rgb = COLOR_WHITE & 0x00FFFFFF;
                        brush_color = compose_color(brush_rgb, brush_opacity);
                        tool = TOOL_ERASER; break;
                    case SDLK_l: tool = TOOL_LINE;         break;
                    case SDLK_r: tool = TOOL_RECT;         break;
                    case SDLK_t: tool = TOOL_RECT_FILL;    break;
                    case SDLK_o: tool = TOOL_ELLIPSE;      break;
                    case SDLK_p: tool = TOOL_ELLIPSE_FILL; break;

                    /* --- brush size --- */
                    case SDLK_LEFTBRACKET:
                        if (brush_radius >   1) brush_radius--;  break;
                    case SDLK_RIGHTBRACKET:
                        if (brush_radius < 128) brush_radius++;  break;

                    /* --- opacity --- */
                    case SDLK_MINUS: case SDLK_KP_MINUS:
                        brush_opacity -= 5;
                        if (brush_opacity < 1) brush_opacity = 1;
                        brush_color = compose_color(brush_rgb, brush_opacity); break;
                    case SDLK_EQUALS: case SDLK_KP_PLUS:
                        brush_opacity += 5;
                        if (brush_opacity > 100) brush_opacity = 100;
                        brush_color = compose_color(brush_rgb, brush_opacity); break;

                    /* --- quick colours: 1-0 --- */
                    case SDLK_1: brush_rgb = COLOR_BLACK  & 0xFFFFFF; tool=TOOL_BRUSH; break;
                    case SDLK_2: brush_rgb = COLOR_RED    & 0xFFFFFF; tool=TOOL_BRUSH; break;
                    case SDLK_3: brush_rgb = COLOR_GREEN  & 0xFFFFFF; tool=TOOL_BRUSH; break;
                    case SDLK_4: brush_rgb = COLOR_BLUE   & 0xFFFFFF; tool=TOOL_BRUSH; break;
                    case SDLK_5: brush_rgb = COLOR_YELLOW & 0xFFFFFF; tool=TOOL_BRUSH; break;
                    case SDLK_6: brush_rgb = COLOR_PURPLE & 0xFFFFFF; tool=TOOL_BRUSH; break;
                    case SDLK_7: brush_rgb = COLOR_ORANGE & 0xFFFFFF; tool=TOOL_BRUSH; break;
                    case SDLK_8: brush_rgb = COLOR_CYAN   & 0xFFFFFF; tool=TOOL_BRUSH; break;
                    case SDLK_9: brush_rgb = COLOR_WHITE  & 0xFFFFFF; tool=TOOL_BRUSH; break;

                    /* --- clear --- */
                    case SDLK_c:
                        push_snapshot(&canvas, undo_stack, &undo_count,
                                      redo_stack, &redo_count);
                        canvas_clear(&canvas, COLOR_BG); break;

                    /* --- flood fill --- */
                    case SDLK_f: {
                        int mx=0, my=0;
                        SDL_GetMouseState(&mx, &my);
                        if (mx>=0&&my>=0&&mx<CANVAS_WIDTH&&my<CANVAS_HEIGHT) {
                            push_snapshot(&canvas, undo_stack, &undo_count,
                                          redo_stack, &redo_count);
                            canvas_flood_fill(&canvas, mx, my, brush_color);
                        }
                        break; }

                    /* --- eyedropper --- */
                    case SDLK_i: {
                        int mx=0, my=0;
                        SDL_GetMouseState(&mx, &my);
                        if (mx>=0&&my>=0&&mx<CANVAS_WIDTH&&my<CANVAS_HEIGHT) {
                            uint32_t picked = canvas_get_pixel(&canvas, mx, my);
                            brush_rgb = picked & 0x00FFFFFF;
                            int pa = (int)((picked>>24)&0xFF);
                            brush_opacity = (pa*100+127)/255;
                            if (brush_opacity < 1) brush_opacity = 1;
                            brush_color = compose_color(brush_rgb, brush_opacity);
                            tool = TOOL_BRUSH;
                        }
                        break; }

                    default: break;
                    }
                    /* recompose in case palette key changed rgb */
                    brush_color = compose_color(brush_rgb, brush_opacity);
                    update_title(window, tool_label(tool),
                                 brush_radius, brush_color, brush_opacity);
                }

                /* --- Ctrl shortcuts --- */
                if (ctrl) {
                    switch (key) {
                    /* file */
                    case SDLK_s:
                        if (!save_bmp(&canvas, "output.bmp"))
                            fprintf(stderr, "save failed\n");
                        break;
                    case SDLK_o:
                        push_snapshot(&canvas, undo_stack, &undo_count,
                                      redo_stack, &redo_count);
                        if (!load_bmp(&canvas, "input.bmp"))
                            fprintf(stderr, "load failed\n");
                        break;
                    /* undo / redo */
                    case SDLK_z:
                        if (undo_count > 0) {
                            Snapshot cur = {0};
                            if (snapshot_from_canvas(&cur, &canvas)) {
                                if (redo_count == MAX_HISTORY) {
                                    snapshot_free(&redo_stack[0]);
                                    memmove(&redo_stack[0],&redo_stack[1],
                                            sizeof(Snapshot)*(MAX_HISTORY-1));
                                    redo_count = MAX_HISTORY - 1;
                                }
                                redo_stack[redo_count++] = cur;
                            }
                            Snapshot prev = undo_stack[--undo_count];
                            snapshot_apply(&prev, &canvas);
                            snapshot_free(&prev);
                        }
                        break;
                    case SDLK_y:
                        if (redo_count > 0) {
                            Snapshot cur = {0};
                            if (snapshot_from_canvas(&cur, &canvas)) {
                                if (undo_count == MAX_HISTORY) {
                                    snapshot_free(&undo_stack[0]);
                                    memmove(&undo_stack[0],&undo_stack[1],
                                            sizeof(Snapshot)*(MAX_HISTORY-1));
                                    undo_count = MAX_HISTORY - 1;
                                }
                                undo_stack[undo_count++] = cur;
                            }
                            Snapshot nxt = redo_stack[--redo_count];
                            snapshot_apply(&nxt, &canvas);
                            snapshot_free(&nxt);
                        }
                        break;
                    /* --- filters --- */
                    case SDLK_g:  /* Ctrl+G  gaussian blur   */
                        push_snapshot(&canvas,undo_stack,&undo_count,redo_stack,&redo_count);
                        filter_gaussian_blur(&canvas); break;
                    case SDLK_h:  /* Ctrl+H  sharpen         */
                        push_snapshot(&canvas,undo_stack,&undo_count,redo_stack,&redo_count);
                        filter_sharpen(&canvas); break;
                    case SDLK_m:  /* Ctrl+M  emboss          */
                        push_snapshot(&canvas,undo_stack,&undo_count,redo_stack,&redo_count);
                        filter_emboss(&canvas); break;
                    case SDLK_n:  /* Ctrl+N  edge detect     */
                        push_snapshot(&canvas,undo_stack,&undo_count,redo_stack,&redo_count);
                        filter_edge_detect(&canvas); break;
                    case SDLK_i:  /* Ctrl+I  invert          */
                        push_snapshot(&canvas,undo_stack,&undo_count,redo_stack,&redo_count);
                        filter_invert(&canvas); break;
                    case SDLK_d:  /* Ctrl+D  grayscale       */
                        push_snapshot(&canvas,undo_stack,&undo_count,redo_stack,&redo_count);
                        filter_grayscale(&canvas); break;
                    case SDLK_v:  /* Ctrl+V  vignette        */
                        push_snapshot(&canvas,undo_stack,&undo_count,redo_stack,&redo_count);
                        filter_vignette(&canvas, 0.8f); break;
                    case SDLK_j:  /* Ctrl+J  box blur        */
                        push_snapshot(&canvas,undo_stack,&undo_count,redo_stack,&redo_count);
                        filter_box_blur(&canvas); break;
                    case SDLK_k:  /* Ctrl+K  pixelate x8     */
                        push_snapshot(&canvas,undo_stack,&undo_count,redo_stack,&redo_count);
                        filter_pixelate(&canvas, 8); break;
                    case SDLK_u:  /* Ctrl+U  posterize 4lvl  */
                        push_snapshot(&canvas,undo_stack,&undo_count,redo_stack,&redo_count);
                        filter_posterize(&canvas, 4); break;
                    default: break;
                    }
                }
                break;
            } /* SDL_KEYDOWN */

            default: break;
            } /* switch event */
        } /* poll events */

        /* ---- render ---- */
        const uint32_t *src = (preview_on && preview_buf)
                              ? preview_buf : canvas.pixels;
        SDL_UpdateTexture(texture, NULL, src, CANVAS_WIDTH * 4);

        SDL_SetRenderDrawColor(renderer, 30, 30, 34, 255);
        SDL_RenderClear(renderer);

        SDL_Rect dest = {0, 0, CANVAS_WIDTH, CANVAS_HEIGHT};
        SDL_RenderCopy(renderer, texture, NULL, &dest);
        SDL_RenderPresent(renderer);

        /* fps counter (updates title suffix every second) */
        fps_frames++;
        Uint32 now = SDL_GetTicks();
        if (now - fps_last >= 1000) {
            fps_shown = fps_frames;
            fps_frames = 0;
            fps_last = now;
            char title[192];
            snprintf(title, sizeof(title),
                     "Openshop | %s | sz %d | op %d%% | #%06X | %d fps",
                     tool_label(tool), brush_radius, brush_opacity,
                     brush_color & 0xFFFFFF, fps_shown);
            SDL_SetWindowTitle(window, title);
        }

    } /* main loop */

    canvas_free(&canvas);
    stack_clear(undo_stack, &undo_count);
    stack_clear(redo_stack, &redo_count);
    snapshot_free(&shape_base);
    free(preview_buf);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
