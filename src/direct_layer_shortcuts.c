#include "direct_layer_shortcuts.h"

DirectLayerShortcutAction direct_layer_shortcut_action_from_modifiers(int ctrl, int alt, int shift) {
    if (!ctrl) {
        return DIRECT_LAYER_SHORTCUT_NONE;
    }
    if (alt && shift) {
        return DIRECT_LAYER_SHORTCUT_TOGGLE_LOCK;
    }
    if (alt) {
        return DIRECT_LAYER_SHORTCUT_TOGGLE_VISIBILITY;
    }
    if (shift) {
        return DIRECT_LAYER_SHORTCUT_SOLO;
    }
    return DIRECT_LAYER_SHORTCUT_SELECT;
}
