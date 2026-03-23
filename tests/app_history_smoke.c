#include "../src/app_history.h"
#include "../src/canvas.h"
#include "../src/layers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int fail_after;
    int calls;
} AllocatorStubState;

static AllocatorStubState *allocator_stub_state = NULL;
static int fail_canvas_init = 0;

static int expect_int_eq(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static void *stub_malloc(size_t size) {
    if (!allocator_stub_state) {
        return malloc(size);
    }
    allocator_stub_state->calls++;
    if (allocator_stub_state->fail_after >= 0 && allocator_stub_state->calls > allocator_stub_state->fail_after) {
        return NULL;
    }
    return malloc(size);
}

static void stub_free(void *ptr) {
    free(ptr);
}

static void install_allocator_stub(AllocatorStubState *state) {
    allocator_stub_state = state;
    app_history_set_allocators(state ? stub_malloc : NULL, state ? stub_free : NULL);
}

static int stub_canvas_init(Canvas *canvas, int width, int height) {
    if (fail_canvas_init) {
        return 0;
    }
    return canvas_init(canvas, width, height);
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

static int test_snapshot_apply_rejects_invalid_metadata_without_mutation(void) {
    LayerStack stack;
    Snapshot snapshot = {0};

    if (!layer_stack_init(&stack, 6, 6, 0xFFFFFFFF)) {
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
    stack.layers[0].visible = 0;
    stack.layers[1].locked = 1;
    stack.layers[1].opacity_percent = 40;
    canvas_set_pixel(&stack.layers[0].canvas, 3, 3, 0xFF224466);
    canvas_set_pixel(&stack.layers[1].canvas, 3, 3, 0x80775533);
    uint32_t base_pixel = canvas_get_pixel(&stack.layers[0].canvas, 3, 3);
    uint32_t top_pixel = canvas_get_pixel(&stack.layers[1].canvas, 3, 3);

    if (!snapshot_from_layers(&snapshot, &stack)) {
        fprintf(stderr, "snapshot_from_layers failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    snapshot.width += 1;
    if (snapshot_apply(&snapshot, &stack)) {
        fprintf(stderr, "snapshot_apply should fail for mismatched dimensions\n");
        snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    if (!expect_int_eq("reject_active_layer", stack.active_layer, 1) ||
        !expect_int_eq("reject_solo_index", stack.solo_index, 1) ||
        !expect_int_eq("reject_base_visible", stack.layers[0].visible, 0) ||
        !expect_int_eq("reject_top_locked", stack.layers[1].locked, 1) ||
        !expect_int_eq("reject_top_opacity", stack.layers[1].opacity_percent, 40) ||
        !expect_pixel_eq("reject_base_pixel", canvas_get_pixel(&stack.layers[0].canvas, 3, 3), base_pixel) ||
        !expect_pixel_eq("reject_top_pixel", canvas_get_pixel(&stack.layers[1].canvas, 3, 3), top_pixel)) {
        snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    snapshot_free(&snapshot);
    layer_stack_free(&stack);
    return 1;
}

static int test_snapshot_apply_preserves_state_on_canvas_allocation_failure(void) {
    LayerStack stack;
    Snapshot snapshot = {0};

    if (!layer_stack_init(&stack, 6, 6, 0xFFFFFFFF) || layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "layer stack initialization failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.active_layer = 1;
    stack.solo_index = 1;
    stack.layers[0].visible = 0;
    stack.layers[1].locked = 1;
    stack.layers[1].opacity_percent = 35;
    canvas_set_pixel(&stack.layers[0].canvas, 2, 2, 0xFF445566);
    canvas_set_pixel(&stack.layers[1].canvas, 2, 2, 0x80778899);
    uint32_t base_pixel = canvas_get_pixel(&stack.layers[0].canvas, 2, 2);
    uint32_t top_pixel = canvas_get_pixel(&stack.layers[1].canvas, 2, 2);

    if (!snapshot_from_layers(&snapshot, &stack)) {
        fprintf(stderr, "snapshot_from_layers failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    canvas_free(&stack.layers[1].canvas);
    stack.layers[1].canvas.width = 0;
    stack.layers[1].canvas.height = 0;

    fail_canvas_init = 1;
    app_history_set_canvas_init(stub_canvas_init);
    if (snapshot_apply(&snapshot, &stack)) {
        fprintf(stderr, "snapshot_apply should fail when canvas allocation fails\n");
        fail_canvas_init = 0;
        app_history_set_canvas_init(NULL);
        snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }
    fail_canvas_init = 0;
    app_history_set_canvas_init(NULL);

    if (!expect_int_eq("apply_alloc_fail_active", stack.active_layer, 1) ||
        !expect_int_eq("apply_alloc_fail_solo", stack.solo_index, 1) ||
        !expect_int_eq("apply_alloc_fail_visible", stack.layers[0].visible, 0) ||
        !expect_int_eq("apply_alloc_fail_locked", stack.layers[1].locked, 1) ||
        !expect_int_eq("apply_alloc_fail_opacity", stack.layers[1].opacity_percent, 35) ||
        !expect_int_eq("apply_alloc_fail_canvas_width", stack.layers[1].canvas.width, 0) ||
        !expect_int_eq("apply_alloc_fail_canvas_height", stack.layers[1].canvas.height, 0) ||
        stack.layers[1].canvas.pixels != NULL ||
        !expect_pixel_eq("apply_alloc_fail_base_pixel", canvas_get_pixel(&stack.layers[0].canvas, 2, 2), base_pixel) ||
        !expect_int_eq("apply_alloc_fail_top_pixel_missing", canvas_get_pixel(&stack.layers[1].canvas, 2, 2), 0) ||
        !expect_pixel_eq("apply_alloc_fail_snapshot_top_pixel", snapshot.pixels[36 + 2 * 6 + 2], top_pixel)) {
        snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    snapshot_free(&snapshot);
    layer_stack_free(&stack);
    return 1;
}

static int test_snapshot_restore_rejects_invalid_entry_without_mutation(void) {
    LayerStack stack;
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count = 0;
    int redo_count = 0;

    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));

    if (!layer_stack_init(&stack, 6, 6, 0xFFFFFFFF)) {
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
    stack.layers[0].visible = 0;
    stack.layers[1].locked = 1;
    stack.layers[1].opacity_percent = 65;
    canvas_set_pixel(&stack.layers[0].canvas, 2, 4, 0xFF556677);
    canvas_set_pixel(&stack.layers[1].canvas, 2, 4, 0x80AABBCC);
    uint32_t base_pixel = canvas_get_pixel(&stack.layers[0].canvas, 2, 4);
    uint32_t top_pixel = canvas_get_pixel(&stack.layers[1].canvas, 2, 4);

    if (!snapshot_from_layers(&undo_stack[0], &stack)) {
        fprintf(stderr, "snapshot_from_layers failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    undo_stack[0].height += 1;
    undo_count = 1;

    if (!snapshot_from_layers(&redo_stack[0], &stack)) {
        fprintf(stderr, "snapshot_from_layers failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        layer_stack_free(&stack);
        return 0;
    }
    redo_count = 1;

    if (snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count)) {
        fprintf(stderr, "snapshot_restore should fail for invalid source metadata\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    if (!expect_int_eq("restore_fail_undo_count", undo_count, 1) ||
        !expect_int_eq("restore_fail_redo_count", redo_count, 1) ||
        !expect_int_eq("restore_fail_active_layer", stack.active_layer, 1) ||
        !expect_int_eq("restore_fail_solo_index", stack.solo_index, 1) ||
        !expect_int_eq("restore_fail_base_visible", stack.layers[0].visible, 0) ||
        !expect_int_eq("restore_fail_top_locked", stack.layers[1].locked, 1) ||
        !expect_int_eq("restore_fail_top_opacity", stack.layers[1].opacity_percent, 65) ||
        !expect_pixel_eq("restore_fail_base_pixel", canvas_get_pixel(&stack.layers[0].canvas, 2, 4), base_pixel) ||
        !expect_pixel_eq("restore_fail_top_pixel", canvas_get_pixel(&stack.layers[1].canvas, 2, 4), top_pixel)) {
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

static int test_snapshot_push_preserves_stacks_on_allocation_failure(void) {
    LayerStack stack;
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count = 0;
    int redo_count = 0;
    AllocatorStubState alloc = {.fail_after = 0, .calls = 0};

    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF778899);
    if (!snapshot_from_layers(&redo_stack[0], &stack)) {
        fprintf(stderr, "snapshot_from_layers failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    redo_count = 1;

    install_allocator_stub(&alloc);
    snapshot_push(&stack, undo_stack, &undo_count, redo_stack, &redo_count);
    install_allocator_stub(NULL);

    if (!expect_int_eq("push_fail_undo_count", undo_count, 0) ||
        !expect_int_eq("push_fail_redo_count", redo_count, 1) ||
        !expect_pixel_eq("push_fail_redo_pixel", redo_stack[0].pixels[0], 0xFF778899)) {
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

static int test_snapshot_push_preserves_full_history_on_allocation_failure(void) {
    LayerStack stack;
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count = 0;
    int redo_count = 0;
    AllocatorStubState alloc = {.fail_after = 0, .calls = 0};

    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }

    for (int i = 0; i < MAX_HISTORY; i++) {
        canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF660000u + (uint32_t)i);
        if (!snapshot_from_layers(&undo_stack[i], &stack)) {
            fprintf(stderr, "snapshot_from_layers failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            layer_stack_free(&stack);
            return 0;
        }
        undo_count++;
    }
    uint32_t oldest_before = undo_stack[0].pixels[0];
    uint32_t newest_before = undo_stack[MAX_HISTORY - 1].pixels[0];

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF880001u);
    if (!snapshot_from_layers(&redo_stack[0], &stack)) {
        fprintf(stderr, "snapshot_from_layers failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        layer_stack_free(&stack);
        return 0;
    }
    redo_count = 1;
    uint32_t redo_before = redo_stack[0].pixels[0];

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF770000u);
    install_allocator_stub(&alloc);
    snapshot_push(&stack, undo_stack, &undo_count, redo_stack, &redo_count);
    install_allocator_stub(NULL);

    if (!expect_int_eq("push_full_fail_undo_count", undo_count, MAX_HISTORY) ||
        !expect_int_eq("push_full_fail_redo_count", redo_count, 1) ||
        !expect_pixel_eq("push_full_fail_oldest", undo_stack[0].pixels[0], oldest_before) ||
        !expect_pixel_eq("push_full_fail_newest", undo_stack[MAX_HISTORY - 1].pixels[0], newest_before) ||
        !expect_pixel_eq("push_full_fail_redo_pixel", redo_stack[0].pixels[0], redo_before)) {
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

static int test_snapshot_restore_preserves_state_on_allocation_failure(void) {
    LayerStack stack;
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count = 0;
    int redo_count = 0;
    AllocatorStubState alloc = {.fail_after = 0, .calls = 0};

    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));

    if (!layer_stack_init(&stack, 6, 6, 0xFFFFFFFF) || layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "layer stack initialization failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.active_layer = 1;
    stack.solo_index = 1;
    stack.layers[0].visible = 0;
    stack.layers[1].locked = 1;
    stack.layers[1].opacity_percent = 65;
    canvas_set_pixel(&stack.layers[0].canvas, 2, 2, 0xFF112244);
    canvas_set_pixel(&stack.layers[1].canvas, 2, 2, 0x80335577);
    if (!snapshot_from_layers(&undo_stack[0], &stack)) {
        fprintf(stderr, "snapshot_from_layers failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    undo_count = 1;

    uint32_t base_pixel = canvas_get_pixel(&stack.layers[0].canvas, 2, 2);
    uint32_t top_pixel = canvas_get_pixel(&stack.layers[1].canvas, 2, 2);

    install_allocator_stub(&alloc);
    if (snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count)) {
        fprintf(stderr, "snapshot_restore should fail when current snapshot allocation fails\n");
        install_allocator_stub(NULL);
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    install_allocator_stub(NULL);

    if (!expect_int_eq("restore_alloc_fail_undo_count", undo_count, 1) ||
        !expect_int_eq("restore_alloc_fail_redo_count", redo_count, 0) ||
        !expect_int_eq("restore_alloc_fail_active", stack.active_layer, 1) ||
        !expect_int_eq("restore_alloc_fail_solo", stack.solo_index, 1) ||
        !expect_int_eq("restore_alloc_fail_visible", stack.layers[0].visible, 0) ||
        !expect_int_eq("restore_alloc_fail_locked", stack.layers[1].locked, 1) ||
        !expect_int_eq("restore_alloc_fail_opacity", stack.layers[1].opacity_percent, 65) ||
        !expect_pixel_eq("restore_alloc_fail_base_pixel", canvas_get_pixel(&stack.layers[0].canvas, 2, 2), base_pixel) ||
        !expect_pixel_eq("restore_alloc_fail_top_pixel", canvas_get_pixel(&stack.layers[1].canvas, 2, 2), top_pixel)) {
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

static int test_snapshot_restore_round_trips_undo_and_redo_order(void) {
    LayerStack stack;
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count = 0;
    int redo_count = 0;

    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));

    if (!layer_stack_init(&stack, 5, 5, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF100001);
    snapshot_push(&stack, undo_stack, &undo_count, redo_stack, &redo_count);

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF100002);
    snapshot_push(&stack, undo_stack, &undo_count, redo_stack, &redo_count);

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF100003);

    if (!snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count)) {
        fprintf(stderr, "first snapshot_restore failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_int_eq("round_trip_after_undo_undo_count", undo_count, 1) ||
        !expect_int_eq("round_trip_after_undo_redo_count", redo_count, 1) ||
        !expect_pixel_eq("round_trip_after_undo_pixel", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF100002) ||
        !expect_pixel_eq("round_trip_redo_top_pixel", redo_stack[0].pixels[0], 0xFF100003)) {
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    if (!snapshot_restore(&stack, redo_stack, &redo_count, undo_stack, &undo_count)) {
        fprintf(stderr, "second snapshot_restore failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_int_eq("round_trip_after_redo_undo_count", undo_count, 2) ||
        !expect_int_eq("round_trip_after_redo_redo_count", redo_count, 0) ||
        !expect_pixel_eq("round_trip_after_redo_pixel", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF100003) ||
        !expect_pixel_eq("round_trip_undo_oldest_pixel", undo_stack[0].pixels[0], 0xFF100001) ||
        !expect_pixel_eq("round_trip_undo_newest_pixel", undo_stack[1].pixels[0], 0xFF100002)) {
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

static int test_snapshot_restore_drops_oldest_when_destination_is_full(void) {
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

    for (int i = 0; i < MAX_HISTORY; i++) {
        canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF200000u + (uint32_t)i);
        if (!snapshot_from_layers(&redo_stack[i], &stack)) {
            fprintf(stderr, "snapshot_from_layers failed\n");
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        redo_count++;
    }

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF300001);
    if (!snapshot_from_layers(&undo_stack[0], &stack)) {
        fprintf(stderr, "snapshot_from_layers failed\n");
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    undo_count = 1;

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF300002);

    if (!snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count)) {
        fprintf(stderr, "snapshot_restore failed for full destination stack\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    if (!expect_int_eq("restore_full_undo_count", undo_count, 0) ||
        !expect_int_eq("restore_full_redo_count", redo_count, MAX_HISTORY) ||
        !expect_pixel_eq("restore_full_live_pixel", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF300001) ||
        !expect_pixel_eq("restore_full_redo_oldest_pixel", redo_stack[0].pixels[0], 0xFF200001) ||
        !expect_pixel_eq("restore_full_redo_newest_pixel", redo_stack[MAX_HISTORY - 1].pixels[0], 0xFF300002)) {
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

static int test_snapshot_restore_preserves_full_destination_on_failure(void) {
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

    for (int i = 0; i < MAX_HISTORY; i++) {
        canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF400000u + (uint32_t)i);
        if (!snapshot_from_layers(&redo_stack[i], &stack)) {
            fprintf(stderr, "snapshot_from_layers failed\n");
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        redo_count++;
    }
    uint32_t redo_oldest_before = redo_stack[0].pixels[0];
    uint32_t redo_newest_before = redo_stack[MAX_HISTORY - 1].pixels[0];

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF500001);
    if (!snapshot_from_layers(&undo_stack[0], &stack)) {
        fprintf(stderr, "snapshot_from_layers failed\n");
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    undo_stack[0].width += 1;
    undo_count = 1;

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF500002);
    uint32_t live_pixel = canvas_get_pixel(&stack.layers[0].canvas, 0, 0);

    if (snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count)) {
        fprintf(stderr, "snapshot_restore should fail for invalid source with full destination\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    if (!expect_int_eq("restore_full_fail_undo_count", undo_count, 1) ||
        !expect_int_eq("restore_full_fail_redo_count", redo_count, MAX_HISTORY) ||
        !expect_pixel_eq("restore_full_fail_live_pixel", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), live_pixel) ||
        !expect_pixel_eq("restore_full_fail_redo_oldest", redo_stack[0].pixels[0], redo_oldest_before) ||
        !expect_pixel_eq("restore_full_fail_redo_newest", redo_stack[MAX_HISTORY - 1].pixels[0], redo_newest_before)) {
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

static int test_snapshot_restore_preserves_full_destination_on_allocation_failure(void) {
    LayerStack stack;
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count = 0;
    int redo_count = 0;
    AllocatorStubState alloc = {.fail_after = 0, .calls = 0};

    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }

    for (int i = 0; i < MAX_HISTORY; i++) {
        canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF520000u + (uint32_t)i);
        if (!snapshot_from_layers(&redo_stack[i], &stack)) {
            fprintf(stderr, "snapshot_from_layers failed\n");
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        redo_count++;
    }
    uint32_t redo_oldest_before = redo_stack[0].pixels[0];
    uint32_t redo_newest_before = redo_stack[MAX_HISTORY - 1].pixels[0];

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF530001);
    if (!snapshot_from_layers(&undo_stack[0], &stack)) {
        fprintf(stderr, "snapshot_from_layers failed\n");
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    undo_count = 1;

    uint32_t live_pixel = canvas_get_pixel(&stack.layers[0].canvas, 0, 0);
    install_allocator_stub(&alloc);
    if (snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count)) {
        fprintf(stderr, "snapshot_restore should fail when allocation fails with full destination\n");
        install_allocator_stub(NULL);
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    install_allocator_stub(NULL);

    if (!expect_int_eq("restore_full_alloc_fail_undo_count", undo_count, 1) ||
        !expect_int_eq("restore_full_alloc_fail_redo_count", redo_count, MAX_HISTORY) ||
        !expect_pixel_eq("restore_full_alloc_fail_live_pixel", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), live_pixel) ||
        !expect_pixel_eq("restore_full_alloc_fail_redo_oldest", redo_stack[0].pixels[0], redo_oldest_before) ||
        !expect_pixel_eq("restore_full_alloc_fail_redo_newest", redo_stack[MAX_HISTORY - 1].pixels[0], redo_newest_before)) {
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

static int test_snapshot_restore_recovers_layer_count_and_metadata(void) {
    LayerStack stack;
    Snapshot undo_stack[MAX_HISTORY];
    Snapshot redo_stack[MAX_HISTORY];
    int undo_count = 0;
    int redo_count = 0;

    memset(undo_stack, 0, sizeof(undo_stack));
    memset(redo_stack, 0, sizeof(redo_stack));

    if (!layer_stack_init(&stack, 6, 6, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Mid", 0x00000000) < 0 ||
        layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "layer_stack_add failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.active_layer = 2;
    stack.solo_index = 1;
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[1].locked = 1;
    stack.layers[1].opacity_percent = 45;
    stack.layers[2].opacity_percent = 70;
    strncpy(stack.layers[0].name, "Base", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[1].name, "HiddenMid", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[2].name, "SoloTop", LAYER_NAME_MAX - 1);
    stack.layers[2].name[LAYER_NAME_MAX - 1] = '\0';
    canvas_set_pixel(&stack.layers[0].canvas, 1, 1, 0xFF010203);
    canvas_set_pixel(&stack.layers[1].canvas, 1, 1, 0x80040506);
    canvas_set_pixel(&stack.layers[2].canvas, 1, 1, 0xC0070809);

    uint32_t base_pixel = canvas_get_pixel(&stack.layers[0].canvas, 1, 1);
    uint32_t mid_pixel = canvas_get_pixel(&stack.layers[1].canvas, 1, 1);
    uint32_t top_pixel = canvas_get_pixel(&stack.layers[2].canvas, 1, 1);

    snapshot_push(&stack, undo_stack, &undo_count, redo_stack, &redo_count);

    if (!layer_stack_delete(&stack, 2)) {
        fprintf(stderr, "layer_stack_delete failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 0;
    stack.solo_index = -1;
    stack.layers[0].visible = 0;
    stack.layers[0].locked = 1;
    stack.layers[0].opacity_percent = 15;
    strncpy(stack.layers[0].name, "MutatedBase", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    canvas_set_pixel(&stack.layers[0].canvas, 1, 1, 0xFFABCDEF);
    canvas_set_pixel(&stack.layers[1].canvas, 1, 1, 0xFF112233);

    if (!snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count)) {
        fprintf(stderr, "snapshot_restore failed for layer-count recovery\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    if (!expect_int_eq("layer_count_restore_undo_count", undo_count, 0) ||
        !expect_int_eq("layer_count_restore_redo_count", redo_count, 1) ||
        !expect_int_eq("layer_count_restore_count", stack.layer_count, 3) ||
        !expect_int_eq("layer_count_restore_active", stack.active_layer, 2) ||
        !expect_int_eq("layer_count_restore_solo", stack.solo_index, 1) ||
        !expect_int_eq("layer_count_restore_mid_visible", stack.layers[1].visible, 0) ||
        !expect_int_eq("layer_count_restore_mid_locked", stack.layers[1].locked, 1) ||
        !expect_int_eq("layer_count_restore_mid_opacity", stack.layers[1].opacity_percent, 45) ||
        !expect_int_eq("layer_count_restore_top_opacity", stack.layers[2].opacity_percent, 70) ||
        strcmp(stack.layers[1].name, "HiddenMid") != 0 ||
        strcmp(stack.layers[2].name, "SoloTop") != 0 ||
        !expect_pixel_eq("layer_count_restore_base_pixel", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), base_pixel) ||
        !expect_pixel_eq("layer_count_restore_mid_pixel", canvas_get_pixel(&stack.layers[1].canvas, 1, 1), mid_pixel) ||
        !expect_pixel_eq("layer_count_restore_top_pixel", canvas_get_pixel(&stack.layers[2].canvas, 1, 1), top_pixel) ||
        !expect_int_eq("layer_count_redo_count", redo_stack[0].layer_count, 2) ||
        !expect_int_eq("layer_count_redo_active", redo_stack[0].active_layer, 0) ||
        !expect_int_eq("layer_count_redo_solo", redo_stack[0].solo_index, -1) ||
        !expect_int_eq("layer_count_redo_base_visible", redo_stack[0].visibility[0], 0) ||
        !expect_int_eq("layer_count_redo_base_locked", redo_stack[0].locked[0], 1) ||
        !expect_int_eq("layer_count_redo_base_opacity", redo_stack[0].opacity_percent[0], 15) ||
        strcmp(redo_stack[0].names[0], "MutatedBase") != 0) {
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    if (!snapshot_restore(&stack, redo_stack, &redo_count, undo_stack, &undo_count)) {
        fprintf(stderr, "snapshot_restore redo failed for layer-count recovery\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    if (!expect_int_eq("layer_count_redo_undo_count", undo_count, 1) ||
        !expect_int_eq("layer_count_redo_redo_count", redo_count, 0) ||
        !expect_int_eq("layer_count_redo_count_live", stack.layer_count, 2) ||
        !expect_int_eq("layer_count_redo_active_live", stack.active_layer, 0) ||
        !expect_int_eq("layer_count_redo_solo_live", stack.solo_index, -1) ||
        !expect_int_eq("layer_count_redo_base_visible_live", stack.layers[0].visible, 0) ||
        !expect_int_eq("layer_count_redo_base_locked_live", stack.layers[0].locked, 1) ||
        !expect_int_eq("layer_count_redo_base_opacity_live", stack.layers[0].opacity_percent, 15) ||
        strcmp(stack.layers[0].name, "MutatedBase") != 0 ||
        !expect_pixel_eq("layer_count_redo_base_pixel_live", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFFABCDEF) ||
        !expect_pixel_eq("layer_count_redo_mid_pixel_live", canvas_get_pixel(&stack.layers[1].canvas, 1, 1), 0xFF112233) ||
        !expect_int_eq("layer_count_redo_undo_snapshot_count", undo_stack[0].layer_count, 3) ||
        !expect_int_eq("layer_count_redo_undo_snapshot_active", undo_stack[0].active_layer, 2) ||
        !expect_int_eq("layer_count_redo_undo_snapshot_solo", undo_stack[0].solo_index, 1)) {
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
    if (!test_snapshot_apply_rejects_invalid_metadata_without_mutation()) {
        return 1;
    }
    if (!test_snapshot_apply_preserves_state_on_canvas_allocation_failure()) {
        return 1;
    }
    if (!test_snapshot_restore_rejects_invalid_entry_without_mutation()) {
        return 1;
    }
    if (!test_snapshot_push_preserves_stacks_on_allocation_failure()) {
        return 1;
    }
    if (!test_snapshot_push_preserves_full_history_on_allocation_failure()) {
        return 1;
    }
    if (!test_snapshot_restore_preserves_state_on_allocation_failure()) {
        return 1;
    }
    if (!test_snapshot_restore_round_trips_undo_and_redo_order()) {
        return 1;
    }
    if (!test_snapshot_restore_drops_oldest_when_destination_is_full()) {
        return 1;
    }
    if (!test_snapshot_restore_preserves_full_destination_on_failure()) {
        return 1;
    }
    if (!test_snapshot_restore_preserves_full_destination_on_allocation_failure()) {
        return 1;
    }
    if (!test_snapshot_restore_recovers_layer_count_and_metadata()) {
        return 1;
    }
    return 0;
}
