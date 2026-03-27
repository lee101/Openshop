#include "paint_shortcuts.h"

PaintShortcutAction paint_shortcut_action(int key) {
    switch (key) {
    case 'b':
        return PAINT_SHORTCUT_TOOL_BRUSH;
    case 'e':
        return PAINT_SHORTCUT_TOOL_ERASER;
    case 'l':
        return PAINT_SHORTCUT_TOOL_LINE;
    case 'r':
        return PAINT_SHORTCUT_TOOL_RECT;
    case 't':
        return PAINT_SHORTCUT_TOOL_FILLED_RECT;
    case 'o':
        return PAINT_SHORTCUT_TOOL_ELLIPSE;
    case 'p':
        return PAINT_SHORTCUT_TOOL_FILLED_ELLIPSE;
    case '1':
        return PAINT_SHORTCUT_COLOR_BRUSH;
    case '2':
        return PAINT_SHORTCUT_COLOR_RED;
    case '3':
        return PAINT_SHORTCUT_COLOR_GREEN;
    case '4':
        return PAINT_SHORTCUT_COLOR_BLUE;
    case '5':
        return PAINT_SHORTCUT_COLOR_YELLOW;
    case '6':
        return PAINT_SHORTCUT_COLOR_PURPLE;
    default:
        return PAINT_SHORTCUT_NONE;
    }
}
