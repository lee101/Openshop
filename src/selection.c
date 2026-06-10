#include "selection.h"

#include <stdlib.h>
#include <string.h>

int selection_init(Selection *sel, int width, int height) {
    if (!sel || width <= 0 || height <= 0) {
        return 0;
    }
    sel->width = width;
    sel->height = height;
    sel->active = 0;
    sel->mask = (uint8_t *)calloc((size_t)width * (size_t)height, 1);
    return sel->mask != NULL;
}

void selection_free(Selection *sel) {
    if (!sel) {
        return;
    }
    free(sel->mask);
    sel->mask = NULL;
    sel->width = 0;
    sel->height = 0;
    sel->active = 0;
}

int selection_resize(Selection *sel, int width, int height) {
    if (!sel || width <= 0 || height <= 0) {
        return 0;
    }
    free(sel->mask);
    sel->mask = (uint8_t *)calloc((size_t)width * (size_t)height, 1);
    sel->width = width;
    sel->height = height;
    sel->active = 0;
    return sel->mask != NULL;
}

void selection_select_all(Selection *sel) {
    if (!sel || !sel->mask) {
        return;
    }
    memset(sel->mask, 255, (size_t)sel->width * (size_t)sel->height);
    sel->active = 1;
}

void selection_deselect(Selection *sel) {
    if (!sel || !sel->mask) {
        return;
    }
    memset(sel->mask, 0, (size_t)sel->width * (size_t)sel->height);
    sel->active = 0;
}

void selection_invert(Selection *sel) {
    size_t count;

    if (!sel || !sel->mask) {
        return;
    }
    count = (size_t)sel->width * (size_t)sel->height;
    if (!sel->active) {
        memset(sel->mask, 0, count);
        sel->active = 1;
        return;
    }
    for (size_t i = 0; i < count; i++) {
        sel->mask[i] = (uint8_t)(255 - sel->mask[i]);
    }
}

static void apply_op(Selection *sel, int x, int y, uint8_t value, SelectionOp op) {
    size_t idx;

    if (x < 0 || y < 0 || x >= sel->width || y >= sel->height) {
        return;
    }
    idx = (size_t)y * (size_t)sel->width + (size_t)x;
    if (op == SELECTION_SUBTRACT) {
        if (value > sel->mask[idx]) {
            sel->mask[idx] = 0;
        } else {
            sel->mask[idx] = (uint8_t)(sel->mask[idx] - value);
        }
    } else {
        if (value > sel->mask[idx]) {
            sel->mask[idx] = value;
        }
    }
}

static void begin_op(Selection *sel, SelectionOp op) {
    if (op == SELECTION_REPLACE) {
        memset(sel->mask, 0, (size_t)sel->width * (size_t)sel->height);
    }
    sel->active = 1;
}

void selection_select_rect(Selection *sel, int x0, int y0, int x1, int y1, SelectionOp op) {
    int left;
    int right;
    int top;
    int bottom;

    if (!sel || !sel->mask) {
        return;
    }
    left = x0 < x1 ? x0 : x1;
    right = x0 < x1 ? x1 : x0;
    top = y0 < y1 ? y0 : y1;
    bottom = y0 < y1 ? y1 : y0;

    begin_op(sel, op);
    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            apply_op(sel, x, y, 255, op);
        }
    }
}

void selection_select_ellipse(Selection *sel, int x0, int y0, int x1, int y1, SelectionOp op) {
    int left;
    int right;
    int top;
    int bottom;
    double cx;
    double cy;
    double rx;
    double ry;

    if (!sel || !sel->mask) {
        return;
    }
    left = x0 < x1 ? x0 : x1;
    right = x0 < x1 ? x1 : x0;
    top = y0 < y1 ? y0 : y1;
    bottom = y0 < y1 ? y1 : y0;
    cx = (left + right) / 2.0;
    cy = (top + bottom) / 2.0;
    rx = (right - left) / 2.0 + 0.5;
    ry = (bottom - top) / 2.0 + 0.5;
    if (rx <= 0.0 || ry <= 0.0) {
        return;
    }

    begin_op(sel, op);
    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            double nx = (x - cx) / rx;
            double ny = (y - cy) / ry;
            if (nx * nx + ny * ny <= 1.0) {
                apply_op(sel, x, y, 255, op);
            }
        }
    }
}

int selection_select_polygon(Selection *sel, const int *xs, const int *ys, int count, SelectionOp op) {
    if (!sel || !sel->mask || !xs || !ys || count < 3) {
        return 0;
    }

    begin_op(sel, op);
    for (int y = 0; y < sel->height; y++) {
        double yc = y + 0.5;
        for (int x = 0; x < sel->width; x++) {
            double xc = x + 0.5;
            int inside = 0;
            for (int i = 0, j = count - 1; i < count; j = i++) {
                double xi = xs[i];
                double yi = ys[i];
                double xj = xs[j];
                double yj = ys[j];
                if ((yi > yc) != (yj > yc) && xc < (xj - xi) * (yc - yi) / (yj - yi) + xi) {
                    inside = !inside;
                }
            }
            if (inside) {
                apply_op(sel, x, y, 255, op);
            }
        }
    }
    return 1;
}

static int wand_matches(uint32_t a, uint32_t b, int tolerance) {
    int dr = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
    int dg = (int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF);
    int db = (int)(a & 0xFF) - (int)(b & 0xFF);

    if (dr < 0) dr = -dr;
    if (dg < 0) dg = -dg;
    if (db < 0) db = -db;
    return dr <= tolerance && dg <= tolerance && db <= tolerance;
}

int selection_magic_wand(Selection *sel, const Canvas *source, int x, int y, int tolerance, SelectionOp op) {
    uint32_t seed_color;
    uint8_t *visited;
    int *stack;
    size_t capacity;
    size_t count;

    if (!sel || !sel->mask || !source || !source->pixels) {
        return 0;
    }
    if (sel->width != source->width || sel->height != source->height) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= sel->width || y >= sel->height) {
        return 0;
    }
    if (tolerance < 0) tolerance = 0;
    if (tolerance > 255) tolerance = 255;

    visited = (uint8_t *)calloc((size_t)sel->width * (size_t)sel->height, 1);
    capacity = 1024;
    stack = (int *)malloc(capacity * sizeof(int));
    if (!visited || !stack) {
        free(visited);
        free(stack);
        return 0;
    }

    seed_color = canvas_get_pixel(source, x, y);
    begin_op(sel, op);

    count = 0;
    stack[count++] = y * sel->width + x;
    visited[y * sel->width + x] = 1;

    while (count > 0) {
        int idx = stack[--count];
        int px = idx % sel->width;
        int py = idx / sel->width;

        if (!wand_matches(seed_color, source->pixels[idx], tolerance)) {
            continue;
        }
        apply_op(sel, px, py, 255, op);

        if (count + 4 >= capacity) {
            size_t next_capacity = capacity * 2;
            int *next = (int *)realloc(stack, next_capacity * sizeof(int));
            if (!next) {
                free(visited);
                free(stack);
                return 0;
            }
            stack = next;
            capacity = next_capacity;
        }

        {
            const int neighbors[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (int n = 0; n < 4; n++) {
                int nx = px + neighbors[n][0];
                int ny = py + neighbors[n][1];
                if (nx < 0 || ny < 0 || nx >= sel->width || ny >= sel->height) {
                    continue;
                }
                int nidx = ny * sel->width + nx;
                if (!visited[nidx]) {
                    visited[nidx] = 1;
                    stack[count++] = nidx;
                }
            }
        }
    }

    free(visited);
    free(stack);
    return 1;
}

void selection_feather(Selection *sel, int radius) {
    uint8_t *src;
    int window;

    if (!sel || !sel->mask || !sel->active || radius <= 0) {
        return;
    }
    if (radius > 64) radius = 64;
    window = radius * 2 + 1;

    src = (uint8_t *)malloc((size_t)sel->width * (size_t)sel->height);
    if (!src) {
        return;
    }

    for (int pass = 0; pass < 2; pass++) {
        memcpy(src, sel->mask, (size_t)sel->width * (size_t)sel->height);
        if (pass == 0) {
            for (int y = 0; y < sel->height; y++) {
                int sum = 0;
                const uint8_t *row = src + (size_t)y * (size_t)sel->width;
                uint8_t *out = sel->mask + (size_t)y * (size_t)sel->width;
                for (int x = -radius; x <= radius; x++) {
                    int cx = x < 0 ? 0 : (x >= sel->width ? sel->width - 1 : x);
                    sum += row[cx];
                }
                for (int x = 0; x < sel->width; x++) {
                    int add = x + radius + 1;
                    int sub = x - radius;
                    out[x] = (uint8_t)(sum / window);
                    if (add >= sel->width) add = sel->width - 1;
                    if (sub < 0) sub = 0;
                    sum += row[add] - row[sub];
                }
            }
        } else {
            for (int x = 0; x < sel->width; x++) {
                int sum = 0;
                for (int y = -radius; y <= radius; y++) {
                    int cy = y < 0 ? 0 : (y >= sel->height ? sel->height - 1 : y);
                    sum += src[(size_t)cy * (size_t)sel->width + (size_t)x];
                }
                for (int y = 0; y < sel->height; y++) {
                    int add = y + radius + 1;
                    int sub = y - radius;
                    sel->mask[(size_t)y * (size_t)sel->width + (size_t)x] = (uint8_t)(sum / window);
                    if (add >= sel->height) add = sel->height - 1;
                    if (sub < 0) sub = 0;
                    sum += src[(size_t)add * (size_t)sel->width + (size_t)x] -
                           src[(size_t)sub * (size_t)sel->width + (size_t)x];
                }
            }
        }
    }
    free(src);
}

uint8_t selection_coverage(const Selection *sel, int x, int y) {
    if (!sel || !sel->mask || !sel->active) {
        return 255;
    }
    if (x < 0 || y < 0 || x >= sel->width || y >= sel->height) {
        return 0;
    }
    return sel->mask[(size_t)y * (size_t)sel->width + (size_t)x];
}

int selection_is_empty(const Selection *sel) {
    size_t count;

    if (!sel || !sel->mask || !sel->active) {
        return 1;
    }
    count = (size_t)sel->width * (size_t)sel->height;
    for (size_t i = 0; i < count; i++) {
        if (sel->mask[i]) {
            return 0;
        }
    }
    return 1;
}

int selection_bounds(const Selection *sel, int *x0, int *y0, int *x1, int *y1) {
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int found = 0;

    if (!sel || !sel->mask || !sel->active) {
        return 0;
    }
    min_x = sel->width;
    min_y = sel->height;
    max_x = -1;
    max_y = -1;
    for (int y = 0; y < sel->height; y++) {
        const uint8_t *row = sel->mask + (size_t)y * (size_t)sel->width;
        for (int x = 0; x < sel->width; x++) {
            if (row[x]) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                found = 1;
            }
        }
    }
    if (!found) {
        return 0;
    }
    if (x0) *x0 = min_x;
    if (y0) *y0 = min_y;
    if (x1) *x1 = max_x;
    if (y1) *y1 = max_y;
    return 1;
}

void selection_clamp_edit(const Selection *sel, Canvas *canvas, const uint32_t *original) {
    size_t count;

    if (!sel || !sel->mask || !sel->active || !canvas || !canvas->pixels || !original) {
        return;
    }
    if (canvas->width != sel->width || canvas->height != sel->height) {
        return;
    }
    count = (size_t)sel->width * (size_t)sel->height;
    for (size_t i = 0; i < count; i++) {
        uint8_t cov = sel->mask[i];
        if (cov == 255) {
            continue;
        }
        if (cov == 0) {
            canvas->pixels[i] = original[i];
            continue;
        }
        {
            uint32_t edited = canvas->pixels[i];
            uint32_t orig = original[i];
            int inv = 255 - cov;
            uint8_t a = (uint8_t)((((edited >> 24) & 0xFF) * cov + ((orig >> 24) & 0xFF) * inv + 127) / 255);
            uint8_t r = (uint8_t)((((edited >> 16) & 0xFF) * cov + ((orig >> 16) & 0xFF) * inv + 127) / 255);
            uint8_t g = (uint8_t)((((edited >> 8) & 0xFF) * cov + ((orig >> 8) & 0xFF) * inv + 127) / 255);
            uint8_t b = (uint8_t)(((edited & 0xFF) * cov + (orig & 0xFF) * inv + 127) / 255);
            canvas->pixels[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }
}
