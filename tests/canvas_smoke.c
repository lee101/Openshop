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

    /* --- 90-degree rotation tests (square canvas) --- */
    Canvas rot;
    if (!canvas_init(&rot, 3, 3)) {
        fprintf(stderr, "rot canvas init failed\n");
        return 1;
    }
    /* Fill a 3×3 grid with distinct values:
     * (0,0)=0xA (1,0)=0xB (2,0)=0xC
     * (0,1)=0xD (1,1)=0xE (2,1)=0xF
     * (0,2)=0x1 (1,2)=0x2 (2,2)=0x3  */
    canvas_set_pixel_raw(&rot, 0, 0, 0xFF0A0A0A);
    canvas_set_pixel_raw(&rot, 1, 0, 0xFF0B0B0B);
    canvas_set_pixel_raw(&rot, 2, 0, 0xFF0C0C0C);
    canvas_set_pixel_raw(&rot, 0, 1, 0xFF0D0D0D);
    canvas_set_pixel_raw(&rot, 1, 1, 0xFF0E0E0E);
    canvas_set_pixel_raw(&rot, 2, 1, 0xFF0F0F0F);
    canvas_set_pixel_raw(&rot, 0, 2, 0xFF010101);
    canvas_set_pixel_raw(&rot, 1, 2, 0xFF020202);
    canvas_set_pixel_raw(&rot, 2, 2, 0xFF030303);

    /* After 90° CW:
     * (0,0)=0x1 (1,0)=0xD (2,0)=0xA
     * (0,1)=0x2 (1,1)=0xE (2,1)=0xB
     * (0,2)=0x3 (1,2)=0xF (2,2)=0xC  */
    canvas_rotate_90_cw(&rot, 0x00000000);
    if (!expect_pixel_eq("rot90cw_00", canvas_get_pixel(&rot, 0, 0), 0xFF010101) ||
        !expect_pixel_eq("rot90cw_10", canvas_get_pixel(&rot, 1, 0), 0xFF0D0D0D) ||
        !expect_pixel_eq("rot90cw_20", canvas_get_pixel(&rot, 2, 0), 0xFF0A0A0A) ||
        !expect_pixel_eq("rot90cw_22", canvas_get_pixel(&rot, 2, 2), 0xFF0C0C0C)) {
        canvas_free(&rot);
        return 1;
    }

    /* Applying CW × 3 should equal CCW × 1. Reset and apply CCW once. */
    canvas_set_pixel_raw(&rot, 0, 0, 0xFF0A0A0A);
    canvas_set_pixel_raw(&rot, 1, 0, 0xFF0B0B0B);
    canvas_set_pixel_raw(&rot, 2, 0, 0xFF0C0C0C);
    canvas_set_pixel_raw(&rot, 0, 1, 0xFF0D0D0D);
    canvas_set_pixel_raw(&rot, 1, 1, 0xFF0E0E0E);
    canvas_set_pixel_raw(&rot, 2, 1, 0xFF0F0F0F);
    canvas_set_pixel_raw(&rot, 0, 2, 0xFF010101);
    canvas_set_pixel_raw(&rot, 1, 2, 0xFF020202);
    canvas_set_pixel_raw(&rot, 2, 2, 0xFF030303);

    /* After 90° CCW:
     * (0,0)=0xC (1,0)=0xF (2,0)=0x3
     * (0,1)=0xB (1,1)=0xE (2,1)=0x2
     * (0,2)=0xA (1,2)=0xD (2,2)=0x1  */
    canvas_rotate_90_ccw(&rot, 0x00000000);
    if (!expect_pixel_eq("rot90ccw_00", canvas_get_pixel(&rot, 0, 0), 0xFF0C0C0C) ||
        !expect_pixel_eq("rot90ccw_20", canvas_get_pixel(&rot, 2, 0), 0xFF030303) ||
        !expect_pixel_eq("rot90ccw_02", canvas_get_pixel(&rot, 0, 2), 0xFF0A0A0A) ||
        !expect_pixel_eq("rot90ccw_22", canvas_get_pixel(&rot, 2, 2), 0xFF010101)) {
        canvas_free(&rot);
        return 1;
    }

    /* CW followed by CCW must restore the original image. */
    canvas_rotate_90_cw(&rot, 0x00000000);
    if (!expect_pixel_eq("rot_roundtrip_00", canvas_get_pixel(&rot, 0, 0), 0xFF0A0A0A) ||
        !expect_pixel_eq("rot_roundtrip_22", canvas_get_pixel(&rot, 2, 2), 0xFF030303)) {
        canvas_free(&rot);
        return 1;
    }
    canvas_free(&rot);

    /* --- grayscale test --- */
    Canvas gs;
    if (!canvas_init(&gs, 3, 1)) {
        fprintf(stderr, "gs canvas init failed\n");
        return 1;
    }
    canvas_set_pixel_raw(&gs, 0, 0, 0xFFFF0000); /* pure red   → luma 76 */
    canvas_set_pixel_raw(&gs, 1, 0, 0xFF00FF00); /* pure green → luma 149 */
    canvas_set_pixel_raw(&gs, 2, 0, 0xFF0000FF); /* pure blue  → luma 28 */
    canvas_grayscale(&gs);
    if (!expect_pixel_eq("gray_red",   canvas_get_pixel(&gs, 0, 0), 0xFF4C4C4C) ||
        !expect_pixel_eq("gray_green", canvas_get_pixel(&gs, 1, 0), 0xFF959595) ||
        !expect_pixel_eq("gray_blue",  canvas_get_pixel(&gs, 2, 0), 0xFF1C1C1C)) {
        canvas_free(&gs);
        return 1;
    }
    /* Alpha must be preserved */
    canvas_set_pixel_raw(&gs, 0, 0, 0x80FF0000);
    canvas_grayscale(&gs);
    if ((canvas_get_pixel(&gs, 0, 0) >> 24) != 0x80) {
        fprintf(stderr, "gray_alpha_preserved failed\n");
        canvas_free(&gs);
        return 1;
    }
    canvas_free(&gs);

    /* --- brightness adjustment tests --- */
    Canvas bright;
    if (!canvas_init(&bright, 2, 1)) {
        fprintf(stderr, "bright canvas init failed\n");
        return 1;
    }
    canvas_set_pixel_raw(&bright, 0, 0, 0xFF808080); /* mid-gray */
    canvas_set_pixel_raw(&bright, 1, 0, 0xFFFF1000); /* near-max red, low channels */

    /* +50 brightness */
    canvas_adjust_brightness(&bright, 50);
    /* 0x80=128 + 50 = 178 = 0xB2 */
    if (!expect_pixel_eq("bright_plus50", canvas_get_pixel(&bright, 0, 0), 0xFFB2B2B2)) {
        canvas_free(&bright);
        return 1;
    }
    /* red: 255+50 clamped to 255; green: 16+50=66=0x42; blue: 0+50=50=0x32 */
    if (!expect_pixel_eq("bright_plus50_clamp", canvas_get_pixel(&bright, 1, 0), 0xFFFF4232)) {
        canvas_free(&bright);
        return 1;
    }

    /* -200 brightness on mid-gray: 178-200 = -22, clamped to 0 */
    canvas_adjust_brightness(&bright, -200);
    if (!expect_pixel_eq("bright_clamp_zero", canvas_get_pixel(&bright, 0, 0), 0xFF000000)) {
        canvas_free(&bright);
        return 1;
    }

    /* alpha is preserved by brightness */
    canvas_set_pixel_raw(&bright, 0, 0, 0x80404040);
    canvas_adjust_brightness(&bright, 10);
    if ((canvas_get_pixel(&bright, 0, 0) >> 24) != 0x80) {
        fprintf(stderr, "bright_alpha_preserved failed\n");
        canvas_free(&bright);
        return 1;
    }
    canvas_free(&bright);

    /* --- contrast adjustment tests --- */
    Canvas con;
    if (!canvas_init(&con, 3, 1)) {
        fprintf(stderr, "con canvas init failed\n");
        return 1;
    }
    /* mid-gray is invariant under any contrast change */
    canvas_set_pixel_raw(&con, 0, 0, 0xFF808080);
    canvas_adjust_contrast(&con, 50);
    if (!expect_pixel_eq("contrast_midgray_invariant", canvas_get_pixel(&con, 0, 0), 0xFF808080)) {
        canvas_free(&con);
        return 1;
    }

    /* channel=200=0xC8, step=+50: (72*150)/100 + 128 = 108+128 = 236 = 0xEC */
    canvas_set_pixel_raw(&con, 1, 0, 0xFFC8C8C8);
    canvas_adjust_contrast(&con, 50);
    if (!expect_pixel_eq("contrast_plus50", canvas_get_pixel(&con, 1, 0), 0xFFECECEC)) {
        canvas_free(&con);
        return 1;
    }

    /* channel=0, step=+50: (-128*150)/100 + 128 = -192+128 = -64, clamped to 0 */
    canvas_set_pixel_raw(&con, 2, 0, 0xFF000000);
    canvas_adjust_contrast(&con, 50);
    if (!expect_pixel_eq("contrast_clamp_low", canvas_get_pixel(&con, 2, 0), 0xFF000000)) {
        canvas_free(&con);
        return 1;
    }

    /* alpha preserved through contrast */
    canvas_set_pixel_raw(&con, 0, 0, 0x7F808080);
    canvas_adjust_contrast(&con, 20);
    if ((canvas_get_pixel(&con, 0, 0) >> 24) != 0x7F) {
        fprintf(stderr, "contrast_alpha_preserved failed\n");
        canvas_free(&con);
        return 1;
    }
    canvas_free(&con);

    /* --- posterize tests (levels=4) --- */
    Canvas post;
    if (!canvas_init(&post, 4, 1)) {
        fprintf(stderr, "post canvas init failed\n");
        return 1;
    }
    /* With 4 levels the lookup is: [0,64)→0, [64,128)→85, [128,192)→170, [192,256)→255 */
    canvas_set_pixel_raw(&post, 0, 0, 0xFF000000); /* all 0   → bucket 0 → 0 */
    canvas_set_pixel_raw(&post, 1, 0, 0xFF404040); /* all 64  → bucket 1 → 85=0x55 */
    canvas_set_pixel_raw(&post, 2, 0, 0xFF808080); /* all 128 → bucket 2 → 170=0xAA */
    canvas_set_pixel_raw(&post, 3, 0, 0xFFC0C0C0); /* all 192 → bucket 3 → 255=0xFF */
    canvas_posterize(&post, 4);
    if (!expect_pixel_eq("post_0",   canvas_get_pixel(&post, 0, 0), 0xFF000000) ||
        !expect_pixel_eq("post_64",  canvas_get_pixel(&post, 1, 0), 0xFF555555) ||
        !expect_pixel_eq("post_128", canvas_get_pixel(&post, 2, 0), 0xFFAAAAAA) ||
        !expect_pixel_eq("post_192", canvas_get_pixel(&post, 3, 0), 0xFFFFFFFF)) {
        canvas_free(&post);
        return 1;
    }
    /* alpha must be preserved */
    canvas_set_pixel_raw(&post, 0, 0, 0x80404040);
    canvas_posterize(&post, 4);
    if ((canvas_get_pixel(&post, 0, 0) >> 24) != 0x80) {
        fprintf(stderr, "post_alpha_preserved failed\n");
        canvas_free(&post);
        return 1;
    }
    canvas_free(&post);

    /* --- sepia tests --- */
    Canvas sepia;
    if (!canvas_init(&sepia, 2, 1)) {
        fprintf(stderr, "sepia canvas init failed\n");
        return 1;
    }
    /* mid-gray: (346*128)>>8=173=0xAD  (308*128)>>8=154=0x9A  (241*128)>>8=120=0x78 */
    canvas_set_pixel_raw(&sepia, 0, 0, 0xFF808080);
    canvas_sepia(&sepia);
    if (!expect_pixel_eq("sepia_midgray", canvas_get_pixel(&sepia, 0, 0), 0xFFAD9A78)) {
        canvas_free(&sepia);
        return 1;
    }
    /* black stays black */
    canvas_set_pixel_raw(&sepia, 1, 0, 0xFF000000);
    canvas_sepia(&sepia);
    if (!expect_pixel_eq("sepia_black", canvas_get_pixel(&sepia, 1, 0), 0xFF000000)) {
        canvas_free(&sepia);
        return 1;
    }
    /* alpha must be preserved */
    canvas_set_pixel_raw(&sepia, 0, 0, 0x80808080);
    canvas_sepia(&sepia);
    if ((canvas_get_pixel(&sepia, 0, 0) >> 24) != 0x80) {
        fprintf(stderr, "sepia_alpha_preserved failed\n");
        canvas_free(&sepia);
        return 1;
    }
    canvas_free(&sepia);

    /* --- sharpen tests --- */
    Canvas sharp;
    if (!canvas_init(&sharp, 5, 5)) {
        fprintf(stderr, "sharp canvas init failed\n");
        return 1;
    }
    /* A flat uniform canvas must be invariant under sharpening:
     * 5*c - c - c - c - c = c for any constant c. */
    canvas_clear(&sharp, 0xFF808080);
    canvas_sharpen(&sharp);
    if (!expect_pixel_eq("sharpen_flat", canvas_get_pixel(&sharp, 2, 2), 0xFF808080)) {
        canvas_free(&sharp);
        return 1;
    }
    /* Border pixels are left unchanged; check corner */
    if (!expect_pixel_eq("sharpen_border", canvas_get_pixel(&sharp, 0, 0), 0xFF808080)) {
        canvas_free(&sharp);
        return 1;
    }
    /* An isolated bright pixel on a black background should stay clamped to 0xFF.
     * center=255, neighbors=0 → 5*255 = 1275, clamped to 255. */
    canvas_clear(&sharp, 0xFF000000);
    canvas_set_pixel_raw(&sharp, 2, 2, 0xFFFFFFFF);
    canvas_sharpen(&sharp);
    if (!expect_pixel_eq("sharpen_bright_center", canvas_get_pixel(&sharp, 2, 2), 0xFFFFFFFF)) {
        canvas_free(&sharp);
        return 1;
    }
    /* alpha must be preserved on interior pixels */
    canvas_clear(&sharp, 0x80808080);
    canvas_sharpen(&sharp);
    if ((canvas_get_pixel(&sharp, 2, 2) >> 24) != 0x80) {
        fprintf(stderr, "sharpen_alpha_preserved failed\n");
        canvas_free(&sharp);
        return 1;
    }
    canvas_free(&sharp);

    if (!test_layers_basic()) {
        return 1;
    }

    printf("ok\n");
    return 0;
}
