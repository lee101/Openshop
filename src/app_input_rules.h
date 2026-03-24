#ifndef APP_INPUT_RULES_H
#define APP_INPUT_RULES_H

enum {
    APP_KEY_BACKSPACE = 8,
    APP_KEY_TAB = 9,
    APP_KEY_ESCAPE = 27,
    APP_KEY_QUOTE = '\'',
    APP_KEY_COMMA = ',',
    APP_KEY_MINUS = '-',
    APP_KEY_PERIOD = '.',
    APP_KEY_SLASH = '/',
    APP_KEY_0 = '0',
    APP_KEY_1 = '1',
    APP_KEY_2 = '2',
    APP_KEY_3 = '3',
    APP_KEY_4 = '4',
    APP_KEY_5 = '5',
    APP_KEY_6 = '6',
    APP_KEY_7 = '7',
    APP_KEY_8 = '8',
    APP_KEY_SEMICOLON = ';',
    APP_KEY_EQUALS = '=',
    APP_KEY_LEFTBRACKET = '[',
    APP_KEY_RIGHTBRACKET = ']',
    APP_KEY_a = 'a',
    APP_KEY_b = 'b',
    APP_KEY_c = 'c',
    APP_KEY_d = 'd',
    APP_KEY_e = 'e',
    APP_KEY_f = 'f',
    APP_KEY_g = 'g',
    APP_KEY_h = 'h',
    APP_KEY_i = 'i',
    APP_KEY_j = 'j',
    APP_KEY_l = 'l',
    APP_KEY_m = 'm',
    APP_KEY_n = 'n',
    APP_KEY_o = 'o',
    APP_KEY_p = 'p',
    APP_KEY_r = 'r',
    APP_KEY_s = 's',
    APP_KEY_t = 't',
    APP_KEY_u = 'u',
    APP_KEY_v = 'v',
    APP_KEY_x = 'x',
    APP_KEY_y = 'y',
    APP_KEY_z = 'z',
    APP_KEY_DELETE = 127,
    APP_KEY_UP = 1073741906,
    APP_KEY_DOWN = 1073741905,
    APP_KEY_LEFT = 1073741904,
    APP_KEY_RIGHT = 1073741903,
    APP_KEY_PAGEUP = 1073741899,
    APP_KEY_PAGEDOWN = 1073741902,
    APP_KEY_KP_MINUS = 1073741910,
    APP_KEY_KP_PLUS = 1073741911,
    APP_KEY_LSHIFT = 1073742049,
    APP_KEY_RSHIFT = 1073742053
};

int app_key_translation_delta(int key, int step, int *dx, int *dy);
int app_should_cancel_shape_on_key(int key, int ctrl);

typedef enum {
    APP_OPACITY_HOTKEY_NONE = 0,
    APP_OPACITY_HOTKEY_SET_MAX,
    APP_OPACITY_HOTKEY_NUDGE_DOWN,
    APP_OPACITY_HOTKEY_NUDGE_UP
} AppOpacityHotkeyAction;

typedef enum {
    APP_LAYER_NAV_NONE = 0,
    APP_LAYER_NAV_SELECT_INDEX,
    APP_LAYER_NAV_CYCLE_UP,
    APP_LAYER_NAV_CYCLE_DOWN
} AppLayerNavigationAction;

typedef enum {
    APP_FILE_HOTKEY_NONE = 0,
    APP_FILE_HOTKEY_SAVE,
    APP_FILE_HOTKEY_LOAD_ACTIVE_LAYER
} AppFileHotkeyAction;

typedef enum {
    APP_HISTORY_HOTKEY_NONE = 0,
    APP_HISTORY_HOTKEY_UNDO,
    APP_HISTORY_HOTKEY_REDO
} AppHistoryHotkeyAction;

typedef enum {
    APP_BRUSH_ADJUST_NONE = 0,
    APP_BRUSH_ADJUST_RADIUS_DOWN,
    APP_BRUSH_ADJUST_RADIUS_UP,
    APP_BRUSH_ADJUST_SHAPE_PREV,
    APP_BRUSH_ADJUST_SHAPE_NEXT,
    APP_BRUSH_ADJUST_OPACITY_DOWN,
    APP_BRUSH_ADJUST_OPACITY_UP
} AppBrushAdjustAction;

typedef enum {
    APP_ACTIVE_EDIT_NONE = 0,
    APP_ACTIVE_EDIT_CLEAR,
    APP_ACTIVE_EDIT_FLIP_HORIZONTAL,
    APP_ACTIVE_EDIT_FLIP_VERTICAL,
    APP_ACTIVE_EDIT_ROTATE_180,
    APP_ACTIVE_EDIT_INVERT_RGB
} AppActiveEditAction;

typedef enum {
    APP_MOUSE_POSITION_NONE = 0,
    APP_MOUSE_POSITION_FILL,
    APP_MOUSE_POSITION_SAMPLE
} AppMousePositionAction;

AppOpacityHotkeyAction app_opacity_hotkey_action(int key);
AppLayerNavigationAction app_layer_navigation_action(int key, int ctrl, int alt, int shift, int *arg);
AppFileHotkeyAction app_file_hotkey_action(int key, int ctrl, int alt, int shift);
AppHistoryHotkeyAction app_history_hotkey_action(int key, int ctrl, int alt, int shift);
AppBrushAdjustAction app_brush_adjust_hotkey_action(int key);
int app_is_add_layer_hotkey(int key, int ctrl, int alt, int shift);
AppActiveEditAction app_active_edit_hotkey_action(int key);
AppMousePositionAction app_mouse_position_hotkey_action(int key);

#endif
