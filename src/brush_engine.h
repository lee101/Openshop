#ifndef BRUSH_ENGINE_H
#define BRUSH_ENGINE_H

#include "canvas.h"
#include <stdint.h>

typedef enum {
    VFX_BRUSH_SOFT_ROUND = 0,
    VFX_BRUSH_AIRBRUSH,
    VFX_BRUSH_SPLATTER,
    VFX_BRUSH_GLOW,
    VFX_BRUSH_SPARKLE,
    VFX_BRUSH_SMOKE,
    VFX_BRUSH_COUNT
} VfxBrushPreset;

typedef struct {
    int radius;
    int hardness_percent;
    int flow_percent;
    int spacing_percent;
    int scatter_percent;
    int count;
    int size_jitter_percent;
    int additive;
} BrushDynamics;

const char *vfx_brush_name(VfxBrushPreset preset);
int vfx_brush_valid(int preset);
BrushDynamics brush_dynamics_for_preset(VfxBrushPreset preset, int radius);
void brush_engine_stamp(Canvas *c, int cx, int cy, const BrushDynamics *dyn, uint32_t argb, uint32_t *seed);
void brush_engine_stroke(Canvas *c, int x0, int y0, int x1, int y1, const BrushDynamics *dyn, uint32_t argb, uint32_t seed);

#endif
