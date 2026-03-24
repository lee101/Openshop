#include "canvas_shortcuts.h"

CanvasShortcutAction canvas_shortcut_action(int key) {
    switch (key) {
    case 'c':
        return CANVAS_SHORTCUT_CLEAR;
    case 'h':
        return CANVAS_SHORTCUT_FLIP_HORIZONTAL;
    case 'v':
        return CANVAS_SHORTCUT_FLIP_VERTICAL;
    case 'j':
        return CANVAS_SHORTCUT_ROTATE_180;
    case 'x':
        return CANVAS_SHORTCUT_INVERT_RGB;
    case 'f':
        return CANVAS_SHORTCUT_FILL;
    case 'i':
        return CANVAS_SHORTCUT_EYEDROPPER;
    default:
        return CANVAS_SHORTCUT_NONE;
    }
}
