#include "retouch.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint8_t clamp_u8(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

static double dab_weight(int dx, int dy, int radius, int hardness_percent) {
    double dist = sqrt((double)(dx * dx + dy * dy));
    double inner = radius * hardness_percent / 100.0;

    if (dist > radius) {
        return 0.0;
    }
    if (dist <= inner) {
        return 1.0;
    }
    return 1.0 - (dist - inner) / ((double)radius - inner + 0.0001);
}

typedef void (*DabFn)(Canvas *c, int cx, int cy, int radius, void *ctx);

static void stroke_walk(Canvas *c, int x0, int y0, int x1, int y1, int radius, DabFn dab, void *ctx) {
    double dx = (double)(x1 - x0);
    double dy = (double)(y1 - y0);
    double length = sqrt(dx * dx + dy * dy);
    int step = radius / 3;
    int steps;

    if (step < 1) {
        step = 1;
    }
    steps = (int)(length / step);
    if (steps < 1) {
        steps = 1;
    }
    for (int i = 0; i <= steps; i++) {
        double t = (double)i / (double)steps;
        dab(c, x0 + (int)(dx * t + 0.5), y0 + (int)(dy * t + 0.5), radius, ctx);
    }
}

typedef struct {
    int offset_x;
    int offset_y;
    int hardness_percent;
    const uint32_t *source;
    int width;
    int height;
} CloneCtx;

static void clone_dab(Canvas *c, int cx, int cy, int radius, void *raw) {
    CloneCtx *ctx = (CloneCtx *)raw;

    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            double w = dab_weight(x, y, radius, ctx->hardness_percent);
            int dst_x = cx + x;
            int dst_y = cy + y;
            int src_x = dst_x + ctx->offset_x;
            int src_y = dst_y + ctx->offset_y;
            uint32_t src;
            int alpha;

            if (w <= 0.0 || dst_x < 0 || dst_y < 0 || dst_x >= c->width || dst_y >= c->height) {
                continue;
            }
            if (src_x < 0 || src_y < 0 || src_x >= ctx->width || src_y >= ctx->height) {
                continue;
            }
            src = ctx->source[(size_t)src_y * (size_t)ctx->width + (size_t)src_x];
            alpha = (int)(w * 255.0 + 0.5);
            canvas_set_pixel(c, dst_x, dst_y, ((uint32_t)alpha << 24) | (src & 0x00FFFFFF));
        }
    }
}

void canvas_clone_stroke(Canvas *c, int offset_x, int offset_y, int x0, int y0, int x1, int y1, int radius, int hardness_percent) {
    CloneCtx ctx;
    size_t count;
    uint32_t *source;

    if (!c || !c->pixels || radius < 1 || (offset_x == 0 && offset_y == 0)) {
        return;
    }
    count = (size_t)c->width * (size_t)c->height;
    source = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!source) {
        return;
    }
    memcpy(source, c->pixels, count * sizeof(uint32_t));

    ctx.offset_x = offset_x;
    ctx.offset_y = offset_y;
    ctx.hardness_percent = hardness_percent;
    ctx.source = source;
    ctx.width = c->width;
    ctx.height = c->height;
    stroke_walk(c, x0, y0, x1, y1, radius, clone_dab, &ctx);
    free(source);
}

typedef struct {
    int amount_percent;
    int burn;
} DodgeCtx;

static void dodge_dab(Canvas *c, int cx, int cy, int radius, void *raw) {
    DodgeCtx *ctx = (DodgeCtx *)raw;

    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            double w = dab_weight(x, y, radius, 40);
            int px = cx + x;
            int py = cy + y;
            uint32_t p;
            double scale;

            if (w <= 0.0 || px < 0 || py < 0 || px >= c->width || py >= c->height) {
                continue;
            }
            p = c->pixels[(size_t)py * (size_t)c->width + (size_t)px];
            scale = 1.0 + (ctx->burn ? -1.0 : 1.0) * w * ctx->amount_percent / 100.0;
            {
                uint8_t r = clamp_u8((int)(((p >> 16) & 0xFF) * scale + 0.5));
                uint8_t g = clamp_u8((int)(((p >> 8) & 0xFF) * scale + 0.5));
                uint8_t b = clamp_u8((int)((p & 0xFF) * scale + 0.5));
                c->pixels[(size_t)py * (size_t)c->width + (size_t)px] =
                    (p & 0xFF000000u) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
        }
    }
}

void canvas_dodge_burn_stroke(Canvas *c, int x0, int y0, int x1, int y1, int radius, int amount_percent, int burn) {
    DodgeCtx ctx;

    if (!c || !c->pixels || radius < 1 || amount_percent <= 0) {
        return;
    }
    if (amount_percent > 100) {
        amount_percent = 100;
    }
    ctx.amount_percent = amount_percent;
    ctx.burn = burn;
    stroke_walk(c, x0, y0, x1, y1, radius, dodge_dab, &ctx);
}

typedef struct {
    int amount_percent;
    int desaturate;
} SpongeCtx;

static void sponge_dab(Canvas *c, int cx, int cy, int radius, void *raw) {
    SpongeCtx *ctx = (SpongeCtx *)raw;

    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            double w = dab_weight(x, y, radius, 40);
            int px = cx + x;
            int py = cy + y;
            uint32_t p;

            if (w <= 0.0 || px < 0 || py < 0 || px >= c->width || py >= c->height) {
                continue;
            }
            p = c->pixels[(size_t)py * (size_t)c->width + (size_t)px];
            {
                int r = (p >> 16) & 0xFF;
                int g = (p >> 8) & 0xFF;
                int b = p & 0xFF;
                int gray = (r * 77 + g * 151 + b * 28) >> 8;
                double k = w * ctx->amount_percent / 100.0;
                double mix = ctx->desaturate ? k : -k;
                uint8_t nr = clamp_u8((int)(r + (gray - r) * mix + 0.5));
                uint8_t ng = clamp_u8((int)(g + (gray - g) * mix + 0.5));
                uint8_t nb = clamp_u8((int)(b + (gray - b) * mix + 0.5));
                c->pixels[(size_t)py * (size_t)c->width + (size_t)px] =
                    (p & 0xFF000000u) | ((uint32_t)nr << 16) | ((uint32_t)ng << 8) | nb;
            }
        }
    }
}

void canvas_sponge_stroke(Canvas *c, int x0, int y0, int x1, int y1, int radius, int amount_percent, int desaturate) {
    SpongeCtx ctx;

    if (!c || !c->pixels || radius < 1 || amount_percent <= 0) {
        return;
    }
    if (amount_percent > 100) {
        amount_percent = 100;
    }
    ctx.amount_percent = amount_percent;
    ctx.desaturate = desaturate;
    stroke_walk(c, x0, y0, x1, y1, radius, sponge_dab, &ctx);
}

void canvas_smudge_stroke(Canvas *c, int x0, int y0, int x1, int y1, int radius, int strength_percent) {
    double dx;
    double dy;
    double length;
    int steps;
    int step;
    int diameter;
    uint32_t *pickup;

    if (!c || !c->pixels || radius < 1 || strength_percent <= 0) {
        return;
    }
    if (strength_percent > 95) {
        strength_percent = 95;
    }
    diameter = radius * 2 + 1;
    pickup = (uint32_t *)calloc((size_t)diameter * (size_t)diameter, sizeof(uint32_t));
    if (!pickup) {
        return;
    }

    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            pickup[(size_t)(y + radius) * (size_t)diameter + (size_t)(x + radius)] =
                canvas_get_pixel(c, x0 + x, y0 + y);
        }
    }

    dx = (double)(x1 - x0);
    dy = (double)(y1 - y0);
    length = sqrt(dx * dx + dy * dy);
    step = radius / 3;
    if (step < 1) {
        step = 1;
    }
    steps = (int)(length / step);
    if (steps < 1) {
        steps = 1;
    }

    for (int i = 1; i <= steps; i++) {
        double t = (double)i / (double)steps;
        int cx = x0 + (int)(dx * t + 0.5);
        int cy = y0 + (int)(dy * t + 0.5);
        for (int y = -radius; y <= radius; y++) {
            for (int x = -radius; x <= radius; x++) {
                double w = dab_weight(x, y, radius, 30) * strength_percent / 100.0;
                int px = cx + x;
                int py = cy + y;
                size_t pi = (size_t)(y + radius) * (size_t)diameter + (size_t)(x + radius);
                uint32_t held = pickup[pi];
                uint32_t cur;

                if (px < 0 || py < 0 || px >= c->width || py >= c->height) {
                    continue;
                }
                cur = c->pixels[(size_t)py * (size_t)c->width + (size_t)px];
                if (w > 0.0) {
                    uint8_t a = clamp_u8((int)(((cur >> 24) & 0xFF) * (1.0 - w) + ((held >> 24) & 0xFF) * w + 0.5));
                    uint8_t r = clamp_u8((int)(((cur >> 16) & 0xFF) * (1.0 - w) + ((held >> 16) & 0xFF) * w + 0.5));
                    uint8_t g = clamp_u8((int)(((cur >> 8) & 0xFF) * (1.0 - w) + ((held >> 8) & 0xFF) * w + 0.5));
                    uint8_t b = clamp_u8((int)((cur & 0xFF) * (1.0 - w) + (held & 0xFF) * w + 0.5));
                    uint32_t blended = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
                    c->pixels[(size_t)py * (size_t)c->width + (size_t)px] = blended;
                    pickup[pi] = blended;
                } else {
                    pickup[pi] = cur;
                }
            }
        }
    }
    free(pickup);
}
