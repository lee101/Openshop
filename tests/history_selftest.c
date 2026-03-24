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
    layer_stack_free(&stack);
    puts("history selftest ok");
    return 0;
}
