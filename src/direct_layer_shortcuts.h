#ifndef DIRECT_LAYER_SHORTCUTS_H
#define DIRECT_LAYER_SHORTCUTS_H

typedef enum {
    DIRECT_LAYER_SHORTCUT_NONE = 0,
    DIRECT_LAYER_SHORTCUT_SELECT,
    DIRECT_LAYER_SHORTCUT_SOLO,
    DIRECT_LAYER_SHORTCUT_TOGGLE_VISIBILITY,
    DIRECT_LAYER_SHORTCUT_TOGGLE_LOCK
} DirectLayerShortcutAction;

DirectLayerShortcutAction direct_layer_shortcut_action_from_modifiers(int ctrl, int alt, int shift);

#endif
