#include "../src/canvas.h"
#include "../src/layers.h"
#include "../src/status_text.h"
#include "../src/title_hints.h"
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
    {
        char hint[40];
        format_hidden_layer_hint(&stack, hint, sizeof(hint));
        if (strcmp(hint, " | hint hu C-A-;/'") != 0) {
            fprintf(stderr, "hidden unlocked hint formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    stack.layers[1].locked = 1;
    {
        char hint[40];
        format_hidden_layer_hint(&stack, hint, sizeof(hint));
        if (strcmp(hint, " | hint hl C-S-,/.") != 0) {
            fprintf(stderr, "hidden locked hint formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    stack.layers[1].locked = 0;
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
    if (!layer_stack_toggle_visibility(&stack, 1)) {
        fprintf(stderr, "rehide top layer after show active failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 0;
    stack.layers[0].locked = 1;
    stack.layers[1].visible = 0;
    stack.layers[1].locked = 0;
    {
        char hint[40];
        format_hidden_layer_hint(&stack, hint, sizeof(hint));
        if (strcmp(hint, " | hints hu C-A-;/' hl C-S-,/.") != 0) {
            fprintf(stderr, "mixed hidden hint formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    stack.layers[0].visible = 1;
    stack.layers[0].locked = 0;
    {
        char hint[40];
        format_hidden_layer_hint(&stack, hint, sizeof(hint));
        if (strcmp(hint, " | hint hu C-A-;/'") != 0) {
            fprintf(stderr, "hidden hint reset formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    if (!layer_stack_isolate(&stack, 1)) {
        fprintf(stderr, "isolate active layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1 || !stack.layers[1].visible || stack.layers[0].visible) {
        fprintf(stderr, "isolate active layer bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("isolated_active_layer", canvas_get_pixel(&composite, 8, 8), 0xFFBF7F7F)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show_all(&stack)) {
        fprintf(stderr, "restore layers after isolate failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    {
        char hint[40];
        format_hidden_layer_hint(&stack, hint, sizeof(hint));
        if (hint[0] != '\0') {
            fprintf(stderr, "visible-only stack should not emit hidden hint\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    stack.layers[0].opacity_percent = 60;
    stack.layers[0].locked = 1;
    stack.active_layer = 0;
    stack.solo_index = 0;
    {
        char title[384];
        format_window_title(&stack, "Brush", "Round", 5, 0xFFAABBCC, 75, title, sizeof(title));
        if (strcmp(title, "Openshop - Brush (Round) | size 5 | brush 75% | layer 1/2 Background [visible, locked 60%] [solo] | vis 2 hid 0 lock 1 solo on | #FFAABBCC") != 0) {
            fprintf(stderr, "window title formatting failed for visible locked solo layer\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    stack.layers[0].locked = 0;
    stack.layers[0].opacity_percent = 100;
    stack.solo_index = -1;
    stack.layers[1].visible = 0;
    stack.layers[1].locked = 1;
    stack.layers[1].opacity_percent = 80;
    stack.layers[1].name[0] = '\0';
    stack.active_layer = 1;
    {
        char title[384];
        format_window_title(&stack, "Eraser", "Square", 3, 0xFF010203, 40, title, sizeof(title));
        if (strcmp(title, "Openshop - Eraser (Square) | size 3 | brush 40% | layer 2/2 Layer [hidden, locked 80%] | vis 1 hid 1 lock 1 solo off | #FF010203 | hint hl C-S-,/.") != 0) {
            fprintf(stderr, "window title formatting failed for hidden locked fallback name\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    stack.layers[0].visible = 0;
    stack.layers[0].locked = 1;
    stack.layers[0].opacity_percent = 55;
    stack.layers[1].locked = 0;
    stack.active_layer = 0;
    {
        char title[384];
        format_window_title(&stack, "Line", "Diamond", 7, 0xFF112233, 65, title, sizeof(title));
        if (strcmp(title, "Openshop - Line (Diamond) | size 7 | brush 65% | layer 1/2 Background [hidden, locked 55%] | vis 0 hid 2 lock 1 solo off | #FF112233 | hints hu C-A-;/' hl C-S-,/.") != 0) {
            fprintf(stderr, "window title formatting failed for mixed hidden hint state\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char title[384];
        format_window_title(&stack, NULL, NULL, 2, 0xFF445566, 90, title, sizeof(title));
        if (strcmp(title, "Openshop - Tool (Brush) | size 2 | brush 90% | layer 1/2 Background [hidden, locked 55%] | vis 0 hid 2 lock 1 solo off | #FF445566 | hints hu C-A-;/' hl C-S-,/.") != 0) {
            fprintf(stderr, "window title formatting failed for default tool and brush labels\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    if (strcmp(status_text_action_error(STATUS_LOCK_TOGGLE), "Could not toggle layer lock") != 0 ||
        strcmp(status_text_action_error(STATUS_LOCK_AND_ADVANCE), "Could not lock layer and advance") != 0 ||
        strcmp(status_text_action_error(STATUS_LOCK_AND_RETREAT), "Could not lock layer and retreat") != 0 ||
        strcmp(status_text_action_error(STATUS_UNLOCK_ALL), "Could not unlock all layers") != 0 ||
        strcmp(status_text_action_error(STATUS_SHOW_UNLOCKED_ONLY), "Could not show unlocked layers only") != 0 ||
        strcmp(status_text_action_error(STATUS_SHOW_LOCKED_ONLY), "Could not show locked layers only") != 0 ||
        strcmp(status_text_action_error(STATUS_SHOW_HIDDEN_LOCKED_ONLY), "Could not show hidden locked layers only") != 0 ||
        strcmp(status_text_action_error(STATUS_SHOW_HIDDEN_UNLOCKED_ONLY), "Could not show hidden unlocked layers only") != 0 ||
        strcmp(status_text_action_error(STATUS_INSERT_LAYER_ABOVE), "Could not insert a layer above the active layer") != 0 ||
        strcmp(status_text_action_error(STATUS_INSERT_LAYER_BELOW), "Could not insert a layer below the active layer") != 0 ||
        strcmp(status_text_action_error(STATUS_FLATTEN_LOCKED), "Flatten failed (check for locked layers)") != 0 ||
        strcmp(status_text_action_error(STATUS_STAMP_VISIBLE_INTO_LOCKED), "Stamp visible failed (active layer may be locked)") != 0 ||
        strcmp(status_text_action_error(STATUS_STAMP_VISIBLE_NEW), "Could not stamp visible image into a new layer") != 0 ||
        strcmp(status_text_action_error(STATUS_DUPLICATE_LAYER), "Could not duplicate layer") != 0 ||
        strcmp(status_text_action_error(STATUS_MOVE_LAYER_BOTTOM), "Layer is already at the bottom") != 0 ||
        strcmp(status_text_action_error(STATUS_MOVE_LAYER_TOP), "Layer is already at the top") != 0 ||
        strcmp(status_text_action_error(STATUS_HIDE_FINAL_VISIBLE), "Cannot hide the final visible layer") != 0 ||
        strcmp(status_text_action_error(STATUS_TOGGLE_SOLO), "Could not toggle solo mode") != 0 ||
        strcmp(status_text_action_error(STATUS_DELETE_FINAL_OR_LOCKED), "Cannot delete the final or a locked layer") != 0 ||
        strcmp(status_text_action_error(STATUS_MERGE_DOWN_BLOCKED), "No lower layer to merge into, or one of the layers is locked") != 0 ||
        strcmp(status_text_action_error(STATUS_MERGE_UP_BLOCKED), "No upper layer to merge into, or one of the layers is locked") != 0 ||
        strcmp(status_text_action_error(STATUS_SAVE_OUTPUT_BMP), "Failed to save output.bmp") != 0 ||
        strcmp(status_text_action_error(STATUS_ACTIVE_LAYER_LOCKED), "Active layer is locked") != 0 ||
        strcmp(status_text_action_error(STATUS_LOAD_INPUT_BMP), "Failed to load input.bmp") != 0 ||
        strcmp(status_text_action_error(STATUS_FILL_FAILED), "Fill failed") != 0) {
        fprintf(stderr, "status text action mapping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (strcmp(status_text_action_error((StatusTextAction)999), "Action failed") != 0) {
        fprintf(stderr, "status text fallback failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    {
        char status_message[64];
        format_status_text_max_layers(MAX_LAYERS, status_message, sizeof(status_message));
        if (strcmp(status_message, "Max layers reached (8)") != 0) {
            fprintf(stderr, "max layer status text formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    stack.layers[0].visible = 1;
    stack.layers[0].locked = 0;
    stack.layers[0].opacity_percent = 100;
    stack.layers[1].visible = 1;
    stack.layers[1].locked = 0;
    stack.layers[1].opacity_percent = 100;
    strncpy(stack.layers[1].name, "Top", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    stack.active_layer = 1;
    if (!layer_stack_invert_visibility(&stack, 1)) {
        fprintf(stderr, "invert visibility failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    if (!layer_stack_invert_visibility(&stack, 1)) {
        fprintf(stderr, "invert visibility with locks failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[0].locked || stack.layers[1].locked) {
        fprintf(stderr, "invert visibility should preserve lock state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_invert_visibility(&stack, 1)) {
        fprintf(stderr, "invert visibility lock restore failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 0;
    if (stack.solo_index != -1 || stack.active_layer != 1 || stack.layers[0].visible || !stack.layers[1].visible) {
        fprintf(stderr, "invert visibility bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    layer_stack_composite(&stack, &composite, 0xFFFFFFFF);
    if (!expect_pixel_eq("inverted_visibility_composite", canvas_get_pixel(&composite, 8, 8), 0xFFBF7F7F)) {
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_invert_visibility(&stack, 1)) {
        fprintf(stderr, "invert visibility restore failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[0].visible || stack.layers[1].visible) {
        fprintf(stderr, "invert visibility restore state failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1)) {
        fprintf(stderr, "restore top layer after invert visibility failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.active_layer = 1;
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    stack.layers[0].opacity_percent = 35;
    stack.layers[1].opacity_percent = 80;
    strncpy(stack.layers[0].name, "Background Locked", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[1].name, "Hidden Top", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    if (!layer_stack_show_hidden_only(&stack, 1)) {
        fprintf(stderr, "show hidden only failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[0].locked || stack.layers[1].locked) {
        fprintf(stderr, "show hidden only should preserve lock state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[0].opacity_percent != 35 || stack.layers[1].opacity_percent != 80 ||
        strcmp(stack.layers[0].name, "Background Locked") != 0 ||
        strcmp(stack.layers[1].name, "Hidden Top") != 0) {
        fprintf(stderr, "show hidden only should preserve opacity and names\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1 || stack.active_layer != 1 || stack.layers[0].visible || !stack.layers[1].visible) {
        fprintf(stderr, "show hidden only bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show_hidden_only(&stack, 1)) {
        fprintf(stderr, "show hidden only fallback failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[0].visible || stack.layers[1].visible) {
        fprintf(stderr, "show hidden only fallback should invert when nothing is hidden\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1)) {
        fprintf(stderr, "restore top layer after hidden-only test failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    stack.active_layer = 1;
    stack.layers[0].opacity_percent = 25;
    stack.layers[1].opacity_percent = 90;
    strncpy(stack.layers[0].name, "Locked Base", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[1].name, "Editable Top", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    if (!layer_stack_show_unlocked_only(&stack, 1)) {
        fprintf(stderr, "show unlocked only failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[0].opacity_percent != 25 || stack.layers[1].opacity_percent != 90 ||
        strcmp(stack.layers[0].name, "Locked Base") != 0 ||
        strcmp(stack.layers[1].name, "Editable Top") != 0) {
        fprintf(stderr, "show unlocked only should preserve opacity and names\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1 || stack.active_layer != 1 || stack.layers[0].visible || !stack.layers[1].visible) {
        fprintf(stderr, "show unlocked only bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 1;
    if (!layer_stack_show_unlocked_only(&stack, 1)) {
        fprintf(stderr, "show unlocked only fallback failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[1].visible) {
        fprintf(stderr, "show unlocked only should keep the active layer visible when all are locked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 0;
    stack.layers[1].locked = 0;
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    stack.active_layer = 0;
    stack.layers[0].opacity_percent = 45;
    stack.layers[1].opacity_percent = 70;
    strncpy(stack.layers[0].name, "Locked Active", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[1].name, "Unlocked Peer", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    if (!layer_stack_show_locked_only(&stack, 0)) {
        fprintf(stderr, "show locked only failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[0].opacity_percent != 45 || stack.layers[1].opacity_percent != 70 ||
        strcmp(stack.layers[0].name, "Locked Active") != 0 ||
        strcmp(stack.layers[1].name, "Unlocked Peer") != 0) {
        fprintf(stderr, "show locked only should preserve opacity and names\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1 || stack.active_layer != 0 || !stack.layers[0].visible || stack.layers[1].visible) {
        fprintf(stderr, "show locked only bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 0;
    if (!layer_stack_show_locked_only(&stack, 0)) {
        fprintf(stderr, "show locked only fallback failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[0].visible) {
        fprintf(stderr, "show locked only should keep the active layer visible when nothing is locked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[0].locked = 0;
    stack.layers[1].locked = 1;
    stack.active_layer = 1;
    stack.layers[0].opacity_percent = 20;
    stack.layers[1].opacity_percent = 65;
    strncpy(stack.layers[0].name, "Visible Unlocked", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[1].name, "Hidden Locked", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    if (!layer_stack_show_hidden_locked_only(&stack, 1)) {
        fprintf(stderr, "show hidden locked only failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[0].opacity_percent != 20 || stack.layers[1].opacity_percent != 65 ||
        strcmp(stack.layers[0].name, "Visible Unlocked") != 0 ||
        strcmp(stack.layers[1].name, "Hidden Locked") != 0) {
        fprintf(stderr, "show hidden locked only should preserve opacity and names\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[0].locked || !stack.layers[1].locked) {
        fprintf(stderr, "show hidden locked only should preserve lock state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1 || stack.active_layer != 1 || stack.layers[0].visible || !stack.layers[1].visible) {
        fprintf(stderr, "show hidden locked only bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 0;
    if (!layer_stack_show_hidden_locked_only(&stack, 1)) {
        fprintf(stderr, "show hidden locked only fallback failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[1].visible) {
        fprintf(stderr, "show hidden locked only should keep the active layer visible when no hidden locked layers exist\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    stack.active_layer = 1;
    stack.layers[0].opacity_percent = 30;
    stack.layers[1].opacity_percent = 75;
    strncpy(stack.layers[0].name, "Visible Locked", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[1].name, "Hidden Unlocked", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    if (!layer_stack_show_hidden_unlocked_only(&stack, 1)) {
        fprintf(stderr, "show hidden unlocked only failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[0].opacity_percent != 30 || stack.layers[1].opacity_percent != 75 ||
        strcmp(stack.layers[0].name, "Visible Locked") != 0 ||
        strcmp(stack.layers[1].name, "Hidden Unlocked") != 0) {
        fprintf(stderr, "show hidden unlocked only should preserve opacity and names\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[0].locked || stack.layers[1].locked) {
        fprintf(stderr, "show hidden unlocked only should preserve lock state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1 || stack.active_layer != 1 || stack.layers[0].visible || !stack.layers[1].visible) {
        fprintf(stderr, "show hidden unlocked only bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 1;
    if (!layer_stack_show_hidden_unlocked_only(&stack, 1)) {
        fprintf(stderr, "show hidden unlocked only fallback failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[1].visible) {
        fprintf(stderr, "show hidden unlocked only should keep the active layer visible when no hidden unlocked layers exist\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 0;
    stack.layers[1].locked = 0;
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 1;
    stack.layers[0].opacity_percent = 100;
    stack.layers[1].opacity_percent = 100;
    strncpy(stack.layers[0].name, "Background", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[1].name, "Top", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
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
    stack.active_layer = 0;
    if (!layer_stack_hide_and_advance(&stack, 1)) {
        fprintf(stderr, "hide and advance from non-active index failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[1].visible || stack.active_layer != 0) {
        fprintf(stderr, "hide and advance should scan from the passed index\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 1)) {
        fprintf(stderr, "restore top layer after non-active hide and advance failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_add(&stack, "Retreat", 0x00000000)) {
        fprintf(stderr, "setup hide and retreat failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[2].visible = 1;
    stack.active_layer = 2;
    if (!layer_stack_hide_and_retreat(&stack, 2)) {
        fprintf(stderr, "hide and retreat failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[2].visible || stack.active_layer != 1) {
        fprintf(stderr, "hide and retreat bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[2].visible = 1;
    stack.active_layer = 2;
    if (!layer_stack_toggle_solo(&stack, 2) || !layer_stack_hide_and_retreat(&stack, 2)) {
        fprintf(stderr, "hide and retreat from solo failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1 || stack.active_layer != 1) {
        fprintf(stderr, "hide and retreat should clear solo and focus previous visible layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 2)) {
        fprintf(stderr, "restore retreat layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 0;
    if (!layer_stack_hide_and_retreat(&stack, 2)) {
        fprintf(stderr, "hide and retreat from non-active index failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[2].visible || stack.active_layer != 1) {
        fprintf(stderr, "hide and retreat should scan backward from the passed index\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show(&stack, 2)) {
        fprintf(stderr, "restore retreat layer after non-active hide and retreat failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_delete(&stack, 2)) {
        fprintf(stderr, "cleanup hide and retreat layer failed\n");
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
    stack.layers[1].visible = 0;
    stack.active_layer = 0;
    if (layer_stack_cycle_visible(&stack, 1) != 2 || stack.active_layer != 2) {
        fprintf(stderr, "visible layer cycling forward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_visible(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "visible layer cycling forward wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_visible(&stack, -1) != 2 || stack.active_layer != 2) {
        fprintf(stderr, "visible layer cycling backward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 0;
    if (layer_stack_cycle_hidden(&stack, 1) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "hidden layer cycling forward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 0;
    if (layer_stack_cycle_hidden(&stack, 1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "hidden layer cycling wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_hidden(&stack, -1) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "hidden layer cycling backward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 1;
    stack.active_layer = 0;
    if (layer_stack_cycle_hidden(&stack, 1) != -1 || stack.active_layer != 0) {
        fprintf(stderr, "hidden layer cycling should fail when none are hidden\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 1;
    stack.layers[3].locked = 1;
    stack.active_layer = 0;
    if (layer_stack_cycle_locked(&stack, 1) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "locked layer cycling forward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_locked(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "locked layer cycling wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_locked(&stack, -1) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "locked layer cycling backward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 2;
    if (layer_stack_select_bottom_locked(&stack) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "select bottom locked failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_locked(&stack) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select top locked failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 0;
    stack.layers[3].locked = 0;
    stack.active_layer = 0;
    if (layer_stack_cycle_locked(&stack, 1) != -1 || stack.active_layer != 0) {
        fprintf(stderr, "locked layer cycling should fail when none are locked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_bottom_locked(&stack) != -1 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom locked should fail when none are locked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_locked(&stack) != -1 || stack.active_layer != 0) {
        fprintf(stderr, "select top locked should fail when none are locked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 0;
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 0;
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 1;
    stack.active_layer = 2;
    if (layer_stack_cycle_hidden_locked(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "hidden locked layer cycling forward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_hidden_locked(&stack, 1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "hidden locked layer cycling wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_hidden_locked(&stack, -1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "hidden locked layer cycling backward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 2;
    stack.solo_index = 2;
    if (layer_stack_select_bottom_hidden_locked(&stack) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom hidden locked failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_hidden_locked(&stack) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select top hidden locked failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 2) {
        fprintf(stderr, "hidden locked selection should preserve solo state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 0;
    stack.layers[3].locked = 0;
    stack.active_layer = 2;
    stack.solo_index = -1;
    if (layer_stack_cycle_hidden_locked(&stack, 1) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "hidden locked layer cycling should fail when none are hidden and locked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_bottom_hidden_locked(&stack) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "select bottom hidden locked should fail when none are hidden and locked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_hidden_locked(&stack) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "select top hidden locked should fail when none are hidden and locked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 0;
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 0;
    stack.active_layer = 2;
    if (layer_stack_cycle_hidden_unlocked(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "hidden unlocked layer cycling forward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_hidden_unlocked(&stack, 1) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "hidden unlocked layer cycling wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_hidden_unlocked(&stack, -1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "hidden unlocked layer cycling backward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 2;
    stack.solo_index = 0;
    if (layer_stack_select_bottom_hidden_unlocked(&stack) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "select bottom hidden unlocked failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_hidden_unlocked(&stack) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select top hidden unlocked failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 0) {
        fprintf(stderr, "hidden unlocked selection should preserve solo state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 1;
    stack.layers[3].locked = 1;
    stack.active_layer = 2;
    stack.solo_index = -1;
    if (layer_stack_cycle_hidden_unlocked(&stack, 1) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "hidden unlocked layer cycling should fail when none are hidden and unlocked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_bottom_hidden_unlocked(&stack) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "select bottom hidden unlocked should fail when none are hidden and unlocked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_hidden_unlocked(&stack) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "select top hidden unlocked should fail when none are hidden and unlocked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 1;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 1;
    stack.layers[0].locked = 0;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 0;
    stack.layers[3].locked = 0;
    stack.layers[1].locked = 1;
    stack.layers[3].locked = 1;
    stack.active_layer = 1;
    if (layer_stack_cycle_unlocked(&stack, 1) != 2 || stack.active_layer != 2) {
        fprintf(stderr, "unlocked layer cycling forward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_unlocked(&stack, 1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "unlocked layer cycling wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_unlocked(&stack, -1) != 2 || stack.active_layer != 2) {
        fprintf(stderr, "unlocked layer cycling backward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_bottom_unlocked(&stack) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom unlocked failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_unlocked(&stack) != 2 || stack.active_layer != 2) {
        fprintf(stderr, "select top unlocked failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 1;
    stack.layers[2].locked = 1;
    stack.active_layer = 1;
    if (layer_stack_cycle_unlocked(&stack, 1) != -1 || stack.active_layer != 1) {
        fprintf(stderr, "unlocked layer cycling should fail when none are unlocked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_bottom_unlocked(&stack) != -1 || stack.active_layer != 1) {
        fprintf(stderr, "select bottom unlocked should fail when none are unlocked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_unlocked(&stack) != -1 || stack.active_layer != 1) {
        fprintf(stderr, "select top unlocked should fail when none are unlocked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 1;
    stack.layers[0].locked = 0;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 0;
    stack.active_layer = 0;
    if (layer_stack_cycle_editable(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "editable layer cycling forward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_editable(&stack, 1) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "editable layer cycling wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_editable(&stack, -1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "editable layer cycling backward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_bottom_editable(&stack) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom editable failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_editable(&stack) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select top editable failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 3)) {
        fprintf(stderr, "toggle solo for editable selection failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_bottom_editable(&stack) != 0 || stack.active_layer != 0 || stack.solo_index != 3) {
        fprintf(stderr, "editable selection should preserve solo state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_cycle_editable(&stack, 1) != 3 || stack.active_layer != 3 || stack.solo_index != 3) {
        fprintf(stderr, "editable cycling should preserve solo state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 3) || stack.solo_index != -1) {
        fprintf(stderr, "toggle solo off after editable selection failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 1;
    stack.layers[3].visible = 0;
    stack.active_layer = 2;
    if (layer_stack_cycle_editable(&stack, 1) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "editable layer cycling should fail when no visible unlocked layers exist\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_bottom_editable(&stack) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "select bottom editable should fail when no visible unlocked layers exist\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_editable(&stack) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "select top editable should fail when no visible unlocked layers exist\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 0;
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 0;
    stack.layers[0].locked = 0;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 0;
    stack.layers[2].opacity_percent = 55;
    stack.layers[3].opacity_percent = 75;
    strncpy(stack.layers[2].name, "Locked Mid", LAYER_NAME_MAX - 1);
    stack.layers[2].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[3].name, "Hidden Editable", LAYER_NAME_MAX - 1);
    stack.layers[3].name[LAYER_NAME_MAX - 1] = '\0';
    stack.active_layer = 2;
    stack.solo_index = 2;
    if (!layer_stack_reveal_editable(&stack, 1) || stack.active_layer != 3 || !stack.layers[3].visible) {
        fprintf(stderr, "reveal editable forward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[2].opacity_percent != 55 || stack.layers[3].opacity_percent != 75 ||
        strcmp(stack.layers[2].name, "Locked Mid") != 0 ||
        strcmp(stack.layers[3].name, "Hidden Editable") != 0) {
        fprintf(stderr, "reveal editable should preserve opacity and names\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1) {
        fprintf(stderr, "reveal editable should clear solo mode\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 0;
    stack.layers[1].visible = 0;
    stack.layers[1].opacity_percent = 65;
    strncpy(stack.layers[1].name, "Backward Editable", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    stack.active_layer = 3;
    if (!layer_stack_reveal_editable(&stack, -1) || stack.active_layer != 1 || !stack.layers[1].visible) {
        fprintf(stderr, "reveal editable backward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[1].opacity_percent != 65 || strcmp(stack.layers[1].name, "Backward Editable") != 0) {
        fprintf(stderr, "reveal editable backward should preserve metadata\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 1;
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 1;
    stack.layers[1].opacity_percent = 35;
    strncpy(stack.layers[1].name, "Only Editable Fallback", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    stack.active_layer = 1;
    stack.solo_index = 2;
    if (!layer_stack_reveal_editable(&stack, 1) || stack.active_layer != 1 || !stack.layers[1].visible) {
        fprintf(stderr, "reveal editable should reveal the active layer when it is the only unlocked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[1].opacity_percent != 35 || strcmp(stack.layers[1].name, "Only Editable Fallback") != 0) {
        fprintf(stderr, "reveal editable fallback should preserve metadata\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1) {
        fprintf(stderr, "reveal editable fallback should clear solo mode\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].visible = 0;
    stack.solo_index = 3;
    if (!layer_stack_reveal_editable(&stack, -1) || stack.active_layer != 1 || !stack.layers[1].visible) {
        fprintf(stderr, "reveal editable backward should reveal the active layer when it is the only unlocked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1) {
        fprintf(stderr, "reveal editable backward fallback should clear solo mode\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 1;
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 1;
    stack.active_layer = 0;
    if (layer_stack_reveal_editable(&stack, 1)) {
        fprintf(stderr, "reveal editable should fail when all layers are locked\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 0;
    stack.layers[1].visible = 1;
    stack.layers[2].visible = 0;
    stack.layers[3].visible = 0;
    stack.layers[0].locked = 0;
    stack.layers[1].locked = 1;
    stack.layers[2].locked = 0;
    stack.layers[3].locked = 0;
    stack.layers[0].opacity_percent = 40;
    stack.layers[2].opacity_percent = 60;
    stack.layers[3].opacity_percent = 85;
    strncpy(stack.layers[0].name, "Bottom Hidden Editable", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[2].name, "Mid Hidden Editable", LAYER_NAME_MAX - 1);
    stack.layers[2].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[3].name, "Top Hidden Editable", LAYER_NAME_MAX - 1);
    stack.layers[3].name[LAYER_NAME_MAX - 1] = '\0';
    stack.active_layer = 1;
    stack.solo_index = 1;
    if (!layer_stack_reveal_hidden_editable(&stack, 0) || stack.active_layer != 0 || !stack.layers[0].visible) {
        fprintf(stderr, "reveal hidden editable from bottom failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[0].opacity_percent != 40 || stack.layers[2].opacity_percent != 60 ||
        stack.layers[3].opacity_percent != 85 ||
        strcmp(stack.layers[0].name, "Bottom Hidden Editable") != 0 ||
        strcmp(stack.layers[2].name, "Mid Hidden Editable") != 0 ||
        strcmp(stack.layers[3].name, "Top Hidden Editable") != 0) {
        fprintf(stderr, "reveal hidden editable should preserve metadata\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1) {
        fprintf(stderr, "reveal hidden editable should clear solo mode\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 0;
    if (!layer_stack_reveal_hidden_editable(&stack, 1) || stack.active_layer != 3 || !stack.layers[3].visible) {
        fprintf(stderr, "reveal hidden editable from top failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[3].opacity_percent != 85 || strcmp(stack.layers[3].name, "Top Hidden Editable") != 0) {
        fprintf(stderr, "reveal hidden editable from top should preserve metadata\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 1;
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 1;
    stack.layers[0].visible = 0;
    stack.layers[2].visible = 0;
    stack.layers[3].visible = 0;
    stack.active_layer = 1;
    if (layer_stack_reveal_hidden_editable(&stack, 0)) {
        fprintf(stderr, "reveal hidden editable should fail when no hidden unlocked layers exist\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 0;
    stack.layers[1].visible = 1;
    stack.layers[2].visible = 0;
    stack.layers[3].visible = 0;
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 0;
    stack.layers[3].locked = 1;
    stack.layers[0].opacity_percent = 30;
    stack.layers[2].opacity_percent = 70;
    stack.layers[3].opacity_percent = 85;
    strncpy(stack.layers[0].name, "Bottom Hidden Locked", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[2].name, "Mid Hidden Unlocked", LAYER_NAME_MAX - 1);
    stack.layers[2].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[3].name, "Top Hidden Locked", LAYER_NAME_MAX - 1);
    stack.layers[3].name[LAYER_NAME_MAX - 1] = '\0';
    stack.active_layer = 1;
    stack.solo_index = 1;
    if (!layer_stack_reveal_hidden_locked(&stack, 0) || stack.active_layer != 0 || !stack.layers[0].visible) {
        fprintf(stderr, "reveal hidden locked from bottom failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[0].opacity_percent != 30 || stack.layers[2].opacity_percent != 70 ||
        stack.layers[3].opacity_percent != 85 ||
        strcmp(stack.layers[0].name, "Bottom Hidden Locked") != 0 ||
        strcmp(stack.layers[2].name, "Mid Hidden Unlocked") != 0 ||
        strcmp(stack.layers[3].name, "Top Hidden Locked") != 0) {
        fprintf(stderr, "reveal hidden locked should preserve metadata\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1) {
        fprintf(stderr, "reveal hidden locked should clear solo mode\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 0;
    if (!layer_stack_reveal_hidden_locked(&stack, 1) || stack.active_layer != 3 || !stack.layers[3].visible) {
        fprintf(stderr, "reveal hidden locked from top failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[3].opacity_percent != 85 || strcmp(stack.layers[3].name, "Top Hidden Locked") != 0) {
        fprintf(stderr, "reveal hidden locked from top should preserve metadata\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1) {
        fprintf(stderr, "reveal hidden locked from top should clear solo mode\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 0;
    stack.layers[3].locked = 0;
    stack.layers[0].visible = 0;
    stack.layers[3].visible = 0;
    stack.active_layer = 1;
    if (layer_stack_reveal_hidden_locked(&stack, 0)) {
        fprintf(stderr, "reveal hidden locked should fail when no hidden locked layers exist\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 1;
    stack.layers[2].visible = 0;
    stack.layers[3].visible = 0;
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 1;
    stack.layers[2].locked = 0;
    stack.layers[3].locked = 0;
    stack.layers[2].opacity_percent = 55;
    stack.layers[3].opacity_percent = 75;
    strncpy(stack.layers[2].name, "Bottom Hidden Unlocked", LAYER_NAME_MAX - 1);
    stack.layers[2].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[3].name, "Top Hidden Unlocked", LAYER_NAME_MAX - 1);
    stack.layers[3].name[LAYER_NAME_MAX - 1] = '\0';
    stack.active_layer = 1;
    stack.solo_index = 1;
    if (!layer_stack_reveal_hidden_unlocked(&stack, 0) || stack.active_layer != 2 || !stack.layers[2].visible) {
        fprintf(stderr, "reveal hidden unlocked from bottom failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[2].opacity_percent != 55 || stack.layers[3].opacity_percent != 75 ||
        strcmp(stack.layers[2].name, "Bottom Hidden Unlocked") != 0 ||
        strcmp(stack.layers[3].name, "Top Hidden Unlocked") != 0) {
        fprintf(stderr, "reveal hidden unlocked should preserve metadata\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1) {
        fprintf(stderr, "reveal hidden unlocked should clear solo mode\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[2].visible = 0;
    if (!layer_stack_reveal_hidden_unlocked(&stack, 1) || stack.active_layer != 3 || !stack.layers[3].visible) {
        fprintf(stderr, "reveal hidden unlocked from top failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[3].opacity_percent != 75 || strcmp(stack.layers[3].name, "Top Hidden Unlocked") != 0) {
        fprintf(stderr, "reveal hidden unlocked from top should preserve metadata\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1) {
        fprintf(stderr, "reveal hidden unlocked from top should clear solo mode\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 1;
    stack.layers[2].visible = 0;
    stack.layers[3].visible = 0;
    stack.active_layer = 1;
    if (layer_stack_reveal_hidden_unlocked(&stack, 0)) {
        fprintf(stderr, "reveal hidden unlocked should fail when no hidden unlocked layers exist\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 0;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 0;
    stack.layers[3].locked = 0;
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 1;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 1;
    stack.layers[0].opacity_percent = 100;
    stack.layers[1].opacity_percent = 100;
    stack.layers[2].opacity_percent = 100;
    stack.layers[3].opacity_percent = 100;
    strncpy(stack.layers[0].name, "Background", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[1].name, "Top", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[2].name, "Third", LAYER_NAME_MAX - 1);
    stack.layers[2].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[3].name, "Fourth", LAYER_NAME_MAX - 1);
    stack.layers[3].name[LAYER_NAME_MAX - 1] = '\0';
    stack.active_layer = 1;
    stack.layers[1].opacity_percent = 70;
    stack.layers[2].opacity_percent = 55;
    strncpy(stack.layers[1].name, "Advance Lock Source", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[2].name, "Advance Lock Target", LAYER_NAME_MAX - 1);
    stack.layers[2].name[LAYER_NAME_MAX - 1] = '\0';
    stack.solo_index = 3;
    if (!layer_stack_lock_and_advance(&stack, 1)) {
        fprintf(stderr, "lock and advance failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[1].locked || stack.active_layer != 2) {
        fprintf(stderr, "lock and advance bookkeeping failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[1].opacity_percent != 70 || stack.layers[2].opacity_percent != 55 ||
        strcmp(stack.layers[1].name, "Advance Lock Source") != 0 ||
        strcmp(stack.layers[2].name, "Advance Lock Target") != 0) {
        fprintf(stderr, "lock and advance should preserve metadata\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 3) {
        fprintf(stderr, "lock and advance should preserve solo state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_lock_and_advance(&stack, 2)) {
        fprintf(stderr, "second lock and advance failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[2].locked || stack.active_layer != 3) {
        fprintf(stderr, "lock and advance should jump to next unlocked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 0;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 0;
    stack.layers[3].locked = 0;
    stack.active_layer = 0;
    if (!layer_stack_lock_and_advance(&stack, 1)) {
        fprintf(stderr, "lock and advance from non-active index failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[1].locked || stack.active_layer != 2) {
        fprintf(stderr, "lock and advance should scan from the passed index\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 0;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 0;
    stack.layers[3].locked = 0;
    stack.active_layer = 2;
    stack.layers[1].opacity_percent = 65;
    stack.layers[2].opacity_percent = 45;
    strncpy(stack.layers[1].name, "Retreat Lock Target", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[2].name, "Retreat Lock Source", LAYER_NAME_MAX - 1);
    stack.layers[2].name[LAYER_NAME_MAX - 1] = '\0';
    stack.solo_index = 0;
    if (!layer_stack_lock_and_retreat(&stack, 2)) {
        fprintf(stderr, "lock and retreat failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[2].locked || stack.active_layer != 1) {
        fprintf(stderr, "lock and retreat should jump to previous unlocked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[1].opacity_percent != 65 || stack.layers[2].opacity_percent != 45 ||
        strcmp(stack.layers[1].name, "Retreat Lock Target") != 0 ||
        strcmp(stack.layers[2].name, "Retreat Lock Source") != 0) {
        fprintf(stderr, "lock and retreat should preserve metadata\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != 0) {
        fprintf(stderr, "lock and retreat should preserve solo state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_lock_and_retreat(&stack, 1)) {
        fprintf(stderr, "second lock and retreat failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[1].locked || stack.active_layer != 0) {
        fprintf(stderr, "lock and retreat should continue scanning backward\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 0;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 0;
    stack.layers[3].locked = 0;
    stack.active_layer = 3;
    if (!layer_stack_lock_and_retreat(&stack, 2)) {
        fprintf(stderr, "lock and retreat from non-active index failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[2].locked || stack.active_layer != 1) {
        fprintf(stderr, "lock and retreat should scan backward from the passed index\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 1;
    stack.layers[2].locked = 0;
    stack.layers[3].locked = 1;
    stack.active_layer = 2;
    stack.solo_index = 3;
    if (!layer_stack_lock_and_advance(&stack, 2)) {
        fprintf(stderr, "lock and advance fallback failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[2].locked || stack.active_layer != 2 || stack.solo_index != 3) {
        fprintf(stderr, "lock and advance should stay on the active layer when it is the last unlocked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 1;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 1;
    stack.layers[3].locked = 1;
    stack.active_layer = 1;
    stack.solo_index = 0;
    if (!layer_stack_lock_and_retreat(&stack, 1)) {
        fprintf(stderr, "lock and retreat fallback failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!stack.layers[1].locked || stack.active_layer != 1 || stack.solo_index != 0) {
        fprintf(stderr, "lock and retreat should stay on the active layer when it is the last unlocked layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].locked = 0;
    stack.layers[1].locked = 0;
    stack.layers[2].locked = 0;
    stack.layers[3].locked = 0;
    stack.solo_index = -1;
    stack.layers[0].opacity_percent = 100;
    stack.layers[1].opacity_percent = 100;
    stack.layers[2].opacity_percent = 100;
    stack.layers[3].opacity_percent = 100;
    strncpy(stack.layers[0].name, "Background", LAYER_NAME_MAX - 1);
    stack.layers[0].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[1].name, "Top", LAYER_NAME_MAX - 1);
    stack.layers[1].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[2].name, "Third", LAYER_NAME_MAX - 1);
    stack.layers[2].name[LAYER_NAME_MAX - 1] = '\0';
    strncpy(stack.layers[3].name, "Fourth", LAYER_NAME_MAX - 1);
    stack.layers[3].name[LAYER_NAME_MAX - 1] = '\0';
    stack.layers[0].locked = 1;
    stack.layers[2].locked = 1;
    if (!layer_stack_unlock_all(&stack)) {
        fprintf(stderr, "unlock all failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.layers[0].locked || stack.layers[1].locked || stack.layers[2].locked || stack.layers[3].locked) {
        fprintf(stderr, "unlock all should clear every layer lock\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].visible = 0;
    stack.layers[0].visible = 0;
    stack.layers[2].visible = 0;
    stack.active_layer = 0;
    if (layer_stack_cycle_visible(&stack, 1) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "visible layer cycling should land on the only visible layer\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 0;
    stack.layers[3].visible = 1;
    stack.active_layer = 1;
    if (layer_stack_select_bottom_visible(&stack) != 0 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom visible failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_visible(&stack) != 3 || stack.active_layer != 3) {
        fprintf(stderr, "select top visible failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 0;
    stack.layers[3].visible = 0;
    stack.active_layer = 2;
    if (layer_stack_select_bottom_visible(&stack) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "select bottom visible should fail when no layers are visible\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_visible(&stack) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "select top visible should fail when no layers are visible\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 0;
    stack.layers[3].visible = 1;
    stack.active_layer = 0;
    stack.solo_index = 3;
    if (!layer_stack_reveal_hidden(&stack, 1) || stack.active_layer != 1 || !stack.layers[1].visible) {
        fprintf(stderr, "reveal hidden forward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_reveal_hidden(&stack, 1) || stack.active_layer != 2 || !stack.layers[2].visible) {
        fprintf(stderr, "reveal hidden forward wrap failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].visible = 0;
    stack.active_layer = 2;
    if (!layer_stack_reveal_hidden(&stack, -1) || stack.active_layer != 1 || !stack.layers[1].visible) {
        fprintf(stderr, "reveal hidden backward failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.solo_index != -1) {
        fprintf(stderr, "reveal hidden should clear solo mode\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_reveal_hidden(&stack, 1)) {
        fprintf(stderr, "reveal hidden should fail when no hidden layers remain\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 0;
    stack.layers[3].visible = 1;
    stack.active_layer = 0;
    if (layer_stack_select_bottom_hidden(&stack) != 1 || stack.active_layer != 1) {
        fprintf(stderr, "select bottom hidden failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_hidden(&stack) != 2 || stack.active_layer != 2) {
        fprintf(stderr, "select top hidden failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].visible = 1;
    stack.layers[2].visible = 1;
    stack.active_layer = 0;
    if (layer_stack_select_bottom_hidden(&stack) != -1 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom hidden should fail when none are hidden\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_hidden(&stack) != -1 || stack.active_layer != 0) {
        fprintf(stderr, "select top hidden should fail when none are hidden\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 0;
    stack.layers[3].visible = 1;
    if (!layer_stack_toggle_solo(&stack, 3)) {
        fprintf(stderr, "toggle solo for visible edge selection failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_bottom_visible(&stack) != 0 || stack.active_layer != 0 || stack.solo_index != 3) {
        fprintf(stderr, "visible edge selection should preserve solo state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_select_top_hidden(&stack) != 2 || stack.active_layer != 2 || stack.solo_index != 3) {
        fprintf(stderr, "hidden edge selection should preserve solo state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_toggle_solo(&stack, 3) || stack.solo_index != -1) {
        fprintf(stderr, "toggle solo off after edge selection failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_show_all(&stack)) {
        fprintf(stderr, "restore visibility after visible selection tests failed\n");
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
    if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != 3) {
        fprintf(stderr, "second stamp visible new layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != 4) {
        fprintf(stderr, "third stamp visible new layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != 5) {
        fprintf(stderr, "fourth stamp visible new layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != 6) {
        fprintf(stderr, "fifth stamp visible new layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_stamp_visible_new(&stack, "Overflow", 0xFFFFFFFF) != 7) {
        fprintf(stderr, "sixth stamp visible new layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
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
