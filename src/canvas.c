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
        /* ITU-R BT.601 integer approximation: 0.299r + 0.587g + 0.114b */
        uint8_t luma = (uint8_t)((77u * r + 150u * g + 29u * b) >> 8);
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)luma << 16) | ((uint32_t)luma << 8) | luma;
    }
}

void canvas_blur(Canvas *c) {
    /* 3x3 box blur — allocates a temporary copy to avoid reading written data */
    if (!c || !c->pixels || c->width < 2 || c->height < 2) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    uint32_t *tmp = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!tmp) {
        return;
    }
    memcpy(tmp, c->pixels, count * sizeof(uint32_t));

    for (int y = 0; y < c->height; y++) {
        for (int x = 0; x < c->width; x++) {
            unsigned int sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
            int samples = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int nx = x + kx;
                    int ny = y + ky;
                    if (nx < 0 || ny < 0 || nx >= c->width || ny >= c->height) {
                        continue;
                    }
                    uint32_t p = tmp[(size_t)ny * (size_t)c->width + (size_t)nx];
                    sum_a += (p >> 24) & 0xFF;
                    sum_r += (p >> 16) & 0xFF;
                    sum_g += (p >> 8) & 0xFF;
                    sum_b += p & 0xFF;
                    samples++;
                }
            }
            if (samples > 0) {
                uint32_t a = (sum_a + (unsigned int)samples / 2) / (unsigned int)samples;
                uint32_t r = (sum_r + (unsigned int)samples / 2) / (unsigned int)samples;
                uint32_t g = (sum_g + (unsigned int)samples / 2) / (unsigned int)samples;
                uint32_t b = (sum_b + (unsigned int)samples / 2) / (unsigned int)samples;
                c->pixels[(size_t)y * (size_t)c->width + (size_t)x] =
                    (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
    free(tmp);
}

/* Rotate within the largest centred square of the canvas.
   For a non-square canvas the outer strips outside the square are untouched.
   output(dx,dy) <- source(dy, s-1-dx) for CW
   output(dx,dy) <- source(s-1-dy, dx) for CCW */
static void canvas_rotate_90_impl(Canvas *c, int cw) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    int s = c->width < c->height ? c->width : c->height;
    int ox = (c->width - s) / 2;
    int oy = (c->height - s) / 2;

    uint32_t *tmp = (uint32_t *)malloc((size_t)s * (size_t)s * sizeof(uint32_t));
    if (!tmp) {
        return;
    }

    /* Copy the square region into tmp */
    for (int y = 0; y < s; y++) {
        for (int x = 0; x < s; x++) {
            tmp[(size_t)y * (size_t)s + (size_t)x] =
                c->pixels[(size_t)(oy + y) * (size_t)c->width + (size_t)(ox + x)];
        }
    }

    /* Write back rotated.
       CW 90°:  result(dy,dx) <- source(s-1-dx, dy)   i.e. tmp[(s-1-dx)*s + dy]
       CCW 90°: result(dy,dx) <- source(dx, s-1-dy)   i.e. tmp[dx*s + (s-1-dy)] */
    for (int dy = 0; dy < s; dy++) {
        for (int dx = 0; dx < s; dx++) {
            uint32_t val;
            if (cw) {
                val = tmp[(size_t)(s - 1 - dx) * (size_t)s + (size_t)dy];
            } else {
                val = tmp[(size_t)dx * (size_t)s + (size_t)(s - 1 - dy)];
            }
            c->pixels[(size_t)(oy + dy) * (size_t)c->width + (size_t)(ox + dx)] = val;
        }
    }

    free(tmp);
}

void canvas_rotate_90_cw(Canvas *c) {
    canvas_rotate_90_impl(c, 1);
}

void canvas_rotate_90_ccw(Canvas *c) {
    canvas_rotate_90_impl(c, 0);
}

static uint8_t clamp_u8(int v);

void canvas_posterize(Canvas *c, int levels) {
    /* Quantise each RGB channel to `levels` equally-spaced values.
       levels < 2 is treated as 2; levels > 255 is a no-op. */
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    if (levels > 255) {
        return;
    }
    if (levels < 2) {
        levels = 2;
    }
    /* Step size between adjacent output levels (in 0..255 range) */
    int step = 256 / levels;
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        int r = (int)((p >> 16) & 0xFF);
        int g = (int)((p >> 8) & 0xFF);
        int b = (int)(p & 0xFF);
        /* Quantise: map channel to nearest level centre */
        r = clamp_u8((r / step) * step + step / 2);
        g = clamp_u8((g / step) * step + step / 2);
        b = clamp_u8((b / step) * step + step / 2);
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

/* HSV conversion helpers for hue_rotate */
static void rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, float *h, float *s, float *v) {
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;
    float cmax = rf > gf ? (rf > bf ? rf : bf) : (gf > bf ? gf : bf);
    float cmin = rf < gf ? (rf < bf ? rf : bf) : (gf < bf ? gf : bf);
    float delta = cmax - cmin;
    *v = cmax;
    *s = (cmax > 0.0f) ? (delta / cmax) : 0.0f;
    if (delta < 1e-6f) {
        *h = 0.0f;
        return;
    }
    if (cmax == rf) {
        *h = 60.0f * ((gf - bf) / delta);
    } else if (cmax == gf) {
        *h = 60.0f * (2.0f + (bf - rf) / delta);
    } else {
        *h = 60.0f * (4.0f + (rf - gf) / delta);
    }
    if (*h < 0.0f) {
        *h += 360.0f;
    }
}

static void hsv_to_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (s < 1e-6f) {
        uint8_t grey = clamp_u8((int)(v * 255.0f + 0.5f));
        *r = *g = *b = grey;
        return;
    }
    h = h - (float)((int)(h / 360.0f)) * 360.0f;
    if (h < 0.0f) h += 360.0f;
    float hi = h / 60.0f;
    int sector = (int)hi;
    float f = hi - (float)sector;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    float rf, gf, bf;
    switch (sector % 6) {
    case 0: rf = v; gf = t; bf = p; break;
    case 1: rf = q; gf = v; bf = p; break;
    case 2: rf = p; gf = v; bf = t; break;
    case 3: rf = p; gf = q; bf = v; break;
    case 4: rf = t; gf = p; bf = v; break;
    default: rf = v; gf = p; bf = q; break;
    }
    *r = clamp_u8((int)(rf * 255.0f + 0.5f));
    *g = clamp_u8((int)(gf * 255.0f + 0.5f));
    *b = clamp_u8((int)(bf * 255.0f + 0.5f));
}

void canvas_hue_rotate(Canvas *c, int degrees) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0 || degrees == 0) {
        return;
    }
    float shift = (float)degrees;
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        uint8_t r = (uint8_t)((p >> 16) & 0xFF);
        uint8_t g = (uint8_t)((p >> 8) & 0xFF);
        uint8_t b = (uint8_t)(p & 0xFF);
        float h, s, v;
        rgb_to_hsv(r, g, b, &h, &s, &v);
        h += shift;
        if (h >= 360.0f) h -= 360.0f;
        if (h < 0.0f) h += 360.0f;
        uint8_t nr, ng, nb;
        hsv_to_rgb(h, s, v, &nr, &ng, &nb);
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)nr << 16) | ((uint32_t)ng << 8) | nb;
    }
}

static uint8_t clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

void canvas_brightness(Canvas *c, int delta) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0 || delta == 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        uint8_t r = clamp_u8((int)((p >> 16) & 0xFF) + delta);
        uint8_t g = clamp_u8((int)((p >> 8) & 0xFF) + delta);
        uint8_t b = clamp_u8((int)(p & 0xFF) + delta);
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
}

void canvas_contrast(Canvas *c, int factor_percent) {
    /* Scales each channel towards/away from mid-grey (128).
       factor_percent=100 is identity, <100 reduces contrast, >100 increases it. */
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        int r = (int)((p >> 16) & 0xFF);
        int g = (int)((p >> 8) & 0xFF);
        int b = (int)(p & 0xFF);
        r = clamp_u8(128 + (r - 128) * factor_percent / 100);
        g = clamp_u8(128 + (g - 128) * factor_percent / 100);
        b = clamp_u8(128 + (b - 128) * factor_percent / 100);
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
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
