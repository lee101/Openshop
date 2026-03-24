#include "brush_shortcuts.h"

BrushShortcutAction brush_shortcut_action(int key) {
    switch (key) {
    case '[':
        return BRUSH_SHORTCUT_RADIUS_DOWN;
    case ']':
        return BRUSH_SHORTCUT_RADIUS_UP;
    case ',':
        return BRUSH_SHORTCUT_SHAPE_PREV;
    case '.':
        return BRUSH_SHORTCUT_SHAPE_NEXT;
    case '-':
        return BRUSH_SHORTCUT_OPACITY_DOWN;
    case '=':
    case '+':
        return BRUSH_SHORTCUT_OPACITY_UP;
    default:
        return BRUSH_SHORTCUT_NONE;
    }
}
