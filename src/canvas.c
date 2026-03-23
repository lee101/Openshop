#include "canvas.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint8_t blend_channel(uint8_t src, uint8_t dst, uint8_t src_alpha) {
    int inv = 255 - src_alpha;
    int value = src * src_alpha + dst * inv + 127;
    return (uint8_t)(value / 255);
}

int canvas_init(Canvas *c, int width, int height) {
    if (!c || width <= 0 || height <= 0) {
        return 0;
    }
    c->width = width;
    c->height = height;
    c->pixels = (uint32_t *)calloc((size_t)width * (size_t)height, sizeof(uint32_t));
    if (!c->pixels) {
        return 0;
    }
    return 1;
}

void canvas_free(Canvas *c) {
    if (!c) {
        return;
    }
    free(c->pixels);
    c->pixels = NULL;
    c->width = 0;
    c->height = 0;
}

void canvas_clear(Canvas *c, uint32_t color) {
    if (!c || !c->pixels) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        c->pixels[i] = color;
    }
}

void canvas_set_pixel_raw(Canvas *c, int x, int y, uint32_t color) {
    if (!c || !c->pixels) {
        return;
    }
    if (x < 0 || y < 0 || x >= c->width || y >= c->height) {
        return;
    }
    c->pixels[(size_t)y * (size_t)c->width + (size_t)x] = color;
}

void canvas_set_pixel(Canvas *c, int x, int y, uint32_t color) {
    if (!c || !c->pixels) {
        return;
    }
    if (x < 0 || y < 0 || x >= c->width || y >= c->height) {
        return;
    }
    uint8_t sa = (uint8_t)((color >> 24) & 0xFF);
    if (sa == 0) {
        return;
    }
    size_t idx = (size_t)y * (size_t)c->width + (size_t)x;
    if (sa == 255) {
        c->pixels[idx] = color;
        return;
    }

    uint32_t dst = c->pixels[idx];
    uint8_t sr = (uint8_t)((color >> 16) & 0xFF);
    uint8_t sg = (uint8_t)((color >> 8) & 0xFF);
    uint8_t sb = (uint8_t)(color & 0xFF);
    uint8_t dr = (uint8_t)((dst >> 16) & 0xFF);
    uint8_t dg = (uint8_t)((dst >> 8) & 0xFF);
    uint8_t db = (uint8_t)(dst & 0xFF);
    uint8_t da = (uint8_t)((dst >> 24) & 0xFF);

    uint8_t out_r = blend_channel(sr, dr, sa);
    uint8_t out_g = blend_channel(sg, dg, sa);
    uint8_t out_b = blend_channel(sb, db, sa);
    uint8_t out_a = (uint8_t)(sa + ((da * (255 - sa) + 127) / 255));

    c->pixels[idx] = ((uint32_t)out_a << 24) | ((uint32_t)out_r << 16) | ((uint32_t)out_g << 8) | out_b;
}

uint32_t canvas_get_pixel(const Canvas *c, int x, int y) {
    if (!c || !c->pixels) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= c->width || y >= c->height) {
        return 0;
    }
    return c->pixels[y * c->width + x];
}

void canvas_draw_circle(Canvas *c, int cx, int cy, int radius, uint32_t color) {
    if (!c || !c->pixels || radius <= 0) {
        return;
    }
    int r2 = radius * radius;
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= r2) {
                canvas_set_pixel(c, cx + x, cy + y, color);
            }
        }
    }
}

void canvas_draw_line(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t color) {
    if (!c || !c->pixels) {
        return;
    }
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        canvas_draw_circle(c, x0, y0, radius, color);
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

void canvas_draw_rect_outline(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t color) {
    if (!c || !c->pixels) {
        return;
    }
    int left = x0 < x1 ? x0 : x1;
    int right = x0 < x1 ? x1 : x0;
    int top = y0 < y1 ? y0 : y1;
    int bottom = y0 < y1 ? y1 : y0;

    canvas_draw_line(c, left, top, right, top, radius, color);
    canvas_draw_line(c, right, top, right, bottom, radius, color);
    canvas_draw_line(c, right, bottom, left, bottom, radius, color);
    canvas_draw_line(c, left, bottom, left, top, radius, color);
}

void canvas_draw_rect_filled(Canvas *c, int x0, int y0, int x1, int y1, uint32_t color) {
    if (!c || !c->pixels) {
        return;
    }
    int left = x0 < x1 ? x0 : x1;
    int right = x0 < x1 ? x1 : x0;
    int top = y0 < y1 ? y0 : y1;
    int bottom = y0 < y1 ? y1 : y0;

    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            canvas_set_pixel(c, x, y, color);
        }
    }
}

void canvas_draw_ellipse_outline(Canvas *c, int cx, int cy, int rx, int ry, int radius, uint32_t color) {
    if (!c || !c->pixels || rx <= 0 || ry <= 0) {
        return;
    }
    for (int y = -ry; y <= ry; y++) {
        double norm = 1.0 - ((double)(y * y) / (double)(ry * ry));
        if (norm < 0.0) {
            continue;
        }
        int x = (int)((double)rx * sqrt(norm) + 0.5);
        canvas_draw_circle(c, cx + x, cy + y, radius, color);
        canvas_draw_circle(c, cx - x, cy + y, radius, color);
    }
}

void canvas_draw_ellipse_filled(Canvas *c, int cx, int cy, int rx, int ry, uint32_t color) {
    if (!c || !c->pixels || rx <= 0 || ry <= 0) {
        return;
    }
    for (int y = -ry; y <= ry; y++) {
        double norm = 1.0 - ((double)(y * y) / (double)(ry * ry));
        if (norm < 0.0) {
            continue;
        }
        int x = (int)((double)rx * sqrt(norm) + 0.5);
        for (int fill_x = -x; fill_x <= x; fill_x++) {
            canvas_set_pixel(c, cx + fill_x, cy + y, color);
        }
    }
}

typedef struct {
    int x;
    int y;
} FillPoint;

int canvas_flood_fill(Canvas *c, int x, int y, uint32_t new_color) {
    if (!c || !c->pixels) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= c->width || y >= c->height) {
        return 0;
    }
    uint32_t target = canvas_get_pixel(c, x, y);
    if (target == new_color) {
        return 1;
    }

    size_t capacity = 1024;
    size_t count = 0;
    FillPoint *stack = (FillPoint *)malloc(capacity * sizeof(FillPoint));
    if (!stack) {
        return 0;
    }

    stack[count++] = (FillPoint){x, y};
    while (count > 0) {
        FillPoint p = stack[--count];
        if (p.x < 0 || p.y < 0 || p.x >= c->width || p.y >= c->height) {
            continue;
        }
        if (canvas_get_pixel(c, p.x, p.y) != target) {
            continue;
        }
        canvas_set_pixel(c, p.x, p.y, new_color);

        if (count + 4 >= capacity) {
            size_t new_capacity = capacity * 2;
            FillPoint *next = (FillPoint *)realloc(stack, new_capacity * sizeof(FillPoint));
            if (!next) {
                free(stack);
                return 0;
            }
            stack = next;
            capacity = new_capacity;
        }

        stack[count++] = (FillPoint){p.x + 1, p.y};
        stack[count++] = (FillPoint){p.x - 1, p.y};
        stack[count++] = (FillPoint){p.x, p.y + 1};
        stack[count++] = (FillPoint){p.x, p.y - 1};
    }

    free(stack);
    return 1;
}

static int color_within_tolerance(uint32_t a, uint32_t b, int tol) {
    int dr = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
    int dg = (int)((a >>  8) & 0xFF) - (int)((b >>  8) & 0xFF);
    int db = (int)( a        & 0xFF) - (int)( b        & 0xFF);
    int da = (int)((a >> 24) & 0xFF) - (int)((b >> 24) & 0xFF);
    return dr*dr + dg*dg + db*db + da*da <= tol * tol * 4;
}

int canvas_flood_fill_tol(Canvas *c, int x, int y, uint32_t new_color, int tolerance) {
    if (!c || !c->pixels) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= c->width || y >= c->height) {
        return 0;
    }
    if (tolerance <= 0) {
        return canvas_flood_fill(c, x, y, new_color);
    }
    uint32_t target = canvas_get_pixel(c, x, y);
    if (target == new_color) {
        return 1;
    }

    /* Visited bitmap to avoid re-enqueueing already-set pixels. */
    size_t total = (size_t)c->width * (size_t)c->height;
    uint8_t *visited = (uint8_t *)calloc(total, 1);
    if (!visited) {
        return 0;
    }

    size_t capacity = 1024;
    size_t count = 0;
    FillPoint *stk = (FillPoint *)malloc(capacity * sizeof(FillPoint));
    if (!stk) {
        free(visited);
        return 0;
    }

    stk[count++] = (FillPoint){x, y};
    visited[(size_t)y * (size_t)c->width + (size_t)x] = 1;

    while (count > 0) {
        FillPoint p = stk[--count];
        if (p.x < 0 || p.y < 0 || p.x >= c->width || p.y >= c->height) {
            continue;
        }
        uint32_t cur = canvas_get_pixel(c, p.x, p.y);
        if (!color_within_tolerance(cur, target, tolerance)) {
            continue;
        }
        canvas_set_pixel_raw(c, p.x, p.y, new_color);

        if (count + 4 >= capacity) {
            size_t new_cap = capacity * 2;
            FillPoint *next = (FillPoint *)realloc(stk, new_cap * sizeof(FillPoint));
            if (!next) {
                free(stk);
                free(visited);
                return 0;
            }
            stk = next;
            capacity = new_cap;
        }

        int nx, ny;
        FillPoint neighbours[4] = {
            {p.x+1, p.y}, {p.x-1, p.y}, {p.x, p.y+1}, {p.x, p.y-1}
        };
        for (int i = 0; i < 4; i++) {
            nx = neighbours[i].x;
            ny = neighbours[i].y;
            if (nx < 0 || ny < 0 || nx >= c->width || ny >= c->height) continue;
            size_t idx = (size_t)ny * (size_t)c->width + (size_t)nx;
            if (!visited[idx]) {
                visited[idx] = 1;
                stk[count++] = neighbours[i];
            }
        }
    }

    free(stk);
    free(visited);
    return 1;
}

void canvas_flip_horizontal(Canvas *c) {
    if (!c || !c->pixels || c->width <= 1 || c->height <= 0) {
        return;
    }
    for (int y = 0; y < c->height; y++) {
        uint32_t *row = c->pixels + (size_t)y * (size_t)c->width;
        int left = 0;
        int right = c->width - 1;
        while (left < right) {
            uint32_t tmp = row[left];
            row[left] = row[right];
            row[right] = tmp;
            left++;
            right--;
        }
    }
}

void canvas_flip_vertical(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 1) {
        return;
    }
    size_t row_bytes = (size_t)c->width * sizeof(uint32_t);
    uint32_t *tmp = (uint32_t *)malloc(row_bytes);
    if (!tmp) {
        return;
    }
    int top = 0;
    int bottom = c->height - 1;
    while (top < bottom) {
        uint32_t *top_row = c->pixels + (size_t)top * (size_t)c->width;
        uint32_t *bottom_row = c->pixels + (size_t)bottom * (size_t)c->width;
        memcpy(tmp, top_row, row_bytes);
        memcpy(top_row, bottom_row, row_bytes);
        memcpy(bottom_row, tmp, row_bytes);
        top++;
        bottom--;
    }
    free(tmp);
}

void canvas_rotate_180(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count / 2; i++) {
        size_t opposite = count - 1 - i;
        uint32_t tmp = c->pixels[i];
        c->pixels[i] = c->pixels[opposite];
        c->pixels[opposite] = tmp;
    }
}

void canvas_invert_rgb(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint32_t a = p & 0xFF000000;
        uint32_t rgb = p & 0x00FFFFFF;
        c->pixels[i] = a | ((~rgb) & 0x00FFFFFF);
    }
}

void canvas_translate(Canvas *c, int dx, int dy, uint32_t fill_color) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    if (dx == 0 && dy == 0) {
        return;
    }

    size_t count = (size_t)c->width * (size_t)c->height;
    uint32_t *copy = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!copy) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        copy[i] = fill_color;
    }

    for (int y = 0; y < c->height; y++) {
        int src_y = y - dy;
        if (src_y < 0 || src_y >= c->height) {
            continue;
        }
        for (int x = 0; x < c->width; x++) {
            int src_x = x - dx;
            if (src_x < 0 || src_x >= c->width) {
                continue;
            }
            copy[(size_t)y * (size_t)c->width + (size_t)x] =
                c->pixels[(size_t)src_y * (size_t)c->width + (size_t)src_x];
        }
    }

    memcpy(c->pixels, copy, count * sizeof(uint32_t));
    free(copy);
}

void canvas_rotate_90_cw(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    int W = c->width;
    int H = c->height;
    size_t count = (size_t)W * (size_t)H;
    uint32_t *copy = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!copy) {
        return;
    }
    /* For a true 90° CW rotation the output is H wide and W tall.
       We keep the same W×H frame: dest(dx,dy) <- src(dy, H-1-dx).
       Pixels outside the rotated image bounds are set transparent. */
    for (int dy = 0; dy < H; dy++) {
        for (int dx = 0; dx < W; dx++) {
            if (dx < H && dy < W) {
                int src_x = dy;
                int src_y = H - 1 - dx;
                copy[(size_t)dy * (size_t)W + (size_t)dx] =
                    c->pixels[(size_t)src_y * (size_t)W + (size_t)src_x];
            } else {
                copy[(size_t)dy * (size_t)W + (size_t)dx] = 0;
            }
        }
    }
    memcpy(c->pixels, copy, count * sizeof(uint32_t));
    free(copy);
}

void canvas_rotate_90_ccw(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    int W = c->width;
    int H = c->height;
    size_t count = (size_t)W * (size_t)H;
    uint32_t *copy = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!copy) {
        return;
    }
    /* For a true 90° CCW rotation the output is H wide and W tall.
       We keep the same W×H frame: dest(dx,dy) <- src(W-1-dy, dx).
       Pixels outside the rotated image bounds are set transparent. */
    for (int dy = 0; dy < H; dy++) {
        for (int dx = 0; dx < W; dx++) {
            if (dx < H && dy < W) {
                int src_x = W - 1 - dy;
                int src_y = dx;
                copy[(size_t)dy * (size_t)W + (size_t)dx] =
                    c->pixels[(size_t)src_y * (size_t)W + (size_t)src_x];
            } else {
                copy[(size_t)dy * (size_t)W + (size_t)dx] = 0;
            }
        }
    }
    memcpy(c->pixels, copy, count * sizeof(uint32_t));
    free(copy);
}

void canvas_grayscale(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        uint8_t r = (uint8_t)((p >> 16) & 0xFF);
        uint8_t g = (uint8_t)((p >> 8) & 0xFF);
        uint8_t b = (uint8_t)(p & 0xFF);
        /* Rec. 601 luminance weights: 0.299*R + 0.587*G + 0.114*B */
        uint8_t gray = (uint8_t)((77 * (unsigned)r + 150 * (unsigned)g + 29 * (unsigned)b) >> 8);
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)gray << 16) | ((uint32_t)gray << 8) | (uint32_t)gray;
    }
}

void canvas_sepia(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        uint8_t r = (uint8_t)((p >> 16) & 0xFF);
        uint8_t g = (uint8_t)((p >> 8) & 0xFF);
        uint8_t b = (uint8_t)(p & 0xFF);
        /* Standard sepia tone matrix (scaled by 128 to avoid floats). */
        int out_r = ((int)r * 50 + (int)g * 98 + (int)b * 24) >> 7;
        int out_g = ((int)r * 45 + (int)g * 88 + (int)b * 22) >> 7;
        int out_b = ((int)r * 35 + (int)g * 68 + (int)b * 17) >> 7;
        if (out_r > 255) out_r = 255;
        if (out_g > 255) out_g = 255;
        if (out_b > 255) out_b = 255;
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)out_r << 16) |
                       ((uint32_t)out_g << 8) | (uint32_t)out_b;
    }
}

static int clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

static uint8_t pixel_luma(uint32_t p) {
    uint8_t a = (uint8_t)((p >> 24) & 0xFF);
    uint8_t r = (uint8_t)((p >> 16) & 0xFF);
    uint8_t g = (uint8_t)((p >> 8) & 0xFF);
    uint8_t b = (uint8_t)(p & 0xFF);
    int lum = (77 * (int)r + 150 * (int)g + 29 * (int)b) >> 8;
    return (uint8_t)((lum * (int)a + 127) / 255);
}

void canvas_adjust_brightness(Canvas *c, int delta) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0 || delta == 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        int r = clamp_u8((int)((p >> 16) & 0xFF) + delta);
        int g = clamp_u8((int)((p >> 8) & 0xFF) + delta);
        int b = clamp_u8((int)(p & 0xFF) + delta);
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

void canvas_adjust_contrast(Canvas *c, int delta) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0 || delta == 0) {
        return;
    }
    /* factor_percent = 100 + delta: >100 increases contrast, <100 reduces it. */
    int factor = 100 + delta;
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        int r = clamp_u8(((int)((p >> 16) & 0xFF) - 128) * factor / 100 + 128);
        int g = clamp_u8(((int)((p >> 8) & 0xFF) - 128) * factor / 100 + 128);
        int b = clamp_u8(((int)(p & 0xFF) - 128) * factor / 100 + 128);
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

void canvas_posterize(Canvas *c, int levels) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    if (levels < 2) levels = 2;
    if (levels > 255) levels = 255;
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        uint8_t r = (uint8_t)((p >> 16) & 0xFF);
        uint8_t g = (uint8_t)((p >> 8) & 0xFF);
        uint8_t b = (uint8_t)(p & 0xFF);
        /* Quantise each channel to `levels` distinct values. */
        r = (uint8_t)((r * levels / 256) * 255 / (levels - 1));
        g = (uint8_t)((g * levels / 256) * 255 / (levels - 1));
        b = (uint8_t)((b * levels / 256) * 255 / (levels - 1));
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

void canvas_threshold(Canvas *c, uint8_t thresh) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        uint8_t r = (uint8_t)((p >> 16) & 0xFF);
        uint8_t g = (uint8_t)((p >> 8) & 0xFF);
        uint8_t b = (uint8_t)(p & 0xFF);
        /* Luminance-based threshold: pixel becomes black or white. */
        uint8_t lum = (uint8_t)((77 * (unsigned)r + 150 * (unsigned)g + 29 * (unsigned)b) >> 8);
        uint8_t out = (lum >= thresh) ? 0xFF : 0x00;
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)out << 16) | ((uint32_t)out << 8) | (uint32_t)out;
    }
}

void canvas_auto_levels(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }

    uint8_t min_r = 255;
    uint8_t min_g = 255;
    uint8_t min_b = 255;
    uint8_t max_r = 0;
    uint8_t max_g = 0;
    uint8_t max_b = 0;
    size_t count = (size_t)c->width * (size_t)c->height;

    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t r = (uint8_t)((p >> 16) & 0xFF);
        uint8_t g = (uint8_t)((p >> 8) & 0xFF);
        uint8_t b = (uint8_t)(p & 0xFF);
        if (r < min_r) min_r = r;
        if (g < min_g) min_g = g;
        if (b < min_b) min_b = b;
        if (r > max_r) max_r = r;
        if (g > max_g) max_g = g;
        if (b > max_b) max_b = b;
    }

    int range_r = (int)max_r - (int)min_r;
    int range_g = (int)max_g - (int)min_g;
    int range_b = (int)max_b - (int)min_b;

    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        int r = (int)((p >> 16) & 0xFF);
        int g = (int)((p >> 8) & 0xFF);
        int b = (int)(p & 0xFF);

        if (range_r > 0) {
            r = ((r - (int)min_r) * 255 + range_r / 2) / range_r;
        }
        if (range_g > 0) {
            g = ((g - (int)min_g) * 255 + range_g / 2) / range_g;
        }
        if (range_b > 0) {
            b = ((b - (int)min_b) * 255 + range_b / 2) / range_b;
        }

        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)clamp_u8(r) << 16) |
                       ((uint32_t)clamp_u8(g) << 8) | (uint32_t)clamp_u8(b);
    }
}

void canvas_blur(Canvas *c, int radius) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0 || radius <= 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    uint32_t *copy = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!copy) {
        return;
    }
    int W = c->width;
    int H = c->height;
    for (int y = 0; y < H; y++) {
        int y0 = y - radius < 0 ? 0 : y - radius;
        int y1 = y + radius >= H ? H - 1 : y + radius;
        for (int x = 0; x < W; x++) {
            int x0 = x - radius < 0 ? 0 : x - radius;
            int x1 = x + radius >= W ? W - 1 : x + radius;
            unsigned long sum_a = 0, sum_r = 0, sum_g = 0, sum_b = 0;
            int n = 0;
            for (int sy = y0; sy <= y1; sy++) {
                for (int sx = x0; sx <= x1; sx++) {
                    uint32_t p = c->pixels[(size_t)sy * (size_t)W + (size_t)sx];
                    sum_a += (p >> 24) & 0xFF;
                    sum_r += (p >> 16) & 0xFF;
                    sum_g += (p >> 8) & 0xFF;
                    sum_b += p & 0xFF;
                    n++;
                }
            }
            copy[(size_t)y * (size_t)W + (size_t)x] =
                (uint32_t)((sum_a / (unsigned)n) << 24) |
                (uint32_t)((sum_r / (unsigned)n) << 16) |
                (uint32_t)((sum_g / (unsigned)n) << 8) |
                (uint32_t)(sum_b / (unsigned)n);
        }
    }
    memcpy(c->pixels, copy, count * sizeof(uint32_t));
    free(copy);
}

void canvas_sharpen(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    uint32_t *orig = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!orig) {
        return;
    }
    memcpy(orig, c->pixels, count * sizeof(uint32_t));

    canvas_blur(c, 1);

    for (size_t i = 0; i < count; i++) {
        uint32_t original = orig[i];
        uint32_t blurred = c->pixels[i];
        uint8_t a = (uint8_t)((original >> 24) & 0xFF);
        int r = clamp_u8(2 * (int)((original >> 16) & 0xFF) - (int)((blurred >> 16) & 0xFF));
        int g = clamp_u8(2 * (int)((original >> 8) & 0xFF) - (int)((blurred >> 8) & 0xFF));
        int b = clamp_u8(2 * (int)(original & 0xFF) - (int)(blurred & 0xFF));
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }

    free(orig);
}

void canvas_edge_detect(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }

    size_t count = (size_t)c->width * (size_t)c->height;
    uint32_t *orig = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!orig) {
        return;
    }
    memcpy(orig, c->pixels, count * sizeof(uint32_t));

    int width = c->width;
    int height = c->height;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int gx = 0;
            int gy = 0;
            uint8_t max_a = 0;

            for (int ky = -1; ky <= 1; ky++) {
                int sy = y + ky;
                if (sy < 0) {
                    sy = 0;
                } else if (sy >= height) {
                    sy = height - 1;
                }

                for (int kx = -1; kx <= 1; kx++) {
                    int sx = x + kx;
                    if (sx < 0) {
                        sx = 0;
                    } else if (sx >= width) {
                        sx = width - 1;
                    }

                    uint32_t p = orig[(size_t)sy * (size_t)width + (size_t)sx];
                    uint8_t lum = pixel_luma(p);
                    uint8_t a = (uint8_t)((p >> 24) & 0xFF);
                    if (a > max_a) {
                        max_a = a;
                    }

                    int wx = 0;
                    int wy = 0;
                    if (ky == -1) {
                        wy = -1;
                    } else if (ky == 1) {
                        wy = 1;
                    }
                    if (kx == -1) {
                        wx = -1;
                    } else if (kx == 1) {
                        wx = 1;
                    }
                    if (kx == 0 && ky != 0) {
                        wx = 0;
                    }
                    if (ky == 0 && kx != 0) {
                        wy = 0;
                    }
                    if (kx == 0 && ky != 0) {
                        wy *= 2;
                    }
                    if (ky == 0 && kx != 0) {
                        wx *= 2;
                    }

                    gx += wx * (int)lum;
                    gy += wy * (int)lum;
                }
            }

            int magnitude = (abs(gx) + abs(gy)) / 4;
            uint8_t edge = (uint8_t)clamp_u8(magnitude);
            c->pixels[(size_t)y * (size_t)width + (size_t)x] =
                ((uint32_t)max_a << 24) | ((uint32_t)edge << 16) |
                ((uint32_t)edge << 8) | (uint32_t)edge;
        }
    }

    free(orig);
}
