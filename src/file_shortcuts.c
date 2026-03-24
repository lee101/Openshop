#include "file_shortcuts.h"

FileShortcutAction file_shortcut_action(int ctrl, int key) {
    if (!ctrl) {
        return FILE_SHORTCUT_NONE;
    }
    if (key == 's') {
        return FILE_SHORTCUT_SAVE;
    }
    if (key == 'o') {
        return FILE_SHORTCUT_LOAD;
    }
    return FILE_SHORTCUT_NONE;
}
