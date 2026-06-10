#include "../src/brush_engine.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int count_painted(const Canvas *c) {
    int painted = 0;
    for (int y = 0; y < c->height; y++) {
        for (int x = 0; x < c->width; x++) {
            if ((canvas_get_pixel(c, x, y) >> 24) != 0) {
                painted++;
            }
        }
    }
    return painted;
}

static void test_presets(void) {
    for (int preset = 0; preset < VFX_BRUSH_COUNT; preset++) {
        BrushDynamics dyn = brush_dynamics_for_preset((VfxBrushPreset)preset, 6);
        assert(dyn.radius == 6);
        assert(dyn.flow_percent > 0 && dyn.flow_percent <= 100);
        assert(dyn.spacing_percent > 0);
        assert(dyn.count >= 1);
        assert(vfx_brush_name((VfxBrushPreset)preset) != NULL);
    }
    assert(vfx_brush_valid(VFX_BRUSH_SOFT_ROUND));
    assert(!vfx_brush_valid(-1));
    assert(!vfx_brush_valid(VFX_BRUSH_COUNT));
}

static void test_soft_round_falloff(void) {
    Canvas c = {0};
    BrushDynamics dyn = brush_dynamics_for_preset(VFX_BRUSH_SOFT_ROUND, 8);
    uint32_t seed = 7;

    assert(canvas_init(&c, 32, 32));
    brush_engine_stamp(&c, 16, 16, &dyn, 0xFFFF0000, &seed);

    {
        uint32_t center_a = canvas_get_pixel(&c, 16, 16) >> 24;
        uint32_t edge_a = canvas_get_pixel(&c, 16 + 7, 16) >> 24;
        assert(center_a > 0);
        assert(edge_a < center_a);
    }
    assert((canvas_get_pixel(&c, 0, 0) >> 24) == 0);
    canvas_free(&c);
}

static void test_stroke_determinism(void) {
    Canvas a = {0};
    Canvas b = {0};
    BrushDynamics dyn = brush_dynamics_for_preset(VFX_BRUSH_SPLATTER, 5);

    assert(canvas_init(&a, 64, 32));
    assert(canvas_init(&b, 64, 32));
    brush_engine_stroke(&a, 8, 16, 56, 16, &dyn, 0xFF00FF00, 1234);
    brush_engine_stroke(&b, 8, 16, 56, 16, &dyn, 0xFF00FF00, 1234);
    assert(memcmp(a.pixels, b.pixels, (size_t)64 * 32 * sizeof(uint32_t)) == 0);
    assert(count_painted(&a) > 0);

    canvas_clear(&b, 0x00000000);
    brush_engine_stroke(&b, 8, 16, 56, 16, &dyn, 0xFF00FF00, 4321);
    assert(memcmp(a.pixels, b.pixels, (size_t)64 * 32 * sizeof(uint32_t)) != 0);

    canvas_free(&a);
    canvas_free(&b);
}

static void test_glow_additive(void) {
    Canvas c = {0};
    BrushDynamics dyn = brush_dynamics_for_preset(VFX_BRUSH_GLOW, 6);
    uint32_t seed = 99;

    assert(canvas_init(&c, 32, 32));
    canvas_clear(&c, 0xFF202020);
    brush_engine_stamp(&c, 16, 16, &dyn, 0xFF4040FF, &seed);
    brush_engine_stamp(&c, 16, 16, &dyn, 0xFF4040FF, &seed);
    {
        uint32_t p = canvas_get_pixel(&c, 16, 16);
        assert((p & 0xFF) > 0x20);
    }
    canvas_free(&c);
}

static void test_airbrush_coverage(void) {
    Canvas c = {0};
    BrushDynamics dyn = brush_dynamics_for_preset(VFX_BRUSH_AIRBRUSH, 6);

    assert(canvas_init(&c, 64, 32));
    brush_engine_stroke(&c, 8, 16, 56, 16, &dyn, 0xFFFF00FF, 42);
    assert(count_painted(&c) > 50);
    canvas_free(&c);
}

int main(void) {
    test_presets();
    test_soft_round_falloff();
    test_stroke_determinism();
    test_glow_additive();
    test_airbrush_coverage();
    printf("brush engine selftest ok\n");
    return 0;
}
