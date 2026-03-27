#include "view_shortcuts.h"

ViewShortcutResult view_shortcut_result(ViewShortcutKey key, int shift) {
    ViewShortcutResult result = {VIEW_SHORTCUT_NONE, 0, 0, 0};
    int step = shift ? 10 : 1;

    switch (key) {
    case VIEW_SHORTCUT_KEY_PAGEUP:
        result.action = VIEW_SHORTCUT_CYCLE;
        result.cycle_direction = 1;
        break;
    case VIEW_SHORTCUT_KEY_PAGEDOWN:
        result.action = VIEW_SHORTCUT_CYCLE;
        result.cycle_direction = -1;
        break;
    case VIEW_SHORTCUT_KEY_UP:
        result.action = VIEW_SHORTCUT_TRANSLATE;
        result.dy = -step;
        break;
    case VIEW_SHORTCUT_KEY_DOWN:
        result.action = VIEW_SHORTCUT_TRANSLATE;
        result.dy = step;
        break;
    case VIEW_SHORTCUT_KEY_LEFT:
        result.action = VIEW_SHORTCUT_TRANSLATE;
        result.dx = -step;
        break;
    case VIEW_SHORTCUT_KEY_RIGHT:
        result.action = VIEW_SHORTCUT_TRANSLATE;
        result.dx = step;
        break;
    case VIEW_SHORTCUT_KEY_NONE:
    default:
        break;
    }

    return result;
}
