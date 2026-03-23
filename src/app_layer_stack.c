#include "app_layer_stack.h"

AppLayerStackCommand app_layer_stack_command_for_key(int key, int ctrl, int shift) {
    AppLayerStackCommand command = {0, APP_LAYER_STACK_NONE, 0};

    if (ctrl && shift && key == 'n') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_ADD_TOP;
    } else if (ctrl && key == 'n') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_INSERT_ABOVE;
    } else if (ctrl && key == ',') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_INSERT_BELOW;
    } else if (ctrl && shift && key == 'l') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_TOGGLE_LOCK;
    } else if (ctrl && shift && key == 'k') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_TOGGLE_LOCK_OTHERS;
    } else if (ctrl && shift && key == 'i') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_TOGGLE_VISIBILITY_OTHERS;
    } else if (ctrl && shift && key == 'u') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_UNLOCK_ALL;
    } else if (ctrl && shift && key == 'm') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_FLATTEN;
    } else if (ctrl && shift && key == 'e') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_STAMP_VISIBLE_INTO;
    } else if (ctrl && shift && key == 'g') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_STAMP_VISIBLE_NEW;
    } else if (ctrl && shift && key == 'd') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_DUPLICATE_BELOW;
    } else if (ctrl && key == 'd') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_DUPLICATE;
    } else if (ctrl && key == '[') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_MOVE_RELATIVE;
        command.argument = -1;
    } else if (ctrl && key == ']') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_MOVE_RELATIVE;
        command.argument = 1;
    } else if (ctrl && key == 1073741898) {
        command.handled = 1;
        command.action = APP_LAYER_STACK_MOVE_TO_EDGE;
        command.argument = 0;
    } else if (ctrl && key == 1073741901) {
        command.handled = 1;
        command.action = APP_LAYER_STACK_MOVE_TO_EDGE;
        command.argument = 1;
    } else if (ctrl && (key == '-' || key == 1073741910)) {
        command.handled = 1;
        command.action = APP_LAYER_STACK_ADJUST_OPACITY;
        command.argument = -10;
    } else if (ctrl && (key == '=' || key == 1073741911)) {
        command.handled = 1;
        command.action = APP_LAYER_STACK_ADJUST_OPACITY;
        command.argument = 10;
    } else if (ctrl && shift && key == 'v') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_TOGGLE_VISIBILITY;
    } else if (ctrl && shift && key == 'h') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_HIDE_AND_ADVANCE;
    } else if (ctrl && key == '/') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_TOGGLE_SOLO;
    } else if (key == 127 || key == '\b') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_DELETE;
    } else if (ctrl && key == 'm') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_MERGE_DOWN;
    } else if (ctrl && key == 'u') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_MERGE_UP;
    } else if (ctrl && key == '0') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_RESET_OPACITY;
    } else if (ctrl && key == 'a') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_SHOW_ALL;
    } else if (ctrl && shift && key == 'r') {
        command.handled = 1;
        command.action = APP_LAYER_STACK_SHOW_ACTIVE;
    }

    return command;
}
