#include "../src/app_brush.h"
#include "../src/app_shape.h"
#include "../src/app_title.h"
#include "../src/brush_shortcuts.h"
#include "../src/canvas_shortcuts.h"
#include "../src/direct_layer_shortcuts.h"
#include "../src/file_shortcuts.h"
#include "../src/history_shortcuts.h"
#include "../src/layer_name_shortcuts.h"
#include "../src/merge_shortcuts.h"
#include "../src/paint_shortcuts.h"
#include "../src/view_shortcuts.h"
#include <stdio.h>
#include <string.h>

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

static int expect_brush_action(const char *label, int key, BrushShortcutAction want) {
    BrushShortcutAction got = brush_shortcut_action(key);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_canvas_action(const char *label, int key, CanvasShortcutAction want) {
    CanvasShortcutAction got = canvas_shortcut_action(key);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_view_result(
    const char *label,
    ViewShortcutKey key,
    int shift,
    ViewShortcutAction want_action,
    int want_cycle_direction,
    int want_dx,
    int want_dy
) {
    ViewShortcutResult got = view_shortcut_result(key, shift);
    if (got.action != want_action ||
        got.cycle_direction != want_cycle_direction ||
        got.dx != want_dx ||
        got.dy != want_dy) {
        fprintf(
            stderr,
            "%s mismatch: got {%d,%d,%d,%d} want {%d,%d,%d,%d}\n",
            label,
            got.action,
            got.cycle_direction,
            got.dx,
            got.dy,
            want_action,
            want_cycle_direction,
            want_dx,
            want_dy
        );
        return 0;
    }
    return 1;
}

static int expect_title(
    const char *label,
    const char *tool_label,
    const char *shape_label,
    int radius,
    int opacity_percent,
    int active_layer_index,
    int layer_count,
    const char *layer_name,
    int active_visible,
    int active_locked,
    int active_opacity_percent,
    int active_is_solo,
    int visible_layer_count,
    unsigned int color,
    const char *want
) {
    char title[256];

    app_title_format(
        title,
        sizeof(title),
        tool_label,
        shape_label,
        radius,
        opacity_percent,
        active_layer_index,
        layer_count,
        layer_name,
        active_visible,
        active_locked,
        active_opacity_percent,
        active_is_solo,
        visible_layer_count,
        color
    );

    if (strcmp(title, want) != 0) {
        fprintf(stderr, "%s mismatch:\n got  %s\n want %s\n", label, title, want);
        return 0;
    }
    return 1;
}

static int expect_title_prefix(
    const char *label,
    size_t title_size,
    const char *tool_label,
    const char *shape_label,
    int radius,
    int opacity_percent,
    int active_layer_index,
    int layer_count,
    const char *layer_name,
    int active_visible,
    int active_locked,
    int active_opacity_percent,
    int active_is_solo,
    int visible_layer_count,
    unsigned int color,
    const char *want_prefix
) {
    char title[256];

    memset(title, 'Z', sizeof(title));
    app_title_format(
        title,
        title_size,
        tool_label,
        shape_label,
        radius,
        opacity_percent,
        active_layer_index,
        layer_count,
        layer_name,
        active_visible,
        active_locked,
        active_opacity_percent,
        active_is_solo,
        visible_layer_count,
        color
    );

    if (strncmp(title, want_prefix, title_size - 1) != 0) {
        fprintf(stderr, "%s prefix mismatch:\n got  %.*s\n want %s\n", label, (int)(title_size - 1), title, want_prefix);
        return 0;
    }
    if (title[title_size - 1] != '\0') {
        fprintf(stderr, "%s missing terminator at %zu\n", label, title_size - 1);
        return 0;
    }
    return 1;
}

static int expect_title_empty_buffer(
    const char *label,
    size_t title_size,
    const char *tool_label,
    const char *shape_label,
    int radius,
    int opacity_percent,
    int active_layer_index,
    int layer_count,
    const char *layer_name,
    int active_visible,
    int active_locked,
    int active_opacity_percent,
    int active_is_solo,
    int visible_layer_count,
    unsigned int color
) {
    char title[8];

    memset(title, 'Q', sizeof(title));
    app_title_format(
        title,
        title_size,
        tool_label,
        shape_label,
        radius,
        opacity_percent,
        active_layer_index,
        layer_count,
        layer_name,
        active_visible,
        active_locked,
        active_opacity_percent,
        active_is_solo,
        visible_layer_count,
        color
    );

    if (title[0] != '\0') {
        fprintf(stderr, "%s expected immediate terminator, got %d\n", label, (int)(unsigned char)title[0]);
        return 0;
    }
    return 1;
}

static int expect_title_unchanged_buffer(
    const char *label,
    size_t title_size,
    const char *tool_label,
    const char *shape_label,
    int radius,
    int opacity_percent,
    int active_layer_index,
    int layer_count,
    const char *layer_name,
    int active_visible,
    int active_locked,
    int active_opacity_percent,
    int active_is_solo,
    int visible_layer_count,
    unsigned int color
) {
    char title[8];

    memset(title, 'Q', sizeof(title));
    app_title_format(
        title,
        title_size,
        tool_label,
        shape_label,
        radius,
        opacity_percent,
        active_layer_index,
        layer_count,
        layer_name,
        active_visible,
        active_locked,
        active_opacity_percent,
        active_is_solo,
        visible_layer_count,
        color
    );

    for (size_t i = 0; i < sizeof(title); ++i) {
        if (title[i] != 'Q') {
            fprintf(stderr, "%s unexpectedly changed byte %zu to %d\n", label, i, (int)(unsigned char)title[i]);
            return 0;
        }
    }
    return 1;
}

static int expect_tool_label(const char *label, Tool tool, const char *want) {
    const char *got = app_tool_label(tool);
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "%s mismatch:\n got  %s\n want %s\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_brush_shape_label(const char *label, BrushShape shape, const char *want) {
    const char *got = app_brush_shape_label(shape);
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "%s mismatch:\n got  %s\n want %s\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_cycle_brush_shape(const char *label, BrushShape shape, int direction, BrushShape want) {
    BrushShape got = app_cycle_brush_shape(shape, direction);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_constrained_shape_end(
    const char *label,
    Tool tool,
    int x0,
    int y0,
    int x1,
    int y1,
    int shift,
    int want_x,
    int want_y
) {
    int got_x = -999;
    int got_y = -999;

    app_constrain_shape_end(tool, x0, y0, x1, y1, shift, &got_x, &got_y);
    if (got_x != want_x || got_y != want_y) {
        fprintf(stderr, "%s mismatch: got {%d,%d} want {%d,%d}\n", label, got_x, got_y, want_x, want_y);
        return 0;
    }
    return 1;
}

static int expect_constrained_shape_end_no_output(
    const char *label,
    Tool tool,
    int x0,
    int y0,
    int x1,
    int y1,
    int shift,
    int *out_x,
    int *out_y,
    int want_x,
    int want_y
) {
    app_constrain_shape_end(tool, x0, y0, x1, y1, shift, out_x, out_y);
    if (out_x && *out_x != want_x) {
        fprintf(stderr, "%s x changed: got %d want %d\n", label, *out_x, want_x);
        return 0;
    }
    if (out_y && *out_y != want_y) {
        fprintf(stderr, "%s y changed: got %d want %d\n", label, *out_y, want_y);
        return 0;
    }
    return 1;
}

int main(void) {
    int ok = 1;
    int sentinel_x = 321;
    int sentinel_y = 654;

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
    ok = ok && expect_brush_action("radius_down", '[', BRUSH_SHORTCUT_RADIUS_DOWN);
    ok = ok && expect_brush_action("radius_up", ']', BRUSH_SHORTCUT_RADIUS_UP);
    ok = ok && expect_brush_action("shape_prev", ',', BRUSH_SHORTCUT_SHAPE_PREV);
    ok = ok && expect_brush_action("shape_next", '.', BRUSH_SHORTCUT_SHAPE_NEXT);
    ok = ok && expect_brush_action("opacity_down", '-', BRUSH_SHORTCUT_OPACITY_DOWN);
    ok = ok && expect_brush_action("opacity_up_equals", '=', BRUSH_SHORTCUT_OPACITY_UP);
    ok = ok && expect_brush_action("opacity_up_plus", '+', BRUSH_SHORTCUT_OPACITY_UP);
    ok = ok && expect_brush_action("brush_other_key", '/', BRUSH_SHORTCUT_NONE);
    ok = ok && expect_canvas_action("canvas_clear", 'c', CANVAS_SHORTCUT_CLEAR);
    ok = ok && expect_canvas_action("canvas_flip_h", 'h', CANVAS_SHORTCUT_FLIP_HORIZONTAL);
    ok = ok && expect_canvas_action("canvas_flip_v", 'v', CANVAS_SHORTCUT_FLIP_VERTICAL);
    ok = ok && expect_canvas_action("canvas_rotate_180", 'j', CANVAS_SHORTCUT_ROTATE_180);
    ok = ok && expect_canvas_action("canvas_invert_rgb", 'x', CANVAS_SHORTCUT_INVERT_RGB);
    ok = ok && expect_canvas_action("canvas_fill", 'f', CANVAS_SHORTCUT_FILL);
    ok = ok && expect_canvas_action("canvas_eyedropper", 'i', CANVAS_SHORTCUT_EYEDROPPER);
    ok = ok && expect_canvas_action("canvas_other_key", 'k', CANVAS_SHORTCUT_NONE);
    ok = ok && expect_view_result("pageup", VIEW_SHORTCUT_KEY_PAGEUP, 0, VIEW_SHORTCUT_CYCLE, 1, 0, 0);
    ok = ok && expect_view_result("pagedown", VIEW_SHORTCUT_KEY_PAGEDOWN, 1, VIEW_SHORTCUT_CYCLE, -1, 0, 0);
    ok = ok && expect_view_result("up", VIEW_SHORTCUT_KEY_UP, 0, VIEW_SHORTCUT_TRANSLATE, 0, 0, -1);
    ok = ok && expect_view_result("down_shift", VIEW_SHORTCUT_KEY_DOWN, 1, VIEW_SHORTCUT_TRANSLATE, 0, 0, 10);
    ok = ok && expect_view_result("left", VIEW_SHORTCUT_KEY_LEFT, 0, VIEW_SHORTCUT_TRANSLATE, 0, -1, 0);
    ok = ok && expect_view_result("right_shift", VIEW_SHORTCUT_KEY_RIGHT, 1, VIEW_SHORTCUT_TRANSLATE, 0, 10, 0);
    ok = ok && expect_view_result("view_none", VIEW_SHORTCUT_KEY_NONE, 0, VIEW_SHORTCUT_NONE, 0, 0, 0);
    ok = ok && expect_tool_label("tool_brush_label", TOOL_BRUSH, "Brush");
    ok = ok && expect_tool_label("tool_filled_ellipse_label", TOOL_FILLED_ELLIPSE, "Filled Ellipse");
    ok = ok && expect_tool_label("tool_label_default", (Tool)999, "Brush");
    ok = ok && expect_brush_shape_label("brush_round_label", BRUSH_SHAPE_ROUND, "Round");
    ok = ok && expect_brush_shape_label("brush_diamond_label", BRUSH_SHAPE_DIAMOND, "Diamond");
    ok = ok && expect_brush_shape_label("brush_shape_label_default", (BrushShape)999, "Round");
    ok = ok && expect_cycle_brush_shape("brush_shape_cycle_forward", BRUSH_SHAPE_ROUND, 1, BRUSH_SHAPE_SQUARE);
    ok = ok && expect_cycle_brush_shape("brush_shape_cycle_wrap_forward", BRUSH_SHAPE_DIAMOND, 1, BRUSH_SHAPE_ROUND);
    ok = ok && expect_cycle_brush_shape("brush_shape_cycle_wrap_backward", BRUSH_SHAPE_ROUND, -1, BRUSH_SHAPE_DIAMOND);
    ok = ok && expect_constrained_shape_end("shape_line_horizontal_snap", TOOL_LINE, 10, 10, 25, 13, 1, 25, 10);
    ok = ok && expect_constrained_shape_end("shape_line_vertical_snap", TOOL_LINE, 10, 10, 13, 25, 1, 10, 25);
    ok = ok && expect_constrained_shape_end("shape_line_diagonal_snap", TOOL_LINE, 10, 10, 18, 15, 1, 18, 18);
    ok = ok && expect_constrained_shape_end("shape_rect_square_snap", TOOL_RECT, 10, 10, 14, 18, 1, 18, 18);
    ok = ok && expect_constrained_shape_end("shape_no_shift_passthrough", TOOL_ELLIPSE, 10, 10, 14, 18, 0, 14, 18);
    ok = ok && expect_constrained_shape_end_no_output("shape_null_out_x", TOOL_LINE, 10, 10, 25, 13, 1, NULL, &sentinel_y, 0, 654);
    ok = ok && expect_constrained_shape_end_no_output("shape_null_out_y", TOOL_LINE, 10, 10, 25, 13, 1, &sentinel_x, NULL, 321, 0);
    ok = ok && expect_title(
        "title_visible_locked_solo",
        "Brush",
        "Round",
        6,
        100,
        0,
        3,
        "Ink",
        1,
        1,
        75,
        1,
        2,
        0xFF1B1F24u,
        "Openshop - Brush (Round) | size 6 | brush 100% | layer 1/3 Ink [visible, locked 75%] [solo] | visible 2/3 | #FF1B1F24"
    );
    ok = ok && expect_title(
        "title_hidden_default_name",
        "Line",
        "Square",
        12,
        40,
        2,
        4,
        "",
        0,
        0,
        100,
        0,
        1,
        0x80123456u,
        "Openshop - Line (Square) | size 12 | brush 40% | layer 3/4 Layer [hidden 100%] | visible 1/4 | #80123456"
    );
    ok = ok && expect_title_prefix(
        "title_truncates",
        16,
        "Filled Rectangle",
        "Diamond",
        99,
        55,
        8,
        12,
        "Very Long Layer Name",
        1,
        0,
        42,
        0,
        7,
        0xABCDEF12u,
        "Openshop - Fill"
    );
    ok = ok && expect_title(
        "title_null_labels_default",
        NULL,
        NULL,
        5,
        20,
        1,
        2,
        NULL,
        1,
        0,
        100,
        0,
        2,
        0x00000000u,
        "Openshop - Brush (Round) | size 5 | brush 20% | layer 2/2 Layer [visible 100%] | visible 2/2 | #00000000"
    );
    ok = ok && expect_title(
        "title_extreme_numeric_values",
        "Ellipse",
        "Diamond",
        0,
        0,
        98,
        120,
        "Edge",
        0,
        1,
        0,
        0,
        0,
        0xFFFFFFFFu,
        "Openshop - Ellipse (Diamond) | size 0 | brush 0% | layer 99/120 Edge [hidden, locked 0%] | visible 0/120 | #FFFFFFFF"
    );
    ok = ok && expect_title_empty_buffer(
        "title_size_one",
        1,
        "Brush",
        "Round",
        3,
        10,
        0,
        1,
        "Tiny",
        1,
        0,
        100,
        0,
        1,
        0xFF000000u
    );
    ok = ok && expect_title_unchanged_buffer(
        "title_size_zero",
        0,
        "Brush",
        "Round",
        3,
        10,
        0,
        1,
        "Tiny",
        1,
        0,
        100,
        0,
        1,
        0xFF000000u
    );

    if (!ok) {
        return 1;
    }

    puts("shortcut selftest ok");
    return 0;
}
