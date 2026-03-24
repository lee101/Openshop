#include "../src/history_state.h"
#include "../src/layers.h"
#include <stdio.h>
#include <string.h>

#define TEST_HISTORY_CAPACITY 4

static int expect_name(const LayerStack *stack, int index, const char *want, const char *label) {
    if (strcmp(stack->layers[index].name, want) != 0) {
        fprintf(stderr, "%s mismatch: got %s want %s\n", label, stack->layers[index].name, want);
        return 0;
    }
    return 1;
}

static int expect_pixel(const LayerStack *stack, int layer_index, int x, int y, uint32_t want, const char *label) {
    uint32_t got = canvas_get_pixel(&stack->layers[layer_index].canvas, x, y);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_int(int got, int want, const char *label) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

int main(void) {
    LayerStack stack;
    Snapshot undo_stack[TEST_HISTORY_CAPACITY] = {0};
    Snapshot redo_stack[TEST_HISTORY_CAPACITY] = {0};
    int undo_count = 0;
    int redo_count = 0;

    {
        Snapshot snapshot = {0};
        LayerStack empty = {0};
        LayerStack sparse = {0};
        uint32_t sparse_pixels[4] = {0};

        if (snapshot_from_layers(NULL, &empty) || snapshot_from_layers(&snapshot, NULL)) {
            fprintf(stderr, "snapshot_from_layers should reject null arguments\n");
            return 1;
        }

        empty.width = 4;
        empty.height = 4;
        empty.layer_count = 0;
        empty.active_layer = 0;
        empty.solo_index = -1;
        if (!snapshot_from_layers(&snapshot, &empty) ||
            !expect_int(snapshot.width, 4, "empty_snapshot_width") ||
            !expect_int(snapshot.height, 4, "empty_snapshot_height") ||
            !expect_int(snapshot.layer_count, 0, "empty_snapshot_layer_count") ||
            !expect_int(snapshot.active_layer, 0, "empty_snapshot_active_layer") ||
            !expect_int(snapshot.solo_index, -1, "empty_snapshot_solo_index") ||
            snapshot.pixels != NULL) {
            fprintf(stderr, "empty snapshot creation failed\n");
            snapshot_free(&snapshot);
            return 1;
        }
        snapshot_free(&snapshot);

        sparse.width = 2;
        sparse.height = 2;
        sparse.layer_count = 1;
        sparse.active_layer = 0;
        sparse.solo_index = -1;
        sparse.layers[0].visible = 1;
        sparse.layers[0].locked = 1;
        sparse.layers[0].opacity_percent = 55;
        strncpy(sparse.layers[0].name, "Sparse", LAYER_NAME_MAX - 1);
        sparse.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
        sparse.layers[0].canvas.width = 2;
        sparse.layers[0].canvas.height = 2;
        sparse.layers[0].canvas.pixels = NULL;
        if (!snapshot_from_layers(&snapshot, &sparse) ||
            !snapshot.pixels ||
            !expect_int(snapshot.visibility[0], 1, "sparse_snapshot_visible") ||
            !expect_int(snapshot.locked[0], 1, "sparse_snapshot_locked") ||
            !expect_int(snapshot.opacity_percent[0], 55, "sparse_snapshot_opacity") ||
            strcmp(snapshot.names[0], "Sparse") != 0) {
            fprintf(stderr, "sparse snapshot creation failed\n");
            snapshot_free(&snapshot);
            return 1;
        }
        memcpy(sparse_pixels, snapshot.pixels, sizeof(sparse_pixels));
        if (sparse_pixels[0] != 0 || sparse_pixels[1] != 0 || sparse_pixels[2] != 0 || sparse_pixels[3] != 0) {
            fprintf(stderr, "sparse snapshot pixels should be zero-filled\n");
            snapshot_free(&snapshot);
            return 1;
        }
        snapshot_free(&snapshot);
    }

    if (!layer_stack_init(&stack, 8, 8, 0xFFFFFFFF)) {
        fprintf(stderr, "layer stack init failed\n");
        return 1;
    }
    if (layer_stack_add(&stack, "Top", 0x00000000) != 1) {
        fprintf(stderr, "layer add failed\n");
        layer_stack_free(&stack);
        return 1;
    }

    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    if (!layer_stack_rename(&stack, 1, "Sketch")) {
        fprintf(stderr, "rename failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_name(&stack, 1, "Top", "undo_name") || undo_count != 0 || redo_count != 1) {
        fprintf(stderr, "undo state failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_redo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_name(&stack, 1, "Sketch", "redo_name") || undo_count != 1 || redo_count != 0) {
        fprintf(stderr, "redo state failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    if (!layer_stack_rename(&stack, 1, "Lineart") || redo_count != 0) {
        fprintf(stderr, "redo clear setup failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_name(&stack, 1, "Sketch", "second_undo_name")) {
        fprintf(stderr, "second undo failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    if (!layer_stack_rename(&stack, 1, "Paint") || redo_count != 0) {
        fprintf(stderr, "redo clear failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    if (snapshot_redo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count)) {
        fprintf(stderr, "redo should be unavailable after new snapshot\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    if (layer_stack_add(&stack, "FX", 0x00000000) != 2) {
        fprintf(stderr, "second extra layer add failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    canvas_set_pixel(&stack.layers[1].canvas, 1, 1, 0xFFFF0000);
    canvas_set_pixel(&stack.layers[2].canvas, 2, 2, 0xFF00FF00);
    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    canvas_set_pixel(&stack.layers[1].canvas, 1, 1, 0xFF0000FF);
    canvas_set_pixel(&stack.layers[2].canvas, 2, 2, 0xFFFFFF00);
    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_pixel(&stack, 1, 1, 1, 0xFFFF0000, "undo_pixel_top") ||
        !expect_pixel(&stack, 2, 2, 2, 0xFF00FF00, "undo_pixel_fx")) {
        fprintf(stderr, "pixel undo failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_redo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_pixel(&stack, 1, 1, 1, 0xFF0000FF, "redo_pixel_top") ||
        !expect_pixel(&stack, 2, 2, 2, 0xFFFFFF00, "redo_pixel_fx")) {
        fprintf(stderr, "pixel redo failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    for (int i = 0; i < TEST_HISTORY_CAPACITY + 1; i++) {
        char name[LAYER_NAME_MAX];

        snprintf(name, sizeof(name), "State %d", i);
        snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
        if (!layer_stack_rename(&stack, 1, name)) {
            fprintf(stderr, "rollover rename failed\n");
            layer_stack_free(&stack);
            return 1;
        }
    }
    if (undo_count != TEST_HISTORY_CAPACITY) {
        fprintf(stderr, "history capacity not enforced\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_name(&stack, 1, "State 3", "rollover_undo_1") ||
        !snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_name(&stack, 1, "State 2", "rollover_undo_2") ||
        !snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_name(&stack, 1, "State 1", "rollover_undo_3") ||
        !snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_name(&stack, 1, "State 0", "rollover_undo_4")) {
        fprintf(stderr, "history rollover undo failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count)) {
        fprintf(stderr, "history rollover should drop oldest snapshot\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    if (!layer_stack_toggle_solo(&stack, 2)) {
        fprintf(stderr, "solo setup failed\n");
        layer_stack_free(&stack);
        return 1;
    }
    stack.active_layer = 2;
    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    if (!layer_stack_delete(&stack, 2) ||
        !expect_int(stack.layer_count, 2, "delete_layer_count") ||
        !expect_int(stack.active_layer, 1, "delete_active_layer") ||
        !expect_int(stack.solo_index, -1, "delete_solo_index")) {
        fprintf(stderr, "delete state setup failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 3, "undo_delete_layer_count") ||
        !expect_int(stack.active_layer, 2, "undo_delete_active_layer") ||
        !expect_int(stack.solo_index, 2, "undo_delete_solo_index") ||
        !expect_name(&stack, 2, "FX", "undo_delete_name")) {
        fprintf(stderr, "undo delete state failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_redo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 2, "redo_delete_layer_count") ||
        !expect_int(stack.active_layer, 1, "redo_delete_active_layer") ||
        !expect_int(stack.solo_index, -1, "redo_delete_solo_index")) {
        fprintf(stderr, "redo delete state failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 3, "restore_for_solo_layer_count") ||
        !expect_int(stack.solo_index, 2, "restore_for_solo_solo_index")) {
        fprintf(stderr, "restore for solo state failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    if (!layer_stack_toggle_solo(&stack, 2) ||
        !expect_int(stack.solo_index, -1, "solo_toggle_off_index")) {
        fprintf(stderr, "solo toggle-off setup failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.solo_index, 2, "undo_solo_toggle_index")) {
        fprintf(stderr, "undo solo toggle failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_redo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.solo_index, -1, "redo_solo_toggle_index")) {
        fprintf(stderr, "redo solo toggle failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    if ((strcmp(stack.layers[1].name, "Paint") != 0 && !layer_stack_rename(&stack, 1, "Paint")) ||
        (strcmp(stack.layers[2].name, "FX") != 0 && !layer_stack_rename(&stack, 2, "FX"))) {
        fprintf(stderr, "structural mutation rename setup failed\n");
        layer_stack_free(&stack);
        return 1;
    }
    stack.active_layer = 1;
    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    if (layer_stack_duplicate(&stack, 1, "Paint Copy") != 2 ||
        !expect_int(stack.layer_count, 4, "duplicate_layer_count") ||
        !expect_int(stack.active_layer, 2, "duplicate_active_layer") ||
        !expect_name(&stack, 2, "Paint Copy", "duplicate_name")) {
        fprintf(stderr, "duplicate setup failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 3, "undo_duplicate_layer_count") ||
        !expect_int(stack.active_layer, 1, "undo_duplicate_active_layer") ||
        !expect_name(&stack, 1, "Paint", "undo_duplicate_name")) {
        fprintf(stderr, "undo duplicate failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_redo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 4, "redo_duplicate_layer_count") ||
        !expect_int(stack.active_layer, 2, "redo_duplicate_active_layer") ||
        !expect_name(&stack, 2, "Paint Copy", "redo_duplicate_name")) {
        fprintf(stderr, "redo duplicate failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    if (!layer_stack_move(&stack, 2, -1) ||
        !expect_int(stack.active_layer, 1, "move_active_layer") ||
        !expect_name(&stack, 1, "Paint Copy", "move_name_after")) {
        fprintf(stderr, "move setup failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.active_layer, 2, "undo_move_active_layer") ||
        !expect_name(&stack, 2, "Paint Copy", "undo_move_name")) {
        fprintf(stderr, "undo move failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_redo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.active_layer, 1, "redo_move_active_layer") ||
        !expect_name(&stack, 1, "Paint Copy", "redo_move_name")) {
        fprintf(stderr, "redo move failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    stack.active_layer = 1;
    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    if (!layer_stack_merge_down(&stack, 1) ||
        !expect_int(stack.layer_count, 3, "merge_layer_count") ||
        !expect_int(stack.active_layer, 0, "merge_active_layer") ||
        !expect_name(&stack, 0, "Background", "merge_base_name")) {
        fprintf(stderr, "merge setup failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 4, "undo_merge_layer_count") ||
        !expect_int(stack.active_layer, 1, "undo_merge_active_layer") ||
        !expect_name(&stack, 1, "Paint Copy", "undo_merge_name")) {
        fprintf(stderr, "undo merge failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_redo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 3, "redo_merge_layer_count") ||
        !expect_int(stack.active_layer, 0, "redo_merge_active_layer")) {
        fprintf(stderr, "redo merge failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 4, "restore_for_edge_layer_count")) {
        fprintf(stderr, "restore for edge move failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    stack.active_layer = 1;
    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    if (!layer_stack_move_to_edge(&stack, 1, 1) ||
        !expect_int(stack.active_layer, 3, "move_to_edge_active") ||
        !expect_name(&stack, 3, "Paint Copy", "move_to_edge_name")) {
        fprintf(stderr, "move to edge setup failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.active_layer, 1, "undo_move_to_edge_active") ||
        !expect_name(&stack, 1, "Paint Copy", "undo_move_to_edge_name")) {
        fprintf(stderr, "undo move to edge failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_redo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.active_layer, 3, "redo_move_to_edge_active") ||
        !expect_name(&stack, 3, "Paint Copy", "redo_move_to_edge_name")) {
        fprintf(stderr, "redo move to edge failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 4, "restore_for_merge_up_layer_count")) {
        fprintf(stderr, "restore for merge up failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    stack.active_layer = 1;
    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    if (!layer_stack_merge_up(&stack, 1) ||
        !expect_int(stack.layer_count, 3, "merge_up_layer_count") ||
        !expect_int(stack.active_layer, 1, "merge_up_active_layer") ||
        !expect_name(&stack, 1, "Paint", "merge_up_name")) {
        fprintf(stderr, "merge up setup failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 4, "undo_merge_up_layer_count") ||
        !expect_int(stack.active_layer, 1, "undo_merge_up_active_layer") ||
        !expect_name(&stack, 1, "Paint Copy", "undo_merge_up_name")) {
        fprintf(stderr, "undo merge up failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_redo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 3, "redo_merge_up_layer_count") ||
        !expect_int(stack.active_layer, 1, "redo_merge_up_active_layer") ||
        !expect_name(&stack, 1, "Paint", "redo_merge_up_name")) {
        fprintf(stderr, "redo merge up failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    if (stack.layer_count != 3 || strcmp(stack.layers[1].name, "Paint") != 0 || strcmp(stack.layers[2].name, "FX") != 0) {
        fprintf(stderr, "flatten precondition state failed\n");
        layer_stack_free(&stack);
        return 1;
    }
    stack.active_layer = 2;
    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    if (!layer_stack_flatten(&stack, 0xFFFFFFFF) ||
        !expect_int(stack.layer_count, 1, "flatten_layer_count") ||
        !expect_int(stack.active_layer, 0, "flatten_active_layer") ||
        !expect_int(stack.solo_index, -1, "flatten_solo_index") ||
        !expect_name(&stack, 0, "Background", "flatten_name")) {
        fprintf(stderr, "flatten setup failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 3, "undo_flatten_layer_count") ||
        !expect_int(stack.active_layer, 2, "undo_flatten_active_layer") ||
        !expect_name(&stack, 1, "Paint", "undo_flatten_mid_name") ||
        !expect_name(&stack, 2, "FX", "undo_flatten_top_name")) {
        fprintf(stderr, "undo flatten failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_redo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !expect_int(stack.layer_count, 1, "redo_flatten_layer_count") ||
        !expect_int(stack.active_layer, 0, "redo_flatten_active_layer")) {
        fprintf(stderr, "redo flatten failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    if (layer_stack_add(&stack, "Meta", 0x00000000) != 1) {
        fprintf(stderr, "metadata layer add failed\n");
        layer_stack_free(&stack);
        return 1;
    }
    snapshot_push(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count);
    if (!layer_stack_toggle_visibility(&stack, 1) ||
        !layer_stack_toggle_lock(&stack, 1) ||
        !layer_stack_set_opacity(&stack, 1, 35) ||
        stack.layers[1].visible ||
        !stack.layers[1].locked ||
        stack.layers[1].opacity_percent != 35) {
        fprintf(stderr, "metadata setup failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_undo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        !stack.layers[1].visible ||
        stack.layers[1].locked ||
        !expect_int(stack.layers[1].opacity_percent, 100, "undo_metadata_opacity")) {
        fprintf(stderr, "undo metadata failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }
    if (!snapshot_redo(&stack, undo_stack, &undo_count, TEST_HISTORY_CAPACITY, redo_stack, &redo_count) ||
        stack.layers[1].visible ||
        !stack.layers[1].locked ||
        !expect_int(stack.layers[1].opacity_percent, 35, "redo_metadata_opacity")) {
        fprintf(stderr, "redo metadata failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 1;
    }

    {
        Snapshot invalid = {0};
        Snapshot saved = {0};

        if (!snapshot_from_layers(&saved, &stack)) {
            fprintf(stderr, "saved snapshot init failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }

        invalid = saved;
        invalid.width += 1;
        if (snapshot_apply(&invalid, &stack)) {
            fprintf(stderr, "snapshot apply should reject mismatched width\n");
            snapshot_free(&saved);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }

        invalid = saved;
        invalid.layer_count = 0;
        if (snapshot_apply(&invalid, &stack)) {
            fprintf(stderr, "snapshot apply should reject zero layer count\n");
            snapshot_free(&saved);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }

        invalid = saved;
        invalid.layer_count = MAX_LAYERS + 1;
        if (snapshot_apply(&invalid, &stack)) {
            fprintf(stderr, "snapshot apply should reject oversized layer count\n");
            snapshot_free(&saved);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }

        invalid = saved;
        invalid.pixels = NULL;
        if (snapshot_apply(&invalid, &stack)) {
            fprintf(stderr, "snapshot apply should reject missing pixels\n");
            snapshot_free(&saved);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }

        if (!expect_int(stack.layer_count, saved.layer_count, "invalid_apply_layer_count_preserved") ||
            !expect_int(stack.active_layer, 1, "invalid_apply_active_preserved") ||
            stack.layers[1].visible ||
            !stack.layers[1].locked ||
            !expect_int(stack.layers[1].opacity_percent, 35, "invalid_apply_opacity_preserved")) {
            fprintf(stderr, "invalid snapshot apply mutated stack\n");
            snapshot_free(&saved);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }

        snapshot_free(&saved);
    }

    {
        Snapshot temp_undo[1] = {0};
        Snapshot temp_redo[1] = {0};
        int temp_undo_count = 0;
        int temp_redo_count = 0;

        if (snapshot_undo(&stack, temp_undo, &temp_undo_count, TEST_HISTORY_CAPACITY, temp_redo, &temp_redo_count) ||
            snapshot_redo(&stack, temp_undo, &temp_undo_count, TEST_HISTORY_CAPACITY, temp_redo, &temp_redo_count)) {
            fprintf(stderr, "empty history stacks should be no-op\n");
            snapshot_stack_clear(temp_undo, &temp_undo_count);
            snapshot_stack_clear(temp_redo, &temp_redo_count);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }

        snapshot_push(&stack, temp_undo, &temp_undo_count, 0, temp_redo, &temp_redo_count);
        if (temp_undo_count != 0 || temp_redo_count != 0) {
            fprintf(stderr, "zero-capacity snapshot push should be no-op\n");
            snapshot_stack_clear(temp_undo, &temp_undo_count);
            snapshot_stack_clear(temp_redo, &temp_redo_count);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }

        if (!snapshot_from_layers(&temp_undo[0], &stack)) {
            fprintf(stderr, "temp snapshot init failed\n");
            snapshot_stack_clear(temp_undo, &temp_undo_count);
            snapshot_stack_clear(temp_redo, &temp_redo_count);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }
        snapshot_push_existing(temp_redo, &temp_redo_count, 0, &temp_undo[0]);
        if (temp_redo_count != 0) {
            fprintf(stderr, "zero-capacity push_existing should be no-op\n");
            snapshot_free(&temp_undo[0]);
            snapshot_stack_clear(temp_undo, &temp_undo_count);
            snapshot_stack_clear(temp_redo, &temp_redo_count);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }
        snapshot_free(&temp_undo[0]);
    }

    {
        Snapshot temp_stack[2] = {0};
        Snapshot owned = {0};
        uint32_t *owned_pixels = NULL;
        int temp_count = 0;

        if (!snapshot_from_layers(&owned, &stack)) {
            fprintf(stderr, "owned snapshot init failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }
        owned_pixels = owned.pixels;
        snapshot_push_existing(temp_stack, &temp_count, 2, &owned);
        if (!expect_int(temp_count, 1, "push_existing_count") ||
            temp_stack[0].pixels != owned_pixels ||
            temp_stack[0].width != owned.width ||
            temp_stack[0].height != owned.height ||
            temp_stack[0].layer_count != owned.layer_count ||
            temp_stack[0].active_layer != owned.active_layer ||
            temp_stack[0].solo_index != owned.solo_index ||
            strcmp(temp_stack[0].names[0], owned.names[0]) != 0) {
            fprintf(stderr, "push_existing ownership transfer failed\n");
            snapshot_stack_clear(temp_stack, &temp_count);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }

        snapshot_stack_clear(temp_stack, &temp_count);
        if (!expect_int(temp_count, 0, "stack_clear_count") ||
            temp_stack[0].pixels != NULL ||
            temp_stack[0].width != 0 ||
            temp_stack[0].height != 0 ||
            temp_stack[0].layer_count != 0 ||
            temp_stack[0].active_layer != 0) {
            fprintf(stderr, "stack_clear should free and zero snapshots\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }
    }

    {
        Snapshot temp_stack[2] = {0};
        Snapshot first = {0};
        Snapshot second = {0};
        Snapshot third = {0};
        uint32_t *second_pixels = NULL;
        uint32_t *third_pixels = NULL;
        int temp_count = 0;

        if (!layer_stack_rename(&stack, 1, "Owned First") ||
            !snapshot_from_layers(&first, &stack) ||
            !layer_stack_rename(&stack, 1, "Owned Second") ||
            !snapshot_from_layers(&second, &stack) ||
            !layer_stack_rename(&stack, 1, "Owned Third") ||
            !snapshot_from_layers(&third, &stack)) {
            fprintf(stderr, "owned rollover snapshot init failed\n");
            snapshot_free(&first);
            snapshot_free(&second);
            snapshot_free(&third);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }

        second_pixels = second.pixels;
        third_pixels = third.pixels;
        snapshot_push_existing(temp_stack, &temp_count, 2, &first);
        snapshot_push_existing(temp_stack, &temp_count, 2, &second);
        snapshot_push_existing(temp_stack, &temp_count, 2, &third);
        if (!expect_int(temp_count, 2, "push_existing_rollover_count") ||
            strcmp(temp_stack[0].names[1], "Owned Second") != 0 ||
            strcmp(temp_stack[1].names[1], "Owned Third") != 0 ||
            temp_stack[0].pixels != second_pixels ||
            temp_stack[1].pixels != third_pixels) {
            fprintf(stderr, "push_existing rollover failed\n");
            snapshot_stack_clear(temp_stack, &temp_count);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 1;
        }

        snapshot_stack_clear(temp_stack, &temp_count);
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    layer_stack_free(&stack);
    puts("history selftest ok");
    return 0;
}
