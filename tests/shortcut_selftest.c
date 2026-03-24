#include "../src/direct_layer_shortcuts.h"
#include "../src/file_shortcuts.h"
#include "../src/history_shortcuts.h"
#include "../src/layer_name_shortcuts.h"
#include "../src/merge_shortcuts.h"
#include "../src/paint_shortcuts.h"
#include <stdio.h>

static int expect_shortcut(const char *label, int ctrl, int alt, int shift, LayerNameResetShortcut want) {
    LayerNameResetShortcut got = layer_name_reset_shortcut_from_modifiers(ctrl, alt, shift);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_direct_action(const char *label, int ctrl, int alt, int shift, DirectLayerShortcutAction want) {
    DirectLayerShortcutAction got = direct_layer_shortcut_action_from_modifiers(ctrl, alt, shift);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_history_action(const char *label, int ctrl, int key, HistoryShortcutAction want) {
    HistoryShortcutAction got = history_shortcut_action(ctrl, key);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_file_action(const char *label, int ctrl, int key, FileShortcutAction want) {
    FileShortcutAction got = file_shortcut_action(ctrl, key);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_merge_action(const char *label, int ctrl, int key, MergeShortcutAction want) {
    MergeShortcutAction got = merge_shortcut_action(ctrl, key);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_paint_action(const char *label, int key, PaintShortcutAction want) {
    PaintShortcutAction got = paint_shortcut_action(key);
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
    ok = ok && expect_direct_action("plain_number", 1, 0, 0, DIRECT_LAYER_SHORTCUT_SELECT);
    ok = ok && expect_direct_action("shift_number", 1, 0, 1, DIRECT_LAYER_SHORTCUT_SOLO);
    ok = ok && expect_direct_action("alt_number", 1, 1, 0, DIRECT_LAYER_SHORTCUT_TOGGLE_VISIBILITY);
    ok = ok && expect_direct_action("alt_shift_number", 1, 1, 1, DIRECT_LAYER_SHORTCUT_TOGGLE_LOCK);
    ok = ok && expect_direct_action("missing_ctrl", 0, 0, 0, DIRECT_LAYER_SHORTCUT_NONE);
    ok = ok && expect_history_action("undo", 1, 'z', HISTORY_SHORTCUT_UNDO);
    ok = ok && expect_history_action("redo", 1, 'y', HISTORY_SHORTCUT_REDO);
    ok = ok && expect_history_action("missing_ctrl_history", 0, 'z', HISTORY_SHORTCUT_NONE);
    ok = ok && expect_history_action("other_key_history", 1, 'x', HISTORY_SHORTCUT_NONE);
    ok = ok && expect_file_action("save", 1, 's', FILE_SHORTCUT_SAVE);
    ok = ok && expect_file_action("load", 1, 'o', FILE_SHORTCUT_LOAD);
    ok = ok && expect_file_action("missing_ctrl_file", 0, 's', FILE_SHORTCUT_NONE);
    ok = ok && expect_file_action("other_key_file", 1, 'p', FILE_SHORTCUT_NONE);
    ok = ok && expect_merge_action("merge_down", 1, 'm', MERGE_SHORTCUT_DOWN);
    ok = ok && expect_merge_action("merge_up", 1, 'u', MERGE_SHORTCUT_UP);
    ok = ok && expect_merge_action("missing_ctrl_merge", 0, 'm', MERGE_SHORTCUT_NONE);
    ok = ok && expect_merge_action("other_key_merge", 1, 'q', MERGE_SHORTCUT_NONE);
    ok = ok && expect_paint_action("tool_brush", 'b', PAINT_SHORTCUT_TOOL_BRUSH);
    ok = ok && expect_paint_action("tool_eraser", 'e', PAINT_SHORTCUT_TOOL_ERASER);
    ok = ok && expect_paint_action("tool_line", 'l', PAINT_SHORTCUT_TOOL_LINE);
    ok = ok && expect_paint_action("tool_rect", 'r', PAINT_SHORTCUT_TOOL_RECT);
    ok = ok && expect_paint_action("tool_filled_rect", 't', PAINT_SHORTCUT_TOOL_FILLED_RECT);
    ok = ok && expect_paint_action("tool_ellipse", 'o', PAINT_SHORTCUT_TOOL_ELLIPSE);
    ok = ok && expect_paint_action("tool_filled_ellipse", 'p', PAINT_SHORTCUT_TOOL_FILLED_ELLIPSE);
    ok = ok && expect_paint_action("color_brush", '1', PAINT_SHORTCUT_COLOR_BRUSH);
    ok = ok && expect_paint_action("color_red", '2', PAINT_SHORTCUT_COLOR_RED);
    ok = ok && expect_paint_action("color_green", '3', PAINT_SHORTCUT_COLOR_GREEN);
    ok = ok && expect_paint_action("color_blue", '4', PAINT_SHORTCUT_COLOR_BLUE);
    ok = ok && expect_paint_action("color_yellow", '5', PAINT_SHORTCUT_COLOR_YELLOW);
    ok = ok && expect_paint_action("color_purple", '6', PAINT_SHORTCUT_COLOR_PURPLE);
    ok = ok && expect_paint_action("paint_other_key", '7', PAINT_SHORTCUT_NONE);

    if (!ok) {
        return 1;
    }

    puts("shortcut selftest ok");
    return 0;
}
