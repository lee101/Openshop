#include "../src/canvas.h"
#include "../src/layers.h"
#include <stdio.h>

static int expect_pixel_eq(const char *label, uint32_t got, uint32_t want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

static int test_layers_basic(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 16, 16, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "layer_stack_add failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    Canvas composite;
    if (!canvas_init(&composite, 16, 16)) {
        fprintf(stderr, "composite init failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    Layer *active = layer_stack_active(&stack);
    canvas_draw_circle(&active->canvas, 8, 8, 3, 0x80FF0000);
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if ((canvas_get_pixel(&composite, 8, 8) & 0x00FFFFFF) == 0x00FFFFFF) {
        fprintf(stderr, "composite did not include top layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_stack_toggle_visibility(&stack, stack.active_layer)) {
        fprintf(stderr, "toggle visibility failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("hidden_top_layer", canvas_get_pixel(&composite, 8, 8), 0xFFFFFFFF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_stack_toggle_visibility(&stack, 0)) {
        fprintf(stderr, "background should not hide when last visible\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_stack_cycle(&stack, -1) != 0 || layer_stack_cycle(&stack, 1) != 1) {
        fprintf(stderr, "layer cycling failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "re-show top layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0xFF0000FF);
    canvas_clear(&stack.layers[1].canvas, 0x8000FF00);
    if (!layer_stack_merge_down(&stack, 1)) {
        fprintf(stderr, "merge down failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 1 || stack.active_layer != 0) {
        fprintf(stderr, "merge down bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("merge_down_blend", canvas_get_pixel(&composite, 0, 0), 0xFF00807F)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_stack_duplicate(&stack, 0, "Background Copy") != 1) {
        fprintf(stderr, "duplicate layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 2 || stack.active_layer != 1) {
        fprintf(stderr, "duplicate bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("duplicate_copy_pixel", canvas_get_pixel(&stack.layers[1].canvas, 0, 0), 0xFF00807F)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_set_pixel(&stack.layers[1].canvas, 0, 0, 0xFFFF00FF);
    if (!expect_pixel_eq("duplicate_independent", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF00807F)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 1)) {
        fprintf(stderr, "delete duplicated layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 1 || stack.active_layer != 0) {
        fprintf(stderr, "delete bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_delete(&stack, 0)) {
        fprintf(stderr, "should not delete final layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_free(&composite);
    layer_stack_free(&stack);
    return 1;
}

int main(void) {
    Canvas c;
    if (!canvas_init(&c, 64, 64)) {
        fprintf(stderr, "canvas_init failed\n");
        return 1;
    }
    canvas_clear(&c, 0xFFFFFFFF);
    canvas_draw_circle(&c, 32, 32, 8, 0xFF000000);
    canvas_draw_line(&c, 0, 0, 63, 63, 2, 0xFF00FF00);
    canvas_draw_rect_outline(&c, 5, 5, 20, 20, 1, 0xFF0000FF);
    canvas_draw_rect_filled(&c, 24, 8, 30, 14, 0xFF8844FF);
    canvas_draw_ellipse_outline(&c, 32, 32, 12, 6, 1, 0xFFFFFF00);
    canvas_draw_ellipse_filled(&c, 48, 18, 6, 4, 0xFF00FFFF);
    if (!canvas_flood_fill(&c, 1, 1, 0xFFFF0000)) {
        fprintf(stderr, "canvas_flood_fill failed\n");
        canvas_free(&c);
        return 1;
    }
    if (!expect_pixel_eq("filled_rect_center", canvas_get_pixel(&c, 27, 11), 0xFF8844FF)) {
        canvas_free(&c);
        return 1;
    }
    if (!expect_pixel_eq("filled_ellipse_center", canvas_get_pixel(&c, 48, 18), 0xFF00FFFF)) {
        canvas_free(&c);
        return 1;
    }

    // basic checksum to ensure drawing occurred
    unsigned long long sum = 0;
    for (int i = 0; i < c.width * c.height; i++) {
        sum += c.pixels[i];
    }
    canvas_free(&c);

    if (sum == 0) {
        fprintf(stderr, "unexpected checksum\n");
        return 1;
    }

    if (!canvas_init(&c, 2, 2)) {
        fprintf(stderr, "canvas_init blend test failed\n");
        return 1;
    }
    canvas_clear(&c, 0xFFFFFFFF);
    canvas_set_pixel(&c, 0, 0, 0x80000000);
    uint32_t blended = canvas_get_pixel(&c, 0, 0);
    if (blended != 0xFF7F7F7F && blended != 0xFF808080) {
        fprintf(stderr, "unexpected blended pixel: 0x%08X\n", blended);
        canvas_free(&c);
        return 1;
    }
    canvas_set_pixel(&c, 0, 0, 0x80000000);
    if (!expect_pixel_eq("double_blend", canvas_get_pixel(&c, 0, 0), 0xFF3F3F3F)) {
        canvas_free(&c);
        return 1;
    }
    canvas_set_pixel(&c, 0, 1, 0x00FF00FF);
    if (!expect_pixel_eq("transparent_noop", canvas_get_pixel(&c, 0, 1), 0xFFFFFFFF)) {
        canvas_free(&c);
        return 1;
    }
    canvas_set_pixel(&c, 1, 1, 0xFFFF0000);
    if (!expect_pixel_eq("opaque_write", canvas_get_pixel(&c, 1, 1), 0xFFFF0000)) {
        canvas_free(&c);
        return 1;
    }
    canvas_set_pixel_raw(&c, 1, 0, 0x00000000);
    if (!expect_pixel_eq("raw_clear", canvas_get_pixel(&c, 1, 0), 0x00000000)) {
        canvas_free(&c);
        return 1;
    }
    canvas_free(&c);

    Canvas transparent;
    if (!canvas_init(&transparent, 1, 1)) {
        fprintf(stderr, "transparent canvas init failed\n");
        return 1;
    }
    canvas_set_pixel(&transparent, 0, 0, 0x80FF0000);
    if (!expect_pixel_eq("blend_into_transparent", canvas_get_pixel(&transparent, 0, 0), 0x80800000)) {
        canvas_free(&transparent);
        return 1;
    }
    canvas_free(&transparent);

    Canvas transform;
    if (!canvas_init(&transform, 3, 2)) {
        fprintf(stderr, "transform canvas init failed\n");
        return 1;
    }
    canvas_clear(&transform, 0xFF000000);
    canvas_set_pixel(&transform, 0, 0, 0xFF010203);
    canvas_set_pixel(&transform, 1, 0, 0xFF111213);
    canvas_set_pixel(&transform, 2, 0, 0xFF212223);
    canvas_set_pixel(&transform, 0, 1, 0xFF313233);
    canvas_set_pixel(&transform, 1, 1, 0xFF414243);
    canvas_set_pixel(&transform, 2, 1, 0xFF515253);

    canvas_flip_horizontal(&transform);
    if (!expect_pixel_eq("flip_h_tl", canvas_get_pixel(&transform, 0, 0), 0xFF212223) ||
        !expect_pixel_eq("flip_h_tr", canvas_get_pixel(&transform, 2, 0), 0xFF010203) ||
        !expect_pixel_eq("flip_h_bl", canvas_get_pixel(&transform, 0, 1), 0xFF515253)) {
        canvas_free(&transform);
        return 1;
    }

    canvas_flip_vertical(&transform);
    if (!expect_pixel_eq("flip_v_tl", canvas_get_pixel(&transform, 0, 0), 0xFF515253) ||
        !expect_pixel_eq("flip_v_br", canvas_get_pixel(&transform, 2, 1), 0xFF010203)) {
        canvas_free(&transform);
        return 1;
    }

    canvas_rotate_180(&transform);
    if (!expect_pixel_eq("rotate_180_tl", canvas_get_pixel(&transform, 0, 0), 0xFF010203) ||
        !expect_pixel_eq("rotate_180_br", canvas_get_pixel(&transform, 2, 1), 0xFF515253)) {
        canvas_free(&transform);
        return 1;
    }

    canvas_rotate_180(&transform);
    canvas_invert_rgb(&transform);
    if (!expect_pixel_eq("invert_tl", canvas_get_pixel(&transform, 0, 0), 0xFFAEADAC)) {
        canvas_free(&transform);
        return 1;
    }
    if (!expect_pixel_eq("invert_br", canvas_get_pixel(&transform, 2, 1), 0xFFFEFDFC)) {
        canvas_free(&transform);
        return 1;
    }
    canvas_free(&transform);

    Canvas translated;
    if (!canvas_init(&translated, 4, 3)) {
        fprintf(stderr, "translate canvas init failed\n");
        return 1;
    }
    canvas_clear(&translated, 0xFF000000);
    canvas_set_pixel(&translated, 1, 1, 0xFF112233);
    canvas_set_pixel(&translated, 2, 1, 0xFF445566);
    canvas_translate(&translated, 1, -1, 0xFFFFFFFF);
    if (!expect_pixel_eq("translate_moved_a", canvas_get_pixel(&translated, 2, 0), 0xFF112233) ||
        !expect_pixel_eq("translate_moved_b", canvas_get_pixel(&translated, 3, 0), 0xFF445566) ||
        !expect_pixel_eq("translate_fill", canvas_get_pixel(&translated, 0, 2), 0xFFFFFFFF)) {
        canvas_free(&translated);
        return 1;
    }
    canvas_translate(&translated, -2, 2, 0xFFABCDEF);
    if (!expect_pixel_eq("translate_back", canvas_get_pixel(&translated, 0, 2), 0xFF112233) ||
        !expect_pixel_eq("translate_crop_fill", canvas_get_pixel(&translated, 3, 0), 0xFFABCDEF)) {
        canvas_free(&translated);
        return 1;
    }
    canvas_free(&translated);

    if (!test_layers_basic()) {
        return 1;
    }

    printf("ok\n");
    return 0;
}
