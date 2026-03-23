#include "app_layer_stack.h"

#include <stddef.h>

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

static void app_layer_stack_push_snapshot(const LayerStack *layers, const AppLayerStackCallbacks *callbacks) {
    if (callbacks && callbacks->push_snapshot) {
        callbacks->push_snapshot(layers, callbacks->userdata);
    }
}

int app_layer_stack_apply(
    AppLayerStackCommand command,
    LayerStack *layers,
    AppLayerStackState *state,
    uint32_t background_color,
    const AppLayerStackCallbacks *callbacks
) {
    Layer *active = NULL;

    if (!layers || !state || !command.handled) {
        return 0;
    }

    switch (command.action) {
    case APP_LAYER_STACK_ADD_TOP:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (layer_stack_add(layers, NULL, 0x00000000) < 0) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_INSERT_ABOVE:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (layer_stack_insert(layers, layers->active_layer + 1, NULL, 0x00000000) < 0) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_INSERT_BELOW:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (layer_stack_insert(layers, layers->active_layer, NULL, 0x00000000) < 0) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_TOGGLE_LOCK:
        app_layer_stack_push_snapshot(layers, callbacks);
        return layer_stack_toggle_lock(layers, layers->active_layer);
    case APP_LAYER_STACK_TOGGLE_LOCK_OTHERS:
        app_layer_stack_push_snapshot(layers, callbacks);
        return layer_stack_toggle_lock_others(layers, layers->active_layer);
    case APP_LAYER_STACK_TOGGLE_VISIBILITY_OTHERS:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_toggle_visibility_others(layers, layers->active_layer)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_UNLOCK_ALL:
        app_layer_stack_push_snapshot(layers, callbacks);
        return layer_stack_unlock_all(layers);
    case APP_LAYER_STACK_FLATTEN:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_flatten(layers, background_color)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_STAMP_VISIBLE_INTO:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_stamp_visible_into(layers, layers->active_layer, background_color)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_STAMP_VISIBLE_NEW:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (layer_stack_stamp_visible_new(layers, "Visible Stamp", background_color) < 0) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_DUPLICATE_BELOW:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (layer_stack_duplicate_below(layers, layers->active_layer, NULL) < 0) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_DUPLICATE:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (layer_stack_duplicate(layers, layers->active_layer, NULL) < 0) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_MOVE_RELATIVE:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_move(layers, layers->active_layer, command.argument)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_MOVE_TO_EDGE:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_move_to(layers, layers->active_layer, command.argument == 0 ? 0 : layers->layer_count - 1)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_ADJUST_OPACITY:
        active = layer_stack_active(layers);
        if (!active) {
            return 0;
        }
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_set_opacity(layers, layers->active_layer, active->opacity_percent + command.argument)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_TOGGLE_VISIBILITY:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_toggle_visibility(layers, layers->active_layer)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_HIDE_AND_ADVANCE:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_hide_and_advance(layers, layers->active_layer)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_TOGGLE_SOLO:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_toggle_solo(layers, layers->active_layer)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_DELETE:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_delete(layers, layers->active_layer)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_MERGE_DOWN:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_merge_down(layers, layers->active_layer)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_MERGE_UP:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_merge_up(layers, layers->active_layer)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_RESET_OPACITY:
        active = layer_stack_active(layers);
        if (!active || active->opacity_percent == 100) {
            return 0;
        }
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_set_opacity(layers, layers->active_layer, 100)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_SHOW_ALL:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_show_all(layers)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_SHOW_ACTIVE:
        app_layer_stack_push_snapshot(layers, callbacks);
        if (!layer_stack_show(layers, layers->active_layer)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_LAYER_STACK_NONE:
    default:
        return 0;
    }
}
