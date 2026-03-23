#include "../src/app_document.h"
#include "../src/canvas.h"
#include "../src/layers.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int save_calls;
    int load_calls;
    int restore_calls;
    int push_calls;
    int last_redo;
    const Canvas *saved_canvas;
    Canvas *loaded_canvas;
    uint32_t loaded_clear_color;
    int save_result;
    int load_result;
    int restore_result;
} DocumentStubState;

static int expect_int_eq(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int stub_save(const Canvas *canvas, const char *path, void *userdata) {
    DocumentStubState *state = (DocumentStubState *)userdata;
    state->save_calls++;
    state->saved_canvas = canvas;
    return canvas && path && strcmp(path, "output.bmp") == 0 ? state->save_result : 0;
}

static int stub_load(Canvas *canvas, const char *path, uint32_t clear_color, void *userdata) {
    DocumentStubState *state = (DocumentStubState *)userdata;
    state->load_calls++;
    state->loaded_canvas = canvas;
    state->loaded_clear_color = clear_color;
    return canvas && path && strcmp(path, "input.bmp") == 0 ? state->load_result : 0;
}

static int stub_restore(LayerStack *layers, int redo_to_undo, void *userdata) {
    DocumentStubState *state = (DocumentStubState *)userdata;
    state->restore_calls++;
    state->last_redo = redo_to_undo;
    return layers ? state->restore_result : 0;
}

static void stub_push(const LayerStack *layers, void *userdata) {
    DocumentStubState *state = (DocumentStubState *)userdata;
    if (layers) {
        state->push_calls++;
    }
}

static int test_save_prefers_preview_canvas(void) {
    LayerStack stack;
    Canvas composite = {0};
    Canvas preview = {0};
    AppDocumentState state = {.preview_active = 1, .needs_composite = 0};
    DocumentStubState stub = {.save_result = 1};
    AppDocumentCallbacks callbacks = {
        .save_canvas = stub_save,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) ||
        !canvas_init(&composite, 4, 4) ||
        !canvas_init(&preview, 4, 4)) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0x00000000, &callbacks) ||
        !expect_int_eq("save_calls", stub.save_calls, 1) ||
        stub.saved_canvas != &preview) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    state.preview_active = 0;
    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0x00000000, &callbacks) ||
        !expect_int_eq("save_calls_after_composite", stub.save_calls, 2) ||
        stub.saved_canvas != &composite) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    layer_stack_free(&stack);
    canvas_free(&composite);
    canvas_free(&preview);
    return 1;
}

static int test_load_requires_unlocked_active_layer(void) {
    LayerStack stack;
    AppDocumentState state = {0};
    DocumentStubState stub = {.load_result = 1};
    AppDocumentCallbacks callbacks = {
        .load_canvas = stub_load,
        .push_snapshot = stub_push,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) || layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.active_layer = 1;
    stack.layers[1].locked = 1;
    if (app_document_apply(APP_DOCUMENT_ACTION_LOAD, &stack, &state, NULL, NULL, 0x00000000, &callbacks) ||
        !expect_int_eq("locked_load_push_calls", stub.push_calls, 0) ||
        !expect_int_eq("locked_load_calls", stub.load_calls, 0)) {
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[1].locked = 0;
    if (!app_document_apply(APP_DOCUMENT_ACTION_LOAD, &stack, &state, NULL, NULL, 0x12345678, &callbacks) ||
        !expect_int_eq("unlocked_load_push_calls", stub.push_calls, 1) ||
        !expect_int_eq("unlocked_load_calls", stub.load_calls, 1) ||
        !expect_int_eq("unlocked_load_needs_composite", state.needs_composite, 1) ||
        stub.loaded_canvas != &stack.layers[1].canvas ||
        stub.loaded_clear_color != 0x12345678) {
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_undo_redo_update_composite_flag(void) {
    LayerStack stack;
    AppDocumentState state = {0};
    DocumentStubState stub = {.restore_result = 1};
    AppDocumentCallbacks callbacks = {
        .restore_history = stub_restore,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "initialization failed\n");
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_UNDO, &stack, &state, NULL, NULL, 0, &callbacks) ||
        !expect_int_eq("undo_restore_calls", stub.restore_calls, 1) ||
        !expect_int_eq("undo_flag", stub.last_redo, 0) ||
        !expect_int_eq("undo_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        return 0;
    }

    state.needs_composite = 0;
    if (!app_document_apply(APP_DOCUMENT_ACTION_REDO, &stack, &state, NULL, NULL, 0, &callbacks) ||
        !expect_int_eq("redo_restore_calls", stub.restore_calls, 2) ||
        !expect_int_eq("redo_flag", stub.last_redo, 1) ||
        !expect_int_eq("redo_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_visibility_and_opacity_actions_push_history(void) {
    LayerStack stack;
    AppDocumentState state = {0};
    DocumentStubState stub = {0};
    AppDocumentCallbacks callbacks = {
        .push_snapshot = stub_push,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) || layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.active_layer = 1;
    stack.layers[1].opacity_percent = 55;
    if (!app_document_apply(APP_DOCUMENT_ACTION_RESET_OPACITY, &stack, &state, NULL, NULL, 0, &callbacks) ||
        !expect_int_eq("reset_push_calls", stub.push_calls, 1) ||
        !expect_int_eq("reset_opacity_value", stack.layers[1].opacity_percent, 100) ||
        !expect_int_eq("reset_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        return 0;
    }

    state.needs_composite = 0;
    stack.layers[1].visible = 0;
    if (!app_document_apply(APP_DOCUMENT_ACTION_SHOW_ACTIVE, &stack, &state, NULL, NULL, 0, &callbacks) ||
        !expect_int_eq("show_active_push_calls", stub.push_calls, 2) ||
        !expect_int_eq("show_active_visible", stack.layers[1].visible, 1) ||
        !expect_int_eq("show_active_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        return 0;
    }

    state.needs_composite = 0;
    stack.solo_index = 1;
    stack.layers[0].visible = 0;
    if (!app_document_apply(APP_DOCUMENT_ACTION_SHOW_ALL, &stack, &state, NULL, NULL, 0, &callbacks) ||
        !expect_int_eq("show_all_push_calls", stub.push_calls, 3) ||
        !expect_int_eq("show_all_visible_base", stack.layers[0].visible, 1) ||
        !expect_int_eq("show_all_solo", stack.solo_index, -1) ||
        !expect_int_eq("show_all_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

int main(void) {
    if (!test_save_prefers_preview_canvas()) {
        return 1;
    }
    if (!test_load_requires_unlocked_active_layer()) {
        return 1;
    }
    if (!test_undo_redo_update_composite_flag()) {
        return 1;
    }
    if (!test_visibility_and_opacity_actions_push_history()) {
        return 1;
    }
    return 0;
}
