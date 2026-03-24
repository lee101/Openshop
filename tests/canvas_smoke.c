#include "../src/canvas.h"
#include "../src/history.h"
#include "../src/layers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_pixel_eq(const char *label, uint32_t got, uint32_t want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

static int snapshot_is_reset(const LayerSnapshot *snapshot) {
    return snapshot &&
           !snapshot->pixels &&
           snapshot->width == 0 &&
           snapshot->height == 0 &&
           snapshot->layer_count == 0 &&
           snapshot->active_layer == 0 &&
           snapshot->solo_index == -1 &&
           snapshot->visibility[0] == 0 &&
           snapshot->names[0][0] == '\0';
}

static int expect_history_counts(const char *label, int undo_count, int redo_count, int want_undo, int want_redo) {
    if (undo_count != want_undo || redo_count != want_redo) {
        fprintf(stderr, "%s count mismatch: undo=%d redo=%d want undo=%d redo=%d\n",
                label, undo_count, redo_count, want_undo, want_redo);
        return 0;
    }
    return 1;
}

static int expect_wrapper_history_counts(const char *label, const LayerHistory *history, int want_undo, int want_redo) {
    return history && expect_history_counts(label, history->undo_count, history->redo_count, want_undo, want_redo);
}

static int snapshot_has_marker_state(const LayerSnapshot *snapshot) {
    return snapshot &&
           snapshot->width == 7 &&
           snapshot->height == 9 &&
           snapshot->layer_count == 3 &&
           snapshot->active_layer == 2 &&
           snapshot->solo_index == 1 &&
           snapshot->visibility[0] == 4 &&
           snapshot->names[0][0] == 'm' &&
           snapshot->pixels == (uint32_t *)1;
}

static void set_snapshot_marker_state(LayerSnapshot *snapshot) {
    if (!snapshot) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->width = 7;
    snapshot->height = 9;
    snapshot->layer_count = 3;
    snapshot->active_layer = 2;
    snapshot->solo_index = 1;
    snapshot->visibility[0] = 4;
    snapshot->names[0][0] = 'm';
    snapshot->pixels = (uint32_t *)1;
}

static int test_layer_snapshot_restore(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "snapshot stack init failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Ink", 0x00000000) != 1) {
        fprintf(stderr, "snapshot add second layer failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF010203);
    canvas_set_pixel(&stack.layers[1].canvas, 1, 1, 0xFFABCDEF);
    stack.layers[1].opacity_percent = 47;
    stack.active_layer = 1;

    LayerSnapshot snapshot = {0};
    if (!layer_snapshot_capture(&snapshot, &stack)) {
        fprintf(stderr, "snapshot capture failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_stack_add(&stack, "Locked Extra", 0x00000000) != 2 ||
        layer_stack_add(&stack, "Locked Extra 2", 0x00000000) != 3) {
        fprintf(stderr, "snapshot extra layer setup failed\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 1;
    stack.layers[2].visible = 0;
    stack.layers[3].opacity_percent = 12;
    stack.solo_index = 3;
    stack.active_layer = 3;
    canvas_set_pixel(&stack.layers[3].canvas, 2, 2, 0xFF556677);

    if (!layer_snapshot_apply(&snapshot, &stack)) {
        fprintf(stderr, "snapshot apply failed\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    if (stack.layer_count != 2 || stack.active_layer != 1 || stack.solo_index != -1) {
        fprintf(stderr, "snapshot apply bookkeeping failed\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[2].canvas.pixels || stack.layers[3].canvas.pixels) {
        fprintf(stderr, "snapshot apply should free truncated layer canvases\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[0].name, "Background") != 0 || strcmp(stack.layers[1].name, "Ink") != 0) {
        fprintf(stderr, "snapshot apply should restore layer names\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[0].visible || !stack.layers[1].visible || stack.layers[1].locked || stack.layers[1].opacity_percent != 47) {
        fprintf(stderr, "snapshot apply should restore layer metadata\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("snapshot_restore_background", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
        !expect_pixel_eq("snapshot_restore_ink", canvas_get_pixel(&stack.layers[1].canvas, 1, 1), 0xFFABCDEF)) {
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    layer_snapshot_free(&snapshot);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_snapshot_expand_restore(void) {
    LayerStack source;
    if (!layer_stack_init(&source, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "snapshot source init failed\n");
        return 0;
    }
    if (layer_stack_add(&source, "Mid", 0x00000000) != 1 ||
        layer_stack_add(&source, "Top", 0x00000000) != 2) {
        fprintf(stderr, "snapshot source add layers failed\n");
        layer_stack_free(&source);
        return 0;
    }

    canvas_set_pixel(&source.layers[0].canvas, 0, 0, 0xFF101112);
    canvas_set_pixel(&source.layers[1].canvas, 1, 1, 0xFF202122);
    canvas_set_pixel(&source.layers[2].canvas, 2, 2, 0xFF303132);
    source.layers[1].visible = 0;
    source.layers[2].locked = 1;
    source.layers[2].opacity_percent = 63;
    source.active_layer = 2;
    source.solo_index = 2;

    LayerSnapshot snapshot = {0};
    if (!layer_snapshot_capture(&snapshot, &source)) {
        fprintf(stderr, "snapshot expand capture failed\n");
        layer_stack_free(&source);
        return 0;
    }
    layer_stack_free(&source);

    LayerStack dest;
    if (!layer_stack_init(&dest, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "snapshot dest init failed\n");
        layer_snapshot_free(&snapshot);
        return 0;
    }

    if (!layer_snapshot_apply(&snapshot, &dest)) {
        fprintf(stderr, "snapshot expand apply failed\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&dest);
        return 0;
    }

    if (dest.layer_count != 3 || dest.active_layer != 2 || dest.solo_index != 2) {
        fprintf(stderr, "snapshot expand bookkeeping failed\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&dest);
        return 0;
    }
    if (strcmp(dest.layers[1].name, "Mid") != 0 || strcmp(dest.layers[2].name, "Top") != 0) {
        fprintf(stderr, "snapshot expand should restore layer names\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&dest);
        return 0;
    }
    if (dest.layers[1].visible || !dest.layers[2].locked || dest.layers[2].opacity_percent != 63) {
        fprintf(stderr, "snapshot expand should restore visibility/lock/opacity\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&dest);
        return 0;
    }
    if (!expect_pixel_eq("snapshot_expand_background", canvas_get_pixel(&dest.layers[0].canvas, 0, 0), 0xFF101112) ||
        !expect_pixel_eq("snapshot_expand_hidden_mid", canvas_get_pixel(&dest.layers[1].canvas, 1, 1), 0xFF202122) ||
        !expect_pixel_eq("snapshot_expand_top", canvas_get_pixel(&dest.layers[2].canvas, 2, 2), 0xFF303132)) {
        layer_snapshot_free(&snapshot);
        layer_stack_free(&dest);
        return 0;
    }

    layer_snapshot_free(&snapshot);
    layer_stack_free(&dest);
    return 1;
}

static int test_layer_snapshot_capture_apply_guard_paths(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "snapshot guard init failed\n");
        return 0;
    }
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF102030);

    LayerSnapshot snapshot = {0};
    set_snapshot_marker_state(&snapshot);

    if (layer_snapshot_capture(NULL, &stack) || layer_snapshot_capture(&snapshot, NULL)) {
        fprintf(stderr, "snapshot capture should fail cleanly on null inputs\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_has_marker_state(&snapshot)) {
        fprintf(stderr, "snapshot capture null-input paths should leave caller snapshot untouched\n");
        layer_stack_free(&stack);
        return 0;
    }

    snapshot.pixels = NULL;
    if (layer_snapshot_apply(NULL, &stack) || layer_snapshot_apply(&snapshot, NULL) || layer_snapshot_apply(&snapshot, &stack)) {
        fprintf(stderr, "snapshot apply should fail cleanly on null or empty inputs\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("snapshot_apply_rejected_null_or_empty_preserves_stack", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF102030)) {
        layer_stack_free(&stack);
        return 0;
    }

    snapshot.width = stack.width + 1;
    snapshot.height = stack.height;
    snapshot.layer_count = 1;
    snapshot.pixels = (uint32_t *)1;
    if (layer_snapshot_apply(&snapshot, &stack)) {
        fprintf(stderr, "snapshot apply should reject width mismatches\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("snapshot_apply_rejected_width_preserves_stack", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF102030)) {
        layer_stack_free(&stack);
        return 0;
    }

    snapshot.width = stack.width;
    snapshot.height = stack.height + 1;
    if (layer_snapshot_apply(&snapshot, &stack)) {
        fprintf(stderr, "snapshot apply should reject height mismatches\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("snapshot_apply_rejected_height_preserves_stack", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF102030)) {
        layer_stack_free(&stack);
        return 0;
    }

    snapshot.height = stack.height;
    snapshot.layer_count = 0;
    if (layer_snapshot_apply(&snapshot, &stack)) {
        fprintf(stderr, "snapshot apply should reject empty layer counts\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("snapshot_apply_rejected_empty_count_preserves_stack", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF102030)) {
        layer_stack_free(&stack);
        return 0;
    }

    snapshot.layer_count = MAX_LAYERS + 1;
    if (layer_snapshot_apply(&snapshot, &stack)) {
        fprintf(stderr, "snapshot apply should reject oversized layer counts\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("snapshot_apply_rejected_oversized_count_preserves_stack", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF102030)) {
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_layer_snapshot_capture_reuses_existing_snapshot(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "snapshot reuse init failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Ink", 0x00000000) != 1) {
        fprintf(stderr, "snapshot reuse add layer failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF112233);
    canvas_set_pixel(&stack.layers[1].canvas, 1, 1, 0xFF445566);
    stack.layers[1].visible = 0;
    stack.layers[1].locked = 1;
    stack.layers[1].opacity_percent = 37;
    stack.active_layer = 1;

    LayerSnapshot snapshot = {0};
    snapshot.width = 9;
    snapshot.height = 7;
    snapshot.layer_count = 3;
    snapshot.active_layer = 2;
    snapshot.solo_index = 1;
    snapshot.visibility[0] = 9;
    snapshot.names[0][0] = 'x';
    snapshot.pixels = (uint32_t *)malloc(4 * sizeof(uint32_t));
    if (!snapshot.pixels) {
        fprintf(stderr, "snapshot reuse seed allocation failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    snapshot.pixels[0] = 0xDEADBEEF;

    if (!layer_snapshot_capture(&snapshot, &stack)) {
        fprintf(stderr, "snapshot reuse capture failed\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    if (snapshot.width != 4 || snapshot.height != 4 || snapshot.layer_count != 2 ||
        snapshot.active_layer != 1 || snapshot.solo_index != -1) {
        fprintf(stderr, "snapshot reuse capture should replace scalar metadata\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }
    if (snapshot.visibility[1] != 0 || snapshot.locked[1] != 1 || snapshot.opacity_percent[1] != 37) {
        fprintf(stderr, "snapshot reuse capture should replace layer metadata\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(snapshot.names[0], "Background") != 0 || strcmp(snapshot.names[1], "Ink") != 0) {
        fprintf(stderr, "snapshot reuse capture should replace layer names\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("snapshot_reuse_background", snapshot.pixels[0], 0xFF112233) ||
        !expect_pixel_eq("snapshot_reuse_ink", snapshot.pixels[21], 0xFF445566)) {
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    layer_snapshot_free(&snapshot);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_snapshot_free_preserves_metadata_arrays(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "snapshot free init failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Ink", 0x00000000) != 1) {
        fprintf(stderr, "snapshot free add layer failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[1].visible = 0;
    stack.layers[1].locked = 1;
    stack.layers[1].opacity_percent = 42;

    LayerSnapshot snapshot = {0};
    if (!layer_snapshot_capture(&snapshot, &stack)) {
        fprintf(stderr, "snapshot free capture failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    layer_snapshot_free(&snapshot);
    if (!snapshot_is_reset(&snapshot)) {
        if (snapshot.pixels != NULL || snapshot.width != 0 || snapshot.height != 0 ||
            snapshot.layer_count != 0 || snapshot.active_layer != 0 || snapshot.solo_index != -1) {
            fprintf(stderr, "snapshot free should clear owned storage and scalar bookkeeping\n");
            layer_stack_free(&stack);
            return 0;
        }
        if (snapshot.visibility[1] != 0 || snapshot.locked[1] != 1 || snapshot.opacity_percent[1] != 42) {
            fprintf(stderr, "snapshot free should preserve metadata arrays for callers that reuse them\n");
            layer_stack_free(&stack);
            return 0;
        }
        if (strcmp(snapshot.names[1], "Ink") != 0) {
            fprintf(stderr, "snapshot free should preserve layer names\n");
            layer_stack_free(&stack);
            return 0;
        }
    } else {
        fprintf(stderr, "snapshot free should not fully reset metadata arrays\n");
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_stack(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 2, 2, 0xFFFFFFFF)) {
        fprintf(stderr, "history stack init failed\n");
        return 0;
    }

    LayerHistory history = {0};

    for (int i = 0; i < HISTORY_CAPACITY + 2; i++) {
        layer_history_record(&history, &stack);
        canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF000000u | (uint32_t)i);
    }

    if (history.undo_count != HISTORY_CAPACITY || history.redo_count != 0) {
        fprintf(stderr, "history stack capacity bookkeeping failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history undo failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("history_undo_latest", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF000014)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_history_step_redo(&history, &stack)) {
        fprintf(stderr, "history redo failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("history_redo_latest", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF000015)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    while (history.undo_count > 1) {
        if (!layer_history_step_undo(&history, &stack)) {
            fprintf(stderr, "history multi-undo failed\n");
            layer_history_reset(&history);
            layer_stack_free(&stack);
            return 0;
        }
    }
    if (!layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history oldest-retained undo failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("history_oldest_retained", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF000001)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history undo should stop at retained oldest snapshot\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    while (history.redo_count > 0) {
        if (!layer_history_step_redo(&history, &stack)) {
            fprintf(stderr, "history multi-redo failed\n");
            layer_history_reset(&history);
            layer_stack_free(&stack);
            return 0;
        }
    }
    if (!expect_pixel_eq("history_redo_back_to_latest", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF000015)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_record(&history, &stack);
    if (history.redo_count != 0) {
        fprintf(stderr, "history push should clear redo stack\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_layer_count_roundtrip(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 3, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "history layer-count init failed\n");
        return 0;
    }

    LayerHistory history = {0};

    layer_history_record(&history, &stack);
    if (layer_stack_add(&stack, "Sketch", 0x00000000) != 1) {
        fprintf(stderr, "history layer-count add layer failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_set_pixel(&stack.layers[1].canvas, 1, 1, 0xFF556677);
    stack.layers[1].visible = 0;
    stack.active_layer = 1;

    layer_history_record(&history, &stack);
    if (layer_stack_add(&stack, "Top", 0x00000000) != 2) {
        fprintf(stderr, "history layer-count add second layer failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_set_pixel(&stack.layers[2].canvas, 2, 2, 0xFF8899AA);
    stack.layers[2].locked = 1;
    stack.layers[2].opacity_percent = 42;
    stack.active_layer = 2;
    stack.solo_index = 2;

    if (!layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history layer-count first undo failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 2 || stack.active_layer != 1 || stack.solo_index != -1) {
        fprintf(stderr, "history layer-count first undo bookkeeping failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("history_layer_count_first_undo", canvas_get_pixel(&stack.layers[1].canvas, 1, 1), 0xFF556677) ||
        stack.layers[1].visible != 0) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history layer-count second undo failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 1 || stack.active_layer != 0) {
        fprintf(stderr, "history layer-count second undo bookkeeping failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_history_step_redo(&history, &stack) ||
        !layer_history_step_redo(&history, &stack)) {
        fprintf(stderr, "history layer-count redo failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 3 || stack.active_layer != 2 || stack.solo_index != 2) {
        fprintf(stderr, "history layer-count redo bookkeeping failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[2].locked || stack.layers[2].opacity_percent != 42 ||
        !expect_pixel_eq("history_layer_count_redo", canvas_get_pixel(&stack.layers[2].canvas, 2, 2), 0xFF8899AA)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_struct_api(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 3, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "history struct init failed\n");
        return 0;
    }

    LayerHistory history = {0};

    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF121212);

    layer_history_record(&history, &stack);
    if (layer_stack_add(&stack, "Overlay", 0x00000000) != 1) {
        fprintf(stderr, "history struct add layer failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_set_pixel(&stack.layers[1].canvas, 1, 1, 0xFF343434);
    stack.active_layer = 1;

    if (!layer_history_step_undo(&history, &stack) || stack.layer_count != 1 ||
        !expect_pixel_eq("history_struct_undo", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF121212)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_history_step_redo(&history, &stack) || stack.layer_count != 2 || stack.active_layer != 1 ||
        !expect_pixel_eq("history_struct_redo", canvas_get_pixel(&stack.layers[1].canvas, 1, 1), 0xFF343434)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_history_step_undo(&history, &stack) || history.redo_count != 1) {
        fprintf(stderr, "history struct redo population failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    layer_history_reset(&history);
    if (history.undo_count != 0 || history.redo_count != 0) {
        fprintf(stderr, "history struct reset should clear populated redo\n");
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_record(&history, &stack);
    if (layer_history_step_redo(&history, &stack) || history.redo_count != 0) {
        fprintf(stderr, "history struct redo should stay empty after reset\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_record(&history, &stack);
    if (history.redo_count != 0) {
        fprintf(stderr, "history struct record should clear redo\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    if (history.undo_count != 0 || history.redo_count != 0) {
        fprintf(stderr, "history struct reset failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_skips_duplicate_snapshots(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 3, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "history duplicate init failed\n");
        return 0;
    }

    LayerHistory history = {0};

    layer_history_record(&history, &stack);
    layer_history_record(&history, &stack);
    if (history.undo_count != 1 || history.redo_count != 0) {
        fprintf(stderr, "history duplicate record should not grow undo or redo\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 1, 1, 0xFF135724);
    layer_history_record(&history, &stack);
    if (history.undo_count != 2 || history.redo_count != 0) {
        fprintf(stderr, "history duplicate changed-state record should only add one snapshot\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel(&stack.layers[0].canvas, 1, 1, 0xFF246813);

    if (!layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history duplicate undo failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("history_duplicate_undo", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF135724)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_history_step_redo(&history, &stack)) {
        fprintf(stderr, "history duplicate redo failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("history_duplicate_redo", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF246813)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history duplicate second undo failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("history_duplicate_second_undo", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF135724)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history duplicate third undo failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("history_duplicate_third_undo", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFFFFFFFF)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history duplicate undo should stop after retained oldest snapshot\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_manual_snapshot_recording(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 3, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "history manual snapshot init failed\n");
        return 0;
    }

    LayerHistory history = {0};
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF112233);
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF445566);

    if (!layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history manual snapshot undo setup failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (history.redo_count != 1 ||
        !expect_pixel_eq("history_manual_snapshot_after_undo", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF112233)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    LayerSnapshot snapshot = {0};
    if (!layer_snapshot_capture(&snapshot, &stack)) {
        fprintf(stderr, "history manual snapshot capture failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF778899);
    if (!layer_history_record_snapshot(&history, &snapshot)) {
        fprintf(stderr, "history manual snapshot commit failed\n");
        layer_snapshot_free(&snapshot);
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_is_reset(&snapshot)) {
        fprintf(stderr, "history manual snapshot should disown caller snapshot after commit\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (history.redo_count != 0 || history.undo_count != 2) {
        fprintf(stderr, "history manual snapshot should clear redo and grow undo\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_history_step_undo(&history, &stack) ||
        !expect_pixel_eq("history_manual_snapshot_undo", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF112233)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_history_step_redo(&history, &stack) ||
        !expect_pixel_eq("history_manual_snapshot_redo", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF778899)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_record_snapshot_discards_duplicate(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 3, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "history duplicate snapshot init failed\n");
        return 0;
    }

    LayerHistory history = {0};
    layer_history_record(&history, &stack);

    LayerSnapshot snapshot = {0};
    if (!layer_snapshot_capture(&snapshot, &stack)) {
        fprintf(stderr, "history duplicate snapshot capture failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_history_record_snapshot(&history, &snapshot)) {
        fprintf(stderr, "history duplicate snapshot should not record identical state\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_is_reset(&snapshot)) {
        fprintf(stderr, "history duplicate snapshot should fully reset discarded snapshot\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (history.undo_count != 1 || history.redo_count != 0) {
        fprintf(stderr, "history duplicate snapshot should leave history unchanged\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_record_snapshot_null_history_resets(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 3, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "history null snapshot init failed\n");
        return 0;
    }

    LayerSnapshot snapshot = {0};
    if (!layer_snapshot_capture(&snapshot, &stack)) {
        fprintf(stderr, "history null snapshot capture failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_history_record_snapshot(NULL, &snapshot)) {
        fprintf(stderr, "history null snapshot should fail without history state\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_is_reset(&snapshot)) {
        fprintf(stderr, "history null snapshot should fully reset discarded snapshot\n");
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_record_snapshot_current_state_clears_redo(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 3, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "history current snapshot redo init failed\n");
        return 0;
    }

    LayerHistory history = {0};
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF010203);
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF040506);

    if (!layer_history_step_undo(&history, &stack) || history.redo_count != 1) {
        fprintf(stderr, "history duplicate redo undo setup failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    LayerSnapshot snapshot = {0};
    if (!layer_snapshot_capture(&snapshot, &stack)) {
        fprintf(stderr, "history current snapshot redo capture failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_history_record_snapshot(&history, &snapshot)) {
        fprintf(stderr, "history current snapshot should record against older undo top\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_is_reset(&snapshot)) {
        fprintf(stderr, "history current snapshot should disown caller snapshot after record\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (history.undo_count != 2 || history.redo_count != 0) {
        fprintf(stderr, "history current snapshot should clear redo after record\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_history_step_undo(&history, &stack) ||
        !expect_pixel_eq("history_current_snapshot_first_undo", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_history_step_undo(&history, &stack) ||
        !expect_pixel_eq("history_current_snapshot_second_undo", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFFFFFFFF)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_discarded_snapshot_keeps_redo(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 3, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "history discarded snapshot init failed\n");
        return 0;
    }

    LayerHistory history = {0};
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF010203);
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF040506);

    if (!layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history discarded snapshot undo setup failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    LayerSnapshot snapshot = {0};
    if (!layer_snapshot_capture(&snapshot, &stack)) {
        fprintf(stderr, "history discarded snapshot capture failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    layer_snapshot_free(&snapshot);

    if (history.redo_count != 1) {
        fprintf(stderr, "history discarded snapshot should keep redo intact\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_history_step_redo(&history, &stack) ||
        !expect_pixel_eq("history_discarded_snapshot_redo", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF040506)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_snapshot_matches_stack_noop_edits(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "history noop-match init failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Ink", 0x00000000) != 1) {
        fprintf(stderr, "history noop-match add layer failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    LayerSnapshot snapshot = {0};
    if (!layer_snapshot_capture(&snapshot, &stack)) {
        fprintf(stderr, "history noop-match capture failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_clear_layer(&stack, 1, 0x00000000) || !layer_snapshot_matches_stack(&snapshot, &stack)) {
        fprintf(stderr, "history noop clear should match captured snapshot\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_translate(&stack.layers[1].canvas, 1, 0, 0x00000000);
    if (!layer_snapshot_matches_stack(&snapshot, &stack)) {
        fprintf(stderr, "history noop translate should match captured snapshot\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel(&stack.layers[1].canvas, 0, 0, 0xFF123456);
    if (layer_snapshot_matches_stack(&snapshot, &stack)) {
        fprintf(stderr, "history changed stack should not match captured snapshot\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    layer_snapshot_free(&snapshot);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_snapshot_matches_stack_guard_paths(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "history match-guard init failed\n");
        return 0;
    }

    LayerSnapshot snapshot = {0};
    if (layer_snapshot_matches_stack(NULL, &stack) || layer_snapshot_matches_stack(&snapshot, NULL)) {
        fprintf(stderr, "history matches-stack should fail cleanly on null inputs\n");
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_snapshot_capture(&snapshot, &stack)) {
        fprintf(stderr, "history match-guard capture failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    snapshot.width++;
    if (layer_snapshot_matches_stack(&snapshot, &stack)) {
        fprintf(stderr, "history matches-stack should reject mismatched snapshot dimensions\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    if (snapshot.width != stack.width + 1 || snapshot.pixels == NULL) {
        fprintf(stderr, "history matches-stack should not mutate caller snapshots on failure\n");
        layer_snapshot_free(&snapshot);
        layer_stack_free(&stack);
        return 0;
    }

    layer_snapshot_free(&snapshot);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_skip_noop_snapshot_commit(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "history noop-commit init failed\n");
        return 0;
    }

    LayerHistory history = {0};
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF010203);
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF040506);

    if (!layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history noop-commit undo setup failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    LayerSnapshot snapshot = {0};
    if (!layer_snapshot_capture(&snapshot, &stack)) {
        fprintf(stderr, "history noop-commit capture failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_snapshot_matches_stack(&snapshot, &stack)) {
        fprintf(stderr, "history noop-commit expected immediate snapshot match\n");
        layer_snapshot_free(&snapshot);
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    layer_snapshot_free(&snapshot);
    if (history.redo_count != 1) {
        fprintf(stderr, "history noop-commit should preserve redo when snapshot is discarded\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_history_step_redo(&history, &stack) ||
        !expect_pixel_eq("history_noop_commit_redo", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF040506)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_snapshot_reset_clears_allocated_state(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 3, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "history snapshot reset init failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Ink", 0x00000000) != 1) {
        fprintf(stderr, "history snapshot reset add layer failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].visible = 0;
    stack.layers[1].locked = 1;
    stack.layers[1].opacity_percent = 42;

    LayerSnapshot snapshot = {0};
    if (!layer_snapshot_capture(&snapshot, &stack)) {
        fprintf(stderr, "history snapshot reset capture failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    layer_snapshot_reset(&snapshot);
    if (!snapshot_is_reset(&snapshot)) {
        fprintf(stderr, "history snapshot reset should clear captured state\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (snapshot.visibility[1] != 0 || snapshot.locked[1] != 0 || snapshot.opacity_percent[1] != 0 ||
        snapshot.names[1][0] != '\0') {
        fprintf(stderr, "history snapshot reset should scrub metadata arrays unlike snapshot free\n");
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_commit_change_helper(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "history commit helper init failed\n");
        return 0;
    }

    LayerHistory history = {0};
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF0A0B0C);
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF0D0E0F);

    if (!layer_history_step_undo(&history, &stack) || history.redo_count != 1) {
        fprintf(stderr, "history commit helper undo setup failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    LayerSnapshot noop_snapshot = {0};
    if (!layer_snapshot_capture(&noop_snapshot, &stack)) {
        fprintf(stderr, "history commit helper noop capture failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_history_commit_change(&history, &noop_snapshot, &stack, 1)) {
        fprintf(stderr, "history commit helper should skip unchanged state\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_is_reset(&noop_snapshot)) {
        fprintf(stderr, "history commit helper should reset noop snapshot ownership\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (history.redo_count != 1) {
        fprintf(stderr, "history commit helper should keep redo for unchanged state\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    LayerSnapshot changed_snapshot = {0};
    if (!layer_snapshot_capture(&changed_snapshot, &stack)) {
        fprintf(stderr, "history commit helper changed capture failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF101112);
    if (!layer_history_commit_change(&history, &changed_snapshot, &stack, 1)) {
        fprintf(stderr, "history commit helper should record changed state\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (history.redo_count != 0) {
        fprintf(stderr, "history commit helper should clear redo for changed state\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_history_step_undo(&history, &stack) ||
        !expect_pixel_eq("history_commit_helper_undo", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF0A0B0C)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_history_step_redo(&history, &stack) ||
        !expect_pixel_eq("history_commit_helper_redo", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF101112)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    LayerSnapshot failed_snapshot = {0};
    if (!layer_snapshot_capture(&failed_snapshot, &stack)) {
        fprintf(stderr, "history commit helper failed capture failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_history_commit_change(&history, &failed_snapshot, &stack, 0)) {
        fprintf(stderr, "history commit helper should skip failed operations\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_is_reset(&failed_snapshot)) {
        fprintf(stderr, "history commit helper should reset failed snapshot ownership\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (history.undo_count != 2 || history.redo_count != 0) {
        fprintf(stderr, "history commit helper should leave history unchanged after failed operation\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_commit_change_resets_without_history_or_layers(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 3, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "history commit null-path init failed\n");
        return 0;
    }

    LayerSnapshot null_history_snapshot = {0};
    if (!layer_snapshot_capture(&null_history_snapshot, &stack)) {
        fprintf(stderr, "history commit null-path capture failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_history_commit_change(NULL, &null_history_snapshot, &stack, 1)) {
        fprintf(stderr, "history commit should fail without history state\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_is_reset(&null_history_snapshot)) {
        fprintf(stderr, "history commit should fully reset snapshot when history is null\n");
        layer_stack_free(&stack);
        return 0;
    }

    LayerSnapshot null_layers_snapshot = {0};
    if (!layer_snapshot_capture(&null_layers_snapshot, &stack)) {
        fprintf(stderr, "history commit null-layers capture failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_history_commit_change(&(LayerHistory){0}, &null_layers_snapshot, NULL, 1)) {
        fprintf(stderr, "history commit should fail without layer state\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_is_reset(&null_layers_snapshot)) {
        fprintf(stderr, "history commit should fully reset snapshot when layers are null\n");
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_visibility_commit_change(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "history visibility commit init failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Top", 0x00000000) != 1) {
        fprintf(stderr, "history visibility commit add layer failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    LayerHistory history = {0};
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[1].canvas, 0, 0, 0xFF102030);
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[1].canvas, 0, 0, 0xFF405060);

    if (!layer_history_step_undo(&history, &stack) || history.redo_count != 1) {
        fprintf(stderr, "history visibility commit undo setup failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    stack.solo_index = 1;
    LayerSnapshot show_all_snapshot = {0};
    if (!layer_snapshot_capture(&show_all_snapshot, &stack)) {
        fprintf(stderr, "history visibility commit show-all capture failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show_all(&stack) ||
        !layer_history_commit_change(&history, &show_all_snapshot, &stack, 1)) {
        fprintf(stderr, "history visibility commit should record show-all change\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (history.redo_count != 0 || stack.solo_index != -1) {
        fprintf(stderr, "history visibility commit should clear redo and solo on show-all\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_history_step_undo(&history, &stack) || stack.solo_index != 1) {
        fprintf(stderr, "history visibility commit undo should restore solo state\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_history_step_redo(&history, &stack) || stack.solo_index != -1) {
        fprintf(stderr, "history visibility commit redo should restore show-all state\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "history visibility commit hide top layer failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    LayerSnapshot show_snapshot = {0};
    if (!layer_snapshot_capture(&show_snapshot, &stack)) {
        fprintf(stderr, "history visibility commit show capture failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1) ||
        !layer_history_commit_change(&history, &show_snapshot, &stack, 1)) {
        fprintf(stderr, "history visibility commit should record reveal change\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_history_step_undo(&history, &stack) || stack.layers[1].visible != 0) {
        fprintf(stderr, "history visibility commit undo should re-hide revealed layer\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    LayerSnapshot noop_show_snapshot = {0};
    if (!layer_snapshot_capture(&noop_show_snapshot, &stack)) {
        fprintf(stderr, "history visibility commit noop show capture failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_history_commit_change(&history, &noop_show_snapshot, &stack, 0)) {
        fprintf(stderr, "history visibility commit should ignore failed reveal\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (history.redo_count != 1) {
        fprintf(stderr, "history visibility commit should preserve redo after failed reveal\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_low_level_undo_redo_rolls_back_failed_apply(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "history low-level rollback init failed\n");
        return 0;
    }

    LayerSnapshot undo_stack[HISTORY_CAPACITY] = {0};
    LayerSnapshot redo_stack[HISTORY_CAPACITY] = {0};
    int undo_count = 0;
    int redo_count = 0;

    if (!layer_snapshot_capture(&undo_stack[undo_count++], &stack)) {
        fprintf(stderr, "history low-level rollback initial capture failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    undo_stack[0].width++;
    if (layer_history_undo(&stack, undo_stack, &undo_count, redo_stack, &redo_count)) {
        fprintf(stderr, "history low-level undo should fail when snapshot apply fails\n");
        layer_history_clear(undo_stack, &undo_count);
        layer_history_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (undo_count != 1 || redo_count != 0) {
        fprintf(stderr, "history low-level undo should roll back stack mutations after apply failure\n");
        layer_history_clear(undo_stack, &undo_count);
        layer_history_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_snapshot_capture(&redo_stack[redo_count++], &stack)) {
        fprintf(stderr, "history low-level rollback redo capture failed\n");
        layer_history_clear(undo_stack, &undo_count);
        layer_stack_free(&stack);
        return 0;
    }

    redo_stack[0].width++;
    if (layer_history_redo(&stack, undo_stack, &undo_count, redo_stack, &redo_count)) {
        fprintf(stderr, "history low-level redo should fail when snapshot apply fails\n");
        layer_history_clear(undo_stack, &undo_count);
        layer_history_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (undo_count != 1 || redo_count != 1) {
        fprintf(stderr, "history low-level redo should roll back stack mutations after apply failure\n");
        layer_history_clear(undo_stack, &undo_count);
        layer_history_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_clear(undo_stack, &undo_count);
    layer_history_clear(redo_stack, &redo_count);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_step_undo_redo_rolls_back_failed_apply(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "history step rollback init failed\n");
        return 0;
    }

    LayerHistory history = {0};
    layer_history_record(&history, &stack);
    canvas_set_pixel(&stack.layers[0].canvas, 0, 0, 0xFF010203);
    layer_history_record(&history, &stack);

    history.undo[history.undo_count - 1].width++;
    if (layer_history_step_undo(&history, &stack)) {
        fprintf(stderr, "history step undo should fail when stored snapshot cannot apply\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (history.undo_count != 2 || history.redo_count != 0) {
        fprintf(stderr, "history step undo should keep history counts unchanged after apply failure\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("history_step_failed_undo_keeps_canvas", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_snapshot_capture(&history.redo[history.redo_count++], &stack)) {
        fprintf(stderr, "history step redo rollback capture failed\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    history.redo[history.redo_count - 1].width++;
    if (layer_history_step_redo(&history, &stack)) {
        fprintf(stderr, "history step redo should fail when stored snapshot cannot apply\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (history.undo_count != 2 || history.redo_count != 1) {
        fprintf(stderr, "history step redo should keep history counts unchanged after apply failure\n");
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("history_step_failed_redo_keeps_canvas", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203)) {
        layer_history_reset(&history);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_low_level_undo_redo_guard_paths(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "history low-level guard init failed\n");
        return 0;
    }

    LayerSnapshot undo_stack[HISTORY_CAPACITY] = {0};
    LayerSnapshot redo_stack[HISTORY_CAPACITY] = {0};
    int undo_count = 0;
    int redo_count = 0;

    if (layer_history_undo(NULL, undo_stack, &undo_count, redo_stack, &redo_count) ||
        layer_history_undo(&stack, NULL, &undo_count, redo_stack, &redo_count) ||
        layer_history_undo(&stack, undo_stack, NULL, redo_stack, &redo_count) ||
        layer_history_undo(&stack, undo_stack, &undo_count, NULL, &redo_count) ||
        layer_history_undo(&stack, undo_stack, &undo_count, redo_stack, NULL) ||
        layer_history_undo(&stack, undo_stack, &undo_count, redo_stack, &redo_count)) {
        fprintf(stderr, "history low-level undo should fail cleanly on null or empty inputs\n");
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_history_redo(NULL, undo_stack, &undo_count, redo_stack, &redo_count) ||
        layer_history_redo(&stack, NULL, &undo_count, redo_stack, &redo_count) ||
        layer_history_redo(&stack, undo_stack, NULL, redo_stack, &redo_count) ||
        layer_history_redo(&stack, undo_stack, &undo_count, NULL, &redo_count) ||
        layer_history_redo(&stack, undo_stack, &undo_count, redo_stack, NULL) ||
        layer_history_redo(&stack, undo_stack, &undo_count, redo_stack, &redo_count)) {
        fprintf(stderr, "history low-level redo should fail cleanly on null or empty inputs\n");
        layer_stack_free(&stack);
        return 0;
    }

    if (!expect_history_counts("history_low_level_guard_paths", undo_count, redo_count, 0, 0)) {
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_step_undo_redo_guard_paths(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "history step guard init failed\n");
        return 0;
    }

    LayerHistory history = {0};
    if (layer_history_step_undo(NULL, &stack) ||
        layer_history_step_undo(&history, NULL) ||
        layer_history_step_undo(&history, &stack) ||
        layer_history_step_redo(NULL, &stack) ||
        layer_history_step_redo(&history, NULL) ||
        layer_history_step_redo(&history, &stack)) {
        fprintf(stderr, "history step undo/redo should fail cleanly on null or empty inputs\n");
        layer_stack_free(&stack);
        return 0;
    }

    if (!expect_wrapper_history_counts("history_step_guard_paths", &history, 0, 0)) {
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_push_record_guard_paths(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "history push/record guard init failed\n");
        return 0;
    }

    LayerSnapshot undo_stack[HISTORY_CAPACITY] = {0};
    LayerSnapshot redo_stack[HISTORY_CAPACITY] = {0};
    int undo_count = 0;
    int redo_count = 0;

    if (!layer_snapshot_capture(&redo_stack[redo_count++], &stack)) {
        fprintf(stderr, "history push/record guard redo capture failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_push(NULL, undo_stack, &undo_count, redo_stack, &redo_count);
    layer_history_push(&stack, NULL, &undo_count, redo_stack, &redo_count);
    layer_history_push(&stack, undo_stack, NULL, redo_stack, &redo_count);

    if (!expect_history_counts("history_push_hard_guard_paths", undo_count, redo_count, 0, 1)) {
        layer_history_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_push(&stack, undo_stack, &undo_count, NULL, &redo_count);
    if (!expect_history_counts("history_push_without_redo_stack", undo_count, redo_count, 1, 1)) {
        layer_history_clear(undo_stack, &undo_count);
        layer_history_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_push(&stack, undo_stack, &undo_count, redo_stack, NULL);
    if (!expect_history_counts("history_push_duplicate_without_redo_count", undo_count, redo_count, 1, 1)) {
        layer_history_clear(undo_stack, &undo_count);
        layer_history_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    LayerHistory history = {0};
    if (!layer_snapshot_capture(&history.redo[history.redo_count++], &stack)) {
        fprintf(stderr, "history push/record wrapper redo capture failed\n");
        layer_history_clear(undo_stack, &undo_count);
        layer_history_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_record(NULL, &stack);
    if (!expect_wrapper_history_counts("history_record_null_history", &history, 0, 1)) {
        layer_history_reset(&history);
        layer_history_clear(undo_stack, &undo_count);
        layer_history_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_record(&history, NULL);
    if (!expect_wrapper_history_counts("history_record_null_layers", &history, 0, 1)) {
        layer_history_reset(&history);
        layer_history_clear(undo_stack, &undo_count);
        layer_history_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_reset(&history);
    layer_history_clear(undo_stack, &undo_count);
    layer_history_clear(redo_stack, &redo_count);
    layer_stack_free(&stack);
    return 1;
}

static int test_layer_history_clear_guard_paths(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "history clear guard init failed\n");
        return 0;
    }

    LayerSnapshot snapshots[HISTORY_CAPACITY] = {0};
    int count = 0;

    if (!layer_snapshot_capture(&snapshots[count++], &stack) ||
        !layer_snapshot_capture(&snapshots[count++], &stack)) {
        fprintf(stderr, "history clear guard capture failed\n");
        layer_history_clear(snapshots, &count);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_clear(NULL, &count);
    if (!expect_history_counts("history_clear_null_stack", count, 0, 2, 0)) {
        layer_history_clear(snapshots, &count);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_clear(snapshots, NULL);
    if (!expect_history_counts("history_clear_null_count", count, 0, 2, 0)) {
        layer_history_clear(snapshots, &count);
        layer_stack_free(&stack);
        return 0;
    }

    layer_history_clear(snapshots, &count);
    if (!expect_history_counts("history_clear_populated_stack", count, 0, 0, 0)) {
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_is_reset(&snapshots[0]) || !snapshot_is_reset(&snapshots[1])) {
        fprintf(stderr, "history clear should reset populated snapshots in place\n");
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_layers_basic(void) {
    LayerStack stack;
    if (!layer_stack_init(&stack, 16, 16, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "layer_stack_add failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    Canvas composite;
    if (!canvas_init(&composite, 16, 16)) {
        fprintf(stderr, "composite init failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    Layer *active = layer_stack_active(&stack);
    canvas_draw_circle(&active->canvas, 8, 8, 3, 0x80FF0000);
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if ((canvas_get_pixel(&composite, 8, 8) & 0x00FFFFFF) == 0x00FFFFFF) {
        fprintf(stderr, "composite did not include top layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_stack_toggle_visibility(&stack, stack.active_layer)) {
        fprintf(stderr, "toggle visibility failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("hidden_top_layer", canvas_get_pixel(&composite, 8, 8), 0xFFFFFFFF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 1)) {
        fprintf(stderr, "solo hidden layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show_all(&stack)) {
        fprintf(stderr, "show all failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1 || !stack.layers[0].visible || !stack.layers[1].visible) {
        fprintf(stderr, "show all bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if ((canvas_get_pixel(&composite, 8, 8) & 0x00FFFFFF) == 0x00FFFFFF) {
        fprintf(stderr, "show all did not restore visible composite\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_show_all(&stack)) {
        fprintf(stderr, "show all should no-op when all layers are already visible and solo is clear\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "rehide top layer after show all failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1) || !stack.layers[1].visible) {
        fprintf(stderr, "show active layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_show(&stack, 1)) {
        fprintf(stderr, "show active layer should no-op when already visible\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "rehide top layer after show active failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1)) {
        fprintf(stderr, "restore top layer for hide-and-advance failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 1;
    if (!layer_stack_hide_and_advance(&stack, 1)) {
        fprintf(stderr, "hide and advance failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[1].visible || stack.active_layer != 0) {
        fprintf(stderr, "hide and advance bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1)) {
        fprintf(stderr, "restore top layer after hide and advance failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 1) || !layer_stack_hide_and_advance(&stack, 1)) {
        fprintf(stderr, "hide and advance from solo failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1 || stack.active_layer != 0) {
        fprintf(stderr, "hide and advance should clear solo and focus visible layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1)) {
        fprintf(stderr, "restore top layer after solo hide and advance failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "rehide top layer after hide and advance tests failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_stack_toggle_visibility(&stack, 0)) {
        fprintf(stderr, "background should not hide when last visible\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 1;

    if (layer_stack_cycle(&stack, -1) != 0 || layer_stack_cycle(&stack, 1) != 1) {
        fprintf(stderr, "layer cycling failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_add(&stack, "Third", 0x00000000) != 2 || layer_stack_add(&stack, "Fourth", 0x00000000) != 3) {
        fprintf(stderr, "setup extended layer cycling failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 3;
    if (layer_stack_cycle(&stack, 1) != 0 || layer_stack_cycle(&stack, -1) != 3) {
        fprintf(stderr, "layer cycling wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 3) || !layer_stack_delete(&stack, 2)) {
        fprintf(stderr, "extended layer cycling cleanup failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_visible(&stack, 1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "visible cycling should wrap to only visible layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_visible(&stack, -1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "visible cycling should stay on only visible layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top(&stack) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "select top layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_bottom(&stack) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_visible(&stack) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "select top visible layer should skip hidden top layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_bottom_visible(&stack) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom visible layer should stay on only visible layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1)) {
        fprintf(stderr, "restore top layer after visible cycling failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_add(&stack, "Third Visible", 0x00000000) != 2 || layer_stack_add(&stack, "Fourth Visible", 0x00000000) != 3) {
        fprintf(stderr, "setup visible cycling skip test failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1) || !layer_stack_toggle_visibility(&stack, 2)) {
        fprintf(stderr, "hide intermediate layers for visible cycling failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 1;
    if (layer_stack_cycle_visible(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "visible cycling forward should skip hidden layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_visible(&stack, -1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "visible cycling backward should wrap across hidden layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_visible(&stack) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select top visible layer should choose highest visible layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_bottom_visible(&stack) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom visible layer should choose lowest visible layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_visible_rank(&stack, 0) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "select first visible layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_visible_rank(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select second visible layer should skip hidden layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_visible_rank(&stack, 2) != -1) {
        fprintf(stderr, "visible rank selection should fail beyond visible range\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_visible_rank(&stack, 0) != 0) {
        fprintf(stderr, "visible rank for background failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_visible_rank(&stack, 1) != -1) {
        fprintf(stderr, "hidden layer should not have a visible rank\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_visible_rank(&stack, 3) != 1) {
        fprintf(stderr, "visible rank should count only visible layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move_to_visible_rank(&stack, 3, 0) || stack.active_layer != 0) {
        fprintf(stderr, "move to first visible rank failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[0].name, "Fourth Visible") != 0 || layer_stack_visible_rank(&stack, 0) != 0 || layer_stack_visible_rank(&stack, 1) != 1) {
        fprintf(stderr, "move to first visible rank bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move_to_visible_rank(&stack, 0, 1) || stack.active_layer != 1) {
        fprintf(stderr, "move to second visible rank failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[1].name, "Fourth Visible") != 0 || layer_stack_visible_rank(&stack, 1) != 1) {
        fprintf(stderr, "move to second visible rank bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_move_to_visible_rank(&stack, 2, 0)) {
        fprintf(stderr, "hidden layer should not move by visible rank\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_move_to_visible_rank(&stack, 1, 2)) {
        fprintf(stderr, "moving beyond the visible rank range should fail\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move_to_visible_rank(&stack, 1, 0) || strcmp(stack.layers[0].name, "Fourth Visible") != 0) {
        fprintf(stderr, "move visible layer to bottom visible slot failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move_to_visible_rank(&stack, 0, 1) || strcmp(stack.layers[1].name, "Fourth Visible") != 0) {
        fprintf(stderr, "move visible layer to top visible slot failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move_to(&stack, 1, 3) || strcmp(stack.layers[3].name, "Fourth Visible") != 0) {
        fprintf(stderr, "restore absolute layer order after visible-rank move tests failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 3) || !layer_stack_delete(&stack, 2)) {
        fprintf(stderr, "visible cycling cleanup failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1)) {
        fprintf(stderr, "restore top layer after visible cycling cleanup failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_insert(&stack, 1, "Inserted", 0x00000000) != 1) {
        fprintf(stderr, "layer insert failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 3 || stack.active_layer != 1) {
        fprintf(stderr, "layer insert bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[1].name, "Inserted") != 0 || strcmp(stack.layers[2].name, "Top") != 0) {
        fprintf(stderr, "layer insert order failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 2)) {
        fprintf(stderr, "solo inserted stack top failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_insert(&stack, 2, "Solo Neighbor", 0x00000000) != 2) {
        fprintf(stderr, "layer insert with solo failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 3) {
        fprintf(stderr, "solo index did not shift with insert\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 3) || stack.solo_index != -1) {
        fprintf(stderr, "toggle solo off after insert failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 2) || !layer_stack_delete(&stack, 1)) {
        fprintf(stderr, "cleanup inserted layers failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 2 || strcmp(stack.layers[1].name, "Top") != 0) {
        fprintf(stderr, "cleanup inserted layer order failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_insert(&stack, 1, "Inserted Below", 0x00000000) != 1) {
        fprintf(stderr, "layer insert below failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 3 || stack.active_layer != 1) {
        fprintf(stderr, "layer insert below bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[0].name, "Background") != 0 || strcmp(stack.layers[1].name, "Inserted Below") != 0 || strcmp(stack.layers[2].name, "Top") != 0) {
        fprintf(stderr, "layer insert below order failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 1) || stack.layer_count != 2) {
        fprintf(stderr, "layer insert below cleanup failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "re-show top layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 1)) {
        fprintf(stderr, "toggle solo failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("solo_active_layer", canvas_get_pixel(&composite, 8, 8), 0xFFBF7F7F)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "hide solo layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("solo_hidden_active_layer", canvas_get_pixel(&composite, 8, 8), 0xFFBF7F7F)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "re-show solo layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 1) || stack.solo_index != -1) {
        fprintf(stderr, "toggle solo off failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock(&stack, 1) || !stack.layers[1].locked) {
        fprintf(stderr, "toggle lock on failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_clear_layer(&stack, 1, 0xFFABCDEF)) {
        fprintf(stderr, "clear should fail on locked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_delete(&stack, 1)) {
        fprintf(stderr, "delete should fail on locked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_merge_down(&stack, 1)) {
        fprintf(stderr, "merge should fail on locked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_into(&stack, 1, 0xFFFFFFFF)) {
        fprintf(stderr, "stamp should fail on locked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_flatten(&stack, 0xFFFFFFFF)) {
        fprintf(stderr, "flatten should fail when any layer is locked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock(&stack, 1) || stack.layers[1].locked) {
        fprintf(stderr, "toggle lock off failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0xFF0000FF);
    canvas_clear(&stack.layers[1].canvas, 0x8000FF00);
    if (!layer_stack_set_opacity(&stack, 1, 50)) {
        fprintf(stderr, "set opacity failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_set_opacity(&stack, 1, 100) || stack.layers[1].opacity_percent != 100) {
        fprintf(stderr, "reset opacity failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_set_opacity(&stack, 1, 50)) {
        fprintf(stderr, "restore opacity after reset failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_adjust_opacity(&stack, 1, -1) || stack.layers[1].opacity_percent != 49) {
        fprintf(stderr, "fine opacity decrease failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_adjust_opacity(&stack, 1, 2) || stack.layers[1].opacity_percent != 51) {
        fprintf(stderr, "fine opacity increase failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_adjust_opacity(&stack, 1, 1000) || stack.layers[1].opacity_percent != 100) {
        fprintf(stderr, "opacity clamp to max failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_adjust_opacity(&stack, 1, 1)) {
        fprintf(stderr, "opacity adjustment should no-op at max\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_adjust_opacity(&stack, 1, -1000) || stack.layers[1].opacity_percent != 0) {
        fprintf(stderr, "opacity clamp to min failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_adjust_opacity(&stack, 1, -1)) {
        fprintf(stderr, "opacity adjustment should no-op at min\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_set_opacity(&stack, 1, 50)) {
        fprintf(stderr, "restore opacity after fine adjustments failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_merge_down(&stack, 1)) {
        fprintf(stderr, "merge down failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 1 || stack.active_layer != 0) {
        fprintf(stderr, "merge down bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("merge_down_blend", canvas_get_pixel(&composite, 0, 0), 0xFF0040BF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_add(&stack, "Upper Merge", 0x00000000) != 1) {
        fprintf(stderr, "add upper merge layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0xFF0000FF);
    canvas_clear(&stack.layers[1].canvas, 0x8000FF00);
    if (!layer_stack_set_opacity(&stack, 1, 50)) {
        fprintf(stderr, "set merge-up opacity failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 0;
    if (!layer_stack_merge_up(&stack, 0)) {
        fprintf(stderr, "merge up failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 1 || stack.active_layer != 0) {
        fprintf(stderr, "merge up bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("merge_up_blend", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF0040BF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_stack_duplicate(&stack, 0, "Background Copy") != 1) {
        fprintf(stderr, "duplicate layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 2 || stack.active_layer != 1) {
        fprintf(stderr, "duplicate bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 1)) {
        fprintf(stderr, "solo duplicated layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock(&stack, 1)) {
        fprintf(stderr, "lock duplicated layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[1].opacity_percent != 100) {
        fprintf(stderr, "duplicate opacity reset unexpectedly\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[1].locked) {
        fprintf(stderr, "lock flag did not persist on duplicated layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("duplicate_copy_pixel", canvas_get_pixel(&stack.layers[1].canvas, 0, 0), 0xFF0040BF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_duplicate(&stack, 0, NULL) != 1) {
        fprintf(stderr, "auto-named duplicate layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[1].name, "Upper Merge Copy") != 0) {
        fprintf(stderr, "first auto duplicate name failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_duplicate(&stack, 0, NULL) != 1) {
        fprintf(stderr, "second auto-named duplicate layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[1].name, "Upper Merge Copy 2") != 0) {
        fprintf(stderr, "second auto duplicate name should be unique\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 1) || !layer_stack_delete(&stack, 1)) {
        fprintf(stderr, "cleanup auto-named duplicates failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move(&stack, 1, -1) || stack.active_layer != 0) {
        fprintf(stderr, "move layer down failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 0) {
        fprintf(stderr, "solo index did not move with layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[0].name, "Background Copy") != 0 || strcmp(stack.layers[1].name, "Upper Merge") != 0) {
        fprintf(stderr, "move layer order failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move(&stack, 0, 1) || stack.active_layer != 1) {
        fprintf(stderr, "move layer up failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[1].locked) {
        fprintf(stderr, "lock flag did not move with layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_move(&stack, 1, 1)) {
        fprintf(stderr, "should not move top layer beyond bounds\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_add(&stack, "Move Middle", 0x00000000) != 2 || layer_stack_add(&stack, "Move Top", 0x00000000) != 3) {
        fprintf(stderr, "setup move-to layers failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move_to(&stack, 1, 3) || stack.active_layer != 3) {
        fprintf(stderr, "move layer to top failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 3) {
        fprintf(stderr, "solo index did not move to top with layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[3].name, "Background Copy") != 0) {
        fprintf(stderr, "move-to-top order failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move_to(&stack, 3, 0) || stack.active_layer != 0) {
        fprintf(stderr, "move layer to bottom failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 0) {
        fprintf(stderr, "solo index did not move to bottom with layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[0].name, "Background Copy") != 0) {
        fprintf(stderr, "move-to-bottom order failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_move_to(&stack, 0, 0)) {
        fprintf(stderr, "move-to should fail when already at target\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_move_to(&stack, 0, 1) || stack.active_layer != 1) {
        fprintf(stderr, "move layer back to original slot failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 1 || strcmp(stack.layers[1].name, "Background Copy") != 0) {
        fprintf(stderr, "move-to restore bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 3) || !layer_stack_delete(&stack, 2)) {
        fprintf(stderr, "move-to cleanup failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_set_pixel(&stack.layers[1].canvas, 0, 0, 0xFFFF00FF);
    if (!expect_pixel_eq("duplicate_independent", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF0040BF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_lock(&stack, 1) || stack.layers[1].locked) {
        fprintf(stderr, "unlock duplicated layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 1)) {
        fprintf(stderr, "delete duplicated layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1) {
        fprintf(stderr, "solo index should clear after deleting solo layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 1 || stack.active_layer != 0) {
        fprintf(stderr, "delete bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_delete(&stack, 0)) {
        fprintf(stderr, "should not delete final layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_stack_add(&stack, "Flatten Top", 0x00000000) != 1) {
        fprintf(stderr, "add flatten layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0xFF0000FF);
    canvas_clear(&stack.layers[1].canvas, 0x8000FF00);
    if (!layer_stack_set_opacity(&stack, 1, 50)) {
        fprintf(stderr, "set flatten opacity failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_flatten(&stack, 0xFFFFFFFF)) {
        fprintf(stderr, "flatten failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 1 || stack.active_layer != 0) {
        fprintf(stderr, "flatten bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("flatten_pixel", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF0040BF)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_stack_add(&stack, "Stamp Target", 0x00000000) != 1) {
        fprintf(stderr, "add stamp layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0xFF123456);
    canvas_clear(&stack.layers[1].canvas, 0x8000FF00);
    if (!layer_stack_set_opacity(&stack, 1, 50)) {
        fprintf(stderr, "set stamp opacity failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_stamp_visible_into(&stack, 1, 0xFFFFFFFF)) {
        fprintf(stderr, "stamp visible failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("stamp_visible_pixel", canvas_get_pixel(&stack.layers[1].canvas, 0, 0), 0xFF0D6740) ||
        !expect_pixel_eq("stamp_preserve_source", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF123456)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_new(&stack, "Visible Stamp", 0xFFFFFFFF) != 2) {
        fprintf(stderr, "stamp visible new layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layer_count != 3 || stack.active_layer != 2) {
        fprintf(stderr, "stamp visible new bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(stack.layers[2].name, "Visible Stamp") != 0) {
        fprintf(stderr, "stamp visible new name failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!expect_pixel_eq("stamp_visible_new_pixel", canvas_get_pixel(&stack.layers[2].canvas, 0, 0), 0xFF0D6740)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    for (int expected_idx = 3; expected_idx < MAX_LAYERS; expected_idx++) {
        if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != expected_idx) {
            fprintf(stderr, "stamp visible new layer %d failed\n", expected_idx);
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        if (expected_idx == 3 && strcmp(stack.layers[expected_idx].name, "Overflow") != 0) {
            fprintf(stderr, "first overflow stamp name failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        if (expected_idx == 4 && strcmp(stack.layers[expected_idx].name, "Overflow 2") != 0) {
            fprintf(stderr, "second overflow stamp name should be unique\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != -1) {
        fprintf(stderr, "stamp visible new should respect max layers\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_free(&composite);
    layer_stack_free(&stack);
    return 1;
}

int main(void) {
    if (!test_layer_snapshot_restore()) {
        return 1;
    }
    if (!test_layer_snapshot_expand_restore()) {
        return 1;
    }
    if (!test_layer_snapshot_capture_apply_guard_paths()) {
        return 1;
    }
    if (!test_layer_snapshot_capture_reuses_existing_snapshot()) {
        return 1;
    }
    if (!test_layer_snapshot_free_preserves_metadata_arrays()) {
        return 1;
    }
    if (!test_layer_history_stack()) {
        return 1;
    }
    if (!test_layer_history_layer_count_roundtrip()) {
        return 1;
    }
    if (!test_layer_history_struct_api()) {
        return 1;
    }
    if (!test_layer_history_skips_duplicate_snapshots()) {
        return 1;
    }
    if (!test_layer_history_manual_snapshot_recording()) {
        return 1;
    }
    if (!test_layer_history_record_snapshot_discards_duplicate()) {
        return 1;
    }
    if (!test_layer_history_record_snapshot_null_history_resets()) {
        return 1;
    }
    if (!test_layer_history_record_snapshot_current_state_clears_redo()) {
        return 1;
    }
    if (!test_layer_history_discarded_snapshot_keeps_redo()) {
        return 1;
    }
    if (!test_layer_snapshot_matches_stack_noop_edits()) {
        return 1;
    }
    if (!test_layer_snapshot_matches_stack_guard_paths()) {
        return 1;
    }
    if (!test_layer_history_skip_noop_snapshot_commit()) {
        return 1;
    }
    if (!test_layer_snapshot_reset_clears_allocated_state()) {
        return 1;
    }
    if (!test_layer_history_commit_change_helper()) {
        return 1;
    }
    if (!test_layer_history_commit_change_resets_without_history_or_layers()) {
        return 1;
    }
    if (!test_layer_history_visibility_commit_change()) {
        return 1;
    }
    if (!test_layer_history_low_level_undo_redo_rolls_back_failed_apply()) {
        return 1;
    }
    if (!test_layer_history_step_undo_redo_rolls_back_failed_apply()) {
        return 1;
    }
    if (!test_layer_history_low_level_undo_redo_guard_paths()) {
        return 1;
    }
    if (!test_layer_history_step_undo_redo_guard_paths()) {
        return 1;
    }
    if (!test_layer_history_push_record_guard_paths()) {
        return 1;
    }
    if (!test_layer_history_clear_guard_paths()) {
        return 1;
    }

    Canvas c;
    if (!canvas_init(&c, 64, 64)) {
        fprintf(stderr, "canvas_init failed\n");
        return 1;
    }
    canvas_clear(&c, 0xFFFFFFFF);
    canvas_draw_circle(&c, 32, 32, 8, 0xFF000000);
    canvas_draw_line(&c, 0, 0, 63, 63, 2, 0xFF00FF00);
    canvas_draw_rect_outline(&c, 5, 5, 20, 20, 1, 0xFF0000FF);
    canvas_draw_rect_filled(&c, 24, 8, 30, 14, 0xFF8844FF);
    canvas_draw_ellipse_outline(&c, 32, 32, 12, 6, 1, 0xFFFFFF00);
    canvas_draw_ellipse_filled(&c, 48, 18, 6, 4, 0xFF00FFFF);
    if (!canvas_flood_fill(&c, 1, 1, 0xFFFF0000)) {
        fprintf(stderr, "canvas_flood_fill failed\n");
        canvas_free(&c);
        return 1;
    }
    if (!expect_pixel_eq("filled_rect_center", canvas_get_pixel(&c, 27, 11), 0xFF8844FF)) {
        canvas_free(&c);
        return 1;
    }
    if (!expect_pixel_eq("filled_ellipse_center", canvas_get_pixel(&c, 48, 18), 0xFF00FFFF)) {
        canvas_free(&c);
        return 1;
    }

    // basic checksum to ensure drawing occurred
    unsigned long long sum = 0;
    for (int i = 0; i < c.width * c.height; i++) {
        sum += c.pixels[i];
    }
    canvas_free(&c);

    if (sum == 0) {
        fprintf(stderr, "unexpected checksum\n");
        return 1;
    }

    if (!canvas_init(&c, 2, 2)) {
        fprintf(stderr, "canvas_init blend test failed\n");
        return 1;
    }
    canvas_clear(&c, 0xFFFFFFFF);
    canvas_set_pixel(&c, 0, 0, 0x80000000);
    uint32_t blended = canvas_get_pixel(&c, 0, 0);
    if (blended != 0xFF7F7F7F && blended != 0xFF808080) {
        fprintf(stderr, "unexpected blended pixel: 0x%08X\n", blended);
        canvas_free(&c);
        return 1;
    }
    canvas_set_pixel(&c, 0, 0, 0x80000000);
    if (!expect_pixel_eq("double_blend", canvas_get_pixel(&c, 0, 0), 0xFF3F3F3F)) {
        canvas_free(&c);
        return 1;
    }
    canvas_set_pixel(&c, 0, 1, 0x00FF00FF);
    if (!expect_pixel_eq("transparent_noop", canvas_get_pixel(&c, 0, 1), 0xFFFFFFFF)) {
        canvas_free(&c);
        return 1;
    }
    canvas_set_pixel(&c, 1, 1, 0xFFFF0000);
    if (!expect_pixel_eq("opaque_write", canvas_get_pixel(&c, 1, 1), 0xFFFF0000)) {
        canvas_free(&c);
        return 1;
    }
    canvas_set_pixel_raw(&c, 1, 0, 0x00000000);
    if (!expect_pixel_eq("raw_clear", canvas_get_pixel(&c, 1, 0), 0x00000000)) {
        canvas_free(&c);
        return 1;
    }
    canvas_free(&c);

    Canvas transparent;
    if (!canvas_init(&transparent, 1, 1)) {
        fprintf(stderr, "transparent canvas init failed\n");
        return 1;
    }
    canvas_set_pixel(&transparent, 0, 0, 0x80FF0000);
    if (!expect_pixel_eq("blend_into_transparent", canvas_get_pixel(&transparent, 0, 0), 0x80800000)) {
        canvas_free(&transparent);
        return 1;
    }
    canvas_free(&transparent);

    Canvas transform;
    if (!canvas_init(&transform, 3, 2)) {
        fprintf(stderr, "transform canvas init failed\n");
        return 1;
    }
    canvas_clear(&transform, 0xFF000000);
    canvas_set_pixel(&transform, 0, 0, 0xFF010203);
    canvas_set_pixel(&transform, 1, 0, 0xFF111213);
    canvas_set_pixel(&transform, 2, 0, 0xFF212223);
    canvas_set_pixel(&transform, 0, 1, 0xFF313233);
    canvas_set_pixel(&transform, 1, 1, 0xFF414243);
    canvas_set_pixel(&transform, 2, 1, 0xFF515253);

    canvas_flip_horizontal(&transform);
    if (!expect_pixel_eq("flip_h_tl", canvas_get_pixel(&transform, 0, 0), 0xFF212223) ||
        !expect_pixel_eq("flip_h_tr", canvas_get_pixel(&transform, 2, 0), 0xFF010203) ||
        !expect_pixel_eq("flip_h_bl", canvas_get_pixel(&transform, 0, 1), 0xFF515253)) {
        canvas_free(&transform);
        return 1;
    }

    canvas_flip_vertical(&transform);
    if (!expect_pixel_eq("flip_v_tl", canvas_get_pixel(&transform, 0, 0), 0xFF515253) ||
        !expect_pixel_eq("flip_v_br", canvas_get_pixel(&transform, 2, 1), 0xFF010203)) {
        canvas_free(&transform);
        return 1;
    }

    canvas_rotate_180(&transform);
    if (!expect_pixel_eq("rotate_180_tl", canvas_get_pixel(&transform, 0, 0), 0xFF010203) ||
        !expect_pixel_eq("rotate_180_br", canvas_get_pixel(&transform, 2, 1), 0xFF515253)) {
        canvas_free(&transform);
        return 1;
    }

    canvas_rotate_180(&transform);
    canvas_invert_rgb(&transform);
    if (!expect_pixel_eq("invert_tl", canvas_get_pixel(&transform, 0, 0), 0xFFAEADAC)) {
        canvas_free(&transform);
        return 1;
    }
    if (!expect_pixel_eq("invert_br", canvas_get_pixel(&transform, 2, 1), 0xFFFEFDFC)) {
        canvas_free(&transform);
        return 1;
    }
    canvas_free(&transform);

    Canvas translated;
    if (!canvas_init(&translated, 4, 3)) {
        fprintf(stderr, "translate canvas init failed\n");
        return 1;
    }
    canvas_clear(&translated, 0xFF000000);
    canvas_set_pixel(&translated, 1, 1, 0xFF112233);
    canvas_set_pixel(&translated, 2, 1, 0xFF445566);
    canvas_translate(&translated, 1, -1, 0xFFFFFFFF);
    if (!expect_pixel_eq("translate_moved_a", canvas_get_pixel(&translated, 2, 0), 0xFF112233) ||
        !expect_pixel_eq("translate_moved_b", canvas_get_pixel(&translated, 3, 0), 0xFF445566) ||
        !expect_pixel_eq("translate_fill", canvas_get_pixel(&translated, 0, 2), 0xFFFFFFFF)) {
        canvas_free(&translated);
        return 1;
    }
    canvas_translate(&translated, -2, 2, 0xFFABCDEF);
    if (!expect_pixel_eq("translate_back", canvas_get_pixel(&translated, 0, 2), 0xFF112233) ||
        !expect_pixel_eq("translate_crop_fill", canvas_get_pixel(&translated, 3, 0), 0xFFABCDEF)) {
        canvas_free(&translated);
        return 1;
    }
    canvas_free(&translated);

    Canvas mask;
    if (!canvas_init(&mask, 9, 9)) {
        fprintf(stderr, "mask canvas init failed\n");
        return 1;
    }
    canvas_clear(&mask, 0x00000000);
    for (int y = -2; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            if (abs(x) <= 2 && abs(y) <= 2) {
                canvas_set_pixel_raw(&mask, 4 + x, 4 + y, 0xFFFFFFFF);
            }
        }
    }
    if (!expect_pixel_eq("mask_square_corner", canvas_get_pixel(&mask, 2, 2), 0xFFFFFFFF)) {
        canvas_free(&mask);
        return 1;
    }
    canvas_clear(&mask, 0x00000000);
    for (int y = -2; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            if (abs(x) + abs(y) <= 2) {
                canvas_set_pixel_raw(&mask, 4 + x, 4 + y, 0xFFFFFFFF);
            }
        }
    }
    if (!expect_pixel_eq("mask_diamond_center", canvas_get_pixel(&mask, 4, 4), 0xFFFFFFFF) ||
        !expect_pixel_eq("mask_diamond_corner", canvas_get_pixel(&mask, 2, 2), 0x00000000) ||
        !expect_pixel_eq("mask_diamond_top", canvas_get_pixel(&mask, 4, 2), 0xFFFFFFFF)) {
        canvas_free(&mask);
        return 1;
    }
    canvas_free(&mask);

    if (!test_layers_basic()) {
        return 1;
    }

    printf("ok\n");
    return 0;
}
