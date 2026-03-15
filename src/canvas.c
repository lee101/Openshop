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

void canvas_set_pixel_raw(Canvas *c, int x, int y, uint32_t color) {
    if (!c || !c->pixels) {
        return;
    }
    if (x < 0 || y < 0 || x >= c->width || y >= c->height) {
        return;
    }
    size_t idx = (size_t)y * (size_t)c->width + (size_t)x;
    c->pixels[idx] = color;
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
