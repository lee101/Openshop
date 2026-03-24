#include "../src/layer_name_shortcuts.h"
#include <stdio.h>

static int expect_shortcut(const char *label, int ctrl, int alt, int shift, LayerNameResetShortcut want) {
    LayerNameResetShortcut got = layer_name_reset_shortcut_from_modifiers(ctrl, alt, shift);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

int main(void) {
    int ok = 1;

    ok = ok && expect_shortcut("plain_f2", 0, 0, 0, LAYER_NAME_RESET_SHORTCUT_ACTIVE);
    ok = ok && expect_shortcut("ctrl_f2", 1, 0, 0, LAYER_NAME_RESET_SHORTCUT_ALL);
    ok = ok && expect_shortcut("ctrl_shift_f2", 1, 0, 1, LAYER_NAME_RESET_SHORTCUT_UNLOCKED);
    ok = ok && expect_shortcut("ctrl_alt_f2", 1, 1, 0, LAYER_NAME_RESET_SHORTCUT_VISIBLE);
    ok = ok && expect_shortcut("ctrl_alt_shift_f2", 1, 1, 1, LAYER_NAME_RESET_SHORTCUT_NON_BACKGROUND_UNLOCKED);
    ok = ok && expect_shortcut("alt_f2", 0, 1, 0, LAYER_NAME_RESET_SHORTCUT_LOCKED);
    ok = ok && expect_shortcut("alt_shift_f2", 0, 1, 1, LAYER_NAME_RESET_SHORTCUT_NON_BACKGROUND_VISIBLE);
    ok = ok && expect_shortcut("shift_f2", 0, 0, 1, LAYER_NAME_RESET_SHORTCUT_NON_BACKGROUND_LOCKED);

    if (!ok) {
        return 1;
    }

    puts("shortcut selftest ok");
    return 0;
}
