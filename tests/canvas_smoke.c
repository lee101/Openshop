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
    if (layer_stack_show(&stack, 1)) {
        fprintf(stderr, "show active layer should report no-op when already visible\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_show_all(&stack)) {
        fprintf(stderr, "show all should be a no-op when nothing changes\n");
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
    if (!layer_stack_rename(&stack, 1, "Foreground Ink")) {
        fprintf(stderr, "rename layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[1].name, "Foreground Ink") != 0) {
        fprintf(stderr, "rename layer text mismatch\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_rename(&stack, 1, "Foreground Ink")) {
        fprintf(stderr, "rename should report no-op when name is unchanged\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_rename(&stack, 1, "")) {
        fprintf(stderr, "rename fallback failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[1].name, "Layer 2") != 0) {
        fprintf(stderr, "rename fallback name mismatch\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_rename(&stack, 1, "Foreground Ink")) {
        fprintf(stderr, "restore renamed layer failed\n");
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
    /* Fill up to MAX_LAYERS by adding overflow layers one at a time */
    for (int expected_idx = 3; expected_idx < MAX_LAYERS; expected_idx++) {
        if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != expected_idx) {
            fprintf(stderr, "stamp visible new layer %d failed\n", expected_idx);
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
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

    {
        Canvas tol_c;
        if (!canvas_init(&tol_c, 3, 3)) {
            fprintf(stderr, "tol fill canvas init failed\n");
            return 1;
        }
        canvas_clear(&tol_c, 0xFFFFFFFF);
        canvas_set_pixel_raw(&tol_c, 1, 0, 0xFFFEFEFE);
        canvas_set_pixel_raw(&tol_c, 1, 1, 0xFFFEFEFE);
        canvas_set_pixel_raw(&tol_c, 1, 2, 0xFFFEFEFE);
        canvas_flood_fill_tol(&tol_c, 0, 0, 0xFF000000, 5);
        if (!expect_pixel_eq("tol_fill_exact", canvas_get_pixel(&tol_c, 0, 0), 0xFF000000) ||
            !expect_pixel_eq("tol_fill_approx", canvas_get_pixel(&tol_c, 1, 1), 0xFF000000)) {
            canvas_free(&tol_c);
            return 1;
        }
        canvas_clear(&tol_c, 0xFFFFFFFF);
        canvas_set_pixel_raw(&tol_c, 1, 0, 0xFFFEFEFE);
        canvas_set_pixel_raw(&tol_c, 1, 1, 0xFFFEFEFE);
        canvas_set_pixel_raw(&tol_c, 1, 2, 0xFFFEFEFE);
        canvas_flood_fill_tol(&tol_c, 0, 0, 0xFF000000, 0);
        if (!expect_pixel_eq("tol0_fill_exact", canvas_get_pixel(&tol_c, 0, 0), 0xFF000000) ||
            !expect_pixel_eq("tol0_no_spill", canvas_get_pixel(&tol_c, 1, 1), 0xFFFEFEFE)) {
            canvas_free(&tol_c);
            return 1;
        }
        canvas_free(&tol_c);
    }

    {
        Canvas sharp;
        if (!canvas_init(&sharp, 3, 3)) {
            fprintf(stderr, "unsharp canvas init failed\n");
            return 1;
        }
        canvas_clear(&sharp, 0xFF808080);
        canvas_unsharp_mask(&sharp);
        if (!expect_pixel_eq("unsharp_flat", canvas_get_pixel(&sharp, 1, 1), 0xFF808080)) {
            canvas_free(&sharp);
            return 1;
        }
        canvas_set_pixel_raw(&sharp, 1, 1, 0x80808080);
        canvas_unsharp_mask(&sharp);
        if ((canvas_get_pixel(&sharp, 1, 1) >> 24) != 0x80) {
            fprintf(stderr, "unsharp_alpha_preserved failed\n");
            canvas_free(&sharp);
            return 1;
        }
        canvas_clear(&sharp, 0xFF000000);
        canvas_set_pixel_raw(&sharp, 1, 1, 0xFFFFFFFF);
        canvas_unsharp_mask(&sharp);
        if (!expect_pixel_eq("unsharp_center_clamp", canvas_get_pixel(&sharp, 1, 1), 0xFFFFFFFF)) {
            canvas_free(&sharp);
            return 1;
        }
        canvas_free(&sharp);
    }

    {
        Canvas edge;
        if (!canvas_init(&edge, 3, 1)) {
            fprintf(stderr, "edge sharpen canvas init failed\n");
            return 1;
        }
        canvas_set_pixel_raw(&edge, 0, 0, 0xFF404040);
        canvas_set_pixel_raw(&edge, 1, 0, 0xFF808080);
        canvas_set_pixel_raw(&edge, 2, 0, 0xFF404040);
        canvas_edge_sharpen(&edge);
        if (!expect_pixel_eq("edge_sharpen_center", canvas_get_pixel(&edge, 1, 0), 0xFFFFFFFF) ||
            !expect_pixel_eq("edge_sharpen_side", canvas_get_pixel(&edge, 0, 0), 0xFF000000)) {
            canvas_free(&edge);
            return 1;
        }
        canvas_set_pixel_raw(&edge, 1, 0, 0x80404040);
        canvas_edge_sharpen(&edge);
        if ((canvas_get_pixel(&edge, 1, 0) >> 24) != 0x80) {
            fprintf(stderr, "edge_sharpen_alpha_preserved failed\n");
            canvas_free(&edge);
            return 1;
        }
        canvas_free(&edge);
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

    /* Contrast tests: delta=16, fp=128+16=144
     * new_ch = 128 + ((ch-128)*fp) >> 7  */
    Canvas ctr;
    if (!canvas_init(&ctr, 1, 1)) {
        fprintf(stderr, "contrast canvas init failed\n");
        return 1;
    }
    /* light pixel R=G=B=200=0xC8: up → 128+(72*144>>7)=128+81=209=0xD1 */
    canvas_set_pixel_raw(&ctr, 0, 0, 0xFFC8C8C8);
    canvas_contrast_up(&ctr);
    if (!expect_pixel_eq("contrast_up_light", canvas_get_pixel(&ctr, 0, 0), 0xFFD1D1D1)) {
        canvas_free(&ctr);
        return 1;
    }
    /* light pixel 0xC8 down → 128+(72*112>>7)=128+63=191=0xBF */
    canvas_set_pixel_raw(&ctr, 0, 0, 0xFFC8C8C8);
    canvas_contrast_down(&ctr);
    if (!expect_pixel_eq("contrast_down_light", canvas_get_pixel(&ctr, 0, 0), 0xFFBFBFBF)) {
        canvas_free(&ctr);
        return 1;
    }
    /* mid-gray 0x80 must not change under either direction */
    canvas_set_pixel_raw(&ctr, 0, 0, 0xFF808080);
    canvas_contrast_up(&ctr);
    if (!expect_pixel_eq("contrast_up_midgray", canvas_get_pixel(&ctr, 0, 0), 0xFF808080)) {
        canvas_free(&ctr);
        return 1;
    }
    /* white must clamp to white after contrast_up */
    canvas_set_pixel_raw(&ctr, 0, 0, 0xFFFFFFFF);
    canvas_contrast_up(&ctr);
    if (!expect_pixel_eq("contrast_up_white_clamp", canvas_get_pixel(&ctr, 0, 0), 0xFFFFFFFF)) {
        canvas_free(&ctr);
        return 1;
    }
    canvas_free(&ctr);

    /* Hue-rotation tests */
    Canvas hue;
    if (!canvas_init(&hue, 1, 1)) {
        fprintf(stderr, "hue canvas init failed\n");
        return 1;
    }
    /* Red (hue=0) +120° → green (hue=120°) */
    canvas_set_pixel_raw(&hue, 0, 0, 0xFFFF0000);
    canvas_hue_rotate(&hue, 120);
    if (!expect_pixel_eq("hue_red_to_green", canvas_get_pixel(&hue, 0, 0), 0xFF00FF00)) {
        canvas_free(&hue);
        return 1;
    }
    /* Green (hue=120°) +120° → blue (hue=240°) */
    canvas_hue_rotate(&hue, 120);
    if (!expect_pixel_eq("hue_green_to_blue", canvas_get_pixel(&hue, 0, 0), 0xFF0000FF)) {
        canvas_free(&hue);
        return 1;
    }
    /* Blue (hue=240°) +120° → red (hue=360°=0°): full cycle */
    canvas_hue_rotate(&hue, 120);
    if (!expect_pixel_eq("hue_blue_to_red_cycle", canvas_get_pixel(&hue, 0, 0), 0xFFFF0000)) {
        canvas_free(&hue);
        return 1;
    }
    /* Gray: saturation=0, hue rotation must leave it unchanged */
    canvas_set_pixel_raw(&hue, 0, 0, 0xFF808080);
    canvas_hue_rotate(&hue, 90);
    if (!expect_pixel_eq("hue_gray_invariant", canvas_get_pixel(&hue, 0, 0), 0xFF808080)) {
        canvas_free(&hue);
        return 1;
    }
    /* Alpha must be preserved */
    canvas_set_pixel_raw(&hue, 0, 0, 0x80FF0000);
    canvas_hue_rotate(&hue, 120);
    {
        uint32_t p = canvas_get_pixel(&hue, 0, 0);
        if ((p >> 24) != 0x80) {
            fprintf(stderr, "hue_alpha_preserved: alpha expected 0x80 got 0x%02X\n", (p >> 24) & 0xFF);
            canvas_free(&hue);
            return 1;
        }
    }
    canvas_free(&hue);
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

    Canvas poster_test;
    if (!canvas_init(&poster_test, 4, 1)) {
        fprintf(stderr, "poster_test canvas init failed\n");
        return 1;
    }
    /*
     * 4-level posterize: bucket = v/64, output = bucket*85
     *   0xFF003264: R=0→0, G=50→0(bkt0*85),  B=100→85(bkt1*85) → 0xFF000055
     *   0xFF0096C8: R=0→0, G=150→170(bkt2*85), B=200→255(bkt3*85) → 0xFF00AAFF
     *   white 0xFFFFFFFF: all 255→255(bkt3*85) → 0xFFFFFFFF
     *   black 0xFF000000: all 0→0  → 0xFF000000
     */
    canvas_set_pixel_raw(&poster_test, 0, 0, 0xFF003264);
    canvas_set_pixel_raw(&poster_test, 1, 0, 0xFF0096C8);
    canvas_set_pixel_raw(&poster_test, 2, 0, 0xFFFFFFFF);
    canvas_set_pixel_raw(&poster_test, 3, 0, 0xFF000000);
    canvas_posterize(&poster_test);
    if (!expect_pixel_eq("posterize_px0",  canvas_get_pixel(&poster_test, 0, 0), 0xFF000055) ||
        !expect_pixel_eq("posterize_px1",  canvas_get_pixel(&poster_test, 1, 0), 0xFF00AAFF) ||
        !expect_pixel_eq("posterize_white", canvas_get_pixel(&poster_test, 2, 0), 0xFFFFFFFF) ||
        !expect_pixel_eq("posterize_black", canvas_get_pixel(&poster_test, 3, 0), 0xFF000000)) {
        canvas_free(&poster_test);
        return 1;
    }
    canvas_free(&poster_test);

    Canvas sepia_test;
    if (!canvas_init(&sepia_test, 1, 1)) {
        fprintf(stderr, "sepia_test canvas init failed\n");
        return 1;
    }
    /* white pixel: all channels 255 */
    canvas_set_pixel_raw(&sepia_test, 0, 0, 0xFFFFFFFF);
    canvas_sepia(&sepia_test);
    {
        uint32_t p = canvas_get_pixel(&sepia_test, 0, 0);
        uint8_t r = (uint8_t)((p >> 16) & 0xFF);
        uint8_t g = (uint8_t)((p >> 8) & 0xFF);
        uint8_t b = (uint8_t)(p & 0xFF);
        /* Sepia of white: R > G > B (warm brownish tone) */
        if (r < g || g < b) {
            fprintf(stderr, "sepia_white: expected R >= G >= B, got R=%u G=%u B=%u\n", r, g, b);
            canvas_free(&sepia_test);
            return 1;
        }
        /* Alpha must be preserved */
        if (((p >> 24) & 0xFF) != 0xFF) {
            fprintf(stderr, "sepia: alpha was not preserved\n");
            canvas_free(&sepia_test);
            return 1;
        }
    }
    canvas_free(&sepia_test);

    Canvas thresh_test;
    if (!canvas_init(&thresh_test, 3, 1)) {
        fprintf(stderr, "thresh_test canvas init failed\n");
        return 1;
    }
    canvas_set_pixel_raw(&thresh_test, 0, 0, 0xFFFFFFFF);
    canvas_set_pixel_raw(&thresh_test, 1, 0, 0xFF7F7F7F);
    canvas_set_pixel_raw(&thresh_test, 2, 0, 0xFF00FF00);
    canvas_threshold(&thresh_test);
    if (!expect_pixel_eq("threshold_white", canvas_get_pixel(&thresh_test, 0, 0), 0xFFFFFFFF) ||
        !expect_pixel_eq("threshold_midgray", canvas_get_pixel(&thresh_test, 1, 0), 0xFF000000) ||
        !expect_pixel_eq("threshold_green", canvas_get_pixel(&thresh_test, 2, 0), 0xFFFFFFFF)) {
        canvas_free(&thresh_test);
        return 1;
    }
    canvas_free(&thresh_test);

    Canvas bright_test;
    if (!canvas_init(&bright_test, 1, 1)) {
        fprintf(stderr, "bright_test canvas init failed\n");
        return 1;
    }
    canvas_set_pixel_raw(&bright_test, 0, 0, 0xFF808080);
    canvas_brightness(&bright_test, 30);
    if (!expect_pixel_eq("brightness_up", canvas_get_pixel(&bright_test, 0, 0), 0xFF9E9E9E)) {
        canvas_free(&bright_test);
        return 1;
    }
    canvas_set_pixel_raw(&bright_test, 0, 0, 0xFFC8C8C8);
    canvas_brightness(&bright_test, 100);
    if (!expect_pixel_eq("brightness_clamp_up", canvas_get_pixel(&bright_test, 0, 0), 0xFFFFFFFF)) {
        canvas_free(&bright_test);
        return 1;
    }
    canvas_set_pixel_raw(&bright_test, 0, 0, 0xFF505050);
    canvas_brightness(&bright_test, -100);
    if (!expect_pixel_eq("brightness_clamp_down", canvas_get_pixel(&bright_test, 0, 0), 0xFF000000)) {
        canvas_free(&bright_test);
        return 1;
    }
    canvas_set_pixel_raw(&bright_test, 0, 0, 0x00808080);
    canvas_brightness(&bright_test, 30);
    if (!expect_pixel_eq("brightness_skip_transparent", canvas_get_pixel(&bright_test, 0, 0), 0x00808080)) {
        canvas_free(&bright_test);
        return 1;
    }
    canvas_free(&bright_test);

    Canvas levels_test;
    if (!canvas_init(&levels_test, 3, 1)) {
        fprintf(stderr, "levels_test canvas init failed\n");
        return 1;
    }
    canvas_clear(&levels_test, 0xFF000000);
    canvas_set_pixel_raw(&levels_test, 0, 0, 0xFF0A800A);
    canvas_set_pixel_raw(&levels_test, 1, 0, 0xFF6E806E);
    canvas_set_pixel_raw(&levels_test, 2, 0, 0xFFD28000);
    canvas_auto_levels(&levels_test);
    {
        uint32_t p0 = canvas_get_pixel(&levels_test, 0, 0);
        uint32_t p2 = canvas_get_pixel(&levels_test, 2, 0);
        int p0_r = (int)((p0 >> 16) & 0xFF);
        int p0_g = (int)((p0 >> 8) & 0xFF);
        int p2_r = (int)((p2 >> 16) & 0xFF);
        int p2_g = (int)((p2 >> 8) & 0xFF);
        if (p0_r != 0 || p2_r != 255 || p0_g != 128 || p2_g != 128) {
            fprintf(stderr, "auto_levels failed: p0_r=%d p0_g=%d p2_r=%d p2_g=%d\n",
                p0_r, p0_g, p2_r, p2_g);
            canvas_free(&levels_test);
            return 1;
        }
    }
    canvas_free(&levels_test);

    if (!test_layers_basic()) {
        return 1;
    }

    /* --- canvas_desaturate test --- */
    Canvas grey;
    if (!canvas_init(&grey, 4, 4)) {
        fprintf(stderr, "grey canvas init failed\n");
        return 1;
    }
    /* Bright red pixel: R=255 G=0 B=0 A=255 → grey ≈ round(0.2126*255) = 54 */
    canvas_set_pixel_raw(&grey, 0, 0, 0xFFFF0000);
    /* Bright green pixel: R=0 G=255 B=0 A=255 → grey ≈ round(0.7152*255) = 182 */
    canvas_set_pixel_raw(&grey, 1, 0, 0xFF00FF00);
    /* Fully transparent pixel should stay transparent */
    canvas_set_pixel_raw(&grey, 2, 0, 0x00FF0000);
    canvas_desaturate(&grey);
    {
        uint32_t p0 = canvas_get_pixel(&grey, 0, 0);
        uint8_t r0 = (uint8_t)((p0 >> 16) & 0xFF);
        uint8_t g0 = (uint8_t)((p0 >> 8) & 0xFF);
        uint8_t b0 = (uint8_t)(p0 & 0xFF);
        if (r0 != g0 || g0 != b0) {
            fprintf(stderr, "desaturate_red: channels not equal: %02X %02X %02X\n", r0, g0, b0);
            canvas_free(&grey);
            return 1;
        }
        if (r0 < 50 || r0 > 58) {
            fprintf(stderr, "desaturate_red: expected ~54, got %d\n", r0);
            canvas_free(&grey);
            return 1;
        }
        uint32_t p1 = canvas_get_pixel(&grey, 1, 0);
        uint8_t r1 = (uint8_t)((p1 >> 16) & 0xFF);
        uint8_t g1 = (uint8_t)((p1 >> 8) & 0xFF);
        uint8_t b1 = (uint8_t)(p1 & 0xFF);
        if (r1 != g1 || g1 != b1) {
            fprintf(stderr, "desaturate_green: channels not equal: %02X %02X %02X\n", r1, g1, b1);
            canvas_free(&grey);
            return 1;
        }
        if (r1 < 178 || r1 > 186) {
            fprintf(stderr, "desaturate_green: expected ~182, got %d\n", r1);
            canvas_free(&grey);
            return 1;
        }
        uint32_t p2 = canvas_get_pixel(&grey, 2, 0);
        uint8_t a2 = (uint8_t)((p2 >> 24) & 0xFF);
        if (a2 != 0) {
            fprintf(stderr, "desaturate_transparent: alpha should be 0, got %d\n", a2);
            canvas_free(&grey);
            return 1;
        }
    }
    canvas_free(&grey);

    /* --- canvas_rotate_90cw test (square canvas) --- */
    Canvas rot;
    if (!canvas_init(&rot, 4, 4)) {
        fprintf(stderr, "rot canvas init failed\n");
        return 1;
    }
    canvas_clear(&rot, 0x00000000);
    /* Place a distinctive marker at top-left (0,0) */
    canvas_set_pixel_raw(&rot, 0, 0, 0xFF110000);
    /* After 90-degree CW rotation on a square canvas:
       (x,y) -> (H-1-y, x)  i.e. top-left (0,0) -> (3,0) bottom-left of new */
    canvas_rotate_90cw(&rot);
    if (!expect_pixel_eq("rotate90cw_origin", canvas_get_pixel(&rot, 3, 0), 0xFF110000)) {
        canvas_free(&rot);
        return 1;
    }
    /* And top-left of result should be transparent (was (0,3) in original which was cleared) */
    if (!expect_pixel_eq("rotate90cw_topleft_clear", canvas_get_pixel(&rot, 0, 0), 0x00000000)) {
        canvas_free(&rot);
        return 1;
    }
    canvas_free(&rot);

    /* --- canvas_rotate_90ccw test (square canvas) --- */
    Canvas rot2;
    if (!canvas_init(&rot2, 4, 4)) {
        fprintf(stderr, "rot2 canvas init failed\n");
        return 1;
    }
    canvas_clear(&rot2, 0x00000000);
    /* Place marker at top-right (3,0) */
    canvas_set_pixel_raw(&rot2, 3, 0, 0xFF002200);
    /* After 90-degree CCW on a square: (x,y) -> (y, W-1-x), so (3,0) -> (0,0) */
    canvas_rotate_90ccw(&rot2);
    if (!expect_pixel_eq("rotate90ccw_topright", canvas_get_pixel(&rot2, 0, 0), 0xFF002200)) {
        canvas_free(&rot2);
        return 1;
    }
    canvas_free(&rot2);

    printf("ok\n");
    return 0;
}
