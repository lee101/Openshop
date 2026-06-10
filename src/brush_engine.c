#include "brush_engine.h"

#include <math.h>
#include <stdlib.h>

const char *vfx_brush_name(VfxBrushPreset preset) {
    switch (preset) {
    case VFX_BRUSH_SOFT_ROUND: return "Soft Round";
    case VFX_BRUSH_AIRBRUSH: return "Airbrush";
    case VFX_BRUSH_SPLATTER: return "Splatter";
    case VFX_BRUSH_GLOW: return "Glow";
    case VFX_BRUSH_SPARKLE: return "Sparkle";
    case VFX_BRUSH_SMOKE: return "Smoke";
    default: return "Soft Round";
    }
}

int vfx_brush_valid(int preset) {
    return preset >= 0 && preset < VFX_BRUSH_COUNT;
}

BrushDynamics brush_dynamics_for_preset(VfxBrushPreset preset, int radius) {
    BrushDynamics dyn = {radius, 60, 100, 25, 0, 1, 0, 0};

    if (dyn.radius < 1) {
        dyn.radius = 1;
    }
    switch (preset) {
    case VFX_BRUSH_AIRBRUSH:
        dyn.hardness_percent = 0;
        dyn.flow_percent = 18;
        dyn.spacing_percent = 12;
        dyn.scatter_percent = 30;
        break;
    case VFX_BRUSH_SPLATTER:
        dyn.hardness_percent = 85;
        dyn.flow_percent = 90;
        dyn.spacing_percent = 60;
        dyn.scatter_percent = 220;
        dyn.count = 6;
        dyn.size_jitter_percent = 70;
        break;
    case VFX_BRUSH_GLOW:
        dyn.hardness_percent = 0;
        dyn.flow_percent = 35;
        dyn.spacing_percent = 15;
        dyn.additive = 1;
        break;
    case VFX_BRUSH_SPARKLE:
        dyn.hardness_percent = 100;
        dyn.flow_percent = 100;
        dyn.spacing_percent = 80;
        dyn.scatter_percent = 260;
        dyn.count = 4;
        dyn.size_jitter_percent = 85;
        dyn.additive = 1;
        break;
    case VFX_BRUSH_SMOKE:
        dyn.hardness_percent = 0;
        dyn.flow_percent = 8;
        dyn.spacing_percent = 20;
        dyn.scatter_percent = 90;
        dyn.count = 3;
        dyn.size_jitter_percent = 40;
        break;
    case VFX_BRUSH_SOFT_ROUND:
    default:
        dyn.hardness_percent = 35;
        dyn.flow_percent = 80;
        dyn.spacing_percent = 12;
        break;
    }
    return dyn;
}

static uint32_t next_rand(uint32_t *seed) {
    *seed = *seed * 1664525u + 1013904223u;
    return *seed;
}

static int rand_range(uint32_t *seed, int span) {
    if (span <= 0) {
        return 0;
    }
    return (int)(next_rand(seed) % (uint32_t)(span * 2 + 1)) - span;
}

static void set_pixel_additive(Canvas *c, int x, int y, uint32_t color) {
    uint32_t dst;
    int r, g, b;
    uint8_t sa;

    if (x < 0 || y < 0 || x >= c->width || y >= c->height) {
        return;
    }
    dst = c->pixels[(size_t)y * (size_t)c->width + (size_t)x];
    sa = (uint8_t)((color >> 24) & 0xFF);
    if (sa == 0) {
        return;
    }
    r = (int)((dst >> 16) & 0xFF) + (int)((color >> 16) & 0xFF) * sa / 255;
    g = (int)((dst >> 8) & 0xFF) + (int)((color >> 8) & 0xFF) * sa / 255;
    b = (int)(dst & 0xFF) + (int)(color & 0xFF) * sa / 255;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    {
        uint8_t da = (uint8_t)((dst >> 24) & 0xFF);
        uint8_t oa = (uint8_t)(sa + ((da * (255 - sa) + 127) / 255));
        c->pixels[(size_t)y * (size_t)c->width + (size_t)x] =
            ((uint32_t)oa << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

static void stamp_soft_dab(Canvas *c, int cx, int cy, int radius, int hardness_percent, int alpha, uint32_t rgb, int additive) {
    double hard = hardness_percent / 100.0;
    double inner = hard * radius;

    if (radius < 1) {
        radius = 1;
    }
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            double dist = sqrt((double)(x * x + y * y));
            double fall;
            int a;

            if (dist > radius) {
                continue;
            }
            if (dist <= inner) {
                fall = 1.0;
            } else {
                fall = 1.0 - (dist - inner) / ((double)radius - inner + 0.0001);
            }
            a = (int)(alpha * fall * fall + 0.5);
            if (a <= 0) {
                continue;
            }
            if (a > 255) {
                a = 255;
            }
            if (additive) {
                set_pixel_additive(c, cx + x, cy + y, ((uint32_t)a << 24) | rgb);
            } else {
                canvas_set_pixel(c, cx + x, cy + y, ((uint32_t)a << 24) | rgb);
            }
        }
    }
}

void brush_engine_stamp(Canvas *c, int cx, int cy, const BrushDynamics *dyn, uint32_t argb, uint32_t *seed) {
    int base_alpha;
    int alpha;
    uint32_t rgb;
    int dabs;

    if (!c || !c->pixels || !dyn || !seed) {
        return;
    }
    base_alpha = (int)((argb >> 24) & 0xFF);
    alpha = base_alpha * dyn->flow_percent / 100;
    if (alpha <= 0) {
        return;
    }
    rgb = argb & 0x00FFFFFFu;
    dabs = dyn->count < 1 ? 1 : dyn->count;

    for (int i = 0; i < dabs; i++) {
        int scatter = dyn->radius * dyn->scatter_percent / 100;
        int dx = rand_range(seed, scatter);
        int dy = rand_range(seed, scatter);
        int radius = dyn->radius;

        if (dyn->size_jitter_percent > 0) {
            int jitter = dyn->radius * dyn->size_jitter_percent / 100;
            radius -= (int)(next_rand(seed) % (uint32_t)(jitter + 1));
            if (radius < 1) {
                radius = 1;
            }
        }
        stamp_soft_dab(c, cx + dx, cy + dy, radius, dyn->hardness_percent, alpha, rgb, dyn->additive);
    }
}

void brush_engine_stroke(Canvas *c, int x0, int y0, int x1, int y1, const BrushDynamics *dyn, uint32_t argb, uint32_t seed) {
    double dx;
    double dy;
    double length;
    int steps;
    int step_size;

    if (!c || !c->pixels || !dyn || dyn->radius < 1) {
        return;
    }
    dx = (double)(x1 - x0);
    dy = (double)(y1 - y0);
    length = sqrt(dx * dx + dy * dy);
    step_size = dyn->radius * dyn->spacing_percent / 100;
    if (step_size < 1) {
        step_size = 1;
    }
    steps = (int)(length / step_size);
    if (steps < 1) {
        steps = 1;
    }

    for (int i = 0; i <= steps; i++) {
        double t = (double)i / (double)steps;
        int px = x0 + (int)(dx * t + 0.5);
        int py = y0 + (int)(dy * t + 0.5);
        brush_engine_stamp(c, px, py, dyn, argb, &seed);
    }
}
