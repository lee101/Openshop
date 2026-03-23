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
    if (!layer_stack_toggle_lock(&stack, 1) || !layer_stack_set_opacity(&stack, 1, 40)) {
        fprintf(stderr, "prepare show all state preservation failed\n");
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
    if (stack.solo_index != -1 || stack.active_layer != 1 || !stack.layers[0].visible || !stack.layers[1].visible) {
        fprintf(stderr, "show all bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[1].locked || stack.layers[1].opacity_percent != 40) {
        fprintf(stderr, "show all should preserve non-visibility layer state\n");
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
    if (!layer_stack_toggle_lock(&stack, 1) || !layer_stack_set_opacity(&stack, 1, 100)) {
        fprintf(stderr, "restore show all preserved state failed\n");
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
    if (!layer_stack_toggle_solo(&stack, 0)) {
        fprintf(stderr, "solo background layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "hide top layer for solo-preserving show failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1) || !stack.layers[1].visible || stack.solo_index != 0) {
        fprintf(stderr, "show should preserve solo state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("show_preserves_solo_composite", canvas_get_pixel(&composite, 8, 8), 0xFFFFFFFF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 0) || stack.solo_index != -1) {
        fprintf(stderr, "clear solo after show preservation failed\n");
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
    if (!layer_stack_show(&stack, 1) || layer_stack_add(&stack, "Unlocked Visible", 0x00000000) != 2) {
        fprintf(stderr, "setup hide and advance unlocked preference failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock(&stack, 0) || !stack.layers[0].locked) {
        fprintf(stderr, "lock visible fallback layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 1;
    if (!layer_stack_hide_and_advance(&stack, 1)) {
        fprintf(stderr, "hide and advance with unlocked preference failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.active_layer != 2 || stack.layers[1].visible) {
        fprintf(stderr, "hide and advance should prefer unlocked visible layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_unlock_all(&stack) || !layer_stack_delete(&stack, 2) || !layer_stack_show(&stack, 1)) {
        fprintf(stderr, "cleanup hide and advance unlocked preference failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "rehide top layer after unlocked preference failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1) || layer_stack_add(&stack, "Locked Fallback", 0x00000000) != 2) {
        fprintf(stderr, "setup hide and advance locked fallback failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock(&stack, 0) || !layer_stack_toggle_lock(&stack, 2)) {
        fprintf(stderr, "lock fallback layers failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 1;
    if (!layer_stack_hide_and_advance(&stack, 1)) {
        fprintf(stderr, "hide and advance with locked fallback failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.active_layer != 2 || stack.layers[1].visible) {
        fprintf(stderr, "hide and advance should fall back to locked visible layer when needed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_unlock_all(&stack) || !layer_stack_delete(&stack, 2) || !layer_stack_show(&stack, 1)) {
        fprintf(stderr, "cleanup hide and advance locked fallback failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "rehide top layer after locked fallback failed\n");
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
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 0;
    stack.active_layer = 0;
    if (layer_stack_cycle_visible(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "visible layer cycling forward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_visible(&stack, 1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "visible layer cycling forward wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_visible(&stack, -1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "visible layer cycling backward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].visible = 1;
    stack.layers[2].visible = 1;
    stack.layers[1].locked = 1;
    stack.layers[2].locked = 1;
    stack.active_layer = 0;
    if (layer_stack_cycle_unlocked(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "unlocked layer cycling forward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_unlocked(&stack, 1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "unlocked layer cycling forward wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_unlocked(&stack, -1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "unlocked layer cycling backward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[2].visible = 0;
    stack.active_layer = 0;
    if (layer_stack_cycle_editable_visible(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "editable visible layer cycling forward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_editable_visible(&stack, 1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "editable visible layer cycling forward wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_editable_visible(&stack, -1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "editable visible layer cycling backward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_edge(&stack, -1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_edge(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select top layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 0;
    stack.layers[2].visible = 0;
    if (layer_stack_select_edge_visible(&stack, -1) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "select bottom visible layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_edge_visible(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select top visible layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 1;
    if (layer_stack_select_edge_unlocked(&stack, -1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom unlocked layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[3].locked = 1;
    stack.layers[2].locked = 0;
    stack.layers[0].locked = 0;
    if (layer_stack_select_edge_unlocked(&stack, 1) != 2 || stack.active_layer != 2) {
        fprintf(stderr, "select top unlocked layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[3].locked = 0;
    if (layer_stack_select_edge_editable_visible(&stack, -1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select bottom editable visible layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[3].locked = 1;
    stack.layers[2].locked = 0;
    stack.layers[2].visible = 1;
    stack.layers[0].visible = 0;
    if (layer_stack_select_edge_editable_visible(&stack, 1) != 2 || stack.active_layer != 2) {
        fprintf(stderr, "select top editable visible layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 1;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 1;
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 0;
    stack.active_layer = 3;
    if (layer_stack_cycle_visible(&stack, 1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "visible cycling precedence baseline failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 3;
    if (layer_stack_cycle_unlocked(&stack, 1) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "unlocked cycling precedence baseline failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].visible = 0;
    stack.active_layer = 3;
    if (layer_stack_cycle_editable_visible(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "editable visible cycling should stay on current slot when it is the only editable visible layer in wrap order\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_edge_visible(&stack, -1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "visible edge selection precedence baseline failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_edge_editable_visible(&stack, -1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "editable visible edge selection should skip locked and hidden layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 1;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 1;
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 0;
    if (layer_stack_select_nth_editable_visible(&stack, 0) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "select first editable visible layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_nth_editable_visible(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select second editable visible layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_nth_editable_visible(&stack, 2) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "select nth editable visible layer should fail past end\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_nth_unlocked(&stack, 0) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "select first unlocked layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_nth_unlocked(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select second unlocked layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_nth_unlocked(&stack, 2) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "select nth unlocked layer should fail past end\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 1;
    stack.active_layer = 3;
    if (layer_stack_select_nth_unlocked(&stack, 0) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select nth unlocked layer should skip locked layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[3].locked = 1;
    if (layer_stack_select_nth_unlocked(&stack, 0) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "select nth unlocked layer should preserve active layer on failure\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 0;
    stack.layers[3].locked = 0;
    stack.layers[0].visible = 0;
    stack.layers[1].visible = 1;
    stack.layers[2].visible = 0;
    stack.layers[3].visible = 1;
    if (layer_stack_select_nth_visible(&stack, 0) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "select first visible layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_nth_visible(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select second visible layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_nth_visible(&stack, 2) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "select nth visible layer should fail past end\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 0;
    if (layer_stack_select_nth_unlocked(&stack, 0) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "select first unlocked layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_nth_unlocked(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select second unlocked layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_nth_unlocked(&stack, 2) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "select nth unlocked layer should fail past end\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].visible = 0;
    stack.active_layer = 3;
    if (layer_stack_select_nth_visible(&stack, 0) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select nth visible layer should skip hidden layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[3].visible = 0;
    if (layer_stack_select_nth_visible(&stack, 0) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "select nth visible layer should preserve active layer on failure\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[3].visible = 1;
    stack.layers[1].visible = 0;
    stack.active_layer = 3;
    if (layer_stack_select_nth_editable_visible(&stack, 0) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select nth editable visible layer should skip hidden editable layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[3].visible = 0;
    if (layer_stack_select_nth_editable_visible(&stack, 0) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "select nth editable visible layer should preserve active layer on failure\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 0;
    stack.layers[3].locked = 0;
    stack.layers[0].visible = 1;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 1;
    stack.layers[1].visible = 1;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 0;
    stack.layers[2].visible = 1;
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
    while (stack.layer_count < MAX_LAYERS) {
        int next = layer_stack_add(&stack, "Overflow Fill", 0x00000000);
        if (next != stack.layer_count - 1) {
            fprintf(stderr, "fill layers to max failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    if (stack.layer_count != MAX_LAYERS || stack.active_layer != MAX_LAYERS - 1) {
        fprintf(stderr, "fill layers to max bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[MAX_LAYERS - 1].visible = 0;
    stack.layers[MAX_LAYERS - 1].locked = 1;
    stack.layers[MAX_LAYERS - 1].opacity_percent = 35;
    stack.solo_index = 1;
    if (layer_stack_add(&stack, "Overflow Add", 0x00000000) != -1) {
        fprintf(stderr, "add should fail at max layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_insert(&stack, 1, "Overflow Insert", 0x00000000) != -1) {
        fprintf(stderr, "insert should fail at max layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_duplicate(&stack, 1, "Overflow Duplicate") != -1) {
        fprintf(stderr, "duplicate should fail at max layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_duplicate_below(&stack, 1, "Overflow Duplicate Below") != -1) {
        fprintf(stderr, "duplicate below should fail at max layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != MAX_LAYERS || stack.active_layer != MAX_LAYERS - 1 || stack.solo_index != 1) {
        fprintf(stderr, "failed add/insert/duplicate should preserve full stack bookkeeping\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[MAX_LAYERS - 1].visible || !stack.layers[MAX_LAYERS - 1].locked ||
        stack.layers[MAX_LAYERS - 1].opacity_percent != 35) {
        fprintf(stderr, "failed add/insert/duplicate should preserve top layer state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[MAX_LAYERS - 1].locked = 0;
    stack.solo_index = -1;
    while (stack.layer_count > 2) {
        if (!layer_stack_delete(&stack, stack.layer_count - 1)) {
            fprintf(stderr, "cleanup filled layers failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    if (stack.layer_count != 2 || strcmp(stack.layers[1].name, "Top") != 0) {
        fprintf(stderr, "cleanup filled layer order failed\n");
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
    if (layer_stack_add(&stack, "Lock Buddy", 0x00000000) < 0) {
        fprintf(stderr, "add extra layer for lock-others failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 1;
    if (!layer_stack_set_opacity(&stack, 1, 35)) {
        fprintf(stderr, "setup lock-others state preservation failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock_others(&stack, 1)) {
        fprintf(stderr, "toggle lock others on failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[1].locked || !stack.layers[0].locked || !stack.layers[2].locked || stack.active_layer != 1 || stack.layers[1].opacity_percent != 35) {
        fprintf(stderr, "toggle lock others did not lock non-active layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock_others(&stack, 1)) {
        fprintf(stderr, "toggle lock others off failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[0].locked || stack.layers[1].locked || stack.layers[2].locked) {
        fprintf(stderr, "toggle lock others did not restore lock state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.active_layer != 1 || stack.layers[1].opacity_percent != 35) {
        fprintf(stderr, "toggle lock others should preserve active-layer state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].visible = 0;
    stack.layers[1].locked = 1;
    stack.layers[2].opacity_percent = 45;
    stack.solo_index = 2;
    if (!layer_stack_toggle_visibility_others(&stack, 2)) {
        fprintf(stderr, "toggle visibility others on failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[2].visible || stack.layers[0].visible || stack.layers[1].visible || stack.solo_index != -1 || stack.active_layer != 1 || !stack.layers[1].locked || stack.layers[2].opacity_percent != 45) {
        fprintf(stderr, "toggle visibility others did not isolate the active layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility_others(&stack, 2)) {
        fprintf(stderr, "toggle visibility others off failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[0].visible || !stack.layers[1].visible || !stack.layers[2].visible || stack.solo_index != -1) {
        fprintf(stderr, "toggle visibility others did not restore visible state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.active_layer != 1 || !stack.layers[1].locked || stack.layers[2].opacity_percent != 45) {
        fprintf(stderr, "toggle visibility others should preserve non-visibility state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 0;
    stack.layers[2].opacity_percent = 100;
    if (!layer_stack_delete(&stack, 2)) {
        fprintf(stderr, "cleanup lock-others layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock(&stack, 0) || !layer_stack_toggle_lock(&stack, 1)) {
        fprintf(stderr, "setup unlock all failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1) || !layer_stack_toggle_solo(&stack, 1) || !layer_stack_set_opacity(&stack, 1, 40)) {
        fprintf(stderr, "setup unlock all state preservation failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 1;
    if (!layer_stack_unlock_all(&stack) || stack.layers[0].locked || stack.layers[1].locked) {
        fprintf(stderr, "unlock all layers failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 1 || stack.active_layer != 1 || stack.layers[1].visible || stack.layers[1].opacity_percent != 40) {
        fprintf(stderr, "unlock all should preserve non-lock layer state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 1) || !layer_stack_show(&stack, 1) || !layer_stack_set_opacity(&stack, 1, 100)) {
        fprintf(stderr, "restore unlock all preserved state failed\n");
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
    if (!layer_stack_set_opacity(&stack, 1, 150) || stack.layers[1].opacity_percent != 100) {
        fprintf(stderr, "opacity should clamp high values\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_set_opacity(&stack, 1, -10) || stack.layers[1].opacity_percent != 0) {
        fprintf(stderr, "opacity should clamp low values\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("opacity_zero_hides_layer", canvas_get_pixel(&composite, 0, 0), 0xFF0000FF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_set_opacity(&stack, 1, 100) || stack.layers[1].opacity_percent != 100) {
        fprintf(stderr, "opacity restore after clamp low failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("opacity_full_restores_layer", canvas_get_pixel(&composite, 0, 0), 0xFF00807F)) {
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
    stack.layers[0].visible = 0;
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
    if (!stack.layers[0].visible || stack.layers[0].opacity_percent != 100) {
        fprintf(stderr, "merge down should preserve visible result and reset opacity\n");
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
    stack.layers[0].visible = 0;
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
    if (!stack.layers[0].visible || stack.layers[0].opacity_percent != 100) {
        fprintf(stderr, "merge up should preserve visible result and reset opacity\n");
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
    if (layer_stack_duplicate_below(&stack, 1, "Background Copy Below") != 1) {
        fprintf(stderr, "duplicate below failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 3 || stack.active_layer != 1) {
        fprintf(stderr, "duplicate below bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[1].name, "Background Copy Below") != 0 || strcmp(stack.layers[2].name, "Background Copy") != 0) {
        fprintf(stderr, "duplicate below order failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("duplicate_below_copy_pixel", canvas_get_pixel(&stack.layers[1].canvas, 0, 0), 0xFF0040BF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 1) || stack.layer_count != 2 || strcmp(stack.layers[1].name, "Background Copy") != 0) {
        fprintf(stderr, "duplicate below cleanup failed\n");
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
    if (!layer_stack_set_opacity(&stack, 1, 35) || !layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "prepare duplicated layer state failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[1].opacity_percent != 35 || stack.layers[1].visible) {
        fprintf(stderr, "duplicate layer state setup did not stick\n");
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
    if (stack.layers[0].visible || stack.layers[0].opacity_percent != 35) {
        fprintf(stderr, "move layer down should preserve visibility and opacity\n");
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
    if (stack.layers[1].visible || stack.layers[1].opacity_percent != 35) {
        fprintf(stderr, "move layer up should preserve visibility and opacity\n");
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
    if (stack.active_layer != 1 || !stack.layers[1].locked || strcmp(stack.layers[1].name, "Background Copy") != 0 ||
        stack.solo_index != 1 || stack.layers[1].visible || stack.layers[1].opacity_percent != 35) {
        fprintf(stderr, "failed move beyond bounds should preserve bookkeeping\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_add(&stack, "Move Top", 0x00000000) != 2) {
        fprintf(stderr, "add extra layer for move-to failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move_to(&stack, 1, 2) || stack.active_layer != 2) {
        fprintf(stderr, "move layer to top failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[2].locked || strcmp(stack.layers[2].name, "Background Copy") != 0) {
        fprintf(stderr, "move layer to top bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[2].visible || stack.layers[2].opacity_percent != 35) {
        fprintf(stderr, "move layer to top should preserve visibility and opacity\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 2) {
        fprintf(stderr, "solo index did not follow move-to top\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move_to(&stack, 2, 0) || stack.active_layer != 0) {
        fprintf(stderr, "move layer to bottom failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[0].locked || strcmp(stack.layers[0].name, "Background Copy") != 0) {
        fprintf(stderr, "move layer to bottom bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[0].visible || stack.layers[0].opacity_percent != 35) {
        fprintf(stderr, "move layer to bottom should preserve visibility and opacity\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 0) {
        fprintf(stderr, "solo index did not follow move-to bottom\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move_to(&stack, 0, 99) || stack.active_layer != 2) {
        fprintf(stderr, "move layer to clamped top failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[2].locked || strcmp(stack.layers[2].name, "Background Copy") != 0 || stack.solo_index != 2 ||
        stack.layers[2].visible || stack.layers[2].opacity_percent != 35) {
        fprintf(stderr, "move layer to clamped top bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move_to(&stack, 2, -99) || stack.active_layer != 0) {
        fprintf(stderr, "move layer to clamped bottom failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[0].locked || strcmp(stack.layers[0].name, "Background Copy") != 0 || stack.solo_index != 0 ||
        stack.layers[0].visible || stack.layers[0].opacity_percent != 35) {
        fprintf(stderr, "move layer to clamped bottom bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_move_to(&stack, 0, -99)) {
        fprintf(stderr, "move layer to same clamped bottom should fail\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.active_layer != 0 || !stack.layers[0].locked || strcmp(stack.layers[0].name, "Background Copy") != 0 ||
        stack.solo_index != 0 || stack.layers[0].visible || stack.layers[0].opacity_percent != 35) {
        fprintf(stderr, "failed clamped move-to should preserve bookkeeping\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 2) || stack.layer_count != 2) {
        fprintf(stderr, "cleanup move-to layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.active_layer != 0 || !stack.layers[0].locked || stack.layers[0].visible || stack.layers[0].opacity_percent != 35) {
        fprintf(stderr, "delete after move should preserve shifted layer state\n");
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
    if (!layer_stack_toggle_lock(&stack, 0) || stack.layers[0].locked) {
        fprintf(stderr, "unlock duplicated layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 0)) {
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
    if (!stack.layers[0].visible || stack.layers[0].locked || stack.layers[0].opacity_percent != 100 || stack.solo_index != -1) {
        fprintf(stderr, "flatten should normalize surviving layer state\n");
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
    if (!stack.layers[1].visible || stack.layers[1].locked || stack.layers[1].opacity_percent != 100 || stack.solo_index != -1) {
        fprintf(stderr, "stamp visible should only normalize target visibility and opacity\n");
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
    if (!stack.layers[2].visible || stack.layers[2].locked || stack.layers[2].opacity_percent != 100 || stack.solo_index != -1) {
        fprintf(stderr, "stamp visible new should normalize new layer state\n");
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
    if (!stack.layers[3].visible || stack.layers[3].locked || stack.layers[3].opacity_percent != 100) {
        fprintf(stderr, "second stamp visible new layer state failed\n");
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
    if (stack.layer_count != MAX_LAYERS || stack.active_layer != MAX_LAYERS - 1 || !stack.layers[MAX_LAYERS - 1].visible ||
        stack.layers[MAX_LAYERS - 1].locked || stack.layers[MAX_LAYERS - 1].opacity_percent != 100) {
        fprintf(stderr, "full stamp visible new stack bookkeeping failed\n");
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
    if (stack.layer_count != MAX_LAYERS || stack.active_layer != MAX_LAYERS - 1) {
        fprintf(stderr, "failed stamp visible new should preserve full stack bookkeeping\n");
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

    printf("ok\n");
    return 0;
}
