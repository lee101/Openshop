#ifndef FILE_SHORTCUTS_H
#define FILE_SHORTCUTS_H

typedef enum {
    FILE_SHORTCUT_NONE = 0,
    FILE_SHORTCUT_SAVE,
    FILE_SHORTCUT_LOAD
} FileShortcutAction;

FileShortcutAction file_shortcut_action(int ctrl, int key);

#endif
