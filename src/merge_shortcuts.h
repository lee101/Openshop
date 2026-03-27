#ifndef MERGE_SHORTCUTS_H
#define MERGE_SHORTCUTS_H

typedef enum {
    MERGE_SHORTCUT_NONE = 0,
    MERGE_SHORTCUT_DOWN,
    MERGE_SHORTCUT_UP
} MergeShortcutAction;

MergeShortcutAction merge_shortcut_action(int ctrl, int key);

#endif
