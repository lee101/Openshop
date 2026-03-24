#include "app_input_rules.h"

int app_key_translation_delta(int key, int step, int *dx, int *dy) {
    if (!dx || !dy) {
        return 0;
    }

    *dx = 0;
    *dy = 0;
    if (key == APP_KEY_UP) {
        *dy = -step;
    } else if (key == APP_KEY_DOWN) {
        *dy = step;
    } else if (key == APP_KEY_LEFT) {
        *dx = -step;
    } else if (key == APP_KEY_RIGHT) {
        *dx = step;
    } else {
        return 0;
    }
    return 1;
}

int app_should_cancel_shape_on_key(int key, int ctrl) {
    if (key == APP_KEY_ESCAPE || key == APP_KEY_LSHIFT || key == APP_KEY_RSHIFT) {
        return 0;
    }
    if (ctrl && (key == APP_KEY_s || key == APP_KEY_o || key == APP_KEY_z || key == APP_KEY_y || key == APP_KEY_n || key == APP_KEY_u ||
                 key == APP_KEY_v || key == APP_KEY_m || key == APP_KEY_d || key == APP_KEY_e || key == APP_KEY_g || key == APP_KEY_h ||
                 key == APP_KEY_l || key == APP_KEY_a || key == APP_KEY_r || key == APP_KEY_0 || key == APP_KEY_COMMA ||
                 key == APP_KEY_PERIOD || key == APP_KEY_SEMICOLON || key == APP_KEY_QUOTE || key == APP_KEY_LEFTBRACKET ||
                 key == APP_KEY_RIGHTBRACKET || key == APP_KEY_MINUS || key == APP_KEY_KP_MINUS || key == APP_KEY_EQUALS ||
                 key == APP_KEY_KP_PLUS || key == APP_KEY_SLASH || key == APP_KEY_1 || key == APP_KEY_2 || key == APP_KEY_3 ||
                 key == APP_KEY_4 || key == APP_KEY_5 || key == APP_KEY_6 || key == APP_KEY_7 || key == APP_KEY_8)) {
        return 1;
    }
    switch (key) {
    case APP_KEY_b:
    case APP_KEY_e:
    case APP_KEY_l:
    case APP_KEY_r:
    case APP_KEY_t:
    case APP_KEY_o:
    case APP_KEY_p:
    case APP_KEY_LEFTBRACKET:
    case APP_KEY_RIGHTBRACKET:
    case APP_KEY_COMMA:
    case APP_KEY_PERIOD:
    case APP_KEY_SEMICOLON:
    case APP_KEY_QUOTE:
    case APP_KEY_MINUS:
    case APP_KEY_KP_MINUS:
    case APP_KEY_EQUALS:
    case APP_KEY_KP_PLUS:
    case APP_KEY_1:
    case APP_KEY_2:
    case APP_KEY_3:
    case APP_KEY_4:
    case APP_KEY_5:
    case APP_KEY_6:
    case APP_KEY_c:
    case APP_KEY_h:
    case APP_KEY_v:
    case APP_KEY_j:
    case APP_KEY_x:
    case APP_KEY_f:
    case APP_KEY_i:
    case APP_KEY_UP:
    case APP_KEY_DOWN:
    case APP_KEY_LEFT:
    case APP_KEY_RIGHT:
    case APP_KEY_PAGEUP:
    case APP_KEY_PAGEDOWN:
    case APP_KEY_DELETE:
    case APP_KEY_BACKSPACE:
        return 1;
    default:
        return 0;
    }
}

AppOpacityHotkeyAction app_opacity_hotkey_action(int key) {
    if (key == APP_KEY_0) {
        return APP_OPACITY_HOTKEY_SET_MAX;
    }
    if (key == APP_KEY_MINUS || key == APP_KEY_KP_MINUS) {
        return APP_OPACITY_HOTKEY_NUDGE_DOWN;
    }
    if (key == APP_KEY_EQUALS || key == APP_KEY_KP_PLUS) {
        return APP_OPACITY_HOTKEY_NUDGE_UP;
    }
    return APP_OPACITY_HOTKEY_NONE;
}

AppLayerNavigationAction app_layer_navigation_action(int key, int ctrl, int alt, int shift, int *arg) {
    if (arg) {
        *arg = 0;
    }
    if (alt || shift) {
        return APP_LAYER_NAV_NONE;
    }
    if (ctrl && key >= APP_KEY_1 && key <= APP_KEY_8) {
        if (arg) {
            *arg = key - APP_KEY_1;
        }
        return APP_LAYER_NAV_SELECT_INDEX;
    }
    if (!ctrl && key == APP_KEY_PAGEUP) {
        return APP_LAYER_NAV_CYCLE_UP;
    }
    if (!ctrl && key == APP_KEY_PAGEDOWN) {
        return APP_LAYER_NAV_CYCLE_DOWN;
    }
    return APP_LAYER_NAV_NONE;
}

AppFileHotkeyAction app_file_hotkey_action(int key, int ctrl, int alt, int shift) {
    if (!ctrl || alt || shift) {
        return APP_FILE_HOTKEY_NONE;
    }
    if (key == APP_KEY_s) {
        return APP_FILE_HOTKEY_SAVE;
    }
    if (key == APP_KEY_o) {
        return APP_FILE_HOTKEY_LOAD_ACTIVE_LAYER;
    }
    return APP_FILE_HOTKEY_NONE;
}
