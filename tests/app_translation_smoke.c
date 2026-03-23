#include "../src/app_translation.h"

#include <stdio.h>

typedef struct {
    int push_count;
    int translate_count;
    int last_dx;
    int last_dy;
    unsigned int last_clear_color;
} TranslationStubState;

static int expect_int_eq(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static void stub_push_snapshot(const LayerStack *layers, void *userdata) {
    TranslationStubState *state = (TranslationStubState *)userdata;
    (void)layers;
    if (state) {
        state->push_count++;
    }
}

static void stub_translate_canvas(Canvas *canvas, int dx, int dy, uint32_t clear_color, void *userdata) {
    TranslationStubState *state = (TranslationStubState *)userdata;
    (void)canvas;
    if (state) {
        state->translate_count++;
        state->last_dx = dx;
        state->last_dy = dy;
        state->last_clear_color = clear_color;
    }
}

static int test_arrow_key_mappings(void) {
    AppTranslationCommand up = app_translation_command_for_key(1073741906, 0);
    AppTranslationCommand down = app_translation_command_for_key(1073741905, 0);
    AppTranslationCommand left = app_translation_command_for_key(1073741904, 0);
    AppTranslationCommand right = app_translation_command_for_key(1073741903, 0);

    return up.handled && down.handled && left.handled && right.handled &&
           expect_int_eq("up_dx", up.dx, 0) &&
           expect_int_eq("up_dy", up.dy, -1) &&
           expect_int_eq("down_dx", down.dx, 0) &&
           expect_int_eq("down_dy", down.dy, 1) &&
           expect_int_eq("left_dx", left.dx, -1) &&
           expect_int_eq("left_dy", left.dy, 0) &&
           expect_int_eq("right_dx", right.dx, 1) &&
           expect_int_eq("right_dy", right.dy, 0);
}

static int test_shift_uses_large_step_and_unknown_key_is_ignored(void) {
    AppTranslationCommand shifted = app_translation_command_for_key(1073741903, 1);
    AppTranslationCommand unknown = app_translation_command_for_key('x', 1);

    return expect_int_eq("shifted_handled", shifted.handled, 1) &&
           expect_int_eq("shifted_dx", shifted.dx, 10) &&
           expect_int_eq("shifted_dy", shifted.dy, 0) &&
           expect_int_eq("unknown_handled", unknown.handled, 0) &&
           expect_int_eq("unknown_dx", unknown.dx, 0) &&
           expect_int_eq("unknown_dy", unknown.dy, 0);
}

static int test_apply_translation_success_and_failures(void) {
    LayerStack stack;
    TranslationStubState stub = {0};
    AppTranslationState state = {0};
    AppTranslationCallbacks callbacks = {
        .push_snapshot = stub_push_snapshot,
        .translate_canvas = stub_translate_canvas,
        .userdata = &stub,
    };
    AppTranslationCommand move = {1, 3, -2};

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }

    if (!app_translation_apply(move, &stack, &state, 0x00000000u, &callbacks) ||
        !expect_int_eq("apply_translate_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("apply_translate_push_count", stub.push_count, 1) ||
        !expect_int_eq("apply_translate_count", stub.translate_count, 1) ||
        !expect_int_eq("apply_translate_dx", stub.last_dx, 3) ||
        !expect_int_eq("apply_translate_dy", stub.last_dy, -2)) {
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[0].locked = 1;
    if (app_translation_apply(move, &stack, &state, 0x00000000u, &callbacks) ||
        !expect_int_eq("apply_translate_locked_push_count", stub.push_count, 1) ||
        !expect_int_eq("apply_translate_locked_count", stub.translate_count, 1)) {
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[0].locked = 0;
    state.needs_composite = 1;
    if (app_translation_apply((AppTranslationCommand){1, 0, 0}, &stack, &state, 0x00000000u, &callbacks) ||
        !expect_int_eq("apply_translate_noop_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("apply_translate_noop_push_count", stub.push_count, 1)) {
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

int main(void) {
    if (!test_arrow_key_mappings()) {
        return 1;
    }
    if (!test_shift_uses_large_step_and_unknown_key_is_ignored()) {
        return 1;
    }
    if (!test_apply_translation_success_and_failures()) {
        return 1;
    }
    return 0;
}
