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

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    layer_stack_free(&stack);
    puts("history selftest ok");
    return 0;
}
