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

void canvas_rotate_90cw(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    int W = c->width;
    int H = c->height;
    size_t count = (size_t)W * (size_t)H;
    uint32_t *copy = (uint32_t *)calloc(count, sizeof(uint32_t));
    if (!copy) {
        return;
    }
    /* Rotated image is H wide × W tall; center it inside the original W×H frame. */
    int x_pad = (W - H) / 2;
    int y_crop = (W - H) / 2;
    for (int dy = 0; dy < H; dy++) {
        for (int dx = 0; dx < W; dx++) {
            int rx = dx - x_pad;
            int ry = dy + y_crop;
            if (rx < 0 || rx >= H || ry < 0 || ry >= W) {
                copy[(size_t)dy * (size_t)W + (size_t)dx] = 0;
                continue;
            }
            int old_y = H - 1 - rx;
            int old_x = ry;
            copy[(size_t)dy * (size_t)W + (size_t)dx] =
                c->pixels[(size_t)old_y * (size_t)W + (size_t)old_x];
        }
    }
    memcpy(c->pixels, copy, count * sizeof(uint32_t));
    free(copy);
}

void canvas_rotate_90ccw(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    int W = c->width;
    int H = c->height;
    size_t count = (size_t)W * (size_t)H;
    uint32_t *copy = (uint32_t *)calloc(count, sizeof(uint32_t));
    if (!copy) {
        return;
    }
    /* Rotated image is H wide × W tall; center it inside the original W×H frame. */
    int x_pad = (W - H) / 2;
    int y_crop = (W - H) / 2;
    for (int dy = 0; dy < H; dy++) {
        for (int dx = 0; dx < W; dx++) {
            int rx = dx - x_pad;
            int ry = dy + y_crop;
            if (rx < 0 || rx >= H || ry < 0 || ry >= W) {
                copy[(size_t)dy * (size_t)W + (size_t)dx] = 0;
                continue;
            }
            int old_x = W - 1 - ry;
            int old_y = rx;
            copy[(size_t)dy * (size_t)W + (size_t)dx] =
                c->pixels[(size_t)old_y * (size_t)W + (size_t)old_x];
        }
    }
    memcpy(c->pixels, copy, count * sizeof(uint32_t));
    free(copy);
}

void canvas_desaturate(Canvas *c) {
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
        /* Luminosity weights: 0.2126 R + 0.7152 G + 0.0722 B */
        uint8_t grey = (uint8_t)((2126 * r + 7152 * g + 722 * b + 5000) / 10000);
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)grey << 16) | ((uint32_t)grey << 8) | grey;
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
        uint32_t r = (p >> 16) & 0xFF;
        uint32_t g = (p >> 8) & 0xFF;
        uint32_t b = p & 0xFF;
        /* Standard sepia matrix (scaled to integer arithmetic) */
        uint32_t nr = (r * 393u + g * 769u + b * 189u) / 1000u;
        uint32_t ng = (r * 349u + g * 686u + b * 168u) / 1000u;
        uint32_t nb = (r * 272u + g * 534u + b * 131u) / 1000u;
        if (nr > 255u) nr = 255u;
        if (ng > 255u) ng = 255u;
        if (nb > 255u) nb = 255u;
        c->pixels[i] = ((uint32_t)a << 24) | (nr << 16) | (ng << 8) | nb;
    }
}

void canvas_posterize(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    static const uint8_t LEVELS = 4;
    static const uint8_t STEP = 64;
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        uint8_t r = (uint8_t)((p >> 16) & 0xFF);
        uint8_t g = (uint8_t)((p >> 8) & 0xFF);
        uint8_t b = (uint8_t)(p & 0xFF);
        uint8_t br = (uint8_t)((r / STEP) * (255u / (LEVELS - 1u)));
        uint8_t bg = (uint8_t)((g / STEP) * (255u / (LEVELS - 1u)));
        uint8_t bb = (uint8_t)((b / STEP) * (255u / (LEVELS - 1u)));
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)br << 16) | ((uint32_t)bg << 8) | bb;
    }
}

void canvas_threshold(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        uint32_t r = (p >> 16) & 0xFF;
        uint32_t g = (p >> 8) & 0xFF;
        uint32_t b = p & 0xFF;
        uint32_t luma = (2126u * r + 7152u * g + 722u * b) / 10000u;
        uint8_t out = luma >= 128u ? 0xFF : 0x00;
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)out << 16) | ((uint32_t)out << 8) | out;
    }
}

void canvas_brightness(Canvas *c, int delta) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0 || delta == 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint32_t a = p & 0xFF000000u;
        if (a == 0) {
            continue;
        }
        int r = (int)((p >> 16) & 0xFF) + delta;
        int g = (int)((p >> 8) & 0xFF) + delta;
        int b = (int)(p & 0xFF) + delta;
        if (r < 0) { r = 0; } else if (r > 255) { r = 255; }
        if (g < 0) { g = 0; } else if (g > 255) { g = 255; }
        if (b < 0) { b = 0; } else if (b > 255) { b = 255; }
        c->pixels[i] = a | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}
static void canvas_contrast_step(Canvas *c, int delta) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    int fp = 128 + delta;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint32_t a = p & 0xFF000000;
        int r = 128 + ((((int)((p >> 16) & 0xFF) - 128) * fp) >> 7);
        int g = 128 + ((((int)((p >> 8)  & 0xFF) - 128) * fp) >> 7);
        int b = 128 + ((((int)( p         & 0xFF) - 128) * fp) >> 7);
        if (r < 0) { r = 0; } else if (r > 255) { r = 255; }
        if (g < 0) { g = 0; } else if (g > 255) { g = 255; }
        if (b < 0) { b = 0; } else if (b > 255) { b = 255; }
        c->pixels[i] = a | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

void canvas_contrast_up(Canvas *c) {
    canvas_contrast_step(c, 16);
}

void canvas_contrast_down(Canvas *c) {
    canvas_contrast_step(c, -16);
}
/* Hue rotation via RGB↔HSV conversion.
 * Hue is represented as [0,360), stored as a float internally.
 * S and V are [0,1] floats; RGB are 0-255 integers. */
void canvas_hue_rotate(Canvas *c, int degrees) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    /* Normalise degrees to [0,360) */
    int delta = ((degrees % 360) + 360) % 360;
    if (delta == 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint32_t a = p & 0xFF000000;
        float r = (float)((p >> 16) & 0xFF) / 255.0f;
        float g = (float)((p >> 8)  & 0xFF) / 255.0f;
        float b = (float)( p         & 0xFF) / 255.0f;

        /* RGB → HSV */
        float cmax = r > g ? (r > b ? r : b) : (g > b ? g : b);
        float cmin = r < g ? (r < b ? r : b) : (g < b ? g : b);
        float diff = cmax - cmin;
        float h = 0.0f, s = 0.0f, v = cmax;
        if (diff > 1e-6f) {
            s = diff / cmax;
            if (cmax == r) {
                h = 60.0f * (g - b) / diff;
            } else if (cmax == g) {
                h = 60.0f * ((b - r) / diff + 2.0f);
            } else {
                h = 60.0f * ((r - g) / diff + 4.0f);
            }
            if (h < 0.0f) h += 360.0f;
        }

        /* Rotate hue */
        h += (float)delta;
        if (h >= 360.0f) h -= 360.0f;

        /* HSV → RGB */
        float nr, ng, nb;
        if (s < 1e-6f) {
            nr = ng = nb = v;
        } else {
            int hi = (int)(h / 60.0f) % 6;
            float f  = h / 60.0f - (float)hi;
            float pv = v * (1.0f - s);
            float qv = v * (1.0f - s * f);
            float tv = v * (1.0f - s * (1.0f - f));
            switch (hi) {
                case 0: nr = v;  ng = tv; nb = pv; break;
                case 1: nr = qv; ng = v;  nb = pv; break;
                case 2: nr = pv; ng = v;  nb = tv; break;
                case 3: nr = pv; ng = qv; nb = v;  break;
                case 4: nr = tv; ng = pv; nb = v;  break;
                default:nr = v;  ng = pv; nb = qv; break;
            }
        }

        uint32_t ir = (uint32_t)(nr * 255.0f + 0.5f);
        uint32_t ig = (uint32_t)(ng * 255.0f + 0.5f);
        uint32_t ib = (uint32_t)(nb * 255.0f + 0.5f);
        if (ir > 255) ir = 255;
        if (ig > 255) ig = 255;
        if (ib > 255) ib = 255;
        c->pixels[i] = a | (ir << 16) | (ig << 8) | ib;
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
