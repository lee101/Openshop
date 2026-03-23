#include "../src/app_canvas_edit.h"

#include <stdio.h>

typedef struct {
    int push_count;
} SnapshotStubState;

static int expect_int_eq(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_uint_eq(const char *label, unsigned int got, unsigned int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

static void stub_push_snapshot(const LayerStack *layers, void *userdata) {
    SnapshotStubState *state = (SnapshotStubState *)userdata;
    (void)layers;
    if (state) {
        state->push_count++;
    }
}

static int test_translate_active_success_and_failures(void) {
    LayerStack stack;
    SnapshotStubState snapshot = {0};
    AppCanvasEditState state = {0};
    AppCanvasEditCallbacks callbacks = {
        .push_snapshot = stub_push_snapshot,
        .userdata = &snapshot,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }

    canvas_set_pixel(&stack.layers[0].canvas, 1, 1, 0xFF112233u);
    if (!app_canvas_edit_translate_active(&stack, &state, 1, 0, 0x00000000u, &callbacks) ||
        !expect_int_eq("translate_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("translate_push_count", snapshot.push_count, 1) ||
        !expect_uint_eq("translate_moved_pixel", canvas_get_pixel(&stack.layers[0].canvas, 2, 1), 0xFF112233u) ||
        !expect_uint_eq("translate_shifted_background_pixel", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFFFFFFFFu) ||
        !expect_uint_eq("translate_filled_edge_pixel", canvas_get_pixel(&stack.layers[0].canvas, 0, 1), 0x00000000u)) {
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[0].locked = 1;
    if (app_canvas_edit_translate_active(&stack, &state, 1, 0, 0x00000000u, &callbacks) ||
        !expect_int_eq("translate_locked_push_count", snapshot.push_count, 1)) {
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[0].locked = 0;
    state.needs_composite = 1;
    if (app_canvas_edit_translate_active(&stack, &state, 0, 0, 0x00000000u, &callbacks) ||
        !expect_int_eq("translate_noop_preserve_flag", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_transform_active_success_and_failures(void) {
    LayerStack stack;
    SnapshotStubState snapshot = {0};
    AppCanvasEditState state = {0};
    AppCanvasEditCallbacks callbacks = {
        .push_snapshot = stub_push_snapshot,
        .userdata = &snapshot,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }

    canvas_set_pixel(&stack.layers[0].canvas, 0, 1, 0xFF445566u);
    if (!app_canvas_edit_transform_active(APP_CANVAS_TRANSFORM_FLIP_HORIZONTAL, &stack, &state, &callbacks) ||
        !expect_int_eq("transform_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("transform_push_count", snapshot.push_count, 1) ||
        !expect_uint_eq("transform_flipped_pixel", canvas_get_pixel(&stack.layers[0].canvas, 3, 1), 0xFF445566u)) {
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel(&stack.layers[0].canvas, 1, 1, 0xFF010203u);
    if (!app_canvas_edit_transform_active(APP_CANVAS_TRANSFORM_INVERT_RGB, &stack, &state, &callbacks) ||
        !expect_uint_eq("transform_inverted_pixel", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFFFEFDFCu)) {
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[0].locked = 1;
    if (app_canvas_edit_transform_active(APP_CANVAS_TRANSFORM_FLIP_VERTICAL, &stack, &state, &callbacks) ||
        !expect_int_eq("transform_locked_push_count", snapshot.push_count, 2)) {
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

int main(void) {
    if (!test_translate_active_success_and_failures()) {
        return 1;
    }
    if (!test_transform_active_success_and_failures()) {
        return 1;
    }
    return 0;
}
