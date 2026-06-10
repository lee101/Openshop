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

static void test_layer_stack_resize_and_crop(void) {
    LayerStack stack;

    assert(layer_stack_init(&stack, 8, 8, 0xFFFFFFFF));
    assert(layer_stack_add(&stack, "Top", 0x00000000) == 1);
    canvas_draw_rect_filled(&stack.layers[1].canvas, 2, 2, 5, 5, 0xFFFF0000);

    assert(layer_stack_resize_image(&stack, 16, 16));
    assert(stack.width == 16 && stack.height == 16);
    assert(stack.layers[0].canvas.width == 16);
    assert(canvas_get_pixel(&stack.layers[1].canvas, 7, 7) == 0xFFFF0000);

    assert(layer_stack_resize_canvas(&stack, 20, 20, 2, 2, 0xFFFFFFFF));
    assert(stack.width == 20);
    assert(canvas_get_pixel(&stack.layers[0].canvas, 0, 0) == 0xFFFFFFFF);
    assert(canvas_get_pixel(&stack.layers[1].canvas, 0, 0) == 0x00000000);
    assert(canvas_get_pixel(&stack.layers[1].canvas, 9, 9) == 0xFFFF0000);

    assert(layer_stack_crop(&stack, 5, 5, 14, 14, 0xFFFFFFFF));
    assert(stack.width == 10 && stack.height == 10);
    assert(canvas_get_pixel(&stack.layers[1].canvas, 4, 4) == 0xFFFF0000);

    assert(!layer_stack_crop(&stack, 50, 50, 60, 60, 0xFFFFFFFF));

    layer_stack_free(&stack);
}

static void test_layer_masks_and_clipping(void) {
    LayerStack stack;
    Canvas composite = {0};
    uint8_t coverage[16];

    assert(layer_stack_init(&stack, 4, 4, 0xFF000000));
    assert(layer_stack_add(&stack, "Top", 0x00000000) == 1);
    canvas_clear(&stack.layers[1].canvas, 0xFFFF0000);

    for (int i = 0; i < 16; i++) {
        coverage[i] = i < 8 ? 255 : 0;
    }
    assert(layer_stack_add_mask(&stack, 1, coverage));
    assert(!layer_stack_add_mask(&stack, 1, coverage));
    assert(stack.layers[1].mask_enabled);

    assert(canvas_init(&composite, 4, 4));
    layer_stack_composite(&stack, &composite, 0xFF000000);
    assert(canvas_get_pixel(&composite, 0, 0) == 0xFFFF0000);
    assert(canvas_get_pixel(&composite, 0, 3) == 0xFF000000);

    assert(layer_stack_set_mask_enabled(&stack, 1, 0));
    layer_stack_composite(&stack, &composite, 0xFF000000);
    assert(canvas_get_pixel(&composite, 0, 3) == 0xFFFF0000);
    assert(layer_stack_set_mask_enabled(&stack, 1, 1));

    assert(layer_stack_duplicate(&stack, 1, NULL) == 2);
    assert(stack.layers[2].mask.pixels != NULL);
    assert(stack.layers[2].mask_enabled);
    assert(layer_stack_delete(&stack, 2));

    assert(layer_stack_remove_mask(&stack, 1, 1));
    assert(stack.layers[1].mask.pixels == NULL);
    assert((canvas_get_pixel(&stack.layers[1].canvas, 0, 3) >> 24) == 0);
    assert((canvas_get_pixel(&stack.layers[1].canvas, 0, 0) >> 24) == 255);

    canvas_free(&composite);
    layer_stack_free(&stack);
}

static void test_clipping_mask(void) {
    LayerStack stack;
    Canvas composite = {0};

    assert(layer_stack_init(&stack, 4, 4, 0xFFFFFFFF));
    assert(layer_stack_add(&stack, "Base", 0x00000000) == 1);
    canvas_draw_rect_filled(&stack.layers[1].canvas, 0, 0, 1, 3, 0xFF00FF00);
    assert(layer_stack_add(&stack, "Clip", 0x00000000) == 2);
    canvas_clear(&stack.layers[2].canvas, 0xFFFF0000);

    assert(!layer_stack_set_clipping(&stack, 0, 1));
    assert(layer_stack_set_clipping(&stack, 2, 1));
    assert(stack.layers[2].clipping);

    assert(canvas_init(&composite, 4, 4));
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    assert(canvas_get_pixel(&composite, 0, 0) == 0xFFFF0000);
    assert(canvas_get_pixel(&composite, 3, 0) == 0xFFFFFFFF);

    assert(layer_stack_set_clipping(&stack, 2, 0));
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    assert(canvas_get_pixel(&composite, 3, 0) == 0xFFFF0000);

    canvas_free(&composite);
    layer_stack_free(&stack);
}

int main(void) {
    test_layer_masks_and_clipping();
    test_clipping_mask();
    test_layer_stack_resize_and_crop();
    test_blend_channels();
    test_blend_composite();
    test_blend_cycle_and_names();
    test_layer_stack_blend_composite();
    test_merge_down_respects_blend_mode();
    printf("blend selftest ok\n");
    return 0;
}
