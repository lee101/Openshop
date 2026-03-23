#include "../src/app_history.h"
#include "../src/canvas.h"
#include "../src/layers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_int_eq(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_pixel_eq(const char *label, uint32_t got, uint32_t want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

static int test_snapshot_apply_restores_lazy_canvas(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 8, 8, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "layer_stack_add failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.active_layer = 1;
    stack.solo_index = 1;
    if (!layer_stack_toggle_lock(&stack, 1) || !layer_stack_set_opacity(&stack, 1, 35)) {
        fprintf(stderr, "failed to prepare top layer state\n");
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 0;
    strncpy(stack.layers[0].name, "Base", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[1].name, "Solo", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    canvas_set_pixel(&stack.layers[0].canvas, 2, 2, 0xFF102030);
    canvas_set_pixel(&stack.layers[1].canvas, 2, 2, 0x80445566);
    uint32_t saved_base_pixel = canvas_get_pixel(&stack.layers[0].canvas, 2, 2);
    uint32_t saved_top_pixel = canvas_get_pixel(&stack.layers[1].canvas, 2, 2);

    Snapshot snapshot = {0};
    if (!snapshot_from_layers(&snapshot, &stack)) {
        fprintf(stderr, "snapshot_from_layers failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.active_layer = 0;
    stack.solo_index = -1;
    stack.layers[0].visible = 1;
    stack.layers[1].locked = 0;
    stack.layers[1].opacity_percent = 100;
    strncpy(stack.layers[1].name, "Mutated", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    canvas_free(&stack.layers[1].canvas);
    stack.layers[1].canvas.width = stack.width;
    stack.layers[1].canvas.height = stack.height;
    canvas_set_pixel(&stack.layers[0].canvas, 2, 2, 0xFFEEDDCC);

    if (!snapshot_apply(&snapshot, &stack)) {
        fprintf(stderr, "snapshot_apply failed\n");
        snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    if (!stack.layers[1].canvas.pixels ||
        !expect_int_eq("apply_active_layer", stack.active_layer, 1) ||
        !expect_int_eq("apply_solo_index", stack.solo_index, 1) ||
        !expect_int_eq("apply_base_visible", stack.layers[0].visible, 0) ||
        !expect_int_eq("apply_top_locked", stack.layers[1].locked, 1) ||
        !expect_int_eq("apply_top_opacity", stack.layers[1].opacity_percent, 35) ||
        strcmp(stack.layers[1].name, "Solo") != 0 ||
        !expect_pixel_eq("apply_base_pixel", canvas_get_pixel(&stack.layers[0].canvas, 2, 2), saved_base_pixel) ||
        !expect_pixel_eq("apply_top_pixel", canvas_get_pixel(&stack.layers[1].canvas, 2, 2), saved_top_pixel)) {
        if (strcmp(stack.layers[1].name, "Solo") != 0) {
            fprintf(stderr, "apply_top_name mismatch: got %s want Solo\n", stack.layers[1].name);
        }
        snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    snapshot_free(&snapshot);
    layer_stack_free(&stack);
    return 1;
}

static int test_snapshot_restore_moves_history_between_stacks(void) {
    LayerStack stack;
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count = 0;
    int redo_count = 0;

    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));

    if (!layer_stack_init(&stack, 8, 8, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "layer_stack_add failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel(&stack.layers[0].canvas, 1, 1, 0xFF111111);
    canvas_set_pixel(&stack.layers[1].canvas, 1, 1, 0x80222222);
    stack.layers[1].opacity_percent = 55;
    stack.active_layer = 1;
    uint32_t saved_base_pixel = canvas_get_pixel(&stack.layers[0].canvas, 1, 1);
    uint32_t saved_top_pixel = canvas_get_pixel(&stack.layers[1].canvas, 1, 1);
    snapshot_push(&stack, undo_stack, &undo_count, redo_stack, &redo_count);

    canvas_set_pixel(&stack.layers[0].canvas, 1, 1, 0xFFABCDEF);
    canvas_set_pixel(&stack.layers[1].canvas, 1, 1, 0x80334455);
    stack.layers[0].visible = 0;
    stack.layers[1].locked = 1;
    stack.layers[1].opacity_percent = 20;
    stack.solo_index = 1;
    stack.active_layer = 0;
    canvas_free(&stack.layers[1].canvas);
    stack.layers[1].canvas.width = stack.width;
    stack.layers[1].canvas.height = stack.height;

    if (!snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count)) {
        fprintf(stderr, "snapshot_restore failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    if (!stack.layers[1].canvas.pixels ||
        !expect_int_eq("restore_undo_count", undo_count, 0) ||
        !expect_int_eq("restore_redo_count", redo_count, 1) ||
        !expect_int_eq("restore_active_layer", stack.active_layer, 1) ||
        !expect_int_eq("restore_solo_index", stack.solo_index, -1) ||
        !expect_int_eq("restore_base_visible", stack.layers[0].visible, 1) ||
        !expect_int_eq("restore_top_locked", stack.layers[1].locked, 0) ||
        !expect_int_eq("restore_top_opacity", stack.layers[1].opacity_percent, 55) ||
        !expect_pixel_eq("restore_base_pixel", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), saved_base_pixel) ||
        !expect_pixel_eq("restore_top_pixel", canvas_get_pixel(&stack.layers[1].canvas, 1, 1), saved_top_pixel) ||
        !expect_int_eq("redo_snapshot_count", redo_stack[0].layer_count, 2) ||
        !expect_int_eq("redo_snapshot_active", redo_stack[0].active_layer, 0) ||
        !expect_int_eq("redo_snapshot_solo", redo_stack[0].solo_index, 1) ||
        !expect_int_eq("redo_snapshot_base_visible", redo_stack[0].visibility[0], 0) ||
        !expect_int_eq("redo_snapshot_top_locked", redo_stack[0].locked[1], 1) ||
        !expect_int_eq("redo_snapshot_top_opacity", redo_stack[0].opacity_percent[1], 20)) {
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    layer_stack_free(&stack);
    return 1;
}

static int test_snapshot_push_caps_history_and_clears_redo(void) {
    LayerStack stack;
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count = 0;
    int redo_count = 0;

    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF000001);
    snapshot_push(&stack, redo_stack, &redo_count, NULL, NULL);

    for (int i = 0; i < MAX_HISTORY + 2; i++) {
        canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF000100u + (uint32_t)i);
        stack.active_layer = i % stack.layer_count;
        snapshot_push(&stack, undo_stack, &undo_count, redo_stack, &redo_count);
    }

    if (!expect_int_eq("cap_undo_count", undo_count, MAX_HISTORY) ||
        !expect_int_eq("cap_redo_cleared", redo_count, 0) ||
        !expect_pixel_eq("cap_oldest_dropped", undo_stack[0].pixels[0], 0xFF000102) ||
        !expect_pixel_eq("cap_newest_kept", undo_stack[MAX_HISTORY - 1].pixels[0], 0xFF000115)) {
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    layer_stack_free(&stack);
    return 1;
}

int main(void) {
    if (!test_snapshot_apply_restores_lazy_canvas()) {
        return 1;
    }
    if (!test_snapshot_restore_moves_history_between_stacks()) {
        return 1;
    }
    if (!test_snapshot_push_caps_history_and_clears_redo()) {
        return 1;
    }
    return 0;
}
