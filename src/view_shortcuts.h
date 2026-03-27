#ifndef VIEW_SHORTCUTS_H
#define VIEW_SHORTCUTS_H

typedef enum {
    VIEW_SHORTCUT_KEY_NONE = 0,
    VIEW_SHORTCUT_KEY_PAGEUP,
    VIEW_SHORTCUT_KEY_PAGEDOWN,
    VIEW_SHORTCUT_KEY_UP,
    VIEW_SHORTCUT_KEY_DOWN,
    VIEW_SHORTCUT_KEY_LEFT,
    VIEW_SHORTCUT_KEY_RIGHT
} ViewShortcutKey;

typedef enum {
    VIEW_SHORTCUT_NONE = 0,
    VIEW_SHORTCUT_CYCLE,
    VIEW_SHORTCUT_TRANSLATE
} ViewShortcutAction;

typedef struct {
    ViewShortcutAction action;
    int cycle_direction;
    int dx;
    int dy;
} ViewShortcutResult;

ViewShortcutResult view_shortcut_result(ViewShortcutKey key, int shift);

#endif
