#include "../src/active_layer_ops.h"
#include "../src/app_hotkey.h"
#include "../src/app_input_rules.h"
#include "../src/brush_render.h"
#include "../src/brush_state.h"
#include "../src/canvas.h"
#include "../src/color_sample.h"
#include "../src/display_canvas.h"
#include "../src/geometry_helpers.h"
#include "../src/layer_action_history.h"
#include "../src/layer_creation.h"
#include "../src/layer_edit_state.h"
#include "../src/layer_selection.h"
#include "../src/layers.h"
#include "../src/shape_draw.h"
#include "../src/shape_preview_state.h"
#include "../src/snapshot_history.h"
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

static int test_layer_action_history_custom_flip(LayerStack *layers, void *ctx) {
    int *flip = (int *)ctx;
    if (!layers || !flip || !*flip) {
        return 0;
    }
    layers->active_layer = 0;
    return 1;
}

static int test_layer_action_history_custom_bad_noop(LayerStack *layers, void *ctx) {
    (void)ctx;
    if (!layers) {
        return 0;
    }
    layers->active_layer = 0;
    return 0;
}

static int test_app_input_rules(void) {
    int dx = 99;
    int dy = 99;

    if (!app_key_translation_delta(APP_KEY_UP, 4, &dx, &dy) || dx != 0 || dy != -4) {
        fprintf(stderr, "translation delta up failed\n");
        return 0;
    }
    if (!app_key_translation_delta(APP_KEY_RIGHT, 3, &dx, &dy) || dx != 3 || dy != 0) {
        fprintf(stderr, "translation delta right failed\n");
        return 0;
    }
    if (app_key_translation_delta(APP_KEY_b, 2, &dx, &dy) || dx != 0 || dy != 0) {
        fprintf(stderr, "translation delta non-arrow should fail cleanly\n");
        return 0;
    }
    if (app_key_translation_delta(APP_KEY_LEFT, 1, NULL, &dy) || app_key_translation_delta(APP_KEY_LEFT, 1, &dx, NULL)) {
        fprintf(stderr, "translation delta null guard failed\n");
        return 0;
    }
    if (!app_translation_hotkey_delta(APP_KEY_LEFT, 0, 0, 1, &dx, &dy) || dx != -10 || dy != 0) {
        fprintf(stderr, "translation hotkey shift-step failed\n");
        return 0;
    }
    if (app_translation_hotkey_delta(APP_KEY_LEFT, 1, 0, 0, &dx, &dy) ||
        app_translation_hotkey_delta(APP_KEY_LEFT, 0, 1, 0, &dx, &dy) ||
        app_translation_hotkey_delta(APP_KEY_b, 0, 0, 0, &dx, &dy)) {
        fprintf(stderr, "translation hotkey modifier rejection failed\n");
        return 0;
    }

    if (app_should_cancel_shape_on_key(APP_KEY_ESCAPE, 0, 0) ||
        app_should_cancel_shape_on_key(APP_KEY_LSHIFT, 0, 0) ||
        app_should_cancel_shape_on_key(APP_KEY_RSHIFT, 0, 0)) {
        fprintf(stderr, "shape cancel should ignore escape and shift keys\n");
        return 0;
    }
    if (!app_should_cancel_shape_on_key(APP_KEY_UP, 0, 0) ||
        !app_should_cancel_shape_on_key(APP_KEY_b, 0, 0) ||
        !app_should_cancel_shape_on_key(APP_KEY_4, 0, 0)) {
        fprintf(stderr, "shape cancel plain hotkeys failed\n");
        return 0;
    }
    if (!app_should_cancel_shape_on_key(APP_KEY_s, 1, 0) ||
        !app_should_cancel_shape_on_key(APP_KEY_8, 1, 0) ||
        !app_should_cancel_shape_on_key(APP_KEY_SLASH, 1, 0)) {
        fprintf(stderr, "shape cancel ctrl hotkeys failed\n");
        return 0;
    }
    if (app_should_cancel_shape_on_key(APP_KEY_s, 0, 0) ||
        app_should_cancel_shape_on_key(APP_KEY_TAB, 0, 0) ||
        app_should_cancel_shape_on_key(APP_KEY_TAB, 1, 0) ||
        app_should_cancel_shape_on_key(APP_KEY_b, 0, 1) ||
        app_should_cancel_shape_on_key(APP_KEY_f, 0, 1)) {
        fprintf(stderr, "shape cancel non-hotkeys or alt-modified keys should fail cleanly\n");
        return 0;
    }
    if (app_opacity_hotkey_action(APP_KEY_0, 0, 0) != APP_OPACITY_HOTKEY_SET_MAX ||
        app_opacity_hotkey_action(APP_KEY_MINUS, 0, 0) != APP_OPACITY_HOTKEY_NUDGE_DOWN ||
        app_opacity_hotkey_action(APP_KEY_KP_MINUS, 0, 0) != APP_OPACITY_HOTKEY_NUDGE_DOWN ||
        app_opacity_hotkey_action(APP_KEY_EQUALS, 0, 0) != APP_OPACITY_HOTKEY_NUDGE_UP ||
        app_opacity_hotkey_action(APP_KEY_KP_PLUS, 0, 0) != APP_OPACITY_HOTKEY_NUDGE_UP) {
        fprintf(stderr, "opacity hotkey mapping failed\n");
        return 0;
    }
    if (app_opacity_hotkey_action(APP_KEY_b, 0, 0) != APP_OPACITY_HOTKEY_NONE ||
        app_opacity_hotkey_action(APP_KEY_MINUS, 1, 0) != APP_OPACITY_HOTKEY_NONE ||
        app_opacity_hotkey_action(APP_KEY_EQUALS, 0, 1) != APP_OPACITY_HOTKEY_NONE) {
        fprintf(stderr, "opacity non-hotkey or modifier rejection failed\n");
        return 0;
    }
    {
        int arg = -1;
        if (app_layer_navigation_action(APP_KEY_3, 1, 0, 0, &arg) != APP_LAYER_NAV_SELECT_INDEX || arg != 2) {
            fprintf(stderr, "layer navigation select index mapping failed\n");
            return 0;
        }
        if (app_layer_navigation_action(APP_KEY_PAGEUP, 0, 0, 0, &arg) != APP_LAYER_NAV_CYCLE_UP ||
            app_layer_navigation_action(APP_KEY_PAGEDOWN, 0, 0, 0, &arg) != APP_LAYER_NAV_CYCLE_DOWN) {
            fprintf(stderr, "layer navigation cycle mapping failed\n");
            return 0;
        }
        if (app_layer_navigation_action(APP_KEY_PAGEUP, 1, 0, 0, &arg) != APP_LAYER_NAV_NONE ||
            app_layer_navigation_action(APP_KEY_3, 1, 0, 1, &arg) != APP_LAYER_NAV_NONE ||
            app_layer_navigation_action(APP_KEY_b, 0, 0, 0, &arg) != APP_LAYER_NAV_NONE) {
            fprintf(stderr, "layer navigation non-hotkey mapping failed\n");
            return 0;
        }
        if (app_file_hotkey_action(APP_KEY_s, 1, 0, 0) != APP_FILE_HOTKEY_SAVE ||
            app_file_hotkey_action(APP_KEY_o, 1, 0, 0) != APP_FILE_HOTKEY_LOAD_ACTIVE_LAYER) {
            fprintf(stderr, "file hotkey mapping failed\n");
            return 0;
        }
        if (app_file_hotkey_action(APP_KEY_s, 0, 0, 0) != APP_FILE_HOTKEY_NONE ||
            app_file_hotkey_action(APP_KEY_o, 1, 1, 0) != APP_FILE_HOTKEY_NONE ||
            app_file_hotkey_action(APP_KEY_b, 1, 0, 0) != APP_FILE_HOTKEY_NONE) {
            fprintf(stderr, "file non-hotkey mapping failed\n");
            return 0;
        }
        if (app_history_hotkey_action(APP_KEY_z, 1, 0, 0) != APP_HISTORY_HOTKEY_UNDO ||
            app_history_hotkey_action(APP_KEY_y, 1, 0, 0) != APP_HISTORY_HOTKEY_REDO) {
            fprintf(stderr, "history hotkey mapping failed\n");
            return 0;
        }
        if (app_history_hotkey_action(APP_KEY_z, 0, 0, 0) != APP_HISTORY_HOTKEY_NONE ||
            app_history_hotkey_action(APP_KEY_y, 1, 0, 1) != APP_HISTORY_HOTKEY_NONE ||
            app_history_hotkey_action(APP_KEY_b, 1, 0, 0) != APP_HISTORY_HOTKEY_NONE) {
            fprintf(stderr, "history non-hotkey mapping failed\n");
            return 0;
        }
        if (app_brush_adjust_hotkey_action(APP_KEY_LEFTBRACKET, 0, 0) != APP_BRUSH_ADJUST_RADIUS_DOWN ||
            app_brush_adjust_hotkey_action(APP_KEY_RIGHTBRACKET, 0, 0) != APP_BRUSH_ADJUST_RADIUS_UP ||
            app_brush_adjust_hotkey_action(APP_KEY_COMMA, 0, 0) != APP_BRUSH_ADJUST_SHAPE_PREV ||
            app_brush_adjust_hotkey_action(APP_KEY_PERIOD, 0, 0) != APP_BRUSH_ADJUST_SHAPE_NEXT ||
            app_brush_adjust_hotkey_action(APP_KEY_MINUS, 0, 0) != APP_BRUSH_ADJUST_OPACITY_DOWN ||
            app_brush_adjust_hotkey_action(APP_KEY_KP_PLUS, 0, 0) != APP_BRUSH_ADJUST_OPACITY_UP) {
            fprintf(stderr, "brush adjust hotkey mapping failed\n");
            return 0;
        }
        if (app_brush_adjust_hotkey_action(APP_KEY_b, 0, 0) != APP_BRUSH_ADJUST_NONE ||
            app_brush_adjust_hotkey_action(APP_KEY_LEFTBRACKET, 1, 0) != APP_BRUSH_ADJUST_NONE ||
            app_brush_adjust_hotkey_action(APP_KEY_MINUS, 0, 1) != APP_BRUSH_ADJUST_NONE) {
            fprintf(stderr, "brush adjust non-hotkey or modifier rejection failed\n");
            return 0;
        }
        if (!app_is_add_layer_hotkey(APP_KEY_n, 1, 0, 1)) {
            fprintf(stderr, "add layer hotkey mapping failed\n");
            return 0;
        }
        if (app_is_add_layer_hotkey(APP_KEY_n, 1, 1, 1) ||
            app_is_add_layer_hotkey(APP_KEY_n, 1, 0, 0) ||
            app_is_add_layer_hotkey(APP_KEY_b, 1, 0, 1)) {
            fprintf(stderr, "add layer non-hotkey mapping failed\n");
            return 0;
        }
        if (app_active_edit_hotkey_action(APP_KEY_c, 0, 0) != APP_ACTIVE_EDIT_CLEAR ||
            app_active_edit_hotkey_action(APP_KEY_h, 0, 0) != APP_ACTIVE_EDIT_FLIP_HORIZONTAL ||
            app_active_edit_hotkey_action(APP_KEY_v, 0, 0) != APP_ACTIVE_EDIT_FLIP_VERTICAL ||
            app_active_edit_hotkey_action(APP_KEY_j, 0, 0) != APP_ACTIVE_EDIT_ROTATE_180 ||
            app_active_edit_hotkey_action(APP_KEY_x, 0, 0) != APP_ACTIVE_EDIT_INVERT_RGB) {
            fprintf(stderr, "active edit hotkey mapping failed\n");
            return 0;
        }
        if (app_active_edit_hotkey_action(APP_KEY_b, 0, 0) != APP_ACTIVE_EDIT_NONE ||
            app_active_edit_hotkey_action(APP_KEY_c, 1, 0) != APP_ACTIVE_EDIT_NONE ||
            app_active_edit_hotkey_action(APP_KEY_v, 0, 1) != APP_ACTIVE_EDIT_NONE) {
            fprintf(stderr, "active edit non-hotkey or modifier rejection failed\n");
            return 0;
        }
        if (app_mouse_position_hotkey_action(APP_KEY_f, 0, 0) != APP_MOUSE_POSITION_FILL ||
            app_mouse_position_hotkey_action(APP_KEY_i, 0, 0) != APP_MOUSE_POSITION_SAMPLE) {
            fprintf(stderr, "mouse position hotkey mapping failed\n");
            return 0;
        }
        if (app_mouse_position_hotkey_action(APP_KEY_b, 0, 0) != APP_MOUSE_POSITION_NONE ||
            app_mouse_position_hotkey_action(APP_KEY_f, 1, 0) != APP_MOUSE_POSITION_NONE ||
            app_mouse_position_hotkey_action(APP_KEY_i, 0, 1) != APP_MOUSE_POSITION_NONE) {
            fprintf(stderr, "mouse position non-hotkey or modifier rejection failed\n");
            return 0;
        }
        if (!app_mouse_position_marks_composite(APP_MOUSE_POSITION_FILL, 1) ||
            app_mouse_position_marks_composite(APP_MOUSE_POSITION_FILL, 0) ||
            app_mouse_position_marks_composite(APP_MOUSE_POSITION_SAMPLE, 1) ||
            app_mouse_position_marks_composite(APP_MOUSE_POSITION_NONE, 1)) {
            fprintf(stderr, "mouse position composite marking failed\n");
            return 0;
        }
        if (app_brush_preset_hotkey_action(APP_KEY_b, 0, 0) != APP_BRUSH_PRESET_DEFAULT ||
            app_brush_preset_hotkey_action(APP_KEY_1, 0, 0) != APP_BRUSH_PRESET_DEFAULT ||
            app_brush_preset_hotkey_action(APP_KEY_e, 0, 0) != APP_BRUSH_PRESET_ERASE ||
            app_brush_preset_hotkey_action(APP_KEY_2, 0, 0) != APP_BRUSH_PRESET_RED ||
            app_brush_preset_hotkey_action(APP_KEY_3, 0, 0) != APP_BRUSH_PRESET_GREEN ||
            app_brush_preset_hotkey_action(APP_KEY_4, 0, 0) != APP_BRUSH_PRESET_BLUE ||
            app_brush_preset_hotkey_action(APP_KEY_5, 0, 0) != APP_BRUSH_PRESET_YELLOW ||
            app_brush_preset_hotkey_action(APP_KEY_6, 0, 0) != APP_BRUSH_PRESET_PURPLE) {
            fprintf(stderr, "brush preset hotkey mapping failed\n");
            return 0;
        }
        if (app_brush_preset_hotkey_action(APP_KEY_l, 0, 0) != APP_BRUSH_PRESET_NONE ||
            app_brush_preset_hotkey_action(APP_KEY_b, 1, 0) != APP_BRUSH_PRESET_NONE ||
            app_brush_preset_hotkey_action(APP_KEY_e, 0, 1) != APP_BRUSH_PRESET_NONE) {
            fprintf(stderr, "brush preset non-hotkey or modifier rejection failed\n");
            return 0;
        }
        if (app_brush_tool_hotkey_action(APP_KEY_l, 0, 0) != APP_BRUSH_TOOL_LINE ||
            app_brush_tool_hotkey_action(APP_KEY_r, 0, 0) != APP_BRUSH_TOOL_RECT ||
            app_brush_tool_hotkey_action(APP_KEY_t, 0, 0) != APP_BRUSH_TOOL_FILLED_RECT ||
            app_brush_tool_hotkey_action(APP_KEY_o, 0, 0) != APP_BRUSH_TOOL_ELLIPSE ||
            app_brush_tool_hotkey_action(APP_KEY_p, 0, 0) != APP_BRUSH_TOOL_FILLED_ELLIPSE) {
            fprintf(stderr, "brush tool hotkey mapping failed\n");
            return 0;
        }
        if (app_brush_tool_hotkey_action(APP_KEY_b, 0, 0) != APP_BRUSH_TOOL_NONE ||
            app_brush_tool_hotkey_action(APP_KEY_l, 1, 0) != APP_BRUSH_TOOL_NONE ||
            app_brush_tool_hotkey_action(APP_KEY_o, 0, 1) != APP_BRUSH_TOOL_NONE) {
            fprintf(stderr, "brush tool non-hotkey or modifier rejection failed\n");
            return 0;
        }
        if (app_escape_action(APP_KEY_ESCAPE, 0, 0, 1) != APP_ESCAPE_CANCEL_SHAPE ||
            app_escape_action(APP_KEY_ESCAPE, 0, 0, 0) != APP_ESCAPE_QUIT) {
            fprintf(stderr, "escape action mapping failed\n");
            return 0;
        }
        if (app_escape_action(APP_KEY_b, 0, 0, 0) != APP_ESCAPE_NONE ||
            app_escape_action(APP_KEY_ESCAPE, 1, 0, 0) != APP_ESCAPE_NONE ||
            app_escape_action(APP_KEY_ESCAPE, 0, 1, 0) != APP_ESCAPE_NONE) {
            fprintf(stderr, "escape non-hotkey or modifier rejection failed\n");
            return 0;
        }
    }

    return 1;
}

static int test_app_hotkey_helpers(void) {
    if (!app_hotkey_matches('x', 1, 0, 1, 'x', 1, 0, 1)) {
        fprintf(stderr, "hotkey exact match failed\n");
        return 0;
    }
    if (app_hotkey_matches('x', 0, 0, 1, 'x', 1, 0, 1) ||
        app_hotkey_matches('x', 1, 1, 1, 'x', 1, 0, 1) ||
        app_hotkey_matches('y', 1, 0, 1, 'x', 1, 0, 1)) {
        fprintf(stderr, "hotkey mismatch detection failed\n");
        return 0;
    }
    return 1;
}

static int test_brush_state_helpers(void) {
    uint32_t brush_rgb = 0;
    uint32_t brush_color = 0;
    int brush_opacity = 40;
    int brush_radius = 63;
    Tool tool = TOOL_RECT;
    BrushShape shape = BRUSH_SHAPE_ROUND;

    if (compose_brush_color(0x00112233, 0) != 0x03112233 ||
        compose_brush_color(0x00112233, 50) != 0x80112233 ||
        compose_brush_color(0x00112233, 101) != 0xFF112233) {
        fprintf(stderr, "compose_brush_color clamping failed\n");
        return 0;
    }
    if (strcmp(tool_label(TOOL_FILLED_ELLIPSE), "Filled Ellipse") != 0 ||
        strcmp(brush_shape_label(BRUSH_SHAPE_DIAMOND), "Diamond") != 0) {
        fprintf(stderr, "brush label formatting failed\n");
        return 0;
    }
    if (strcmp(tool_label((Tool)999), "Brush") != 0 ||
        strcmp(brush_shape_label((BrushShape)999), "Round") != 0) {
        fprintf(stderr, "brush label fallback failed\n");
        return 0;
    }

    brush_state_set_color_tool(0x00ABCDEF, brush_opacity, &brush_rgb, &brush_color, &tool, TOOL_BRUSH);
    if (brush_rgb != 0x00ABCDEF || brush_color != compose_brush_color(0x00ABCDEF, brush_opacity) ||
        tool != TOOL_BRUSH) {
        fprintf(stderr, "brush_state_set_color_tool failed\n");
        return 0;
    }
    brush_state_set_color_tool(0x00FFFFFF, brush_opacity, NULL, &brush_color, &tool, TOOL_ERASER);
    if (tool != TOOL_BRUSH) {
        fprintf(stderr, "brush_state_set_color_tool null guard failed\n");
        return 0;
    }

    brush_state_adjust_opacity(70, brush_rgb, &brush_opacity, &brush_color);
    if (brush_opacity != 100 || brush_color != compose_brush_color(brush_rgb, 100)) {
        fprintf(stderr, "brush_state_adjust_opacity upper clamp failed\n");
        return 0;
    }
    brush_state_adjust_opacity(-500, brush_rgb, &brush_opacity, &brush_color);
    if (brush_opacity != 1 || brush_color != compose_brush_color(brush_rgb, 1)) {
        fprintf(stderr, "brush_state_adjust_opacity lower clamp failed\n");
        return 0;
    }
    brush_state_adjust_opacity(10, brush_rgb, NULL, &brush_color);
    if (brush_color != compose_brush_color(brush_rgb, 1)) {
        fprintf(stderr, "brush_state_adjust_opacity null guard failed\n");
        return 0;
    }

    brush_state_adjust_radius(10, &brush_radius);
    if (brush_radius != 64) {
        fprintf(stderr, "brush_state_adjust_radius upper clamp failed\n");
        return 0;
    }
    brush_state_adjust_radius(-100, &brush_radius);
    if (brush_radius != 1) {
        fprintf(stderr, "brush_state_adjust_radius lower clamp failed\n");
        return 0;
    }
    brush_state_set_tool(TOOL_ERASER, &tool);
    if (tool != TOOL_ERASER) {
        fprintf(stderr, "brush_state_set_tool failed\n");
        return 0;
    }
    brush_state_adjust_radius(5, NULL);
    brush_state_set_tool(TOOL_ERASER, NULL);

    if (cycle_brush_shape(BRUSH_SHAPE_ROUND, -1) != BRUSH_SHAPE_DIAMOND ||
        cycle_brush_shape(BRUSH_SHAPE_DIAMOND, 1) != BRUSH_SHAPE_ROUND ||
        cycle_brush_shape((BrushShape)999, 1) != BRUSH_SHAPE_ROUND ||
        cycle_brush_shape((BrushShape)-1, -1) != BRUSH_SHAPE_DIAMOND) {
        fprintf(stderr, "cycle_brush_shape wrap failed\n");
        return 0;
    }
    brush_state_cycle_shape_in_place(&shape, 1);
    if (shape != BRUSH_SHAPE_SQUARE) {
        fprintf(stderr, "brush_state_cycle_shape_in_place failed\n");
        return 0;
    }
    brush_state_cycle_shape_in_place(&shape, -1);
    if (shape != BRUSH_SHAPE_ROUND) {
        fprintf(stderr, "brush_state_cycle_shape_in_place reverse failed\n");
        return 0;
    }
    brush_state_cycle_shape_in_place(NULL, 1);

    return 1;
}

static int test_layer_action_history_helpers(void) {
    LayerStack stack = {0};
    Snapshot undo_stack[4] = {0};
    Snapshot redo_stack[4] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int custom_flip = 0;

    if (!layer_stack_init(&stack, 3, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "layer action history init failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Top", 0x00000000) != 1) {
        fprintf(stderr, "layer action history setup failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.active_layer = 1;
    if (layer_action_history_apply_indexed(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                           4, layer_stack_show, 1)) {
        fprintf(stderr, "indexed no-op should not push history\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_action_history_apply_indexed_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                       4, layer_stack_show, 1) != LAYER_ACTION_HISTORY_UNCHANGED) {
        fprintf(stderr, "indexed no-op result should be unchanged\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (undo_count != 0 || redo_count != 0) {
        fprintf(stderr, "indexed no-op should preserve history counts\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[1].visible = 0;
    if (!layer_action_history_apply_indexed(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                            4, layer_stack_show, 1)) {
        fprintf(stderr, "indexed change should push history\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_action_history_apply_indexed_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                       4, layer_stack_show, 1) != LAYER_ACTION_HISTORY_UNCHANGED) {
        fprintf(stderr, "indexed post-change redundant show should be unchanged\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (undo_count != 1 || redo_count != 0 || !stack.layers[1].visible) {
        fprintf(stderr, "indexed change history bookkeeping failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4)) {
        fprintf(stderr, "indexed change snapshot restore failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (undo_count != 0 || redo_count != 1 || stack.layers[1].visible) {
        fprintf(stderr, "indexed change undo state failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[0].visible = 1;
    stack.layers[1].visible = 1;
    stack.active_layer = 0;
    if (layer_action_history_apply_directional(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                               4, layer_stack_reveal_hidden, 1)) {
        fprintf(stderr, "directional no-op should not push history\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_action_history_apply_directional_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                           4, layer_stack_reveal_hidden, 1) != LAYER_ACTION_HISTORY_UNCHANGED) {
        fprintf(stderr, "directional no-op result should be unchanged\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (undo_count != 0 || redo_count != 1 || stack.active_layer != 0) {
        fprintf(stderr, "directional no-op should preserve history and state\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[1].visible = 0;
    if (!layer_action_history_apply_directional(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                4, layer_stack_reveal_hidden, 1)) {
        fprintf(stderr, "directional change should push history\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (undo_count != 1 || redo_count != 0 || stack.active_layer != 1 || !stack.layers[1].visible) {
        fprintf(stderr, "directional change history bookkeeping failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    custom_flip = 0;
    stack.active_layer = 1;
    if (layer_action_history_apply_custom(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          4, test_layer_action_history_custom_flip, &custom_flip)) {
        fprintf(stderr, "custom no-op should not push history\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_action_history_apply_custom_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                      4, test_layer_action_history_custom_flip, &custom_flip) != LAYER_ACTION_HISTORY_UNCHANGED) {
        fprintf(stderr, "custom no-op result should be unchanged\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (undo_count != 0 || redo_count != 0 || stack.active_layer != 1) {
        fprintf(stderr, "custom no-op should preserve history and state\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_action_history_apply_custom_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                      4, test_layer_action_history_custom_bad_noop, NULL) != LAYER_ACTION_HISTORY_FAILED) {
        fprintf(stderr, "custom bad no-op should fail\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (undo_count != 0 || redo_count != 0 || stack.active_layer != 1) {
        fprintf(stderr, "custom bad no-op should roll back state and preserve history\n");
        layer_stack_free(&stack);
        return 0;
    }

    custom_flip = 1;
    if (layer_action_history_apply_custom_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                      4, test_layer_action_history_custom_flip, &custom_flip) != LAYER_ACTION_HISTORY_CHANGED) {
        fprintf(stderr, "custom change result should be changed\n");
        layer_stack_free(&stack);
        return 0;
    }
    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    stack.active_layer = 1;
    if (!layer_action_history_apply_custom(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                           4, test_layer_action_history_custom_flip, &custom_flip)) {
        fprintf(stderr, "custom change should push history\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (undo_count != 1 || redo_count != 0 || stack.active_layer != 0) {
        fprintf(stderr, "custom change history bookkeeping failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    stack.active_layer = 1;
    if (!layer_action_history_apply_custom(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                           1, test_layer_action_history_custom_flip, &custom_flip)) {
        fprintf(stderr, "custom rollover setup failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 1;
    if (!layer_action_history_apply_custom(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                           1, test_layer_action_history_custom_flip, &custom_flip)) {
        fprintf(stderr, "custom rollover second change failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (undo_count != 1 || redo_count != 0 || stack.active_layer != 0) {
        fprintf(stderr, "custom rollover bookkeeping failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 1)) {
        fprintf(stderr, "custom rollover restore failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.active_layer != 1 || undo_count != 0 || redo_count != 1) {
        fprintf(stderr, "custom rollover should keep only the newest prior state\n");
        layer_stack_free(&stack);
        return 0;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    layer_stack_free(&stack);
    return 1;
}

static int test_color_sample_helpers(void) {
    Canvas canvas = {0};
    uint32_t brush_rgb = 0;
    uint32_t brush_color = 0;
    int brush_opacity = 0;
    Tool tool = TOOL_RECT;

    if (!canvas_init(&canvas, 2, 2)) {
        fprintf(stderr, "sample canvas init failed\n");
        return 0;
    }
    canvas_clear(&canvas, 0x00000000);
    canvas_set_pixel_raw(&canvas, 1, 1, 0x80445566);

    if (!sample_canvas_brush_state(&canvas, 1, 1, &brush_rgb, &brush_color, &brush_opacity, &tool)) {
        fprintf(stderr, "sample_canvas_brush_state basic sample failed\n");
        canvas_free(&canvas);
        return 0;
    }
    if (brush_rgb != 0x00445566 || brush_opacity != 50 ||
        brush_color != compose_brush_color(brush_rgb, brush_opacity) || tool != TOOL_BRUSH) {
        fprintf(stderr, "sample_canvas_brush_state basic output failed\n");
        canvas_free(&canvas);
        return 0;
    }

    tool = TOOL_RECT;
    if (sample_canvas_brush_state(&canvas, -1, 0, &brush_rgb, &brush_color, &brush_opacity, &tool)) {
        fprintf(stderr, "sample_canvas_brush_state bounds check failed\n");
        canvas_free(&canvas);
        return 0;
    }
    if (sample_canvas_brush_state(&canvas, 2, 1, &brush_rgb, &brush_color, &brush_opacity, &tool) ||
        sample_canvas_brush_state(&canvas, 1, 2, &brush_rgb, &brush_color, &brush_opacity, &tool) ||
        tool != TOOL_RECT) {
        fprintf(stderr, "sample_canvas_brush_state upper bounds failed\n");
        canvas_free(&canvas);
        return 0;
    }

    if (sample_canvas_brush_state(NULL, 0, 0, &brush_rgb, &brush_color, &brush_opacity, &tool) ||
        sample_canvas_brush_state(&canvas, 0, 0, NULL, &brush_color, &brush_opacity, &tool) ||
        sample_canvas_brush_state(&canvas, 0, 0, &brush_rgb, NULL, &brush_opacity, &tool) ||
        sample_canvas_brush_state(&canvas, 0, 0, &brush_rgb, &brush_color, NULL, &tool) ||
        sample_canvas_brush_state(&canvas, 0, 0, &brush_rgb, &brush_color, &brush_opacity, NULL)) {
        fprintf(stderr, "sample_canvas_brush_state null guard failed\n");
        canvas_free(&canvas);
        return 0;
    }

    canvas_set_pixel_raw(&canvas, 0, 0, 0x00443322);
    if (!sample_canvas_brush_state(&canvas, 0, 0, &brush_rgb, &brush_color, &brush_opacity, &tool) ||
        brush_opacity != 1 || brush_color != compose_brush_color(brush_rgb, 1)) {
        fprintf(stderr, "sample_canvas_brush_state alpha clamp failed\n");
        canvas_free(&canvas);
        return 0;
    }

    canvas_set_pixel_raw(&canvas, 0, 1, 0xFF123456);
    if (!sample_canvas_brush_state(&canvas, 0, 1, &brush_rgb, &brush_color, &brush_opacity, &tool) ||
        brush_rgb != 0x00123456 || brush_opacity != 100 ||
        brush_color != compose_brush_color(brush_rgb, 100)) {
        fprintf(stderr, "sample_canvas_brush_state opaque sample failed\n");
        canvas_free(&canvas);
        return 0;
    }

    canvas_free(&canvas);
    return 1;
}

static int test_geometry_helpers(void) {
    int out_x = 0;
    int out_y = 0;
    int guard_x = 7;
    int guard_y = 9;

    if (!brush_mask_contains(BRUSH_SHAPE_ROUND, 2, 0, 2) ||
        brush_mask_contains(BRUSH_SHAPE_ROUND, 3, 3, 2)) {
        fprintf(stderr, "brush_mask_contains round failed\n");
        return 0;
    }
    if (!brush_mask_contains(BRUSH_SHAPE_SQUARE, 2, -2, 2) ||
        brush_mask_contains(BRUSH_SHAPE_SQUARE, 3, 0, 2)) {
        fprintf(stderr, "brush_mask_contains square failed\n");
        return 0;
    }
    if (!brush_mask_contains(BRUSH_SHAPE_DIAMOND, 1, 1, 2) ||
        brush_mask_contains(BRUSH_SHAPE_DIAMOND, 2, 2, 2)) {
        fprintf(stderr, "brush_mask_contains diamond failed\n");
        return 0;
    }
    if (!brush_mask_contains((BrushShape)999, 0, 2, 2) ||
        brush_mask_contains((BrushShape)999, 3, 0, 2)) {
        fprintf(stderr, "brush_mask_contains fallback failed\n");
        return 0;
    }

    constrain_shape_end(TOOL_LINE, 10, 10, 16, 11, 1, &out_x, &out_y);
    if (out_x != 16 || out_y != 10) {
        fprintf(stderr, "constrain_shape_end horizontal line failed\n");
        return 0;
    }
    constrain_shape_end(TOOL_LINE, 10, 10, 11, 16, 1, &out_x, &out_y);
    if (out_x != 10 || out_y != 16) {
        fprintf(stderr, "constrain_shape_end vertical line failed\n");
        return 0;
    }
    constrain_shape_end(TOOL_LINE, 10, 10, 13, 14, 1, &out_x, &out_y);
    if (out_x != 14 || out_y != 14) {
        fprintf(stderr, "constrain_shape_end diagonal line failed\n");
        return 0;
    }
    constrain_shape_end(TOOL_RECT, 10, 10, 12, 15, 1, &out_x, &out_y);
    if (out_x != 15 || out_y != 15) {
        fprintf(stderr, "constrain_shape_end square shape failed\n");
        return 0;
    }
    constrain_shape_end(TOOL_RECT, 10, 10, 12, 15, 0, &out_x, &out_y);
    if (out_x != 12 || out_y != 15) {
        fprintf(stderr, "constrain_shape_end shift-off passthrough failed\n");
        return 0;
    }
    constrain_shape_end(TOOL_BRUSH, 10, 10, 4, 3, 1, &out_x, &out_y);
    if (out_x != 4 || out_y != 3) {
        fprintf(stderr, "constrain_shape_end non-shape passthrough failed\n");
        return 0;
    }
    constrain_shape_end(TOOL_LINE, 1, 2, 3, 4, 1, NULL, &guard_y);
    constrain_shape_end(TOOL_LINE, 1, 2, 3, 4, 1, &guard_x, NULL);
    if (guard_x != 7 || guard_y != 9) {
        fprintf(stderr, "constrain_shape_end null output guard failed\n");
        return 0;
    }

    return 1;
}

static int test_layer_edit_state_helpers(void) {
    LayerStack stack;

    if (active_layer_clear_color(NULL, 0xFFAABBCC) != 0xFFAABBCC) {
        fprintf(stderr, "active_layer_clear_color null fallback failed\n");
        return 0;
    }
    if (active_layer_editable(NULL)) {
        fprintf(stderr, "active_layer_editable null guard failed\n");
        return 0;
    }

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init for edit state failed\n");
        return 0;
    }

    if (active_layer_clear_color(&stack, 0xFFFFFFFF) != 0xFFFFFFFF) {
        fprintf(stderr, "active_layer_clear_color background layer failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!active_layer_editable(&stack)) {
        fprintf(stderr, "active_layer_editable unlocked base failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    if (layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "layer_stack_add for edit state failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (active_layer_clear_color(&stack, 0xFFFFFFFF) != 0x00000000) {
        fprintf(stderr, "active_layer_clear_color transparent layer failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[stack.active_layer].locked = 1;
    if (active_layer_editable(&stack)) {
        fprintf(stderr, "active_layer_editable locked layer failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[stack.active_layer].locked = 0;
    free(stack.layers[stack.active_layer].canvas.pixels);
    stack.layers[stack.active_layer].canvas.pixels = NULL;
    if (active_layer_editable(&stack)) {
        fprintf(stderr, "active_layer_editable null pixels failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 99;
    if (active_layer_editable(&stack)) {
        fprintf(stderr, "active_layer_editable out of range failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_layer_selection_helpers(void) {
    LayerStack stack;

    if (layer_selection_try_select_index(NULL, 0)) {
        fprintf(stderr, "layer_selection_try_select_index null guard failed\n");
        return 0;
    }
    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init for selection failed\n");
        return 0;
    }
    if (layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "layer_stack_add for selection failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_selection_try_select_index(&stack, 0) || stack.active_layer != 0) {
        fprintf(stderr, "layer_selection_try_select_index basic select failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_selection_try_select_index(&stack, -1) ||
        layer_selection_try_select_index(&stack, stack.layer_count) ||
        stack.active_layer != 0) {
        fprintf(stderr, "layer_selection_try_select_index bounds guard failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_selection_try_select_index(&stack, 1) || stack.active_layer != 1) {
        fprintf(stderr, "layer_selection_try_select_index second select failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_selection_try_select_index(&stack, 1) || stack.active_layer != 1) {
        fprintf(stderr, "layer_selection_try_select_index same index should be unchanged\n");
        layer_stack_free(&stack);
        return 0;
    }

    layer_stack_free(&stack);
    return 1;
}

static int test_layer_creation_helpers(void) {
    LayerStack stack;
    Snapshot undo_stack[MAX_LAYERS] = {0};
    Snapshot redo_stack[MAX_LAYERS] = {0};
    int undo_count = 0;
    int redo_count = 0;

    if (layer_creation_try_add(NULL, undo_stack, &undo_count, redo_stack, &redo_count, 0x00000000, 4) ||
        layer_creation_try_add(&stack, NULL, &undo_count, redo_stack, &redo_count, 0x00000000, 4) ||
        layer_creation_try_add(&stack, undo_stack, NULL, redo_stack, &redo_count, 0x00000000, 4) ||
        layer_creation_try_add(&stack, undo_stack, &undo_count, NULL, &redo_count, 0x00000000, 4) ||
        layer_creation_try_add(&stack, undo_stack, &undo_count, redo_stack, NULL, 0x00000000, 4)) {
        fprintf(stderr, "layer_creation_try_add null guard failed\n");
        return 0;
    }

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init for creation failed\n");
        return 0;
    }

    if (!layer_creation_try_add(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0x00000000, 4) ||
        stack.layer_count != 2 || undo_count != 1 || redo_count != 0) {
        fprintf(stderr, "layer_creation_try_add basic add failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    {
        int width_before = stack.width;
        int undo_before = undo_count;
        int redo_before = redo_count;
        int layers_before = stack.layer_count;

        stack.width = 0;
        if (layer_creation_try_add(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0x00000000, 4) ||
            undo_count != undo_before || redo_count != redo_before || stack.layer_count != layers_before) {
            fprintf(stderr, "layer_creation_try_add should not mutate history on add failure\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        stack.width = width_before;
    }
    if (layer_creation_try_add(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0x00000000, 0) ||
        stack.layer_count != 2) {
        fprintf(stderr, "layer_creation_try_add max history guard failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (canvas_get_pixel(&stack.layers[1].canvas, 0, 0) != 0x00000000) {
        fprintf(stderr, "layer_creation_try_add clear color failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (!snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count, MAX_LAYERS) ||
        redo_count != 1) {
        fprintf(stderr, "layer_creation_try_add restore setup failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_creation_try_add(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0x00000000, 4) ||
        redo_count != 0) {
        fprintf(stderr, "layer_creation_try_add should clear redo stack\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    while (stack.layer_count < MAX_LAYERS) {
        if (!layer_creation_try_add(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0x00000000, MAX_LAYERS)) {
            fprintf(stderr, "layer_creation_try_add fill to max failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;

        if (layer_creation_try_add(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0x00000000, MAX_LAYERS)) {
            fprintf(stderr, "layer_creation_try_add max layer guard failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        if (undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "layer_creation_try_add max layer should not mutate history\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    if (undo_count > MAX_LAYERS) {
        fprintf(stderr, "layer_creation_try_add max layer guard failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (undo_count > MAX_LAYERS) {
        fprintf(stderr, "layer_creation_try_add history bounds failed\n");
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

static int test_shape_preview_state_helpers(void) {
    LayerStack stack;
    Canvas composite = {0};
    uint32_t base_pixels[4] = {0};
    int shaping = 0;
    int preview_active = 1;
    int start_x = -1;
    int start_y = -1;

    shape_preview_cancel(&shaping, &preview_active);
    if (shaping != 0 || preview_active != 0) {
        fprintf(stderr, "shape_preview_cancel failed\n");
        return 0;
    }
    shaping = 9;
    shape_preview_cancel(&shaping, NULL);
    if (shaping != 0) {
        fprintf(stderr, "shape_preview_cancel partial null failed\n");
        return 0;
    }
    shape_preview_cancel(NULL, NULL);

    if (!layer_stack_init(&stack, 2, 2, 0xFFFFFFFF)) {
        fprintf(stderr, "layer_stack_init for shape preview failed\n");
        return 0;
    }
    if (!canvas_init(&composite, 2, 2)) {
        fprintf(stderr, "composite init for shape preview failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    composite.pixels[0] = 0xFF010203;
    composite.pixels[1] = 0xFF040506;
    composite.pixels[2] = 0xFF070809;
    composite.pixels[3] = 0xFF0A0B0C;

    if (!shape_preview_begin_if_editable(&stack, 3, 4, &composite, base_pixels, &shaping, &start_x, &start_y) ||
        !shaping || start_x != 3 || start_y != 4 ||
        base_pixels[0] != 0xFF010203 || base_pixels[3] != 0xFF0A0B0C) {
        fprintf(stderr, "shape_preview_begin_if_editable basic start failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[stack.active_layer].locked = 1;
    shaping = 0;
    if (shape_preview_begin_if_editable(&stack, 1, 1, &composite, base_pixels, &shaping, &start_x, &start_y) || shaping) {
        fprintf(stderr, "shape_preview_begin_if_editable locked guard failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[stack.active_layer].locked = 0;

    shaping = 0;
    if (shape_preview_begin_if_editable(NULL, 1, 1, &composite, base_pixels, &shaping, &start_x, &start_y) || shaping) {
        fprintf(stderr, "shape_preview_begin_if_editable null layer guard failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    if (shape_preview_begin_if_editable(&stack, 1, 1, &composite, base_pixels, NULL, &start_x, &start_y) ||
        shape_preview_begin_if_editable(&stack, 1, 1, &composite, base_pixels, &shaping, NULL, &start_y) ||
        shape_preview_begin_if_editable(&stack, 1, 1, &composite, base_pixels, &shaping, &start_x, NULL)) {
        fprintf(stderr, "shape_preview_begin_if_editable null guard failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    shaping = 0;
    memset(base_pixels, 0, sizeof(base_pixels));
    if (!shape_preview_begin_if_editable(&stack, 5, 6, NULL, base_pixels, &shaping, &start_x, &start_y) ||
        !shaping || start_x != 5 || start_y != 6 ||
        base_pixels[0] != 0 || base_pixels[3] != 0) {
        fprintf(stderr, "shape_preview_begin_if_editable null composite failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    memset(base_pixels, 0xAB, sizeof(base_pixels));
    shaping = 0;
    composite.pixels = NULL;
    if (!shape_preview_begin_if_editable(&stack, 7, 8, &composite, base_pixels, &shaping, &start_x, &start_y) ||
        !shaping || start_x != 7 || start_y != 8 ||
        base_pixels[0] != 0xABABABABu || base_pixels[3] != 0xABABABABu) {
        fprintf(stderr, "shape_preview_begin_if_editable null pixel source failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_free(&stack.layers[stack.active_layer].canvas);
    shaping = 0;
    if (shape_preview_begin_if_editable(&stack, 9, 10, &composite, base_pixels, &shaping, &start_x, &start_y) || shaping) {
        fprintf(stderr, "shape_preview_begin_if_editable active null pixels failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_free(&composite);
    layer_stack_free(&stack);
    return 1;
}

static int test_brush_render_helpers(void) {
    Canvas canvas = {0};
    Canvas blank = {0};
    Canvas guard = {0};

    if (!canvas_init(&canvas, 7, 7)) {
        fprintf(stderr, "brush render canvas init failed\n");
        return 0;
    }
    if (!canvas_init(&blank, 7, 7)) {
        fprintf(stderr, "brush render blank canvas init failed\n");
        canvas_free(&canvas);
        return 0;
    }
    canvas_clear(&canvas, 0x00000000);
    canvas_clear(&blank, 0x00000000);
    guard.width = 1;
    guard.height = 1;
    guard.pixels = (uint32_t[]){0xAABBCCDDu};

    stamp_brush(NULL, 0, 0, 1, 0xFFFFFFFF, BRUSH_SHAPE_ROUND);
    erase_stamp(NULL, 0, 0, 1, 0x00000000, BRUSH_SHAPE_ROUND);
    draw_brush_line(NULL, 0, 0, 1, 1, 1, 0xFFFFFFFF, BRUSH_SHAPE_ROUND);
    erase_line(NULL, 0, 0, 1, 1, 1, 0x00000000, BRUSH_SHAPE_ROUND);
    stamp_brush(&guard, 0, 0, 0, 0xFFFFFFFF, BRUSH_SHAPE_ROUND);
    erase_stamp(&guard, 0, 0, 0, 0x00000000, BRUSH_SHAPE_ROUND);
    if (!expect_pixel_eq("brush_render_guard_pixel", guard.pixels[0], 0xAABBCCDDu)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    stamp_brush(&canvas, 3, 3, 1, 0xFF112233, BRUSH_SHAPE_DIAMOND);
    if (!expect_pixel_eq("stamp_center", canvas_get_pixel(&canvas, 3, 3), 0xFF112233) ||
        !expect_pixel_eq("stamp_diamond_tip", canvas_get_pixel(&canvas, 3, 2), 0xFF112233) ||
        !expect_pixel_eq("stamp_diamond_corner", canvas_get_pixel(&canvas, 2, 2), 0x00000000)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    draw_brush_line(&canvas, 1, 1, 5, 1, 1, 0xFF445566, BRUSH_SHAPE_SQUARE);
    if (!expect_pixel_eq("line_mid", canvas_get_pixel(&canvas, 3, 1), 0xFF445566) ||
        !expect_pixel_eq("line_thickness", canvas_get_pixel(&canvas, 3, 2), 0xFF445566)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    stamp_brush(&blank, 3, 3, 0, 0xFFFFFFFF, BRUSH_SHAPE_ROUND);
    if (!expect_pixel_eq("stamp_zero_radius", canvas_get_pixel(&blank, 3, 3), 0x00000000)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    draw_brush_line(&blank, 0, 0, 6, 6, 1, 0xFF778899, BRUSH_SHAPE_ROUND);
    if (!expect_pixel_eq("line_diagonal_mid", canvas_get_pixel(&blank, 3, 3), 0xFF778899)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    erase_stamp(&canvas, 3, 3, 1, 0x00000000, BRUSH_SHAPE_DIAMOND);
    if (!expect_pixel_eq("erase_center", canvas_get_pixel(&canvas, 3, 3), 0x00000000)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    erase_line(&canvas, 1, 1, 5, 1, 1, 0x00000000, BRUSH_SHAPE_SQUARE);
    if (!expect_pixel_eq("erase_line_mid", canvas_get_pixel(&canvas, 3, 1), 0x00000000)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    erase_line(&blank, 6, 0, 0, 6, 1, 0x00000000, BRUSH_SHAPE_ROUND);
    if (!expect_pixel_eq("erase_diagonal_mid", canvas_get_pixel(&blank, 3, 3), 0x00000000)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    canvas_free(&canvas);
    canvas_free(&blank);
    return 1;
}

static int test_shape_draw_helpers(void) {
    Canvas canvas = {0};
    Canvas blank = {0};
    Canvas guard = {1, 1, (uint32_t[]){0x12345678u}};

    if (!canvas_init(&canvas, 9, 9)) {
        fprintf(stderr, "shape draw canvas init failed\n");
        return 0;
    }
    if (!canvas_init(&blank, 9, 9)) {
        fprintf(stderr, "shape draw blank canvas init failed\n");
        canvas_free(&canvas);
        return 0;
    }
    canvas_clear(&canvas, 0x00000000);
    canvas_clear(&blank, 0x00000000);

    draw_shape(NULL, TOOL_LINE, 0, 0, 1, 1, 1, 0xFFFFFFFF);
    draw_shape(&guard, (Tool)999, 0, 0, 0, 0, 1, 0xFFFFFFFF);
    if (!expect_pixel_eq("shape_draw_default_guard", guard.pixels[0], 0x12345678u)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    draw_shape(&canvas, TOOL_FILLED_RECT, 2, 2, 5, 5, 1, 0xFF778899);
    if (!expect_pixel_eq("shape_filled_rect_center", canvas_get_pixel(&canvas, 3, 3), 0xFF778899)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    draw_shape(&canvas, TOOL_RECT, 1, 1, 7, 7, 1, 0xFFAA5500);
    if (!expect_pixel_eq("shape_rect_outline_edge", canvas_get_pixel(&canvas, 1, 4), 0xFFAA5500) ||
        !expect_pixel_eq("shape_rect_outline_interior", canvas_get_pixel(&canvas, 4, 4), 0xFF778899)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    draw_shape(&canvas, TOOL_LINE, 0, 0, 8, 8, 1, 0xFF112233);
    if (!expect_pixel_eq("shape_line_mid", canvas_get_pixel(&canvas, 4, 4), 0xFF112233)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    draw_shape(&canvas, TOOL_FILLED_ELLIPSE, 2, 2, 6, 6, 1, 0xFF445566);
    if (!expect_pixel_eq("shape_filled_ellipse_center", canvas_get_pixel(&canvas, 4, 4), 0xFF445566)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    draw_shape(&blank, TOOL_ELLIPSE, 1, 1, 7, 7, 1, 0xFFABCDEF);
    if (!expect_pixel_eq("shape_ellipse_outline_edge", canvas_get_pixel(&blank, 4, 1), 0xFFABCDEF) ||
        !expect_pixel_eq("shape_ellipse_outline_center", canvas_get_pixel(&blank, 4, 4), 0x00000000)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    draw_shape(&blank, TOOL_BRUSH, 0, 0, 8, 8, 1, 0xFFFFFFFF);
    if (!expect_pixel_eq("shape_default_noop", canvas_get_pixel(&blank, 0, 0), 0x00000000)) {
        canvas_free(&canvas);
        canvas_free(&blank);
        return 0;
    }

    canvas_free(&canvas);
    canvas_free(&blank);
    return 1;
}

static int test_snapshot_history_helpers(void) {
    LayerStack stack;
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    Snapshot snap = {0};
    Snapshot guard_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int guard_count = 1;

    guard_stack[0].pixels = (uint32_t *)malloc(sizeof(uint32_t));
    if (!guard_stack[0].pixels) {
        fprintf(stderr, "snapshot guard allocation failed\n");
        return 0;
    }
    guard_stack[0].pixels[0] = 0xDEADBEEFu;
    snapshot_free(NULL);
    snapshot_stack_clear(NULL, &guard_count);
    snapshot_stack_clear(guard_stack, NULL);
    if (!guard_stack[0].pixels || guard_stack[0].pixels[0] != 0xDEADBEEFu || guard_count != 1) {
        fprintf(stderr, "snapshot free/clear guard checks failed\n");
        free(guard_stack[0].pixels);
        return 0;
    }
    snapshot_stack_clear(guard_stack, &guard_count);
    if (guard_count != 0 || guard_stack[0].pixels != NULL) {
        fprintf(stderr, "snapshot_stack_clear basic clear failed\n");
        return 0;
    }

    if (!layer_stack_init(&stack, 3, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "snapshot test stack init failed\n");
        return 0;
    }
    snapshot_push(NULL, undo_stack, &undo_count, redo_stack, &redo_count, 2);
    snapshot_push(&stack, NULL, &undo_count, redo_stack, &redo_count, 2);
    snapshot_push(&stack, undo_stack, NULL, redo_stack, &redo_count, 2);
    snapshot_push(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0);
    if (undo_count != 0 || redo_count != 0) {
        fprintf(stderr, "snapshot_push guard checks failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (snapshot_from_layers(NULL, &stack) || snapshot_from_layers(&snap, NULL) ||
        snapshot_apply(NULL, &stack) || snapshot_apply(&snap, NULL) ||
        snapshot_restore(NULL, undo_stack, &undo_count, redo_stack, &redo_count, 2) ||
        snapshot_restore(&stack, NULL, &undo_count, redo_stack, &redo_count, 2) ||
        snapshot_restore(&stack, undo_stack, NULL, redo_stack, &redo_count, 2) ||
        snapshot_restore(&stack, undo_stack, &undo_count, NULL, &redo_count, 2) ||
        snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, NULL, 2) ||
        snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0)) {
        fprintf(stderr, "snapshot guard checks failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_add(&stack, "Top", 0x00000000) < 0) {
        fprintf(stderr, "snapshot test add layer failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[0].locked = 1;
    stack.layers[1].visible = 0;
    canvas_set_pixel_raw(&stack.layers[1].canvas, 1, 1, 0xFF112233);
    if (!snapshot_from_layers(&snap, &stack)) {
        fprintf(stderr, "snapshot_from_layers failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel_raw(&stack.layers[1].canvas, 1, 1, 0x00000000);
    stack.layers[0].locked = 0;
    stack.layers[1].visible = 1;
    if (!snapshot_apply(&snap, &stack) ||
        !expect_pixel_eq("snapshot_apply_pixel", canvas_get_pixel(&stack.layers[1].canvas, 1, 1), 0xFF112233) ||
        !stack.layers[0].locked || stack.layers[1].visible) {
        fprintf(stderr, "snapshot_apply restore failed\n");
        snapshot_free(&snap);
        layer_stack_free(&stack);
        return 0;
    }
    snapshot_free(&snap);

    if (snapshot_apply(&snap, &stack)) {
        fprintf(stderr, "snapshot_apply freed snapshot failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    if (!snapshot_from_layers(&snap, &stack)) {
        fprintf(stderr, "snapshot_apply rollback setup failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    snap.pixels[0] = 0xFF445566;
    canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
    canvas_free(&stack.layers[1].canvas);
    stack.layers[1].locked = 1;
    if (snapshot_apply(&snap, &stack) ||
        !expect_pixel_eq("snapshot_apply_atomic_rollback", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
        stack.layers[1].canvas.pixels != NULL || !stack.layers[1].locked) {
        fprintf(stderr, "snapshot_apply should roll back partial restore failures\n");
        snapshot_free(&snap);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[1].locked = 0;
    if (!layer_stack_clear_layer(&stack, 1, 0x00000000)) {
        fprintf(stderr, "snapshot_apply rollback cleanup failed\n");
        snapshot_free(&snap);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_set_pixel_raw(&stack.layers[1].canvas, 1, 1, 0xFF112233);
    snapshot_free(&snap);

    snapshot_push(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 2);
    if (undo_count != 1) {
        fprintf(stderr, "snapshot_push count failed\n");
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel_raw(&stack.layers[1].canvas, 1, 1, 0xFF556677);
    if (!snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 2) ||
        undo_count != 0 || redo_count != 1 ||
        !expect_pixel_eq("snapshot_restore_pixel", canvas_get_pixel(&stack.layers[1].canvas, 1, 1), 0xFF112233)) {
        fprintf(stderr, "snapshot_restore failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    if (!snapshot_from_layers(&undo_stack[0], &stack)) {
        fprintf(stderr, "snapshot invalid restore setup failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    undo_stack[0].width++;
    undo_count = 1;
    if (snapshot_restore(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 2) ||
        undo_count != 1 || redo_count != 0) {
        fprintf(stderr, "snapshot_restore should fail without consuming invalid history\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    canvas_set_pixel_raw(&stack.layers[1].canvas, 0, 0, 0xFF000001);
    snapshot_push(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 2);
    canvas_set_pixel_raw(&stack.layers[1].canvas, 0, 1, 0xFF000002);
    snapshot_push(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 2);
    canvas_set_pixel_raw(&stack.layers[1].canvas, 0, 2, 0xFF000003);
    snapshot_push(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 2);
    if (undo_count != 2 || redo_count != 0) {
        fprintf(stderr, "snapshot_push max history failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    {
        Snapshot bad = {0};
        bad.width = stack.width + 1;
        bad.height = stack.height;
        bad.layer_count = stack.layer_count;
        bad.pixels = (uint32_t *)calloc((size_t)stack.width * (size_t)stack.height * (size_t)stack.layer_count,
                                        sizeof(uint32_t));
        if (!bad.pixels || snapshot_apply(&bad, &stack)) {
            fprintf(stderr, "snapshot_apply dimension mismatch failed\n");
            free(bad.pixels);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        free(bad.pixels);
    }
    {
        Snapshot bad = {0};
        bad.width = stack.width;
        bad.height = stack.height;
        bad.layer_count = 0;
        bad.pixels = (uint32_t *)calloc((size_t)stack.width * (size_t)stack.height, sizeof(uint32_t));
        if (!bad.pixels || snapshot_apply(&bad, &stack)) {
            fprintf(stderr, "snapshot_apply zero layer count failed\n");
            free(bad.pixels);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        free(bad.pixels);
    }
    {
        Snapshot bad = {0};
        bad.width = stack.width;
        bad.height = stack.height;
        bad.layer_count = MAX_LAYERS + 1;
        bad.pixels = (uint32_t *)calloc((size_t)stack.width * (size_t)stack.height, sizeof(uint32_t));
        if (!bad.pixels || snapshot_apply(&bad, &stack)) {
            fprintf(stderr, "snapshot_apply oversized layer count failed\n");
            free(bad.pixels);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        free(bad.pixels);
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    layer_stack_free(&stack);
    return 1;
}

static int test_active_layer_ops_helpers(void) {
    LayerStack stack;
    LayerStack narrow_stack;
    LayerStack short_stack;
    LayerStack single_stack;
    Snapshot undo_stack[4] = {0};
    Snapshot redo_stack[4] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int last_x = 0;
    int last_y = 0;

    if (active_layer_try_clear(NULL, undo_stack, &undo_count, redo_stack, &redo_count, 0xFFFFFFFF, 4) ||
        active_layer_try_clear(&stack, NULL, &undo_count, redo_stack, &redo_count, 0xFFFFFFFF, 4) ||
        active_layer_try_clear(&stack, undo_stack, NULL, redo_stack, &redo_count, 0xFFFFFFFF, 4) ||
        active_layer_try_clear(&stack, undo_stack, &undo_count, NULL, &redo_count, 0xFFFFFFFF, 4) ||
        active_layer_try_clear(&stack, undo_stack, &undo_count, redo_stack, NULL, 0xFFFFFFFF, 4)) {
        fprintf(stderr, "active_layer_try_clear null guard failed\n");
        return 0;
    }
    if (active_layer_try_flip_horizontal(NULL, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
        active_layer_try_flip_horizontal(&stack, NULL, &undo_count, redo_stack, &redo_count, 4) ||
        active_layer_try_flip_horizontal(&stack, undo_stack, NULL, redo_stack, &redo_count, 4) ||
        active_layer_try_flip_horizontal(&stack, undo_stack, &undo_count, NULL, &redo_count, 4) ||
        active_layer_try_flip_horizontal(&stack, undo_stack, &undo_count, redo_stack, NULL, 4)) {
        fprintf(stderr, "active_layer_try_flip_horizontal null guard failed\n");
        return 0;
    }
    if (active_layer_try_flip_vertical(NULL, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
        active_layer_try_flip_vertical(&stack, NULL, &undo_count, redo_stack, &redo_count, 4) ||
        active_layer_try_flip_vertical(&stack, undo_stack, NULL, redo_stack, &redo_count, 4) ||
        active_layer_try_flip_vertical(&stack, undo_stack, &undo_count, NULL, &redo_count, 4) ||
        active_layer_try_flip_vertical(&stack, undo_stack, &undo_count, redo_stack, NULL, 4)) {
        fprintf(stderr, "active_layer_try_flip_vertical null guard failed\n");
        return 0;
    }
    if (active_layer_try_rotate_180(NULL, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
        active_layer_try_rotate_180(&stack, NULL, &undo_count, redo_stack, &redo_count, 4) ||
        active_layer_try_rotate_180(&stack, undo_stack, NULL, redo_stack, &redo_count, 4) ||
        active_layer_try_rotate_180(&stack, undo_stack, &undo_count, NULL, &redo_count, 4) ||
        active_layer_try_rotate_180(&stack, undo_stack, &undo_count, redo_stack, NULL, 4)) {
        fprintf(stderr, "active_layer_try_rotate_180 null guard failed\n");
        return 0;
    }
    if (active_layer_try_invert_rgb(NULL, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
        active_layer_try_invert_rgb(&stack, NULL, &undo_count, redo_stack, &redo_count, 4) ||
        active_layer_try_invert_rgb(&stack, undo_stack, NULL, redo_stack, &redo_count, 4) ||
        active_layer_try_invert_rgb(&stack, undo_stack, &undo_count, NULL, &redo_count, 4) ||
        active_layer_try_invert_rgb(&stack, undo_stack, &undo_count, redo_stack, NULL, 4)) {
        fprintf(stderr, "active_layer_try_invert_rgb null guard failed\n");
        return 0;
    }
    if (active_layer_try_adjust_opacity(NULL, undo_stack, &undo_count, redo_stack, &redo_count, 50, 4) ||
        active_layer_try_adjust_opacity(&stack, NULL, &undo_count, redo_stack, &redo_count, 50, 4) ||
        active_layer_try_adjust_opacity(&stack, undo_stack, NULL, redo_stack, &redo_count, 50, 4) ||
        active_layer_try_adjust_opacity(&stack, undo_stack, &undo_count, NULL, &redo_count, 50, 4) ||
        active_layer_try_adjust_opacity(&stack, undo_stack, &undo_count, redo_stack, NULL, 50, 4)) {
        fprintf(stderr, "active_layer_try_adjust_opacity null guard failed\n");
        return 0;
    }
    if (active_layer_try_nudge_opacity(NULL, undo_stack, &undo_count, redo_stack, &redo_count, 10, 4) ||
        active_layer_try_nudge_opacity(&stack, NULL, &undo_count, redo_stack, &redo_count, 10, 4) ||
        active_layer_try_nudge_opacity(&stack, undo_stack, NULL, redo_stack, &redo_count, 10, 4) ||
        active_layer_try_nudge_opacity(&stack, undo_stack, &undo_count, NULL, &redo_count, 10, 4) ||
        active_layer_try_nudge_opacity(&stack, undo_stack, &undo_count, redo_stack, NULL, 10, 4)) {
        fprintf(stderr, "active_layer_try_nudge_opacity null guard failed\n");
        return 0;
    }
    if (active_layer_try_flood_fill(NULL, undo_stack, &undo_count, redo_stack, &redo_count, 0, 0, 0xFFFFFFFF, 4) ||
        active_layer_try_flood_fill(&stack, NULL, &undo_count, redo_stack, &redo_count, 0, 0, 0xFFFFFFFF, 4) ||
        active_layer_try_flood_fill(&stack, undo_stack, NULL, redo_stack, &redo_count, 0, 0, 0xFFFFFFFF, 4) ||
        active_layer_try_flood_fill(&stack, undo_stack, &undo_count, NULL, &redo_count, 0, 0, 0xFFFFFFFF, 4) ||
        active_layer_try_flood_fill(&stack, undo_stack, &undo_count, redo_stack, NULL, 0, 0, 0xFFFFFFFF, 4)) {
        fprintf(stderr, "active_layer_try_flood_fill null guard failed\n");
        return 0;
    }
    if (active_layer_try_commit_shape(NULL, undo_stack, &undo_count, redo_stack, &redo_count,
                                      TOOL_LINE, 0, 0, 1, 1, 1, 0xFFFFFFFF, 4) ||
        active_layer_try_commit_shape(&stack, NULL, &undo_count, redo_stack, &redo_count,
                                      TOOL_LINE, 0, 0, 1, 1, 1, 0xFFFFFFFF, 4) ||
        active_layer_try_commit_shape(&stack, undo_stack, NULL, redo_stack, &redo_count,
                                      TOOL_LINE, 0, 0, 1, 1, 1, 0xFFFFFFFF, 4) ||
        active_layer_try_commit_shape(&stack, undo_stack, &undo_count, NULL, &redo_count,
                                      TOOL_LINE, 0, 0, 1, 1, 1, 0xFFFFFFFF, 4) ||
        active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, NULL,
                                      TOOL_LINE, 0, 0, 1, 1, 1, 0xFFFFFFFF, 4)) {
        fprintf(stderr, "active_layer_try_commit_shape null guard failed\n");
        return 0;
    }
    if (active_layer_try_begin_brush_stroke(NULL, undo_stack, &undo_count, redo_stack, &redo_count,
                                            TOOL_BRUSH, 0, 0, 1, 0xFFFFFFFF, BRUSH_SHAPE_ROUND,
                                            0xFFFFFFFF, 4) ||
        active_layer_try_begin_brush_stroke(&stack, NULL, &undo_count, redo_stack, &redo_count,
                                            TOOL_BRUSH, 0, 0, 1, 0xFFFFFFFF, BRUSH_SHAPE_ROUND,
                                            0xFFFFFFFF, 4) ||
        active_layer_try_begin_brush_stroke(&stack, undo_stack, NULL, redo_stack, &redo_count,
                                            TOOL_BRUSH, 0, 0, 1, 0xFFFFFFFF, BRUSH_SHAPE_ROUND,
                                            0xFFFFFFFF, 4) ||
        active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, NULL, &redo_count,
                                            TOOL_BRUSH, 0, 0, 1, 0xFFFFFFFF, BRUSH_SHAPE_ROUND,
                                            0xFFFFFFFF, 4) ||
        active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, redo_stack, NULL,
                                            TOOL_BRUSH, 0, 0, 1, 0xFFFFFFFF, BRUSH_SHAPE_ROUND,
                                            0xFFFFFFFF, 4)) {
        fprintf(stderr, "active_layer_try_begin_brush_stroke null guard failed\n");
        return 0;
    }
    if (active_layer_apply_translation(NULL, undo_stack, &undo_count, redo_stack, &redo_count, 1, 0, 0xFFFFFFFF, 4) ||
        active_layer_apply_translation(&stack, NULL, &undo_count, redo_stack, &redo_count, 1, 0, 0xFFFFFFFF, 4) ||
        active_layer_apply_translation(&stack, undo_stack, NULL, redo_stack, &redo_count, 1, 0, 0xFFFFFFFF, 4) ||
        active_layer_apply_translation(&stack, undo_stack, &undo_count, NULL, &redo_count, 1, 0, 0xFFFFFFFF, 4) ||
        active_layer_apply_translation(&stack, undo_stack, &undo_count, redo_stack, NULL, 1, 0, 0xFFFFFFFF, 4)) {
        fprintf(stderr, "active_layer_apply_translation null guard failed\n");
        return 0;
    }

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF)) {
        fprintf(stderr, "active layer ops stack init failed\n");
        return 0;
    }
    if (!layer_stack_init(&narrow_stack, 1, 3, 0xFFFFFFFF)) {
        fprintf(stderr, "active layer ops narrow stack init failed\n");
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_init(&short_stack, 3, 1, 0xFFFFFFFF)) {
        fprintf(stderr, "active layer ops short stack init failed\n");
        layer_stack_free(&narrow_stack);
        layer_stack_free(&stack);
        return 0;
    }
    if (!layer_stack_init(&single_stack, 1, 1, 0xFFFFFFFF)) {
        fprintf(stderr, "active layer ops single stack init failed\n");
        layer_stack_free(&short_stack);
        layer_stack_free(&narrow_stack);
        layer_stack_free(&stack);
        return 0;
    }
    {
        int width_before = stack.width;
        int undo_before = undo_count;
        int redo_before = redo_count;

        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFFABCDEF);
        stack.width = -1;
        if (active_layer_try_begin_brush_stroke_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                            TOOL_BRUSH, 0, 0, 1, 0xFF010203, BRUSH_SHAPE_ROUND,
                                                            0xFFFFFFFF, 4) != ACTIVE_LAYER_ACTION_FAILED ||
            undo_count != undo_before || redo_count != redo_before ||
            !expect_pixel_eq("active_begin_history_capture_guard", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFFABCDEF)) {
            fprintf(stderr, "active_layer_try_begin_brush_stroke should fail cleanly when history capture fails\n");
            stack.width = width_before;
            layer_stack_free(&single_stack);
            layer_stack_free(&short_stack);
            layer_stack_free(&narrow_stack);
            layer_stack_free(&stack);
            return 0;
        }
        stack.width = width_before;
    }

    canvas_set_pixel_raw(&narrow_stack.layers[0].canvas, 0, 0, 0xFF010203);
    if (active_layer_try_flip_horizontal(&narrow_stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
        !expect_pixel_eq("active_flip_h_narrow", canvas_get_pixel(&narrow_stack.layers[0].canvas, 0, 0), 0xFF010203) ||
        undo_count != 0 || redo_count != 0) {
        fprintf(stderr, "active_layer_try_flip_horizontal narrow no-op failed\n");
        layer_stack_free(&short_stack);
        layer_stack_free(&narrow_stack);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel_raw(&short_stack.layers[0].canvas, 0, 0, 0xFF040506);
    if (active_layer_try_flip_vertical(&short_stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
        !expect_pixel_eq("active_flip_v_short", canvas_get_pixel(&short_stack.layers[0].canvas, 0, 0), 0xFF040506) ||
        undo_count != 0 || redo_count != 0) {
        fprintf(stderr, "active_layer_try_flip_vertical short no-op failed\n");
        layer_stack_free(&single_stack);
        layer_stack_free(&short_stack);
        layer_stack_free(&narrow_stack);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel_raw(&single_stack.layers[0].canvas, 0, 0, 0xFF070809);
    if (active_layer_try_rotate_180(&single_stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
        !expect_pixel_eq("active_rotate_single", canvas_get_pixel(&single_stack.layers[0].canvas, 0, 0), 0xFF070809) ||
        undo_count != 0 || redo_count != 0) {
        fprintf(stderr, "active_layer_try_rotate_180 single no-op failed\n");
        layer_stack_free(&single_stack);
        layer_stack_free(&short_stack);
        layer_stack_free(&narrow_stack);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFF112233);
    if (!active_layer_try_clear(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0xFFFFFFFF, 4) ||
        !expect_pixel_eq("active_clear_base", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFFFFFFFF) ||
        undo_count != 1) {
        fprintf(stderr, "active_layer_try_clear failed\n");
        layer_stack_free(&single_stack);
        layer_stack_free(&short_stack);
        layer_stack_free(&narrow_stack);
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFF556677);
    if (active_layer_try_clear(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0xFFFFFFFF, 0) ||
        !expect_pixel_eq("active_clear_history_guard", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF556677)) {
        fprintf(stderr, "active_layer_try_clear history guard failed\n");
        layer_stack_free(&single_stack);
        layer_stack_free(&short_stack);
        layer_stack_free(&narrow_stack);
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFFFFFFFF);
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        if (active_layer_try_clear_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                               0xFFFFFFFF, 4) != ACTIVE_LAYER_ACTION_UNCHANGED) {
            fprintf(stderr, "active_layer_try_clear_with_result no-op result failed\n");
            layer_stack_free(&single_stack);
            layer_stack_free(&short_stack);
            layer_stack_free(&narrow_stack);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        if (active_layer_try_clear(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0xFFFFFFFF, 4) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_clear no-op failed\n");
            layer_stack_free(&single_stack);
            layer_stack_free(&short_stack);
            layer_stack_free(&narrow_stack);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    canvas_clear(&stack.layers[0].canvas, 0xFF222222);
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        if (active_layer_try_flip_horizontal(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
            !expect_pixel_eq("active_flip_h_uniform", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF222222) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_flip_horizontal uniform no-op failed\n");
            layer_stack_free(&single_stack);
            layer_stack_free(&short_stack);
            layer_stack_free(&narrow_stack);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        if (active_layer_try_flip_vertical(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
            !expect_pixel_eq("active_flip_v_uniform", canvas_get_pixel(&stack.layers[0].canvas, 3, 3), 0xFF222222) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_flip_vertical uniform no-op failed\n");
            layer_stack_free(&single_stack);
            layer_stack_free(&short_stack);
            layer_stack_free(&narrow_stack);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        if (active_layer_try_rotate_180(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
            !expect_pixel_eq("active_rotate_uniform", canvas_get_pixel(&stack.layers[0].canvas, 1, 2), 0xFF222222) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_rotate_180 uniform no-op failed\n");
            layer_stack_free(&single_stack);
            layer_stack_free(&short_stack);
            layer_stack_free(&narrow_stack);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }

    canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF000001);
    if (active_layer_try_flip_horizontal_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) !=
        ACTIVE_LAYER_ACTION_CHANGED) {
        fprintf(stderr, "active_layer_try_flip_horizontal_with_result changed result failed\n");
        layer_stack_free(&single_stack);
        layer_stack_free(&short_stack);
        layer_stack_free(&narrow_stack);
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF000001);
    if (!active_layer_try_flip_horizontal(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
        !expect_pixel_eq("active_flip_h", canvas_get_pixel(&stack.layers[0].canvas, 3, 0), 0xFF000001)) {
        fprintf(stderr, "active_layer_try_flip_horizontal failed\n");
        layer_stack_free(&single_stack);
        layer_stack_free(&short_stack);
        layer_stack_free(&narrow_stack);
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel_raw(&stack.layers[0].canvas, 2, 0, 0xFF010203);
    if (!active_layer_try_flip_vertical(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
        !expect_pixel_eq("active_flip_v", canvas_get_pixel(&stack.layers[0].canvas, 2, 3), 0xFF010203)) {
        fprintf(stderr, "active_layer_try_flip_vertical failed\n");
        layer_stack_free(&single_stack);
        layer_stack_free(&short_stack);
        layer_stack_free(&narrow_stack);
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 1, 0xFF0A0B0C);
    if (!active_layer_try_rotate_180(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
        !expect_pixel_eq("active_rotate_180", canvas_get_pixel(&stack.layers[0].canvas, 3, 2), 0xFF0A0B0C)) {
        fprintf(stderr, "active_layer_try_rotate_180 failed\n");
        layer_stack_free(&single_stack);
        layer_stack_free(&short_stack);
        layer_stack_free(&narrow_stack);
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 0, 0xFF102030);
    if (!active_layer_try_invert_rgb(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
        !expect_pixel_eq("active_invert_rgb", canvas_get_pixel(&stack.layers[0].canvas, 1, 0), 0xFFEFDFCF)) {
        fprintf(stderr, "active_layer_try_invert_rgb failed\n");
        layer_stack_free(&single_stack);
        layer_stack_free(&short_stack);
        layer_stack_free(&narrow_stack);
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0x00000000);
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        if (active_layer_try_invert_rgb(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
            !expect_pixel_eq("active_invert_rgb_transparent", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0x00000000) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_invert_rgb transparent no-op failed\n");
            layer_stack_free(&single_stack);
            layer_stack_free(&short_stack);
            layer_stack_free(&narrow_stack);
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }

    if (active_layer_try_adjust_opacity_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                    55, 4) != ACTIVE_LAYER_ACTION_CHANGED ||
        stack.layers[0].opacity_percent != 55) {
        fprintf(stderr, "active_layer_try_adjust_opacity_with_result changed failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    stack.layers[0].opacity_percent = 100;
    if (!active_layer_try_adjust_opacity(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 55, 4) ||
        stack.layers[0].opacity_percent != 55) {
        fprintf(stderr, "active_layer_try_adjust_opacity failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (!active_layer_try_nudge_opacity(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 10, 4) ||
        stack.layers[0].opacity_percent != 65) {
        fprintf(stderr, "active_layer_try_nudge_opacity failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (!active_layer_try_adjust_opacity(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 100, 4) ||
        stack.layers[0].opacity_percent != 100) {
        fprintf(stderr, "active_layer_try_adjust_opacity max set failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        if (active_layer_try_adjust_opacity_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                        101, 4) != ACTIVE_LAYER_ACTION_UNCHANGED) {
            fprintf(stderr, "active_layer_try_adjust_opacity_with_result unchanged failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        if (active_layer_try_adjust_opacity(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 101, 4) ||
            stack.layers[0].opacity_percent != 100 ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_adjust_opacity clamped no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    if (active_layer_try_adjust_opacity(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 100, 4)) {
        fprintf(stderr, "active_layer_try_adjust_opacity no-op failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_clear(&stack.layers[0].canvas, 0x00000000);
    if (active_layer_try_begin_brush_stroke_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                        TOOL_BRUSH, 2, 2, 1, 0xFF556677, BRUSH_SHAPE_SQUARE,
                                                        0xFFFFFFFF, 4) != ACTIVE_LAYER_ACTION_CHANGED ||
        !expect_pixel_eq("active_stroke_brush_result", canvas_get_pixel(&stack.layers[0].canvas, 2, 2), 0xFF556677)) {
        fprintf(stderr, "active_layer_try_begin_brush_stroke_with_result changed failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    canvas_clear(&stack.layers[0].canvas, 0x00000000);
    if (!active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                             TOOL_BRUSH, 2, 2, 1, 0xFF556677, BRUSH_SHAPE_SQUARE,
                                             0xFFFFFFFF, 4) ||
        !expect_pixel_eq("active_stroke_brush", canvas_get_pixel(&stack.layers[0].canvas, 2, 2), 0xFF556677)) {
        fprintf(stderr, "active_layer_try_begin_brush_stroke brush failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0x00000000);
    if (active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                            TOOL_BRUSH, 2, 2, 1, 0xFF556677, BRUSH_SHAPE_SQUARE,
                                            0xFFFFFFFF, 0) ||
        !expect_pixel_eq("active_stroke_brush_history_guard", canvas_get_pixel(&stack.layers[0].canvas, 2, 2), 0x00000000)) {
        fprintf(stderr, "active_layer_try_begin_brush_stroke history guard failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel_raw(&stack.layers[0].canvas, 2, 2, 0xFFFFFFFF);
    if (!active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                             TOOL_ERASER, 2, 2, 1, 0xFF000000, BRUSH_SHAPE_ROUND,
                                             0xFF123456, 4) ||
        !expect_pixel_eq("active_stroke_eraser", canvas_get_pixel(&stack.layers[0].canvas, 2, 2), 0xFF123456)) {
        fprintf(stderr, "active_layer_try_begin_brush_stroke eraser failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_begin_brush_stroke_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                            TOOL_LINE, 0, 0, 1, 0xFF556677, BRUSH_SHAPE_SQUARE,
                                                            0xFFFFFFFF, 4) != ACTIVE_LAYER_ACTION_UNCHANGED) {
            fprintf(stderr, "active_layer_try_begin_brush_stroke_with_result unchanged failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        if (active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                TOOL_LINE, 0, 0, 1, 0xFF556677, BRUSH_SHAPE_SQUARE,
                                                0xFFFFFFFF, 4) ||
            !expect_pixel_eq("active_stroke_invalid_tool", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_begin_brush_stroke invalid tool no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_clear(&stack.layers[0].canvas, 0xFF556677);
        if (active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                TOOL_BRUSH, 1, 1, 1, 0xFF556677, BRUSH_SHAPE_ROUND,
                                                0xFFFFFFFF, 4) ||
            !expect_pixel_eq("active_stroke_same_color", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF556677) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_begin_brush_stroke same-color no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                TOOL_BRUSH, -1, 0, 1, 0xFF556677, BRUSH_SHAPE_SQUARE,
                                                0xFFFFFFFF, 4) ||
            !expect_pixel_eq("active_stroke_oob", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_begin_brush_stroke bounds no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                TOOL_BRUSH, 0, 0, 0, 0xFF556677, BRUSH_SHAPE_SQUARE,
                                                0xFFFFFFFF, 4) ||
            !expect_pixel_eq("active_stroke_zero_radius", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_begin_brush_stroke zero-radius no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                TOOL_BRUSH, 0, 0, 1, 0x00556677, BRUSH_SHAPE_SQUARE,
                                                0xFFFFFFFF, 4) ||
            !expect_pixel_eq("active_stroke_transparent", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_begin_brush_stroke transparent no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_clear(&stack.layers[0].canvas, 0xFFFFFFFF);
        if (active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                TOOL_ERASER, 1, 1, 1, 0xFF556677, BRUSH_SHAPE_ROUND,
                                                0xFFFFFFFF, 4) ||
            !expect_pixel_eq("active_stroke_blank_erase", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFFFFFFFF) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_begin_brush_stroke blank erase no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }

    canvas_clear(&stack.layers[0].canvas, 0x00000000);
    if (active_layer_try_commit_shape_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                  TOOL_FILLED_RECT, 1, 1, 2, 2, 1, 0xFF998877, 4) !=
        ACTIVE_LAYER_ACTION_CHANGED ||
        !expect_pixel_eq("active_commit_shape_result", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF998877)) {
        fprintf(stderr, "active_layer_try_commit_shape_with_result changed failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    canvas_clear(&stack.layers[0].canvas, 0x00000000);
    if (!active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                       TOOL_FILLED_RECT, 1, 1, 2, 2, 1, 0xFF998877, 4) ||
        !expect_pixel_eq("active_commit_shape", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF998877)) {
        fprintf(stderr, "active_layer_try_commit_shape failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_commit_shape_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                      TOOL_BRUSH, 0, 0, 2, 2, 1, 0xFF998877, 4) !=
            ACTIVE_LAYER_ACTION_UNCHANGED) {
            fprintf(stderr, "active_layer_try_commit_shape_with_result unchanged failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_BRUSH, 0, 0, 2, 2, 1, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_invalid_tool", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape invalid tool no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_clear(&stack.layers[0].canvas, 0xFF998877);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_FILLED_RECT, 0, 0, 2, 2, 1, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_same_fill", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF998877) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape same fill no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_clear(&stack.layers[0].canvas, 0xFF998877);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_ELLIPSE, 0, 0, 4, 4, 1, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_same_ellipse", canvas_get_pixel(&stack.layers[0].canvas, 2, 0), 0xFF998877) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape same ellipse no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_ELLIPSE, -8, -8, -2, -2, 1, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_offscreen_ellipse", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape offscreen ellipse no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_clear(&stack.layers[0].canvas, 0xFF998877);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_RECT, 0, 0, 3, 3, 1, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_same_rect", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF998877) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape same rect no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_RECT, -8, -8, -2, -2, 1, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_offscreen_rect", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape offscreen rect no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_clear(&stack.layers[0].canvas, 0xFF998877);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_FILLED_ELLIPSE, 0, 0, 4, 4, 1, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_same_filled_ellipse", canvas_get_pixel(&stack.layers[0].canvas, 2, 2), 0xFF998877) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape same filled ellipse no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_clear(&stack.layers[0].canvas, 0xFF998877);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_LINE, 0, 0, 3, 3, 1, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_same_line", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF998877) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape same line no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_LINE, -8, -8, -2, -2, 1, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_offscreen_line", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape offscreen line no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_FILLED_ELLIPSE, -8, -8, -2, -2, 1, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_offscreen_filled_ellipse", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape offscreen filled ellipse no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_FILLED_RECT, -5, -5, -1, -1, 1, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_offscreen_fill", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape offscreen fill no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_LINE, 0, 0, 2, 2, 0, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_zero_radius", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape zero-radius no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_ELLIPSE, 1, 1, 1, 3, 1, 0xFF998877, 4) ||
            !expect_pixel_eq("active_commit_shape_degenerate_ellipse", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape degenerate ellipse no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFF010203);
        if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_FILLED_RECT, 0, 0, 2, 2, 1, 0x00998877, 4) ||
            !expect_pixel_eq("active_commit_shape_transparent", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF010203) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_commit_shape transparent no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    if (active_layer_try_adjust_opacity(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 10, 0) ||
        stack.layers[0].opacity_percent != 100) {
        fprintf(stderr, "active_layer_try_adjust_opacity history guard failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (active_layer_try_nudge_opacity(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 5, 0) ||
        stack.layers[0].opacity_percent != 100) {
        fprintf(stderr, "active_layer_try_nudge_opacity history guard failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0x00000000);
    if (active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                      TOOL_FILLED_RECT, 1, 1, 2, 2, 1, 0xFF998877, 0) ||
        !expect_pixel_eq("active_commit_shape_history_guard", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0x00000000)) {
        fprintf(stderr, "active_layer_try_commit_shape history guard failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_clear(&stack.layers[0].canvas, 0x00000000);
    last_x = 0;
    last_y = 0;
    if (active_layer_continue_brush_stroke_with_result(&stack, TOOL_BRUSH, 3, 0, 1, 0xFF334455,
                                                       BRUSH_SHAPE_SQUARE, &last_x, &last_y, 0xFFFFFFFF) !=
        ACTIVE_LAYER_ACTION_CHANGED ||
        !expect_pixel_eq("active_continue_brush_result", canvas_get_pixel(&stack.layers[0].canvas, 3, 0), 0xFF334455) ||
        last_x != 3 || last_y != 0) {
        fprintf(stderr, "active_layer_continue_brush_stroke_with_result changed failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_clear(&stack.layers[0].canvas, 0x00000000);
    last_x = 0;
    last_y = 0;
    if (!active_layer_continue_brush_stroke(&stack, TOOL_BRUSH, 3, 0, 1, 0xFF334455,
                                            BRUSH_SHAPE_SQUARE, &last_x, &last_y, 0xFFFFFFFF) ||
        !expect_pixel_eq("active_continue_brush", canvas_get_pixel(&stack.layers[0].canvas, 3, 0), 0xFF334455) ||
        last_x != 3 || last_y != 0) {
        fprintf(stderr, "active_layer_continue_brush_stroke brush failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    last_x = 0;
    last_y = 0;
    canvas_set_pixel_raw(&stack.layers[0].canvas, 3, 0, 0xFFFFFFFF);
    if (!active_layer_continue_brush_stroke(&stack, TOOL_ERASER, 3, 0, 1, 0xFF000000,
                                            BRUSH_SHAPE_ROUND, &last_x, &last_y, 0xFF123456) ||
        !expect_pixel_eq("active_continue_eraser", canvas_get_pixel(&stack.layers[0].canvas, 3, 0), 0xFF123456) ||
        last_x != 3 || last_y != 0) {
        fprintf(stderr, "active_layer_continue_brush_stroke eraser failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    {
        int last_x = 1;
        int last_y = 1;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFF556677);
        if (active_layer_continue_brush_stroke(&stack, TOOL_LINE, 2, 1, 1, 0xFF998877,
                                               BRUSH_SHAPE_ROUND, &last_x, &last_y, 0xFFFFFFFF) ||
            !expect_pixel_eq("active_continue_invalid_tool", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF556677) ||
            last_x != 1 || last_y != 1) {
            fprintf(stderr, "active_layer_continue_brush_stroke invalid tool failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int last_x = 1;
        int last_y = 1;
        canvas_clear(&stack.layers[0].canvas, 0xFF998877);
        if (active_layer_continue_brush_stroke_with_result(&stack, TOOL_BRUSH, 2, 1, 1, 0xFF998877,
                                                           BRUSH_SHAPE_ROUND, &last_x, &last_y, 0xFFFFFFFF) !=
            ACTIVE_LAYER_ACTION_UNCHANGED ||
            last_x != 1 || last_y != 1) {
            fprintf(stderr, "active_layer_continue_brush_stroke_with_result unchanged failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
        last_x = 1;
        last_y = 1;
        if (active_layer_continue_brush_stroke(&stack, TOOL_BRUSH, 2, 1, 1, 0xFF998877,
                                               BRUSH_SHAPE_ROUND, &last_x, &last_y, 0xFFFFFFFF) ||
            !expect_pixel_eq("active_continue_same_color", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF998877) ||
            last_x != 1 || last_y != 1) {
            fprintf(stderr, "active_layer_continue_brush_stroke same-color failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int last_x = 1;
        int last_y = 1;
        canvas_clear(&stack.layers[0].canvas, 0xFFFFFFFF);
        if (active_layer_continue_brush_stroke(&stack, TOOL_ERASER, 2, 1, 1, 0xFF000000,
                                               BRUSH_SHAPE_ROUND, &last_x, &last_y, 0xFFFFFFFF) ||
            !expect_pixel_eq("active_continue_blank_erase", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFFFFFFFF) ||
            last_x != 1 || last_y != 1) {
            fprintf(stderr, "active_layer_continue_brush_stroke blank erase failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int last_x = 1;
        int last_y = 1;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFF556677);
        if (active_layer_continue_brush_stroke(&stack, TOOL_BRUSH, 2, 1, 0, 0xFF998877,
                                               BRUSH_SHAPE_ROUND, &last_x, &last_y, 0xFFFFFFFF) ||
            !expect_pixel_eq("active_continue_zero_radius", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF556677) ||
            last_x != 1 || last_y != 1) {
            fprintf(stderr, "active_layer_continue_brush_stroke zero radius failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int last_x = 1;
        int last_y = 1;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFF556677);
        if (active_layer_continue_brush_stroke(&stack, TOOL_BRUSH, 2, 1, 1, 0x00998877,
                                               BRUSH_SHAPE_ROUND, &last_x, &last_y, 0xFFFFFFFF) ||
            !expect_pixel_eq("active_continue_transparent", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF556677) ||
            last_x != 1 || last_y != 1) {
            fprintf(stderr, "active_layer_continue_brush_stroke transparent failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int last_x = 1;
        int last_y = 1;
        if (active_layer_continue_brush_stroke(&stack, TOOL_BRUSH, -1, 0, 1, 0xFFFFFFFF,
                                               BRUSH_SHAPE_ROUND, &last_x, &last_y, 0xFFFFFFFF) ||
            last_x != 1 || last_y != 1) {
            fprintf(stderr, "active_layer_continue_brush_stroke bounds guard failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int last_x = -1;
        int last_y = 1;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFF556677);
        if (active_layer_continue_brush_stroke(&stack, TOOL_BRUSH, 2, 1, 1, 0xFF998877,
                                               BRUSH_SHAPE_ROUND, &last_x, &last_y, 0xFFFFFFFF) ||
            !expect_pixel_eq("active_continue_prev_oob", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF556677) ||
            last_x != -1 || last_y != 1) {
            fprintf(stderr, "active_layer_continue_brush_stroke previous-point guard failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int last_x = 1;
        int last_y = 1;
        if (active_layer_continue_brush_stroke(NULL, TOOL_BRUSH, 0, 0, 1, 0xFFFFFFFF,
                                               BRUSH_SHAPE_ROUND, &last_x, &last_y, 0xFFFFFFFF) ||
            active_layer_continue_brush_stroke(&stack, TOOL_BRUSH, 0, 0, 1, 0xFFFFFFFF,
                                               BRUSH_SHAPE_ROUND, NULL, &last_y, 0xFFFFFFFF) ||
            active_layer_continue_brush_stroke(&stack, TOOL_BRUSH, 0, 0, 1, 0xFFFFFFFF,
                                               BRUSH_SHAPE_ROUND, &last_x, NULL, 0xFFFFFFFF)) {
            fprintf(stderr, "active_layer_continue_brush_stroke null guard failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }

    canvas_clear(&stack.layers[0].canvas, 0x00000000);
    canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFF010101);
    if (!active_layer_try_flood_fill(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                     1, 1, 0xFFABC123, 4) ||
        !expect_pixel_eq("active_fill", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFFABC123)) {
        fprintf(stderr, "active_layer_try_flood_fill failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        if (active_layer_try_flood_fill(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                        1, 1, 0xFFABC123, 4) ||
            !expect_pixel_eq("active_fill_same_color", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFFABC123) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_flood_fill same-color no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        if (active_layer_try_flood_fill_action_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                      1, 1, 0xFFABC123, 4) != ACTIVE_LAYER_ACTION_UNCHANGED) {
            fprintf(stderr, "active_layer_try_flood_fill_action_result same-color no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int changed = 1;
        int undo_before = undo_count;
        int redo_before = redo_count;
        if (!active_layer_try_flood_fill_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                     1, 1, 0xFFABC123, 4, &changed) ||
            changed ||
            !expect_pixel_eq("active_fill_same_color_result", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFFABC123) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_flood_fill_with_result same-color no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFF010101);
        if (active_layer_try_flood_fill(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                        1, 1, 0x00ABC123, 4) ||
            !expect_pixel_eq("active_fill_transparent", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF010101) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_flood_fill transparent no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFF010101);
        if (active_layer_try_flood_fill_action_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                      1, 1, 0x00ABC123, 4) != ACTIVE_LAYER_ACTION_FAILED) {
            fprintf(stderr, "active_layer_try_flood_fill_action_result transparent guard failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        int changed = 1;
        int undo_before = undo_count;
        int redo_before = redo_count;
        canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFF010101);
        if (active_layer_try_flood_fill_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                    1, 1, 0x00ABC123, 4, &changed) ||
            !expect_pixel_eq("active_fill_transparent_result", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF010101) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_try_flood_fill_with_result transparent guard failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFF010101);
    if (active_layer_try_flood_fill(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                    1, 1, 0xFFABC123, 0) ||
        !expect_pixel_eq("active_fill_history_guard", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF010101)) {
        fprintf(stderr, "active_layer_try_flood_fill history guard failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (active_layer_try_flood_fill(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                    -1, 1, 0xFFFFFFFF, 4)) {
        fprintf(stderr, "active_layer_try_flood_fill bounds guard failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFFABCDEF);
    if (active_layer_apply_translation_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                   1, 1, 0xFFFFFFFF, 4) != ACTIVE_LAYER_ACTION_CHANGED ||
        !expect_pixel_eq("active_translate_result", canvas_get_pixel(&stack.layers[0].canvas, 2, 2), 0xFFABCDEF)) {
        fprintf(stderr, "active_layer_apply_translation_with_result changed failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    canvas_clear(&stack.layers[0].canvas, 0xFFFFFFFF);
    canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFFABCDEF);
    if (!active_layer_apply_translation(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                        1, 1, 0xFFFFFFFF, 4) ||
        !expect_pixel_eq("active_translate", canvas_get_pixel(&stack.layers[0].canvas, 2, 2), 0xFFABCDEF)) {
        fprintf(stderr, "active_layer_apply_translation failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    if (active_layer_apply_translation_with_result(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                   0, 0, 0xFFFFFFFF, 4) != ACTIVE_LAYER_ACTION_UNCHANGED ||
        active_layer_apply_translation(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                       0, 0, 0xFFFFFFFF, 4)) {
        fprintf(stderr, "active_layer_apply_translation no-op failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_set_pixel_raw(&stack.layers[0].canvas, 1, 1, 0xFF102030);
    if (active_layer_apply_translation(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                       1, 0, 0xFFFFFFFF, 0) ||
        !expect_pixel_eq("active_translate_history_guard", canvas_get_pixel(&stack.layers[0].canvas, 1, 1), 0xFF102030)) {
        fprintf(stderr, "active_layer_apply_translation history guard failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    canvas_clear(&stack.layers[0].canvas, 0xFFFFFFFF);
    {
        int undo_before = undo_count;
        int redo_before = redo_count;
        if (active_layer_apply_translation(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                           1, 0, 0xFFFFFFFF, 4) ||
            !expect_pixel_eq("active_translate_blank", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFFFFFFFF) ||
            undo_count != undo_before || redo_count != redo_before) {
            fprintf(stderr, "active_layer_apply_translation blank no-op failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }

    if (layer_stack_add(&stack, "Overlay", 0x00000000) < 0) {
        fprintf(stderr, "active layer overlay add failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }
    stack.active_layer = 1;
    canvas_set_pixel_raw(&stack.layers[0].canvas, 0, 0, 0xFFFFFFFF);
    canvas_set_pixel_raw(&stack.layers[1].canvas, 0, 0, 0xFFFFFFFF);
    if (!active_layer_try_clear(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0xFF123456, 4) ||
        !expect_pixel_eq("active_clear_transparent", canvas_get_pixel(&stack.layers[1].canvas, 0, 0), 0x00000000)) {
        fprintf(stderr, "active_layer_try_clear transparent failed\n");
        snapshot_stack_clear(undo_stack, &undo_count);
        snapshot_stack_clear(redo_stack, &redo_count);
        layer_stack_free(&stack);
        return 0;
    }

    stack.layers[1].locked = 1;
    {
        int last_x = 0;
        int last_y = 0;
        if (active_layer_try_flip_horizontal(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
            active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                TOOL_BRUSH, 0, 0, 1, 0xFFFFFFFF, BRUSH_SHAPE_ROUND,
                                                0xFFFFFFFF, 4) ||
            active_layer_continue_brush_stroke(&stack, TOOL_BRUSH, 0, 0, 1, 0xFFFFFFFF,
                                               BRUSH_SHAPE_ROUND, &last_x, &last_y, 0xFFFFFFFF) ||
            active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_LINE, 0, 0, 1, 1, 1, 0xFFFFFFFF, 4) ||
            active_layer_try_invert_rgb(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
            active_layer_try_flood_fill(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0, 0, 0xFFFFFFFF, 4) ||
            active_layer_apply_translation(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 1, 0, 0xFFFFFFFF, 4)) {
            fprintf(stderr, "active layer locked guards failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }
    stack.layers[1].locked = 0;
    canvas_free(&stack.layers[1].canvas);
    {
        int last_x = 0;
        int last_y = 0;
        if (active_layer_try_flip_vertical(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
            active_layer_try_begin_brush_stroke(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                                TOOL_BRUSH, 0, 0, 1, 0xFFFFFFFF, BRUSH_SHAPE_ROUND,
                                                0xFFFFFFFF, 4) ||
            active_layer_continue_brush_stroke(&stack, TOOL_BRUSH, 0, 0, 1, 0xFFFFFFFF,
                                               BRUSH_SHAPE_ROUND, &last_x, &last_y, 0xFFFFFFFF) ||
            active_layer_try_commit_shape(&stack, undo_stack, &undo_count, redo_stack, &redo_count,
                                          TOOL_LINE, 0, 0, 1, 1, 1, 0xFFFFFFFF, 4) ||
            active_layer_try_rotate_180(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 4) ||
            active_layer_try_flood_fill(&stack, undo_stack, &undo_count, redo_stack, &redo_count, 0, 0, 0xFFFFFFFF, 4) ||
            active_layer_apply_translation(&stack, undo_stack, &undo_count, redo_stack, &redo_count, -1, 0, 0xFFFFFFFF, 4)) {
            fprintf(stderr, "active layer null pixel guards failed\n");
            snapshot_stack_clear(undo_stack, &undo_count);
            snapshot_stack_clear(redo_stack, &redo_count);
            layer_stack_free(&stack);
            return 0;
        }
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    layer_stack_free(&single_stack);
    layer_stack_free(&short_stack);
    layer_stack_free(&narrow_stack);
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
    if (layer_stack_show_all(&stack)) {
        fprintf(stderr, "show all no-op failed\n");
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
    if (layer_stack_show(&stack, 1)) {
        fprintf(stderr, "show active layer no-op failed\n");
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
    if (layer_stack_isolate(&stack, 1)) {
        fprintf(stderr, "redundant isolate should no-op\n");
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
    {
        Canvas preview = {2, 2, (uint32_t[]){0xFF000001, 0xFF000002, 0xFF000003, 0xFF000004}};
        if (current_display_canvas(1, &preview, &composite) != &preview ||
            current_display_pixels(1, &preview, &composite) != preview.pixels) {
            fprintf(stderr, "display canvas should prefer preview canvas\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        preview.pixels = NULL;
        if (current_display_canvas(1, &preview, &composite) != &composite ||
            current_display_pixels(1, &preview, &composite) != composite.pixels) {
            fprintf(stderr, "display canvas should fall back to composite canvas\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        if (current_display_canvas(0, NULL, &composite) != &composite ||
            current_display_pixels(0, NULL, &composite) != composite.pixels) {
            fprintf(stderr, "display canvas should use composite without preview\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        preview.pixels = (uint32_t[]){0xFF000001, 0xFF000002, 0xFF000003, 0xFF000004};
        if (current_display_canvas(1, &preview, NULL) != &preview ||
            current_display_pixels(1, &preview, NULL) != preview.pixels) {
            fprintf(stderr, "display canvas should use active preview without composite\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        if (current_display_canvas(1, NULL, NULL) != NULL ||
            current_display_pixels(1, NULL, NULL) != NULL) {
            fprintf(stderr, "display canvas should return null without sources\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        if (current_display_canvas(0, &preview, NULL) != NULL ||
            current_display_pixels(0, &preview, NULL) != NULL) {
            fprintf(stderr, "display canvas should not use preview when preview is inactive\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char hint[2] = {'X', 'Y'};
        format_hidden_layer_hint(NULL, hint, sizeof(hint));
        if (hint[0] != '\0' || hint[1] != 'Y') {
            fprintf(stderr, "null layer hint formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char hint[2] = {'X', 'Y'};
        format_hidden_layer_hint(&stack, hint, 0);
        if (hint[0] != 'X' || hint[1] != 'Y') {
            fprintf(stderr, "hidden hint zero-size guard failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        format_hidden_layer_hint(&stack, NULL, 4);
    }
    {
        char hint[1] = {'X'};
        format_hidden_layer_hint(&stack, hint, sizeof(hint));
        if (hint[0] != '\0') {
            fprintf(stderr, "tiny hidden hint formatting failed\n");
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
    {
        char title[32];
        memset(title, 'X', sizeof(title));
        format_window_title(&stack, "Brush", "Round", 2, 0xFF445566, 90, title, sizeof(title));
        if (title[sizeof(title) - 1] != '\0') {
            fprintf(stderr, "window title should stay null terminated\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char title[2] = {'X', 'Y'};
        format_window_title(&stack, "Brush", "Round", 2, 0xFF445566, 90, title, 0);
        if (title[0] != 'X' || title[1] != 'Y') {
            fprintf(stderr, "window title zero-size guard failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        format_window_title(&stack, "Brush", "Round", 2, 0xFF445566, 90, NULL, 8);
    }
    {
        LayerStack empty = {0};
        char title[64];
        format_window_title(&empty, NULL, NULL, 1, 0xFF000000, 1, title, sizeof(title));
        if (strcmp(title, "Openshop - Tool (Brush) | size 1 | brush 1% | layer 1/0 Layer [") != 0 ||
            title[sizeof(title) - 1] != '\0') {
            fprintf(stderr, "window title formatting failed for empty stack fallback\n");
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
    {
        char status_message[128];
        format_status_text_startup("Layer stack init", status_message, sizeof(status_message));
        if (strcmp(status_message, "Layer stack init failed") != 0) {
            fprintf(stderr, "startup status text formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char status_message[128];
        format_status_text_sdl("SDL_CreateTexture", "driver missing", status_message, sizeof(status_message));
        if (strcmp(status_message, "SDL_CreateTexture failed: driver missing") != 0) {
            fprintf(stderr, "sdl status text formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char status_message[128];
        format_status_text_file_load("input.bmp", status_message, sizeof(status_message));
        if (strcmp(status_message, "Failed to load input.bmp") != 0) {
            fprintf(stderr, "file load status text formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char status_message[128];
        format_status_text_file_save("output.bmp", status_message, sizeof(status_message));
        if (strcmp(status_message, "Failed to save output.bmp") != 0) {
            fprintf(stderr, "file save status text formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char status_message[64];
        format_status_text_startup(NULL, status_message, sizeof(status_message));
        if (strcmp(status_message, "Startup failed") != 0) {
            fprintf(stderr, "startup fallback status text formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char status_message[2] = {'X', 'Y'};
        format_status_text_startup("Init", status_message, 0);
        if (status_message[0] != 'X' || status_message[1] != 'Y') {
            fprintf(stderr, "startup zero-size guard failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        format_status_text_startup("Init", NULL, 8);
    }
    {
        char status_message[1] = {'X'};
        format_status_text_max_layers(MAX_LAYERS, status_message, sizeof(status_message));
        if (status_message[0] != '\0') {
            fprintf(stderr, "max layer tiny-buffer formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char status_message[64];
        format_status_text_sdl(NULL, NULL, status_message, sizeof(status_message));
        if (strcmp(status_message, "SDL failed: ") != 0) {
            fprintf(stderr, "sdl fallback status text formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char status_message[2] = {'X', 'Y'};
        format_status_text_sdl("SDL", "detail", status_message, 0);
        if (status_message[0] != 'X' || status_message[1] != 'Y') {
            fprintf(stderr, "sdl zero-size guard failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        format_status_text_sdl("SDL", "detail", NULL, 8);
    }
    {
        char status_message[1] = {'X'};
        format_status_text_sdl("SDL", "detail", status_message, sizeof(status_message));
        if (status_message[0] != '\0') {
            fprintf(stderr, "sdl tiny-buffer formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char status_message[32];
        format_status_text_file_load(NULL, status_message, sizeof(status_message));
        if (strcmp(status_message, "Failed to load ") != 0) {
            fprintf(stderr, "file load fallback status text formatting failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char status_message[2] = {'X', 'Y'};
        format_status_text_file_load("input.bmp", status_message, 0);
        if (status_message[0] != 'X' || status_message[1] != 'Y') {
            fprintf(stderr, "file load zero-size guard failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        format_status_text_file_load("input.bmp", NULL, 8);
    }
    {
        char status_message[16];
        memset(status_message, 'X', sizeof(status_message));
        format_status_text_file_save("very-long-output-name.bmp", status_message, sizeof(status_message));
        if (status_message[sizeof(status_message) - 1] != '\0') {
            fprintf(stderr, "file save status text should stay null terminated\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
    }
    {
        char status_message[2] = {'X', 'Y'};
        format_status_text_file_save("output.bmp", status_message, 0);
        if (status_message[0] != 'X' || status_message[1] != 'Y') {
            fprintf(stderr, "file save zero-size guard failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        format_status_text_file_save("output.bmp", NULL, 8);
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
    if (layer_stack_show_unlocked_only(&stack, 1)) {
        fprintf(stderr, "show unlocked only preserve fallback should no-op\n");
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
    if (layer_stack_show_locked_only(&stack, 0)) {
        fprintf(stderr, "show locked only preserve fallback should no-op\n");
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
    if (layer_stack_show_locked_only(&stack, 0)) {
        fprintf(stderr, "redundant show locked only should no-op after preserve fallback\n");
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
    if (layer_stack_show_hidden_locked_only(&stack, 1)) {
        fprintf(stderr, "show hidden locked only preserve fallback should no-op\n");
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
    if (layer_stack_show_hidden_unlocked_only(&stack, 1)) {
        fprintf(stderr, "show hidden unlocked only preserve fallback should no-op\n");
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
    if (layer_stack_show_hidden_unlocked_only(&stack, 1)) {
        fprintf(stderr, "redundant show hidden unlocked only should no-op after preserve fallback\n");
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
    if (layer_stack_show(&stack, 1)) {
        fprintf(stderr, "show visible top layer before hide-and-advance should no-op\n");
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
    if (layer_stack_cycle(&stack, 0) != -1 || stack.active_layer != 1) {
        fprintf(stderr, "layer cycling zero-direction no-op failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    {
        LayerStack single = {0};
        if (!layer_stack_init(&single, 4, 4, 0xFFFFFFFF)) {
            fprintf(stderr, "single-layer cycle init failed\n");
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        if (layer_stack_cycle(&single, 1) != -1 || single.active_layer != 0) {
            fprintf(stderr, "single-layer cycle should be unchanged\n");
            layer_stack_free(&single);
            canvas_free(&composite);
            layer_stack_free(&stack);
            return 0;
        }
        layer_stack_free(&single);
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
    if (layer_stack_cycle_visible(&stack, 0) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "visible layer cycling zero-direction no-op failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 0;
    stack.layers[1].visible = 0;
    stack.layers[3].visible = 0;
    stack.active_layer = 2;
    if (layer_stack_cycle_visible(&stack, 1) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "visible layer cycling single-match should be unchanged\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[3].visible = 1;
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
    if (layer_stack_cycle_hidden(&stack, 0) != -1 || stack.active_layer != 1) {
        fprintf(stderr, "hidden layer cycling zero-direction no-op failed\n");
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
    if (layer_stack_cycle_locked(&stack, 0) != -1 || stack.active_layer != 1) {
        fprintf(stderr, "locked layer cycling zero-direction no-op failed\n");
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
    if (layer_stack_select_bottom_locked(&stack) != -1 || stack.active_layer != 1) {
        fprintf(stderr, "select bottom locked no-op failed\n");
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
    if (layer_stack_select_top_locked(&stack) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "select top locked no-op failed\n");
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
    if (layer_stack_cycle_hidden_locked(&stack, 0) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "hidden locked layer cycling zero-direction no-op failed\n");
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
    if (layer_stack_select_bottom_hidden_locked(&stack) != -1 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom hidden locked no-op failed\n");
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
    if (layer_stack_select_top_hidden_locked(&stack) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "select top hidden locked no-op failed\n");
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
    if (layer_stack_cycle_hidden_unlocked(&stack, 0) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "hidden unlocked layer cycling zero-direction no-op failed\n");
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
    if (layer_stack_select_bottom_hidden_unlocked(&stack) != -1 || stack.active_layer != 1) {
        fprintf(stderr, "select bottom hidden unlocked no-op failed\n");
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
    if (layer_stack_select_top_hidden_unlocked(&stack) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "select top hidden unlocked no-op failed\n");
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
    if (layer_stack_cycle_unlocked(&stack, 0) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "unlocked layer cycling zero-direction no-op failed\n");
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
    if (layer_stack_select_bottom_unlocked(&stack) != -1 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom unlocked no-op failed\n");
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
    if (layer_stack_select_top_unlocked(&stack) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "select top unlocked no-op failed\n");
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
    if (layer_stack_cycle_editable(&stack, 0) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "editable layer zero-direction cycling should fail\n");
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
    if (layer_stack_select_bottom_editable(&stack) != -1 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom editable no-op failed\n");
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
    if (layer_stack_select_top_editable(&stack) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "select top editable no-op failed\n");
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
    stack.active_layer = 1;
    stack.layers[3].visible = 0;
    if (layer_stack_reveal_editable(&stack, 0)) {
        fprintf(stderr, "reveal editable zero-direction should fail\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.active_layer != 1 || stack.layers[3].visible) {
        fprintf(stderr, "reveal editable zero-direction should preserve state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[3].visible = 1;
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
    if (layer_stack_reveal_editable(&stack, 1)) {
        fprintf(stderr, "reveal editable no-op failed when editable target is already active and visible\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_reveal_editable(&stack, -1)) {
        fprintf(stderr, "reveal editable backward no-op failed when editable target is already active and visible\n");
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
    if (layer_stack_reveal_hidden_editable(&stack, 1)) {
        fprintf(stderr, "reveal hidden editable from top should fail when no hidden unlocked layers exist\n");
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
    if (layer_stack_reveal_hidden_locked(&stack, 1)) {
        fprintf(stderr, "reveal hidden locked from top should fail when no hidden locked layers exist\n");
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
    if (layer_stack_reveal_hidden_unlocked(&stack, 1)) {
        fprintf(stderr, "reveal hidden unlocked from top should fail when no hidden unlocked layers exist\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 1;
    stack.layers[1].locked = 1;
    stack.active_layer = 1;
    if (!layer_stack_reveal_hidden_locked(&stack, 0) || stack.active_layer != 1 || !stack.layers[1].visible) {
        fprintf(stderr, "reveal hidden locked should reveal active hidden target\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 1;
    stack.layers[1].locked = 0;
    stack.active_layer = 1;
    if (!layer_stack_reveal_hidden_unlocked(&stack, 0) || stack.active_layer != 1 || !stack.layers[1].visible) {
        fprintf(stderr, "reveal hidden unlocked should reveal active hidden target\n");
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
    if (layer_stack_lock_and_advance(&stack, 2)) {
        fprintf(stderr, "redundant lock and advance should no-op when already locked\n");
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
    if (layer_stack_lock_and_retreat(&stack, 1)) {
        fprintf(stderr, "redundant lock and retreat should no-op when already locked\n");
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
    if (layer_stack_unlock_all(&stack)) {
        fprintf(stderr, "unlock all no-op failed\n");
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
    if (layer_stack_select_bottom_visible(&stack) != -1 || stack.active_layer != 0) {
        fprintf(stderr, "select bottom visible no-op failed\n");
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
    if (layer_stack_select_top_visible(&stack) != -1 || stack.active_layer != 3) {
        fprintf(stderr, "select top visible no-op failed\n");
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
    stack.layers[2].visible = 0;
    stack.active_layer = 1;
    if (layer_stack_reveal_hidden(&stack, 0)) {
        fprintf(stderr, "reveal hidden zero-direction should fail\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (stack.active_layer != 1 || stack.layers[2].visible) {
        fprintf(stderr, "reveal hidden zero-direction should preserve state\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[0].visible = 1;
    stack.layers[1].visible = 0;
    stack.layers[2].visible = 1;
    stack.layers[3].visible = 1;
    stack.active_layer = 1;
    if (!layer_stack_reveal_hidden(&stack, 1) || stack.active_layer != 1 || !stack.layers[1].visible) {
        fprintf(stderr, "reveal hidden should reveal active hidden target\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    stack.layers[2].visible = 1;
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
    if (layer_stack_reveal_hidden(&stack, -1)) {
        fprintf(stderr, "reveal hidden backward should fail when no hidden layers remain\n");
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
    if (layer_stack_select_bottom_hidden(&stack) != -1 || stack.active_layer != 1) {
        fprintf(stderr, "select bottom hidden no-op failed\n");
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
    if (layer_stack_select_top_hidden(&stack) != -1 || stack.active_layer != 2) {
        fprintf(stderr, "select top hidden no-op failed\n");
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
    stack.active_layer = 3;
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
    if (layer_stack_set_opacity(&stack, 1, 100) ||
        layer_stack_set_opacity(&stack, 1, 101) ||
        stack.layers[1].opacity_percent != 100) {
        fprintf(stderr, "same opacity no-op failed\n");
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
    if (!layer_stack_clear_layer(&stack, 0, 0xFF556677) ||
        !expect_pixel_eq("clear_changed_pixel", canvas_get_pixel(&stack.layers[0].canvas, 0, 0), 0xFF556677)) {
        fprintf(stderr, "clear changed layer failed\n");
        canvas_free(&composite);
        layer_stack_free(&stack);
        return 0;
    }
    if (layer_stack_clear_layer(&stack, 0, 0xFF556677)) {
        fprintf(stderr, "clear same-color no-op failed\n");
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
    if (layer_stack_move(&stack, 1, 0) ||
        stack.active_layer != 1 ||
        strcmp(stack.layers[0].name, "Upper Merge") != 0 ||
        strcmp(stack.layers[1].name, "Background Copy") != 0) {
        fprintf(stderr, "zero-direction move no-op failed\n");
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
    if (canvas_flood_fill(&c, 1, 1, 0xFFFF0000) ||
        !expect_pixel_eq("canvas_flood_fill_same_color", canvas_get_pixel(&c, 1, 1), 0xFFFF0000)) {
        fprintf(stderr, "canvas_flood_fill same-color no-op failed\n");
        canvas_free(&c);
        return 1;
    }
    if (canvas_flood_fill(&c, 1, 1, 0x00FF0000) ||
        !expect_pixel_eq("canvas_flood_fill_transparent", canvas_get_pixel(&c, 1, 1), 0xFFFF0000)) {
        fprintf(stderr, "canvas_flood_fill transparent guard failed\n");
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
    canvas_set_pixel_raw(&transform, 1, 0, 0x00010203);
    canvas_invert_rgb(&transform);
    if (!expect_pixel_eq("invert_transparent_preserve", canvas_get_pixel(&transform, 1, 0), 0x00010203)) {
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

    if (!test_brush_state_helpers()) {
        return 1;
    }

    if (!test_app_input_rules()) {
        return 1;
    }

    if (!test_app_hotkey_helpers()) {
        return 1;
    }

    if (!test_layer_action_history_helpers()) {
        return 1;
    }

    if (!test_color_sample_helpers()) {
        return 1;
    }

    if (!test_geometry_helpers()) {
        return 1;
    }

    if (!test_layer_edit_state_helpers()) {
        return 1;
    }

    if (!test_layer_selection_helpers()) {
        return 1;
    }

    if (!test_layer_creation_helpers()) {
        return 1;
    }

    if (!test_shape_preview_state_helpers()) {
        return 1;
    }

    if (!test_brush_render_helpers()) {
        return 1;
    }

    if (!test_shape_draw_helpers()) {
        return 1;
    }

    if (!test_snapshot_history_helpers()) {
        return 1;
    }

    if (!test_active_layer_ops_helpers()) {
        return 1;
    }

    if (!test_layers_basic()) {
        return 1;
    }

    printf("ok\n");
    return 0;
}
