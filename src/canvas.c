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

void canvas_rotate_90_cw(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    int W = c->width, H = c->height;
    size_t count = (size_t)W * (size_t)H;
    uint32_t *buf = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!buf) {
        return;
    }

    /*
     * 90° CW rotation (clockwise as seen on screen) around the canvas centre,
     * result cropped/padded to keep the same WxH frame.
     *
     * Using the standard 90° CW formula for a WxH → HxW image:
     *   R(rx, ry) = S(ry, H-1-rx)   (rx in [0,H-1], ry in [0,W-1])
     * Then centre-crop the HxW rotated image back into WxH:
     *   x_pad  = (W - H) / 2        (pad left/right when W > H)
     *   y_crop = (W - H) / 2        (crop top/bottom when W > H)
     *   dest(dx,dy) = R(dx - x_pad, dy + y_crop)
     *               = S(dy + x_pad, H-1-(dx - x_pad))
     *               = S(dy + x_pad, (W+H-2)/2 - dx)
     *
     * For W=800, H=600: x_pad=100, cross=699 → S(dy+100, 699-dx)
     * Pixels outside the source bounds are filled with 0 (transparent).
     */
    int x_pad = (W - H) / 2;
    int cross  = (W - 1 + H - 1) / 2;   /* floor((W+H-2)/2) */

    for (int dy = 0; dy < H; dy++) {
        for (int dx = 0; dx < W; dx++) {
            int sx = dy + x_pad;
            int sy = cross - dx;
            uint32_t px = (sx >= 0 && sx < W && sy >= 0 && sy < H)
                          ? c->pixels[(size_t)sy * (size_t)W + (size_t)sx]
                          : 0;
            buf[(size_t)dy * (size_t)W + (size_t)dx] = px;
        }
    }

    memcpy(c->pixels, buf, count * sizeof(uint32_t));
    free(buf);
}

void canvas_rotate_90_ccw(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    int W = c->width, H = c->height;
    size_t count = (size_t)W * (size_t)H;
    uint32_t *buf = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!buf) {
        return;
    }

    /*
     * 90° CCW rotation (counter-clockwise as seen on screen) around the
     * canvas centre, result cropped/padded to keep the same WxH frame.
     *
     * Standard 90° CCW: R(rx, ry) = S(W-1-ry, rx)  (rx in [0,H-1], ry in [0,W-1])
     * Centre-crop back to WxH (same offsets as CW):
     *   dest(dx,dy) = R(dx - x_pad, dy + y_crop)
     *               = S(W-1-(dy+x_pad), dx - x_pad)
     *               = S((W+H-2)/2 - dy, dx - x_pad)
     *
     * For W=800, H=600: S(699-dy, dx-100)
     * Pixels outside the source bounds are filled with 0 (transparent).
     */
    int x_pad = (W - H) / 2;
    int cross  = (W - 1 + H - 1) / 2;

    for (int dy = 0; dy < H; dy++) {
        for (int dx = 0; dx < W; dx++) {
            int sx = cross - dy;
            int sy = dx - x_pad;
            uint32_t px = (sx >= 0 && sx < W && sy >= 0 && sy < H)
                          ? c->pixels[(size_t)sy * (size_t)W + (size_t)sx]
                          : 0;
            buf[(size_t)dy * (size_t)W + (size_t)dx] = px;
        }
    }

    memcpy(c->pixels, buf, count * sizeof(uint32_t));
    free(buf);
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
        /* ITU-R BT.601 luminance coefficients: 0.299R + 0.587G + 0.114B */
        uint8_t y = (uint8_t)((299u * r + 587u * g + 114u * b) / 1000u);
        c->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)y << 16) | ((uint32_t)y << 8) | y;
    }
}

void canvas_auto_levels(Canvas *c) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0) {
        return;
    }
    size_t count = (size_t)c->width * (size_t)c->height;

    /* Pass 1: find per-channel min and max across non-transparent pixels. */
    int rmin = 255, rmax = 0;
    int gmin = 255, gmax = 0;
    int bmin = 255, bmax = 0;
    int found = 0;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        if (((p >> 24) & 0xFF) == 0) {
            continue; /* skip fully-transparent pixels */
        }
        int r = (int)((p >> 16) & 0xFF);
        int g = (int)((p >> 8) & 0xFF);
        int b = (int)(p & 0xFF);
        if (r < rmin) rmin = r;
        if (r > rmax) rmax = r;
        if (g < gmin) gmin = g;
        if (g > gmax) gmax = g;
        if (b < bmin) bmin = b;
        if (b > bmax) bmax = b;
        found = 1;
    }
    if (!found) {
        return;
    }

    /* Pass 2: stretch each channel to [0, 255].  Skip channels already at full range. */
    int rrange = rmax - rmin;
    int grange = gmax - gmin;
    int brange = bmax - bmin;

    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        if (a == 0) {
            continue;
        }
        int r = (int)((p >> 16) & 0xFF);
        int g = (int)((p >> 8) & 0xFF);
        int b = (int)(p & 0xFF);
        if (rrange > 0) r = (r - rmin) * 255 / rrange;
        if (grange > 0) g = (g - gmin) * 255 / grange;
        if (brange > 0) b = (b - bmin) * 255 / brange;
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
