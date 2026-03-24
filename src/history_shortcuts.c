#include "history_shortcuts.h"

HistoryShortcutAction history_shortcut_action(int ctrl, int key) {
    if (!ctrl) {
        return HISTORY_SHORTCUT_NONE;
    }
    if (key == 'z') {
        return HISTORY_SHORTCUT_UNDO;
    }
    if (key == 'y') {
        return HISTORY_SHORTCUT_REDO;
    }
    return HISTORY_SHORTCUT_NONE;
}
