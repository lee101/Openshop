#include "merge_shortcuts.h"

MergeShortcutAction merge_shortcut_action(int ctrl, int key) {
    if (!ctrl) {
        return MERGE_SHORTCUT_NONE;
    }
    if (key == 'm') {
        return MERGE_SHORTCUT_DOWN;
    }
    if (key == 'u') {
        return MERGE_SHORTCUT_UP;
    }
    return MERGE_SHORTCUT_NONE;
}
