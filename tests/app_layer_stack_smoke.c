#include "../src/app_layer_stack.h"

#include <stdio.h>

static int expect_int_eq(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int test_creation_and_duplication_commands(void) {
    AppLayerStackCommand add_top = app_layer_stack_command_for_key('n', 1, 1);
    AppLayerStackCommand insert_above = app_layer_stack_command_for_key('n', 1, 0);
    AppLayerStackCommand insert_below = app_layer_stack_command_for_key(',', 1, 0);
    AppLayerStackCommand dup_below = app_layer_stack_command_for_key('d', 1, 1);
    AppLayerStackCommand dup = app_layer_stack_command_for_key('d', 1, 0);

    return expect_int_eq("add_top_action", add_top.action, APP_LAYER_STACK_ADD_TOP) &&
           expect_int_eq("insert_above_action", insert_above.action, APP_LAYER_STACK_INSERT_ABOVE) &&
           expect_int_eq("insert_below_action", insert_below.action, APP_LAYER_STACK_INSERT_BELOW) &&
           expect_int_eq("dup_below_action", dup_below.action, APP_LAYER_STACK_DUPLICATE_BELOW) &&
           expect_int_eq("dup_action", dup.action, APP_LAYER_STACK_DUPLICATE);
}

static int test_move_and_opacity_commands(void) {
    AppLayerStackCommand move_down = app_layer_stack_command_for_key('[', 1, 0);
    AppLayerStackCommand move_up = app_layer_stack_command_for_key(']', 1, 0);
    AppLayerStackCommand move_bottom = app_layer_stack_command_for_key(1073741898, 1, 0);
    AppLayerStackCommand move_top = app_layer_stack_command_for_key(1073741901, 1, 0);
    AppLayerStackCommand fade = app_layer_stack_command_for_key('-', 1, 0);
    AppLayerStackCommand brighten = app_layer_stack_command_for_key(1073741911, 1, 0);
    AppLayerStackCommand reset = app_layer_stack_command_for_key('0', 1, 0);

    return expect_int_eq("move_down_action", move_down.action, APP_LAYER_STACK_MOVE_RELATIVE) &&
           expect_int_eq("move_down_arg", move_down.argument, -1) &&
           expect_int_eq("move_up_arg", move_up.argument, 1) &&
           expect_int_eq("move_bottom_action", move_bottom.action, APP_LAYER_STACK_MOVE_TO_EDGE) &&
           expect_int_eq("move_bottom_arg", move_bottom.argument, 0) &&
           expect_int_eq("move_top_arg", move_top.argument, 1) &&
           expect_int_eq("fade_action", fade.action, APP_LAYER_STACK_ADJUST_OPACITY) &&
           expect_int_eq("fade_arg", fade.argument, -10) &&
           expect_int_eq("brighten_arg", brighten.argument, 10) &&
           expect_int_eq("reset_action", reset.action, APP_LAYER_STACK_RESET_OPACITY);
}

static int test_visibility_and_merge_commands(void) {
    AppLayerStackCommand toggle_vis = app_layer_stack_command_for_key('v', 1, 1);
    AppLayerStackCommand hide_advance = app_layer_stack_command_for_key('h', 1, 1);
    AppLayerStackCommand solo = app_layer_stack_command_for_key('/', 1, 0);
    AppLayerStackCommand show_all = app_layer_stack_command_for_key('a', 1, 0);
    AppLayerStackCommand show_active = app_layer_stack_command_for_key('r', 1, 1);
    AppLayerStackCommand flatten = app_layer_stack_command_for_key('m', 1, 1);
    AppLayerStackCommand merge_down = app_layer_stack_command_for_key('m', 1, 0);
    AppLayerStackCommand merge_up = app_layer_stack_command_for_key('u', 1, 0);

    return expect_int_eq("toggle_vis_action", toggle_vis.action, APP_LAYER_STACK_TOGGLE_VISIBILITY) &&
           expect_int_eq("hide_advance_action", hide_advance.action, APP_LAYER_STACK_HIDE_AND_ADVANCE) &&
           expect_int_eq("solo_action", solo.action, APP_LAYER_STACK_TOGGLE_SOLO) &&
           expect_int_eq("show_all_action", show_all.action, APP_LAYER_STACK_SHOW_ALL) &&
           expect_int_eq("show_active_action", show_active.action, APP_LAYER_STACK_SHOW_ACTIVE) &&
           expect_int_eq("flatten_action", flatten.action, APP_LAYER_STACK_FLATTEN) &&
           expect_int_eq("merge_down_action", merge_down.action, APP_LAYER_STACK_MERGE_DOWN) &&
           expect_int_eq("merge_up_action", merge_up.action, APP_LAYER_STACK_MERGE_UP);
}

static int test_lock_and_stamp_commands(void) {
    AppLayerStackCommand toggle_lock = app_layer_stack_command_for_key('l', 1, 1);
    AppLayerStackCommand toggle_lock_others = app_layer_stack_command_for_key('k', 1, 1);
    AppLayerStackCommand toggle_visibility_others = app_layer_stack_command_for_key('i', 1, 1);
    AppLayerStackCommand unlock_all = app_layer_stack_command_for_key('u', 1, 1);
    AppLayerStackCommand stamp_into = app_layer_stack_command_for_key('e', 1, 1);
    AppLayerStackCommand stamp_new = app_layer_stack_command_for_key('g', 1, 1);
    AppLayerStackCommand delete_key = app_layer_stack_command_for_key(127, 0, 0);
    AppLayerStackCommand backspace_key = app_layer_stack_command_for_key('\b', 0, 0);

    return expect_int_eq("toggle_lock_action", toggle_lock.action, APP_LAYER_STACK_TOGGLE_LOCK) &&
           expect_int_eq("toggle_lock_others_action", toggle_lock_others.action, APP_LAYER_STACK_TOGGLE_LOCK_OTHERS) &&
           expect_int_eq(
               "toggle_visibility_others_action",
               toggle_visibility_others.action,
               APP_LAYER_STACK_TOGGLE_VISIBILITY_OTHERS) &&
           expect_int_eq("unlock_all_action", unlock_all.action, APP_LAYER_STACK_UNLOCK_ALL) &&
           expect_int_eq("stamp_into_action", stamp_into.action, APP_LAYER_STACK_STAMP_VISIBLE_INTO) &&
           expect_int_eq("stamp_new_action", stamp_new.action, APP_LAYER_STACK_STAMP_VISIBLE_NEW) &&
           expect_int_eq("delete_key_action", delete_key.action, APP_LAYER_STACK_DELETE) &&
           expect_int_eq("backspace_key_action", backspace_key.action, APP_LAYER_STACK_DELETE);
}

static int test_unhandled_key(void) {
    AppLayerStackCommand command = app_layer_stack_command_for_key('q', 0, 0);
    return expect_int_eq("handled", command.handled, 0) &&
           expect_int_eq("action", command.action, APP_LAYER_STACK_NONE) &&
           expect_int_eq("argument", command.argument, 0);
}

int main(void) {
    if (!test_creation_and_duplication_commands()) {
        return 1;
    }
    if (!test_move_and_opacity_commands()) {
        return 1;
    }
    if (!test_visibility_and_merge_commands()) {
        return 1;
    }
    if (!test_lock_and_stamp_commands()) {
        return 1;
    }
    if (!test_unhandled_key()) {
        return 1;
    }
    return 0;
}
