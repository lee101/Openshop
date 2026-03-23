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

static int test_document_command_for_key(void) {
    AppDocumentCommand save = app_document_command_for_key('s', 1, 0);
    AppDocumentCommand load = app_document_command_for_key('o', 1, 0);
    AppDocumentCommand undo = app_document_command_for_key('z', 1, 0);
    AppDocumentCommand redo = app_document_command_for_key('y', 1, 0);
    AppDocumentCommand reset = app_document_command_for_key('0', 1, 0);
    AppDocumentCommand show_all = app_document_command_for_key('a', 1, 0);
    AppDocumentCommand show_active = app_document_command_for_key('r', 1, 1);
    AppDocumentCommand none = app_document_command_for_key('q', 0, 0);

    return expect_int_eq("save_handled", save.handled, 1) &&
           expect_int_eq("save_action", save.action, APP_DOCUMENT_ACTION_SAVE) &&
           expect_int_eq("load_action", load.action, APP_DOCUMENT_ACTION_LOAD) &&
           expect_int_eq("undo_action", undo.action, APP_DOCUMENT_ACTION_UNDO) &&
           expect_int_eq("redo_action", redo.action, APP_DOCUMENT_ACTION_REDO) &&
           expect_int_eq("reset_action", reset.action, APP_DOCUMENT_ACTION_RESET_OPACITY) &&
           expect_int_eq("show_all_action", show_all.action, APP_DOCUMENT_ACTION_SHOW_ALL) &&
           expect_int_eq("show_active_action", show_active.action, APP_DOCUMENT_ACTION_SHOW_ACTIVE) &&
           expect_int_eq("none_handled", none.handled, 0) &&
           expect_int_eq("none_action", none.action, APP_DOCUMENT_ACTION_SAVE);
}

static int test_document_command_precedence(void) {
    AppDocumentCommand shifted_show_active = app_document_command_for_key('r', 1, 1);
    AppDocumentCommand plain_r = app_document_command_for_key('r', 1, 0);
    AppDocumentCommand shifted_save = app_document_command_for_key('s', 1, 1);
    AppDocumentCommand shifted_show_all = app_document_command_for_key('a', 1, 1);

    return expect_int_eq("shifted_show_active_handled", shifted_show_active.handled, 1) &&
           expect_int_eq("shifted_show_active_action", shifted_show_active.action, APP_DOCUMENT_ACTION_SHOW_ACTIVE) &&
           expect_int_eq("plain_r_handled", plain_r.handled, 0) &&
           expect_int_eq("plain_r_action", plain_r.action, APP_DOCUMENT_ACTION_SAVE) &&
           expect_int_eq("shifted_save_action", shifted_save.action, APP_DOCUMENT_ACTION_SAVE) &&
           expect_int_eq("shifted_show_all_action", shifted_show_all.action, APP_DOCUMENT_ACTION_SHOW_ALL);
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

static int test_document_failure_and_noop_paths_preserve_flags(void) {
    LayerStack stack;
    AppDocumentState state = {0};
    DocumentStubState stub = {.load_result = 0, .restore_result = 0};
    AppDocumentCallbacks callbacks = {
        .load_canvas = stub_load,
        .restore_history = stub_restore,
        .push_snapshot = stub_push,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) || layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.active_layer = 1;
    stack.layers[1].locked = 0;
    stack.layers[1].opacity_percent = 100;

    if (app_document_apply(APP_DOCUMENT_ACTION_LOAD, &stack, &state, NULL, NULL, 0xAABBCCDD, &callbacks) ||
        !expect_int_eq("failed_load_pushes_snapshot", stub.push_calls, 1) ||
        !expect_int_eq("failed_load_calls_loader", stub.load_calls, 1) ||
        !expect_int_eq("failed_load_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        return 0;
    }

    if (app_document_apply(APP_DOCUMENT_ACTION_UNDO, &stack, &state, NULL, NULL, 0, &callbacks) ||
        !expect_int_eq("failed_undo_restore_calls", stub.restore_calls, 1) ||
        !expect_int_eq("failed_undo_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        return 0;
    }

    if (app_document_apply(APP_DOCUMENT_ACTION_RESET_OPACITY, &stack, &state, NULL, NULL, 0, &callbacks) ||
        !expect_int_eq("noop_reset_push_calls", stub.push_calls, 1) ||
        !expect_int_eq("noop_reset_opacity", stack.layers[1].opacity_percent, 100) ||
        !expect_int_eq("noop_reset_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_document_edge_case_selection_and_save_fallbacks(void) {
    LayerStack stack;
    Canvas composite = {0};
    Canvas preview = {0};
    AppDocumentState state = {.preview_active = 1, .needs_composite = 0};
    DocumentStubState stub = {.save_result = 1};
    AppDocumentCallbacks callbacks = {
        .save_canvas = stub_save,
        .push_snapshot = stub_push,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) ||
        !canvas_init(&composite, 4, 4) ||
        layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        canvas_free(&composite);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("save_fallback_calls", stub.save_calls, 1) ||
        stub.saved_canvas != &composite) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        return 0;
    }

    stack.active_layer = -1;
    if (app_document_apply(APP_DOCUMENT_ACTION_SHOW_ACTIVE, &stack, &state, NULL, NULL, 0, &callbacks) ||
        !expect_int_eq("show_active_invalid_push_calls", stub.push_calls, 1) ||
        !expect_int_eq("show_active_invalid_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        return 0;
    }

    layer_stack_free(&stack);
    canvas_free(&composite);
    return 1;
}

static int test_document_preview_state_survives_save_and_load_paths(void) {
    LayerStack stack;
    Canvas composite = {0};
    Canvas preview = {0};
    AppDocumentState state = {.preview_active = 1, .needs_composite = 0};
    DocumentStubState stub = {.save_result = 0, .load_result = 1};
    AppDocumentCallbacks callbacks = {
        .save_canvas = stub_save,
        .load_canvas = stub_load,
        .push_snapshot = stub_push,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) ||
        !canvas_init(&composite, 4, 4) ||
        !canvas_init(&preview, 4, 4) ||
        layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_save_calls", stub.save_calls, 1) ||
        stub.saved_canvas != &preview ||
        !expect_int_eq("preview_save_preview_active", state.preview_active, 1) ||
        !expect_int_eq("preview_save_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    stack.active_layer = 1;
    if (!app_document_apply(APP_DOCUMENT_ACTION_LOAD, &stack, &state, &preview, &composite, 0xCAFEBABE, &callbacks) ||
        !expect_int_eq("preview_load_push_calls", stub.push_calls, 1) ||
        !expect_int_eq("preview_load_calls", stub.load_calls, 1) ||
        stub.loaded_canvas != &stack.layers[1].canvas ||
        stub.loaded_clear_color != 0xCAFEBABE ||
        !expect_int_eq("preview_load_preview_active", state.preview_active, 1) ||
        !expect_int_eq("preview_load_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    state.needs_composite = 0;
    stub.load_result = 0;
    if (app_document_apply(APP_DOCUMENT_ACTION_LOAD, &stack, &state, &preview, &composite, 0xDEADBEEF, &callbacks) ||
        !expect_int_eq("preview_failed_load_push_calls", stub.push_calls, 2) ||
        !expect_int_eq("preview_failed_load_calls", stub.load_calls, 2) ||
        stub.loaded_canvas != &stack.layers[1].canvas ||
        stub.loaded_clear_color != 0xDEADBEEF ||
        !expect_int_eq("preview_failed_load_preview_active", state.preview_active, 1) ||
        !expect_int_eq("preview_failed_load_needs_composite", state.needs_composite, 0)) {
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

static int test_document_failure_chain_preserves_pending_composite_flag(void) {
    LayerStack stack;
    Canvas composite = {0};
    Canvas preview = {0};
    AppDocumentState state = {.preview_active = 1, .needs_composite = 1};
    DocumentStubState stub = {.save_result = 0, .load_result = 0, .restore_result = 0};
    AppDocumentCallbacks callbacks = {
        .save_canvas = stub_save,
        .load_canvas = stub_load,
        .restore_history = stub_restore,
        .push_snapshot = stub_push,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) ||
        !canvas_init(&composite, 4, 4) ||
        !canvas_init(&preview, 4, 4) ||
        layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    stack.active_layer = 1;

    if (app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("chain_save_calls", stub.save_calls, 1) ||
        stub.saved_canvas != &preview ||
        !expect_int_eq("chain_save_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("chain_save_preview_active", state.preview_active, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (app_document_apply(APP_DOCUMENT_ACTION_LOAD, &stack, &state, &preview, &composite, 0x01020304, &callbacks) ||
        !expect_int_eq("chain_load_push_calls", stub.push_calls, 1) ||
        !expect_int_eq("chain_load_calls", stub.load_calls, 1) ||
        stub.loaded_canvas != &stack.layers[1].canvas ||
        stub.loaded_clear_color != 0x01020304 ||
        !expect_int_eq("chain_load_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (app_document_apply(APP_DOCUMENT_ACTION_UNDO, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("chain_restore_calls", stub.restore_calls, 1) ||
        !expect_int_eq("chain_undo_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    stack.active_layer = -1;
    if (app_document_apply(APP_DOCUMENT_ACTION_SHOW_ACTIVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("chain_show_push_calls", stub.push_calls, 2) ||
        !expect_int_eq("chain_show_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("chain_show_preview_active", state.preview_active, 1)) {
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

static int test_document_success_chain_reasserts_composite_flag(void) {
    LayerStack stack;
    Canvas composite = {0};
    Canvas preview = {0};
    AppDocumentState state = {.preview_active = 1, .needs_composite = 1};
    DocumentStubState stub = {.save_result = 1, .load_result = 1, .restore_result = 1};
    AppDocumentCallbacks callbacks = {
        .save_canvas = stub_save,
        .load_canvas = stub_load,
        .restore_history = stub_restore,
        .push_snapshot = stub_push,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) ||
        !canvas_init(&composite, 4, 4) ||
        !canvas_init(&preview, 4, 4) ||
        layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    stack.active_layer = 1;
    stack.layers[1].visible = 0;
    stack.layers[1].opacity_percent = 25;
    stack.solo_index = 1;
    state.needs_composite = 0;

    if (!app_document_apply(APP_DOCUMENT_ACTION_LOAD, &stack, &state, &preview, &composite, 0x0A0B0C0D, &callbacks) ||
        !expect_int_eq("success_load_push_calls", stub.push_calls, 1) ||
        !expect_int_eq("success_load_calls", stub.load_calls, 1) ||
        stub.loaded_canvas != &stack.layers[1].canvas ||
        stub.loaded_clear_color != 0x0A0B0C0D ||
        !expect_int_eq("success_load_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_SHOW_ACTIVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("success_show_push_calls", stub.push_calls, 2) ||
        !expect_int_eq("success_show_visible", stack.layers[1].visible, 1) ||
        !expect_int_eq("success_show_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_SHOW_ALL, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("success_show_all_push_calls", stub.push_calls, 3) ||
        !expect_int_eq("success_show_all_solo", stack.solo_index, -1) ||
        !expect_int_eq("success_show_all_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_RESET_OPACITY, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("success_reset_push_calls", stub.push_calls, 4) ||
        !expect_int_eq("success_reset_opacity", stack.layers[1].opacity_percent, 100) ||
        !expect_int_eq("success_reset_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_UNDO, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("success_undo_restore_calls", stub.restore_calls, 1) ||
        !expect_int_eq("success_undo_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("success_save_calls", stub.save_calls, 1) ||
        stub.saved_canvas != &preview ||
        !expect_int_eq("success_save_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("success_save_preview_active", state.preview_active, 1)) {
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

static int test_document_preview_toggle_updates_save_canvas_selection(void) {
    LayerStack stack;
    Canvas composite = {0};
    Canvas preview = {0};
    AppDocumentState state = {.preview_active = 1, .needs_composite = 0};
    DocumentStubState stub = {.save_result = 1, .load_result = 0};
    AppDocumentCallbacks callbacks = {
        .save_canvas = stub_save,
        .load_canvas = stub_load,
        .push_snapshot = stub_push,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) ||
        !canvas_init(&composite, 4, 4) ||
        !canvas_init(&preview, 4, 4) ||
        layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    stack.active_layer = 1;

    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("toggle_save_preview_calls", stub.save_calls, 1) ||
        stub.saved_canvas != &preview ||
        !expect_int_eq("toggle_save_preview_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    state.preview_active = 0;
    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("toggle_save_composite_calls", stub.save_calls, 2) ||
        stub.saved_canvas != &composite ||
        !expect_int_eq("toggle_save_composite_preview_active", state.preview_active, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (app_document_apply(APP_DOCUMENT_ACTION_LOAD, &stack, &state, &preview, &composite, 0x11223344, &callbacks) ||
        !expect_int_eq("toggle_failed_load_push_calls", stub.push_calls, 1) ||
        !expect_int_eq("toggle_failed_load_calls", stub.load_calls, 1) ||
        stub.loaded_canvas != &stack.layers[1].canvas ||
        stub.loaded_clear_color != 0x11223344 ||
        !expect_int_eq("toggle_failed_load_preview_active", state.preview_active, 0) ||
        !expect_int_eq("toggle_failed_load_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    canvas_free(&preview);
    state.preview_active = 1;
    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("toggle_fallback_save_calls", stub.save_calls, 3) ||
        stub.saved_canvas != &composite ||
        !expect_int_eq("toggle_fallback_save_preview_active", state.preview_active, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        return 0;
    }

    layer_stack_free(&stack);
    canvas_free(&composite);
    return 1;
}

static int test_document_preview_toggle_coexists_with_visibility_actions(void) {
    LayerStack stack;
    Canvas composite = {0};
    Canvas preview = {0};
    AppDocumentState state = {.preview_active = 1, .needs_composite = 0};
    DocumentStubState stub = {.save_result = 1};
    AppDocumentCallbacks callbacks = {
        .save_canvas = stub_save,
        .push_snapshot = stub_push,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) ||
        !canvas_init(&composite, 4, 4) ||
        !canvas_init(&preview, 4, 4) ||
        layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    stack.active_layer = 1;
    stack.layers[0].visible = 0;
    stack.layers[1].visible = 0;
    stack.layers[1].opacity_percent = 40;
    stack.solo_index = 1;

    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_chain_first_save_calls", stub.save_calls, 1) ||
        stub.saved_canvas != &preview ||
        !expect_int_eq("preview_chain_first_save_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    state.preview_active = 0;
    if (!app_document_apply(APP_DOCUMENT_ACTION_SHOW_ALL, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_chain_show_all_push_calls", stub.push_calls, 1) ||
        !expect_int_eq("preview_chain_show_all_base_visible", stack.layers[0].visible, 1) ||
        !expect_int_eq("preview_chain_show_all_active_visible", stack.layers[1].visible, 1) ||
        !expect_int_eq("preview_chain_show_all_solo", stack.solo_index, -1) ||
        !expect_int_eq("preview_chain_show_all_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("preview_chain_show_all_preview_active", state.preview_active, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_RESET_OPACITY, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_chain_reset_push_calls", stub.push_calls, 2) ||
        !expect_int_eq("preview_chain_reset_opacity", stack.layers[1].opacity_percent, 100) ||
        !expect_int_eq("preview_chain_reset_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    canvas_free(&preview);
    state.preview_active = 1;
    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_chain_fallback_save_calls", stub.save_calls, 2) ||
        stub.saved_canvas != &composite ||
        !expect_int_eq("preview_chain_fallback_save_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("preview_chain_fallback_save_preview_active", state.preview_active, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        return 0;
    }

    layer_stack_free(&stack);
    canvas_free(&composite);
    return 1;
}

static int test_document_preview_toggle_coexists_with_undo_redo(void) {
    LayerStack stack;
    Canvas composite = {0};
    Canvas preview = {0};
    AppDocumentState state = {.preview_active = 1, .needs_composite = 0};
    DocumentStubState stub = {.save_result = 1, .restore_result = 1};
    AppDocumentCallbacks callbacks = {
        .save_canvas = stub_save,
        .restore_history = stub_restore,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) ||
        !canvas_init(&composite, 4, 4) ||
        !canvas_init(&preview, 4, 4) ||
        layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_undo_first_save_calls", stub.save_calls, 1) ||
        stub.saved_canvas != &preview ||
        !expect_int_eq("preview_undo_first_save_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    state.preview_active = 0;
    if (!app_document_apply(APP_DOCUMENT_ACTION_UNDO, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_undo_restore_calls", stub.restore_calls, 1) ||
        !expect_int_eq("preview_undo_last_flag", stub.last_redo, 0) ||
        !expect_int_eq("preview_undo_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("preview_undo_preview_active", state.preview_active, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_undo_second_save_calls", stub.save_calls, 2) ||
        stub.saved_canvas != &composite ||
        !expect_int_eq("preview_undo_second_save_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    state.preview_active = 1;
    if (!app_document_apply(APP_DOCUMENT_ACTION_REDO, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_redo_restore_calls", stub.restore_calls, 2) ||
        !expect_int_eq("preview_redo_last_flag", stub.last_redo, 1) ||
        !expect_int_eq("preview_redo_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("preview_redo_preview_active", state.preview_active, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_redo_third_save_calls", stub.save_calls, 3) ||
        stub.saved_canvas != &preview ||
        !expect_int_eq("preview_redo_third_save_needs_composite", state.needs_composite, 1)) {
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

static int test_document_preview_toggle_coexists_with_load_and_undo_failures(void) {
    LayerStack stack;
    Canvas composite = {0};
    Canvas preview = {0};
    AppDocumentState state = {.preview_active = 1, .needs_composite = 0};
    DocumentStubState stub = {.save_result = 1, .load_result = 0, .restore_result = 0};
    AppDocumentCallbacks callbacks = {
        .save_canvas = stub_save,
        .load_canvas = stub_load,
        .restore_history = stub_restore,
        .push_snapshot = stub_push,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) ||
        !canvas_init(&composite, 4, 4) ||
        !canvas_init(&preview, 4, 4) ||
        layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    stack.active_layer = 1;

    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_fail_first_save_calls", stub.save_calls, 1) ||
        stub.saved_canvas != &preview ||
        !expect_int_eq("preview_fail_first_save_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    state.preview_active = 0;
    if (app_document_apply(APP_DOCUMENT_ACTION_LOAD, &stack, &state, &preview, &composite, 0x55667788, &callbacks) ||
        !expect_int_eq("preview_fail_load_push_calls", stub.push_calls, 1) ||
        !expect_int_eq("preview_fail_load_calls", stub.load_calls, 1) ||
        stub.loaded_canvas != &stack.layers[1].canvas ||
        stub.loaded_clear_color != 0x55667788 ||
        !expect_int_eq("preview_fail_load_needs_composite", state.needs_composite, 0) ||
        !expect_int_eq("preview_fail_load_preview_active", state.preview_active, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_fail_second_save_calls", stub.save_calls, 2) ||
        stub.saved_canvas != &composite ||
        !expect_int_eq("preview_fail_second_save_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    state.preview_active = 1;
    if (app_document_apply(APP_DOCUMENT_ACTION_UNDO, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_fail_restore_calls", stub.restore_calls, 1) ||
        !expect_int_eq("preview_fail_restore_last_flag", stub.last_redo, 0) ||
        !expect_int_eq("preview_fail_restore_needs_composite", state.needs_composite, 0) ||
        !expect_int_eq("preview_fail_restore_preview_active", state.preview_active, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    canvas_free(&preview);
    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_fail_third_save_calls", stub.save_calls, 3) ||
        stub.saved_canvas != &composite ||
        !expect_int_eq("preview_fail_third_save_needs_composite", state.needs_composite, 0) ||
        !expect_int_eq("preview_fail_third_save_preview_active", state.preview_active, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        return 0;
    }

    layer_stack_free(&stack);
    canvas_free(&composite);
    return 1;
}

static int test_document_preview_toggle_coexists_with_redo_and_show_failures(void) {
    LayerStack stack;
    Canvas composite = {0};
    Canvas preview = {0};
    AppDocumentState state = {.preview_active = 1, .needs_composite = 0};
    DocumentStubState stub = {.save_result = 1, .restore_result = 0};
    AppDocumentCallbacks callbacks = {
        .save_canvas = stub_save,
        .restore_history = stub_restore,
        .push_snapshot = stub_push,
        .userdata = &stub,
    };

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) ||
        !canvas_init(&composite, 4, 4) ||
        !canvas_init(&preview, 4, 4) ||
        layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_redo_fail_first_save_calls", stub.save_calls, 1) ||
        stub.saved_canvas != &preview ||
        !expect_int_eq("preview_redo_fail_first_save_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    state.preview_active = 0;
    if (app_document_apply(APP_DOCUMENT_ACTION_REDO, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_redo_fail_restore_calls", stub.restore_calls, 1) ||
        !expect_int_eq("preview_redo_fail_last_flag", stub.last_redo, 1) ||
        !expect_int_eq("preview_redo_fail_needs_composite", state.needs_composite, 0) ||
        !expect_int_eq("preview_redo_fail_preview_active", state.preview_active, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_redo_fail_second_save_calls", stub.save_calls, 2) ||
        stub.saved_canvas != &composite ||
        !expect_int_eq("preview_redo_fail_second_save_needs_composite", state.needs_composite, 0)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    state.preview_active = 1;
    stack.active_layer = -1;
    if (app_document_apply(APP_DOCUMENT_ACTION_SHOW_ACTIVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_show_fail_push_calls", stub.push_calls, 1) ||
        !expect_int_eq("preview_show_fail_needs_composite", state.needs_composite, 0) ||
        !expect_int_eq("preview_show_fail_preview_active", state.preview_active, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        canvas_free(&preview);
        return 0;
    }

    canvas_free(&preview);
    if (!app_document_apply(APP_DOCUMENT_ACTION_SAVE, &stack, &state, &preview, &composite, 0, &callbacks) ||
        !expect_int_eq("preview_show_fail_third_save_calls", stub.save_calls, 3) ||
        stub.saved_canvas != &composite ||
        !expect_int_eq("preview_show_fail_third_save_needs_composite", state.needs_composite, 0) ||
        !expect_int_eq("preview_show_fail_third_save_preview_active", state.preview_active, 1)) {
        layer_stack_free(&stack);
        canvas_free(&composite);
        return 0;
    }

    layer_stack_free(&stack);
    canvas_free(&composite);
    return 1;
}

int main(void) {
    if (!test_document_command_for_key()) {
        return 1;
    }
    if (!test_document_command_precedence()) {
        return 1;
    }
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
    if (!test_document_failure_and_noop_paths_preserve_flags()) {
        return 1;
    }
    if (!test_document_edge_case_selection_and_save_fallbacks()) {
        return 1;
    }
    if (!test_document_preview_state_survives_save_and_load_paths()) {
        return 1;
    }
    if (!test_document_failure_chain_preserves_pending_composite_flag()) {
        return 1;
    }
    if (!test_document_success_chain_reasserts_composite_flag()) {
        return 1;
    }
    if (!test_document_preview_toggle_updates_save_canvas_selection()) {
        return 1;
    }
    if (!test_document_preview_toggle_coexists_with_visibility_actions()) {
        return 1;
    }
    if (!test_document_preview_toggle_coexists_with_undo_redo()) {
        return 1;
    }
    if (!test_document_preview_toggle_coexists_with_load_and_undo_failures()) {
        return 1;
    }
    if (!test_document_preview_toggle_coexists_with_redo_and_show_failures()) {
        return 1;
    }
    return 0;
}
