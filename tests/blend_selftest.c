#include "../src/blend.h"
#include "../src/layers.h"

#include <assert.h>
#include <stdio.h>

static void test_blend_channels(void) {
    assert(blend_mode_channel(BLEND_NORMAL, 10, 200) == 200);
    assert(blend_mode_channel(BLEND_MULTIPLY, 255, 128) == 128);
    assert(blend_mode_channel(BLEND_MULTIPLY, 0, 128) == 0);
    assert(blend_mode_channel(BLEND_SCREEN, 0, 128) == 128);
    assert(blend_mode_channel(BLEND_SCREEN, 255, 128) == 255);
    assert(blend_mode_channel(BLEND_DARKEN, 100, 200) == 100);
    assert(blend_mode_channel(BLEND_LIGHTEN, 100, 200) == 200);
    assert(blend_mode_channel(BLEND_LINEAR_DODGE, 200, 100) == 255);
    assert(blend_mode_channel(BLEND_LINEAR_BURN, 100, 100) == 0);
    assert(blend_mode_channel(BLEND_DIFFERENCE, 200, 50) == 150);
    assert(blend_mode_channel(BLEND_COLOR_DODGE, 128, 255) == 255);
    assert(blend_mode_channel(BLEND_COLOR_BURN, 128, 0) == 0);
    assert(blend_mode_channel(BLEND_EXCLUSION, 0, 128) == 128);
    assert(blend_mode_channel(BLEND_OVERLAY, 255, 128) == 255);
    assert(blend_mode_channel(BLEND_OVERLAY, 0, 128) == 0);
    assert(blend_mode_channel(BLEND_SOFT_LIGHT, 128, 128) >= 126);
    assert(blend_mode_channel(BLEND_SOFT_LIGHT, 128, 128) <= 130);
}

static void test_blend_composite(void) {
    uint32_t out = blend_mode_composite(0xFFFFFFFF, 0xFF808080, BLEND_MULTIPLY);
    assert(out == 0xFF808080);

    out = blend_mode_composite(0xFF000000, 0xFF808080, BLEND_SCREEN);
    assert(out == 0xFF808080);

    out = blend_mode_composite(0xFF102030, 0x00FFFFFF, BLEND_MULTIPLY);
    assert(out == 0xFF102030);

    out = blend_mode_composite(0x00000000, 0xFF334455, BLEND_MULTIPLY);
    assert(out == 0xFF334455);

    out = blend_mode_composite(0xFFFFFFFF, 0x80000000, BLEND_NORMAL);
    assert(((out >> 16) & 0xFF) > 0x70 && ((out >> 16) & 0xFF) < 0x90);
}

static void test_blend_cycle_and_names(void) {
    assert(blend_mode_cycle(BLEND_NORMAL, 1) == BLEND_MULTIPLY);
    assert(blend_mode_cycle(BLEND_NORMAL, -1) == BLEND_EXCLUSION);
    assert(blend_mode_cycle(BLEND_EXCLUSION, 1) == BLEND_NORMAL);
    assert(blend_mode_valid(BLEND_NORMAL));
    assert(blend_mode_valid(BLEND_EXCLUSION));
    assert(!blend_mode_valid(-1));
    assert(!blend_mode_valid(BLEND_MODE_COUNT));
    for (int mode = 0; mode < BLEND_MODE_COUNT; mode++) {
        assert(blend_mode_name((BlendMode)mode) != NULL);
    }
}

static void test_layer_stack_blend_composite(void) {
    LayerStack stack;
    Canvas composite = {0};

    assert(layer_stack_init(&stack, 4, 4, 0xFFFFFFFF));
    assert(layer_stack_add(&stack, "Multiply", 0x00000000) == 1);
    canvas_clear(&stack.layers[1].canvas, 0xFF808080);

    assert(layer_stack_set_blend_mode(&stack, 1, BLEND_MULTIPLY));
    assert(stack.layers[1].blend_mode == BLEND_MULTIPLY);
    assert(!layer_stack_set_blend_mode(&stack, 1, BLEND_MULTIPLY));
    assert(!layer_stack_set_blend_mode(&stack, 1, 999));

    assert(canvas_init(&composite, 4, 4));
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    assert(canvas_get_pixel(&composite, 0, 0) == 0xFF808080);

    assert(layer_stack_set_blend_mode(&stack, 1, BLEND_SCREEN));
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    assert(canvas_get_pixel(&composite, 0, 0) == 0xFFFFFFFF);

    assert(layer_stack_cycle_blend_mode(&stack, 1, 1));
    assert(stack.layers[1].blend_mode == BLEND_OVERLAY);

    assert(layer_stack_duplicate(&stack, 1, NULL) == 2);
    assert(stack.layers[2].blend_mode == BLEND_OVERLAY);

    assert(layer_stack_set_blend_mode(&stack, 2, BLEND_MULTIPLY));
    assert(layer_stack_merge_down(&stack, 2));
    assert(stack.layers[1].blend_mode == BLEND_NORMAL);

    canvas_free(&composite);
    layer_stack_free(&stack);
}

static void test_merge_down_respects_blend_mode(void) {
    LayerStack stack;

    assert(layer_stack_init(&stack, 2, 2, 0xFFFFFFFF));
    assert(layer_stack_add(&stack, "Top", 0x00000000) == 1);
    canvas_clear(&stack.layers[0].canvas, 0xFFFFFFFF);
    canvas_clear(&stack.layers[1].canvas, 0xFF404040);
    assert(layer_stack_set_blend_mode(&stack, 1, BLEND_MULTIPLY));
    assert(layer_stack_merge_down(&stack, 1));
    assert(stack.layer_count == 1);
    assert(canvas_get_pixel(&stack.layers[0].canvas, 0, 0) == 0xFF404040);
    layer_stack_free(&stack);
}

int main(void) {
    test_blend_channels();
    test_blend_composite();
    test_blend_cycle_and_names();
    test_layer_stack_blend_composite();
    test_merge_down_respects_blend_mode();
    printf("blend selftest ok\n");
    return 0;
}
