#ifndef HISTORY_SHORTCUTS_H
#define HISTORY_SHORTCUTS_H

typedef enum {
    HISTORY_SHORTCUT_NONE = 0,
    HISTORY_SHORTCUT_UNDO,
    HISTORY_SHORTCUT_REDO
} HistoryShortcutAction;

HistoryShortcutAction history_shortcut_action(int ctrl, int key);

#endif
