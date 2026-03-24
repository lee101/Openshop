#include "layer_name_shortcuts.h"

LayerNameResetShortcut layer_name_reset_shortcut_from_modifiers(int ctrl, int alt, int shift) {
    if (ctrl && alt && shift) {
        return LAYER_NAME_RESET_SHORTCUT_NON_BACKGROUND_UNLOCKED;
    }
    if (ctrl && alt) {
        return LAYER_NAME_RESET_SHORTCUT_VISIBLE;
    }
    if (ctrl && shift) {
        return LAYER_NAME_RESET_SHORTCUT_UNLOCKED;
    }
    if (ctrl) {
        return LAYER_NAME_RESET_SHORTCUT_ALL;
    }
    if (alt && shift) {
        return LAYER_NAME_RESET_SHORTCUT_NON_BACKGROUND_VISIBLE;
    }
    if (alt) {
        return LAYER_NAME_RESET_SHORTCUT_LOCKED;
    }
    if (shift) {
        return LAYER_NAME_RESET_SHORTCUT_NON_BACKGROUND_LOCKED;
    }
    return LAYER_NAME_RESET_SHORTCUT_ACTIVE;
}
