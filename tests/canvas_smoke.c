#include "../src/canvas.h"
#include "../src/layers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (!layer_stack_toggle_solo(&stack, 1)) {
        fprintf(stderr, "solo hidden layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show_all(&stack)) {
        fprintf(stderr, "show all failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1 || !stack.layers[0].visible || !stack.layers[1].visible) {
        fprintf(stderr, "show all bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if ((canvas_get_pixel(&composite, 8, 8) & 0x00FFFFFF) == 0x00FFFFFF) {
        fprintf(stderr, "show all did not restore visible composite\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "rehide top layer after show all failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1) || !stack.layers[1].visible) {
        fprintf(stderr, "show active layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "rehide top layer after show active failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1)) {
        fprintf(stderr, "restore top layer for hide-and-advance failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 1;
    if (!layer_stack_hide_and_advance(&stack, 1)) {
        fprintf(stderr, "hide and advance failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[1].visible || stack.active_layer != 0) {
        fprintf(stderr, "hide and advance bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1)) {
        fprintf(stderr, "restore top layer after hide and advance failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 1) || !layer_stack_hide_and_advance(&stack, 1)) {
        fprintf(stderr, "hide and advance from solo failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1 || stack.active_layer != 0) {
        fprintf(stderr, "hide and advance should clear solo and focus visible layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1)) {
        fprintf(stderr, "restore top layer after solo hide and advance failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "rehide top layer after hide and advance tests failed\n");
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
    stack.active_layer = 1;

    if (layer_stack_cycle(&stack, -1) != 0 || layer_stack_cycle(&stack, 1) != 1) {
        fprintf(stderr, "layer cycling failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_add(&stack, "Third", 0x00000000) != 2 || layer_stack_add(&stack, "Fourth", 0x00000000) != 3) {
        fprintf(stderr, "setup extended layer cycling failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 3;
    if (layer_stack_cycle(&stack, 1) != 0 || layer_stack_cycle(&stack, -1) != 3) {
        fprintf(stderr, "layer cycling wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 3) || !layer_stack_delete(&stack, 2)) {
        fprintf(stderr, "extended layer cycling cleanup failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_insert(&stack, 1, "Inserted", 0x00000000) != 1) {
        fprintf(stderr, "layer insert failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 3 || stack.active_layer != 1) {
        fprintf(stderr, "layer insert bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[1].name, "Inserted") != 0 || strcmp(stack.layers[2].name, "Top") != 0) {
        fprintf(stderr, "layer insert order failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 2)) {
        fprintf(stderr, "solo inserted stack top failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_insert(&stack, 2, "Solo Neighbor", 0x00000000) != 2) {
        fprintf(stderr, "layer insert with solo failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 3) {
        fprintf(stderr, "solo index did not shift with insert\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 3) || stack.solo_index != -1) {
        fprintf(stderr, "toggle solo off after insert failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 2) || !layer_stack_delete(&stack, 1)) {
        fprintf(stderr, "cleanup inserted layers failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 2 || strcmp(stack.layers[1].name, "Top") != 0) {
        fprintf(stderr, "cleanup inserted layer order failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_insert(&stack, 1, "Inserted Below", 0x00000000) != 1) {
        fprintf(stderr, "layer insert below failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 3 || stack.active_layer != 1) {
        fprintf(stderr, "layer insert below bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[0].name, "Background") != 0 || strcmp(stack.layers[1].name, "Inserted Below") != 0 || strcmp(stack.layers[2].name, "Top") != 0) {
        fprintf(stderr, "layer insert below order failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 1) || stack.layer_count != 2) {
        fprintf(stderr, "layer insert below cleanup failed\n");
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
    if (!layer_stack_toggle_solo(&stack, 1)) {
        fprintf(stderr, "toggle solo failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("solo_active_layer", canvas_get_pixel(&composite, 8, 8), 0xFFBF7F7F)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "hide solo layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("solo_hidden_active_layer", canvas_get_pixel(&composite, 8, 8), 0xFFBF7F7F)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "re-show solo layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 1) || stack.solo_index != -1) {
        fprintf(stderr, "toggle solo off failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock(&stack, 1) || !stack.layers[1].locked) {
        fprintf(stderr, "toggle lock on failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_clear_layer(&stack, 1, 0xFFABCDEF)) {
        fprintf(stderr, "clear should fail on locked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_delete(&stack, 1)) {
        fprintf(stderr, "delete should fail on locked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_merge_down(&stack, 1)) {
        fprintf(stderr, "merge should fail on locked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_into(&stack, 1, 0xFFFFFFFF)) {
        fprintf(stderr, "stamp should fail on locked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_flatten(&stack, 0xFFFFFFFF)) {
        fprintf(stderr, "flatten should fail when any layer is locked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock(&stack, 1) || stack.layers[1].locked) {
        fprintf(stderr, "toggle lock off failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0xFF0000FF);
    canvas_clear(&stack.layers[1].canvas, 0x8000FF00);
    if (!layer_stack_set_opacity(&stack, 1, 50)) {
        fprintf(stderr, "set opacity failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_set_opacity(&stack, 1, 100) || stack.layers[1].opacity_percent != 100) {
        fprintf(stderr, "reset opacity failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_set_opacity(&stack, 1, 50)) {
        fprintf(stderr, "restore opacity after reset failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
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
    if (!expect_pixel_eq("merge_down_blend", canvas_get_pixel(&composite, 0, 0), 0xFF0040BF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_add(&stack, "Upper Merge", 0x00000000) != 1) {
        fprintf(stderr, "add upper merge layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0xFF0000FF);
    canvas_clear(&stack.layers[1].canvas, 0x8000FF00);
    if (!layer_stack_set_opacity(&stack, 1, 50)) {
        fprintf(stderr, "set merge-up opacity failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 0;
    if (!layer_stack_merge_up(&stack, 0)) {
        fprintf(stderr, "merge up failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 1 || stack.active_layer != 0) {
        fprintf(stderr, "merge up bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("merge_up_blend", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF0040BF)) {
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
    if (!layer_stack_toggle_solo(&stack, 1)) {
        fprintf(stderr, "solo duplicated layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock(&stack, 1)) {
        fprintf(stderr, "lock duplicated layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[1].opacity_percent != 100) {
        fprintf(stderr, "duplicate opacity reset unexpectedly\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[1].locked) {
        fprintf(stderr, "lock flag did not persist on duplicated layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("duplicate_copy_pixel", canvas_get_pixel(&stack.layers[1].canvas, 0, 0), 0xFF0040BF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move(&stack, 1, -1) || stack.active_layer != 0) {
        fprintf(stderr, "move layer down failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 0) {
        fprintf(stderr, "solo index did not move with layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[0].name, "Background Copy") != 0 || strcmp(stack.layers[1].name, "Upper Merge") != 0) {
        fprintf(stderr, "move layer order failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move(&stack, 0, 1) || stack.active_layer != 1) {
        fprintf(stderr, "move layer up failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[1].locked) {
        fprintf(stderr, "lock flag did not move with layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_move(&stack, 1, 1)) {
        fprintf(stderr, "should not move top layer beyond bounds\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_set_pixel(&stack.layers[1].canvas, 0, 0, 0xFFFF00FF);
    if (!expect_pixel_eq("duplicate_independent", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF0040BF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock(&stack, 1) || stack.layers[1].locked) {
        fprintf(stderr, "unlock duplicated layer failed\n");
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
    if (stack.solo_index != -1) {
        fprintf(stderr, "solo index should clear after deleting solo layer\n");
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

    if (layer_stack_add(&stack, "Flatten Top", 0x00000000) != 1) {
        fprintf(stderr, "add flatten layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0xFF0000FF);
    canvas_clear(&stack.layers[1].canvas, 0x8000FF00);
    if (!layer_stack_set_opacity(&stack, 1, 50)) {
        fprintf(stderr, "set flatten opacity failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_flatten(&stack, 0xFFFFFFFF)) {
        fprintf(stderr, "flatten failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 1 || stack.active_layer != 0) {
        fprintf(stderr, "flatten bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("flatten_pixel", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF0040BF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_stack_add(&stack, "Stamp Target", 0x00000000) != 1) {
        fprintf(stderr, "add stamp layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0xFF123456);
    canvas_clear(&stack.layers[1].canvas, 0x8000FF00);
    if (!layer_stack_set_opacity(&stack, 1, 50)) {
        fprintf(stderr, "set stamp opacity failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_stamp_visible_into(&stack, 1, 0xFFFFFFFF)) {
        fprintf(stderr, "stamp visible failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("stamp_visible_pixel", canvas_get_pixel(&stack.layers[1].canvas, 0, 0), 0xFF0D6740) ||
        !expect_pixel_eq("stamp_preserve_source", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF123456)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_new(&stack, "Visible Stamp", 0xFFFFFFFF) != 2) {
        fprintf(stderr, "stamp visible new layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 3 || stack.active_layer != 2) {
        fprintf(stderr, "stamp visible new bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[2].name, "Visible Stamp") != 0) {
        fprintf(stderr, "stamp visible new name failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("stamp_visible_new_pixel", canvas_get_pixel(&stack.layers[2].canvas, 0, 0), 0xFF0D6740)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != 3) {
        fprintf(stderr, "second stamp visible new layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != 4) {
        fprintf(stderr, "third stamp visible new layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != 5) {
        fprintf(stderr, "fourth stamp visible new layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != 6) {
        fprintf(stderr, "fifth stamp visible new layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != 7) {
        fprintf(stderr, "sixth stamp visible new layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != -1) {
        fprintf(stderr, "stamp visible new should respect max layers\n");
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

    Canvas mask;
    if (!canvas_init(&mask, 9, 9)) {
        fprintf(stderr, "mask canvas init failed\n");
        return 1;
    }
    canvas_clear(&mask, 0x00000000);
    for (int y = -2; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            if (abs(x) <= 2 && abs(y) <= 2) {
                canvas_set_pixel_raw(&mask, 4 + x, 4 + y, 0xFFFFFFFF);
            }
        }
    }
    if (!expect_pixel_eq("mask_square_corner", canvas_get_pixel(&mask, 2, 2), 0xFFFFFFFF)) {
        canvas_free(&mask);
        return 1;
    }
    canvas_clear(&mask, 0x00000000);
    for (int y = -2; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            if (abs(x) + abs(y) <= 2) {
                canvas_set_pixel_raw(&mask, 4 + x, 4 + y, 0xFFFFFFFF);
            }
        }
    }
    if (!expect_pixel_eq("mask_diamond_center", canvas_get_pixel(&mask, 4, 4), 0xFFFFFFFF) ||
        !expect_pixel_eq("mask_diamond_corner", canvas_get_pixel(&mask, 2, 2), 0x00000000) ||
        !expect_pixel_eq("mask_diamond_top", canvas_get_pixel(&mask, 4, 2), 0xFFFFFFFF)) {
        canvas_free(&mask);
        return 1;
    }
    canvas_free(&mask);

    if (!test_layers_basic()) {
        return 1;
    }

    /* --- canvas_rotate_90_cw: square 2x2 ---
       Input layout (row-major, W=H=2):
         (0,0)=0xFF000001  (1,0)=0xFF000002
         (0,1)=0xFF000003  (1,1)=0xFF000004
       Expected after 90° CW (dest(dx,dy) <- src(dy, H-1-dx)):
         (0,0)=src(0,1)=0xFF000003  (1,0)=src(0,0)=0xFF000001
         (0,1)=src(1,1)=0xFF000004  (1,1)=src(1,0)=0xFF000002  */
    Canvas rot_cw;
    if (!canvas_init(&rot_cw, 2, 2)) {
        fprintf(stderr, "rot_cw init failed\n");
        return 1;
    }
    canvas_set_pixel_raw(&rot_cw, 0, 0, 0xFF000001);
    canvas_set_pixel_raw(&rot_cw, 1, 0, 0xFF000002);
    canvas_set_pixel_raw(&rot_cw, 0, 1, 0xFF000003);
    canvas_set_pixel_raw(&rot_cw, 1, 1, 0xFF000004);
    canvas_rotate_90_cw(&rot_cw);
    if (!expect_pixel_eq("rot_cw_00", canvas_get_pixel(&rot_cw, 0, 0), 0xFF000003) ||
        !expect_pixel_eq("rot_cw_10", canvas_get_pixel(&rot_cw, 1, 0), 0xFF000001) ||
        !expect_pixel_eq("rot_cw_01", canvas_get_pixel(&rot_cw, 0, 1), 0xFF000004) ||
        !expect_pixel_eq("rot_cw_11", canvas_get_pixel(&rot_cw, 1, 1), 0xFF000002)) {
        canvas_free(&rot_cw);
        return 1;
    }
    canvas_free(&rot_cw);

    /* --- canvas_rotate_90_ccw: square 2x2 ---
       Input layout (W=H=2):
         (0,0)=0xFF000001  (1,0)=0xFF000002
         (0,1)=0xFF000003  (1,1)=0xFF000004
       Expected after 90° CCW (dest(dx,dy) <- src(W-1-dy, dx)):
         (0,0)=src(W-1-0,0)=src(1,0)=0xFF000002
         (1,0)=src(W-1-0,1)=src(1,1)=0xFF000004
         (0,1)=src(W-1-1,0)=src(0,0)=0xFF000001
         (1,1)=src(W-1-1,1)=src(0,1)=0xFF000003  */
    Canvas rot_ccw;
    if (!canvas_init(&rot_ccw, 2, 2)) {
        fprintf(stderr, "rot_ccw init failed\n");
        return 1;
    }
    canvas_set_pixel_raw(&rot_ccw, 0, 0, 0xFF000001);
    canvas_set_pixel_raw(&rot_ccw, 1, 0, 0xFF000002);
    canvas_set_pixel_raw(&rot_ccw, 0, 1, 0xFF000003);
    canvas_set_pixel_raw(&rot_ccw, 1, 1, 0xFF000004);
    canvas_rotate_90_ccw(&rot_ccw);
    if (!expect_pixel_eq("rot_ccw_00", canvas_get_pixel(&rot_ccw, 0, 0), 0xFF000002) ||
        !expect_pixel_eq("rot_ccw_10", canvas_get_pixel(&rot_ccw, 1, 0), 0xFF000004) ||
        !expect_pixel_eq("rot_ccw_01", canvas_get_pixel(&rot_ccw, 0, 1), 0xFF000001) ||
        !expect_pixel_eq("rot_ccw_11", canvas_get_pixel(&rot_ccw, 1, 1), 0xFF000003)) {
        canvas_free(&rot_ccw);
        return 1;
    }
    canvas_free(&rot_ccw);

    /* --- canvas_grayscale: red pixel -> gray ---
       Input: 0xFF804020 (r=0x80, g=0x40, b=0x20)
       gray = (77*0x80 + 150*0x40 + 29*0x20) >> 8
            = (77*128 + 150*64 + 29*32) >> 8
            = (9856 + 9600 + 928) >> 8
            = 20384 >> 8 = 79 = 0x4F
       Expected: 0xFF4F4F4F */
    Canvas gray_c;
    if (!canvas_init(&gray_c, 1, 1)) {
        fprintf(stderr, "gray canvas init failed\n");
        return 1;
    }
    canvas_set_pixel_raw(&gray_c, 0, 0, 0xFF804020);
    canvas_grayscale(&gray_c);
    {
        uint32_t gp = canvas_get_pixel(&gray_c, 0, 0);
        uint8_t ga = (uint8_t)((gp >> 24) & 0xFF);
        uint8_t gr = (uint8_t)((gp >> 16) & 0xFF);
        uint8_t gg = (uint8_t)((gp >> 8) & 0xFF);
        uint8_t gb = (uint8_t)(gp & 0xFF);
        if (ga != 0xFF || gr != gg || gg != gb) {
            fprintf(stderr, "grayscale R!=G or G!=B or alpha changed: 0x%08X\n", gp);
            canvas_free(&gray_c);
            return 1;
        }
    }
    canvas_free(&gray_c);

    /* --- canvas_sepia: pure red pixel warm-tones ---
       Input: 0xFFFF0000 (r=255, g=0, b=0)
       out_r = (255*50 + 0 + 0) >> 7 = 12750 >> 7 = 99
       out_g = (255*45) >> 7 = 11475 >> 7 = 89
       out_b = (255*35) >> 7 = 8925 >> 7 = 69
       Expected: 0xFF63593F (approximately warm brown) */
    Canvas sepia_c;
    if (!canvas_init(&sepia_c, 1, 1)) {
        fprintf(stderr, "sepia canvas init failed\n");
        return 1;
    }
    canvas_set_pixel_raw(&sepia_c, 0, 0, 0xFFFF0000);
    canvas_sepia(&sepia_c);
    {
        uint32_t sp = canvas_get_pixel(&sepia_c, 0, 0);
        uint8_t sa = (uint8_t)((sp >> 24) & 0xFF);
        uint8_t sr = (uint8_t)((sp >> 16) & 0xFF);
        uint8_t sg = (uint8_t)((sp >> 8) & 0xFF);
        uint8_t sb = (uint8_t)(sp & 0xFF);
        /* sepia should be warm: R > G > B, alpha preserved */
        if (sa != 0xFF || sr <= sg || sg <= sb) {
            fprintf(stderr, "sepia warm tone failed (R>G>B expected): 0x%08X\n", sp);
            canvas_free(&sepia_c);
            return 1;
        }
    }
    canvas_free(&sepia_c);

    /* --- canvas_adjust_brightness ---
       Input: 0xFF808080 (mid-gray). delta = +20 -> each channel clamped to 128+20=148=0x94
       Expected: 0xFF949494 */
    Canvas bright_c;
    if (!canvas_init(&bright_c, 1, 1)) {
        fprintf(stderr, "brightness canvas init failed\n");
        return 1;
    }
    canvas_set_pixel_raw(&bright_c, 0, 0, 0xFF808080);
    canvas_adjust_brightness(&bright_c, 20);
    if (!expect_pixel_eq("brightness_up", canvas_get_pixel(&bright_c, 0, 0), 0xFF949494)) {
        canvas_free(&bright_c);
        return 1;
    }
    /* Darken back: 0x94 - 20 = 0x80; should round-trip */
    canvas_adjust_brightness(&bright_c, -20);
    if (!expect_pixel_eq("brightness_down", canvas_get_pixel(&bright_c, 0, 0), 0xFF808080)) {
        canvas_free(&bright_c);
        return 1;
    }
    /* Clamp test: brighten a near-white pixel; channels must not exceed 255 */
    canvas_set_pixel_raw(&bright_c, 0, 0, 0xFFF0F0F0);
    canvas_adjust_brightness(&bright_c, 50);
    {
        uint32_t cp = canvas_get_pixel(&bright_c, 0, 0);
        if ((cp & 0x00FFFFFF) != 0x00FFFFFF) {
            fprintf(stderr, "brightness clamp failed: 0x%08X\n", cp);
            canvas_free(&bright_c);
            return 1;
        }
    }
    canvas_free(&bright_c);

    /* --- canvas_adjust_contrast ---
       Input: 0xFF808080 (128,128,128 = midpoint). Contrast at midpoint is invariant.
       Expected: unchanged for any delta */
    Canvas contrast_c;
    if (!canvas_init(&contrast_c, 1, 1)) {
        fprintf(stderr, "contrast canvas init failed\n");
        return 1;
    }
    canvas_set_pixel_raw(&contrast_c, 0, 0, 0xFF808080);
    canvas_adjust_contrast(&contrast_c, 50);
    if (!expect_pixel_eq("contrast_midpoint_invariant", canvas_get_pixel(&contrast_c, 0, 0), 0xFF808080)) {
        canvas_free(&contrast_c);
        return 1;
    }
    /* A bright channel (200) with +100 delta (factor=200) should move away from 128 */
    canvas_set_pixel_raw(&contrast_c, 0, 0, 0xFFC8C8C8); /* 200 per channel */
    canvas_adjust_contrast(&contrast_c, 100);
    {
        uint32_t cp = canvas_get_pixel(&contrast_c, 0, 0);
        uint8_t cr = (uint8_t)((cp >> 16) & 0xFF);
        /* (200-128)*200/100+128 = 72*2+128 = 272 -> clamped to 255 */
        if (cr != 255) {
            fprintf(stderr, "contrast bright channel failed: got %d want 255\n", (int)cr);
            canvas_free(&contrast_c);
            return 1;
        }
    }
    canvas_free(&contrast_c);

    /* --- canvas_posterize: 2 levels (black/white) ---
       Input: 0xFF804020 (r=128, g=64, b=32)
       levels=2: r: 128*2/256=1 -> 1*255/1=255=0xFF
                 g: 64*2/256=0  -> 0*255/1=0=0x00
                 b: 32*2/256=0  -> 0=0x00
       Expected: 0xFFFF0000 */
    Canvas post_c;
    if (!canvas_init(&post_c, 1, 1)) {
        fprintf(stderr, "posterize canvas init failed\n");
        return 1;
    }
    canvas_set_pixel_raw(&post_c, 0, 0, 0xFF804020);
    canvas_posterize(&post_c, 2);
    if (!expect_pixel_eq("posterize_2lev", canvas_get_pixel(&post_c, 0, 0), 0xFFFF0000)) {
        canvas_free(&post_c);
        return 1;
    }
    /* A fully white pixel must remain white for any levels count. */
    canvas_set_pixel_raw(&post_c, 0, 0, 0xFFFFFFFF);
    canvas_posterize(&post_c, 4);
    if (!expect_pixel_eq("posterize_white_invariant", canvas_get_pixel(&post_c, 0, 0), 0xFFFFFFFF)) {
        canvas_free(&post_c);
        return 1;
    }
    canvas_free(&post_c);

    /* --- canvas_threshold ---
       A bright pixel (lum > 128) -> white; a dark pixel (lum < 128) -> black. */
    Canvas thresh_c;
    if (!canvas_init(&thresh_c, 1, 1)) {
        fprintf(stderr, "threshold canvas init failed\n");
        return 1;
    }
    /* White pixel: lum=255 >= 128 -> 0xFFFFFFFF */
    canvas_set_pixel_raw(&thresh_c, 0, 0, 0xFFFFFFFF);
    canvas_threshold(&thresh_c, 128);
    if (!expect_pixel_eq("threshold_white", canvas_get_pixel(&thresh_c, 0, 0), 0xFFFFFFFF)) {
        canvas_free(&thresh_c);
        return 1;
    }
    /* Dark pixel: 0xFF101010, lum=(77*16+150*16+29*16)>>8=(256*16)>>8=16 < 128 -> 0xFF000000 */
    canvas_set_pixel_raw(&thresh_c, 0, 0, 0xFF101010);
    canvas_threshold(&thresh_c, 128);
    if (!expect_pixel_eq("threshold_black", canvas_get_pixel(&thresh_c, 0, 0), 0xFF000000)) {
        canvas_free(&thresh_c);
        return 1;
    }
    canvas_free(&thresh_c);

    /* --- canvas_blur: 3x1 row, radius=1 ---
       Row: [0xFF000000, 0xFFFF0000, 0xFF000000]
       For center pixel (1,0) the 3x1 neighbourhood averages to:
         a=(0+255+0)/3=85, r=(0+255+0)/3=85, g=0, b=0 -> 0x55550000
       For edge pixels (0,0) and (2,0), neighbourhood is 2 wide:
         a=(0+255)/2=127, r=(0+255)/2=127 -> 0x7F7F0000 */
    Canvas blur_c;
    if (!canvas_init(&blur_c, 3, 1)) {
        fprintf(stderr, "blur canvas init failed\n");
        return 1;
    }
    canvas_set_pixel_raw(&blur_c, 0, 0, 0xFF000000);
    canvas_set_pixel_raw(&blur_c, 1, 0, 0xFFFF0000);
    canvas_set_pixel_raw(&blur_c, 2, 0, 0xFF000000);
    canvas_blur(&blur_c, 1);
    {
        uint32_t center = canvas_get_pixel(&blur_c, 1, 0);
        uint8_t cr = (uint8_t)((center >> 16) & 0xFF);
        uint8_t cg = (uint8_t)((center >> 8) & 0xFF);
        /* center red channel should be ~85 (255/3), green must stay 0 */
        if (cr != 85 || cg != 0) {
            fprintf(stderr, "blur center failed: 0x%08X (want r=85 g=0)\n", center);
            canvas_free(&blur_c);
            return 1;
        }
    }
    /* A uniform canvas must be unchanged by blur. */
    canvas_clear(&blur_c, 0xFF123456);
    canvas_blur(&blur_c, 1);
    if (!expect_pixel_eq("blur_uniform_invariant", canvas_get_pixel(&blur_c, 1, 0), 0xFF123456)) {
        canvas_free(&blur_c);
        return 1;
    }
    canvas_free(&blur_c);

    printf("ok\n");
    return 0;
}
