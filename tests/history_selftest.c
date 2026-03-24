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

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    layer_stack_free(&stack);
    puts("history selftest ok");
    return 0;
}
