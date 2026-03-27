#include "app_shape_cancel.h"

int app_should_cancel_shape_on_key(AppShapeCancelKey key, int ctrl) {
    if (key == APP_SHAPE_CANCEL_KEY_ESCAPE || key == APP_SHAPE_CANCEL_KEY_LSHIFT || key == APP_SHAPE_CANCEL_KEY_RSHIFT) {
        return 0;
    }
    if (ctrl && (key == APP_SHAPE_CANCEL_KEY_S || key == APP_SHAPE_CANCEL_KEY_O || key == APP_SHAPE_CANCEL_KEY_Z ||
                 key == APP_SHAPE_CANCEL_KEY_Y || key == APP_SHAPE_CANCEL_KEY_N || key == APP_SHAPE_CANCEL_KEY_U ||
                 key == APP_SHAPE_CANCEL_KEY_V || key == APP_SHAPE_CANCEL_KEY_M || key == APP_SHAPE_CANCEL_KEY_D ||
                 key == APP_SHAPE_CANCEL_KEY_E || key == APP_SHAPE_CANCEL_KEY_G || key == APP_SHAPE_CANCEL_KEY_H ||
                 key == APP_SHAPE_CANCEL_KEY_L || key == APP_SHAPE_CANCEL_KEY_A || key == APP_SHAPE_CANCEL_KEY_R ||
                 key == APP_SHAPE_CANCEL_KEY_0 || key == APP_SHAPE_CANCEL_KEY_COMMA ||
                 key == APP_SHAPE_CANCEL_KEY_LEFTBRACKET || key == APP_SHAPE_CANCEL_KEY_RIGHTBRACKET ||
                 key == APP_SHAPE_CANCEL_KEY_MINUS || key == APP_SHAPE_CANCEL_KEY_KP_MINUS ||
                 key == APP_SHAPE_CANCEL_KEY_EQUALS || key == APP_SHAPE_CANCEL_KEY_KP_PLUS ||
                 key == APP_SHAPE_CANCEL_KEY_SLASH || key == APP_SHAPE_CANCEL_KEY_1 || key == APP_SHAPE_CANCEL_KEY_2 ||
                 key == APP_SHAPE_CANCEL_KEY_3 || key == APP_SHAPE_CANCEL_KEY_4 || key == APP_SHAPE_CANCEL_KEY_5 ||
                 key == APP_SHAPE_CANCEL_KEY_6 || key == APP_SHAPE_CANCEL_KEY_7 || key == APP_SHAPE_CANCEL_KEY_8)) {
        return 1;
    }

    switch (key) {
    case APP_SHAPE_CANCEL_KEY_B:
    case APP_SHAPE_CANCEL_KEY_E:
    case APP_SHAPE_CANCEL_KEY_L:
    case APP_SHAPE_CANCEL_KEY_R:
    case APP_SHAPE_CANCEL_KEY_T:
    case APP_SHAPE_CANCEL_KEY_O:
    case APP_SHAPE_CANCEL_KEY_P:
    case APP_SHAPE_CANCEL_KEY_LEFTBRACKET:
    case APP_SHAPE_CANCEL_KEY_RIGHTBRACKET:
    case APP_SHAPE_CANCEL_KEY_COMMA:
    case APP_SHAPE_CANCEL_KEY_PERIOD:
    case APP_SHAPE_CANCEL_KEY_MINUS:
    case APP_SHAPE_CANCEL_KEY_KP_MINUS:
    case APP_SHAPE_CANCEL_KEY_EQUALS:
    case APP_SHAPE_CANCEL_KEY_KP_PLUS:
    case APP_SHAPE_CANCEL_KEY_1:
    case APP_SHAPE_CANCEL_KEY_2:
    case APP_SHAPE_CANCEL_KEY_3:
    case APP_SHAPE_CANCEL_KEY_4:
    case APP_SHAPE_CANCEL_KEY_5:
    case APP_SHAPE_CANCEL_KEY_6:
    case APP_SHAPE_CANCEL_KEY_C:
    case APP_SHAPE_CANCEL_KEY_H:
    case APP_SHAPE_CANCEL_KEY_V:
    case APP_SHAPE_CANCEL_KEY_J:
    case APP_SHAPE_CANCEL_KEY_X:
    case APP_SHAPE_CANCEL_KEY_F:
    case APP_SHAPE_CANCEL_KEY_I:
    case APP_SHAPE_CANCEL_KEY_UP:
    case APP_SHAPE_CANCEL_KEY_DOWN:
    case APP_SHAPE_CANCEL_KEY_LEFT:
    case APP_SHAPE_CANCEL_KEY_RIGHT:
    case APP_SHAPE_CANCEL_KEY_PAGEUP:
    case APP_SHAPE_CANCEL_KEY_PAGEDOWN:
    case APP_SHAPE_CANCEL_KEY_F2:
    case APP_SHAPE_CANCEL_KEY_DELETE:
    case APP_SHAPE_CANCEL_KEY_BACKSPACE:
        return 1;
    default:
        return 0;
    }
}
