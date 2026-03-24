#include "../src/app_brush.h"
#include "../src/app_brush_mask.h"
#include "../src/app_color.h"
#include "../src/app_layer_state.h"
#include "../src/app_preview.h"
#include "../src/app_sampled_color.h"
#include "../src/app_shape.h"
#include "../src/app_shape_cancel.h"
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

static int expect_shape_cancel(const char *label, int key, int ctrl, int want) {
    int got = app_should_cancel_shape_on_key(key, ctrl);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_begin_shape_preview(
    const char *label,
    int start_x,
    int start_y,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y,
    uint32_t *shape_base_pixels,
    const uint32_t *composite_pixels,
    size_t pixel_count,
    int want_shaping,
    int want_shape_start_x,
    int want_shape_start_y,
    const uint32_t *want_shape_base_pixels
) {
    size_t i;

    app_begin_shape_preview(
        start_x,
        start_y,
        shaping,
        shape_start_x,
        shape_start_y,
        shape_base_pixels,
        composite_pixels,
        pixel_count
    );

    if (shaping && *shaping != want_shaping) {
        fprintf(stderr, "%s shaping mismatch: got %d want %d\n", label, *shaping, want_shaping);
        return 0;
    }
    if (shape_start_x && *shape_start_x != want_shape_start_x) {
        fprintf(stderr, "%s shape_start_x mismatch: got %d want %d\n", label, *shape_start_x, want_shape_start_x);
        return 0;
    }
    if (shape_start_y && *shape_start_y != want_shape_start_y) {
        fprintf(stderr, "%s shape_start_y mismatch: got %d want %d\n", label, *shape_start_y, want_shape_start_y);
        return 0;
    }
    if (shape_base_pixels && want_shape_base_pixels) {
        for (i = 0; i < pixel_count; i++) {
            if (shape_base_pixels[i] != want_shape_base_pixels[i]) {
                fprintf(stderr, "%s shape_base_pixels[%zu] mismatch: got 0x%08X want 0x%08X\n", label, i, shape_base_pixels[i], want_shape_base_pixels[i]);
                return 0;
            }
        }
    }
    return 1;
}

typedef struct {
    const char *label;
    int start_x;
    int start_y;
    int initial_shaping;
    int initial_shape_start_x;
    int initial_shape_start_y;
    uint32_t *shape_base_pixels;
    const uint32_t *composite_pixels;
    size_t pixel_count;
    int want_shaping;
    int want_shape_start_x;
    int want_shape_start_y;
    const uint32_t *want_shape_base_pixels;
} BeginShapePreviewCase;

static int run_begin_shape_preview_case(
    const BeginShapePreviewCase *test_case,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y
) {
    *shaping = test_case->initial_shaping;
    *shape_start_x = test_case->initial_shape_start_x;
    *shape_start_y = test_case->initial_shape_start_y;

    return expect_begin_shape_preview(
        test_case->label,
        test_case->start_x,
        test_case->start_y,
        shaping,
        shape_start_x,
        shape_start_y,
        test_case->shape_base_pixels,
        test_case->composite_pixels,
        test_case->pixel_count,
        test_case->want_shaping,
        test_case->want_shape_start_x,
        test_case->want_shape_start_y,
        test_case->want_shape_base_pixels
    );
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

static int expect_brush_color(const char *label, unsigned int rgb_color, int opacity_percent, unsigned int want) {
    unsigned int got = app_compose_brush_color(rgb_color, opacity_percent);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_brush_mask(const char *label, BrushShape shape, int x, int y, int radius, int want) {
    int got = app_brush_mask_contains(shape, x, y, radius);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_sampled_brush_color(
    const char *label,
    unsigned int sampled_color,
    Tool initial_tool,
    unsigned int initial_brush_color,
    unsigned int initial_brush_color_rgb,
    int initial_brush_opacity,
    Tool want_tool,
    unsigned int want_brush_color,
    unsigned int want_brush_color_rgb,
    int want_brush_opacity
) {
    Tool tool = initial_tool;
    unsigned int brush_color = initial_brush_color;
    unsigned int brush_color_rgb = initial_brush_color_rgb;
    int brush_opacity = initial_brush_opacity;

    app_apply_sampled_brush_color(sampled_color, &tool, &brush_color, &brush_color_rgb, &brush_opacity);
    if (tool != want_tool || brush_color != want_brush_color || brush_color_rgb != want_brush_color_rgb || brush_opacity != want_brush_opacity) {
        fprintf(
            stderr,
            "%s mismatch: got {%d,0x%08X,0x%08X,%d} want {%d,0x%08X,0x%08X,%d}\n",
            label,
            tool,
            brush_color,
            brush_color_rgb,
            brush_opacity,
            want_tool,
            want_brush_color,
            want_brush_color_rgb,
            want_brush_opacity
        );
        return 0;
    }
    return 1;
}

static int expect_sampled_brush_color_noop(
    const char *label,
    unsigned int sampled_color,
    Tool *tool,
    unsigned int *brush_color,
    unsigned int *brush_color_rgb,
    int *brush_opacity,
    Tool want_tool,
    unsigned int want_brush_color,
    unsigned int want_brush_color_rgb,
    int want_brush_opacity
) {
    app_apply_sampled_brush_color(sampled_color, tool, brush_color, brush_color_rgb, brush_opacity);
    if (tool && *tool != want_tool) {
        fprintf(stderr, "%s tool changed: got %d want %d\n", label, *tool, want_tool);
        return 0;
    }
    if (brush_color && *brush_color != want_brush_color) {
        fprintf(stderr, "%s brush_color changed: got 0x%08X want 0x%08X\n", label, *brush_color, want_brush_color);
        return 0;
    }
    if (brush_color_rgb && *brush_color_rgb != want_brush_color_rgb) {
        fprintf(stderr, "%s brush_color_rgb changed: got 0x%08X want 0x%08X\n", label, *brush_color_rgb, want_brush_color_rgb);
        return 0;
    }
    if (brush_opacity && *brush_opacity != want_brush_opacity) {
        fprintf(stderr, "%s brush_opacity changed: got %d want %d\n", label, *brush_opacity, want_brush_opacity);
        return 0;
    }
    return 1;
}

static int expect_layer_clear_color(const char *label, int active_layer_index, unsigned int want) {
    unsigned int got = app_active_layer_clear_color(active_layer_index);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_layer_editable(const char *label, Layer *layer, int want) {
    int got = app_layer_editable(layer);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
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

static int expect_cancel_shape_preview(
    const char *label,
    int *shaping,
    int *preview_active,
    int want_shaping,
    int want_preview_active
) {
    app_cancel_shape_preview(shaping, preview_active);
    if (shaping && *shaping != want_shaping) {
        fprintf(stderr, "%s shaping mismatch: got %d want %d\n", label, *shaping, want_shaping);
        return 0;
    }
    if (preview_active && *preview_active != want_preview_active) {
        fprintf(stderr, "%s preview mismatch: got %d want %d\n", label, *preview_active, want_preview_active);
        return 0;
    }
    return 1;
}

static int expect_preview_canvas_selection(
    const char *label,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_active,
    const Canvas *want
) {
    const Canvas *got = app_preview_canvas_or_composite(composite, preview_canvas, preview_active);

    if (got != want) {
        fprintf(stderr, "%s canvas mismatch: got %p want %p\n", label, (const void *)got, (const void *)want);
        return 0;
    }
    return 1;
}

static int expect_restore_shape_preview(
    const char *label,
    uint32_t *preview_pixels,
    const uint32_t *shape_base_pixels,
    size_t pixel_count,
    int *preview_active,
    int want_preview_active,
    const uint32_t *want_preview_pixels,
    size_t want_pixel_count
) {
    size_t i;

    app_restore_shape_preview(preview_pixels, shape_base_pixels, pixel_count, preview_active);
    if (preview_active && *preview_active != want_preview_active) {
        fprintf(stderr, "%s preview active mismatch: got %d want %d\n", label, *preview_active, want_preview_active);
        return 0;
    }
    if (preview_pixels && want_preview_pixels) {
        for (i = 0; i < want_pixel_count; i++) {
            if (preview_pixels[i] != want_preview_pixels[i]) {
                fprintf(stderr, "%s preview_pixels[%zu] mismatch: got 0x%08X want 0x%08X\n", label, i, preview_pixels[i], want_preview_pixels[i]);
                return 0;
            }
        }
    }
    return 1;
}

int main(void) {
    int ok = 1;
    int sentinel_x = 321;
    int sentinel_y = 654;
    int shaping = 1;
    int preview_active = 1;
    int shape_start_x = -1;
    int shape_start_y = -1;
    Tool sampled_tool = TOOL_ERASER;
    unsigned int sampled_brush_color = 0xAA112233u;
    unsigned int sampled_brush_color_rgb = 0x00112233u;
    int sampled_brush_opacity = 42;
    uint32_t preview_source[4] = {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
    uint32_t preview_copy[4] = {0u, 0u, 0u, 0u};
    uint32_t preview_sentinel[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
    Layer editable_layer = {0};
    Layer locked_layer = {0};
    Layer empty_layer = {0};
    Canvas composite_canvas = {4, 4, preview_source};
    Canvas preview_canvas = {4, 4, preview_copy};
    Canvas preview_canvas_without_pixels = {4, 4, NULL};
    uint32_t preview_restore_copy[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
    uint32_t preview_restore_source[4] = {0x01010101u, 0x02020202u, 0x03030303u, 0x04040404u};
    uint32_t preview_restore_sentinel[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};

    editable_layer.locked = 0;
    editable_layer.canvas.pixels = (uint32_t *)&editable_layer;
    locked_layer.locked = 1;
    locked_layer.canvas.pixels = (uint32_t *)&locked_layer;
    empty_layer.locked = 0;
    empty_layer.canvas.pixels = NULL;

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
    ok = ok && expect_shape_cancel("shape_cancel_ctrl_save", APP_SHAPE_CANCEL_KEY_S, 1, 1);
    ok = ok && expect_shape_cancel("shape_cancel_ctrl_bracket", APP_SHAPE_CANCEL_KEY_LEFTBRACKET, 1, 1);
    ok = ok && expect_shape_cancel("shape_cancel_ctrl_slash", APP_SHAPE_CANCEL_KEY_SLASH, 1, 1);
    ok = ok && expect_shape_cancel("shape_cancel_ctrl_digit_1", APP_SHAPE_CANCEL_KEY_1, 1, 1);
    ok = ok && expect_shape_cancel("shape_cancel_ctrl_unmapped_not_cancel", APP_SHAPE_CANCEL_KEY_OTHER, 1, 0);
    ok = ok && expect_shape_cancel("shape_cancel_ctrl_digit_7", APP_SHAPE_CANCEL_KEY_7, 1, 1);
    ok = ok && expect_shape_cancel("shape_cancel_ctrl_digit_8", APP_SHAPE_CANCEL_KEY_8, 1, 1);
    ok = ok && expect_shape_cancel("shape_cancel_tool_switch", APP_SHAPE_CANCEL_KEY_B, 0, 1);
    ok = ok && expect_shape_cancel("shape_cancel_plain_bracket", APP_SHAPE_CANCEL_KEY_RIGHTBRACKET, 0, 1);
    ok = ok && expect_shape_cancel("shape_cancel_plain_digit_6", APP_SHAPE_CANCEL_KEY_6, 0, 1);
    ok = ok && expect_shape_cancel("shape_cancel_plain_slash_not_cancel", APP_SHAPE_CANCEL_KEY_SLASH, 0, 0);
    ok = ok && expect_shape_cancel("shape_cancel_plain_kp_plus", APP_SHAPE_CANCEL_KEY_KP_PLUS, 0, 1);
    ok = ok && expect_shape_cancel("shape_cancel_plain_kp_minus", APP_SHAPE_CANCEL_KEY_KP_MINUS, 0, 1);
    ok = ok && expect_shape_cancel("shape_cancel_arrow", APP_SHAPE_CANCEL_KEY_LEFT, 0, 1);
    ok = ok && expect_shape_cancel("shape_cancel_function_key", APP_SHAPE_CANCEL_KEY_F2, 0, 1);
    ok = ok && expect_shape_cancel("shape_cancel_delete", APP_SHAPE_CANCEL_KEY_DELETE, 0, 1);
    ok = ok && expect_shape_cancel("shape_cancel_backspace", APP_SHAPE_CANCEL_KEY_BACKSPACE, 0, 1);
    ok = ok && expect_shape_cancel("shape_cancel_escape_exempt", APP_SHAPE_CANCEL_KEY_ESCAPE, 0, 0);
    ok = ok && expect_shape_cancel("shape_cancel_shift_exempt", APP_SHAPE_CANCEL_KEY_LSHIFT, 0, 0);
    ok = ok && expect_shape_cancel("shape_cancel_plain_save_not_cancel", APP_SHAPE_CANCEL_KEY_S, 0, 0);
    ok = ok && expect_shape_cancel("shape_cancel_unmapped_key", APP_SHAPE_CANCEL_KEY_OTHER, 0, 0);
    ok = ok && expect_brush_mask("brush_mask_round_inside", BRUSH_SHAPE_ROUND, 1, 1, 2, 1);
    ok = ok && expect_brush_mask("brush_mask_round_edge", BRUSH_SHAPE_ROUND, 2, 0, 2, 1);
    ok = ok && expect_brush_mask("brush_mask_round_outside", BRUSH_SHAPE_ROUND, 2, 1, 2, 0);
    ok = ok && expect_brush_mask("brush_mask_round_negative_symmetry", BRUSH_SHAPE_ROUND, -1, -1, 2, 1);
    ok = ok && expect_brush_mask("brush_mask_round_zero_radius", BRUSH_SHAPE_ROUND, 0, 0, 0, 1);
    ok = ok && expect_brush_mask("brush_mask_square_corner", BRUSH_SHAPE_SQUARE, 2, 2, 2, 1);
    ok = ok && expect_brush_mask("brush_mask_square_zero_radius", BRUSH_SHAPE_SQUARE, 0, 0, 0, 1);
    ok = ok && expect_brush_mask("brush_mask_square_negative_symmetry", BRUSH_SHAPE_SQUARE, -2, -2, 2, 1);
    ok = ok && expect_brush_mask("brush_mask_diamond_edge", BRUSH_SHAPE_DIAMOND, 2, 0, 2, 1);
    ok = ok && expect_brush_mask("brush_mask_diamond_outside", BRUSH_SHAPE_DIAMOND, 2, 1, 2, 0);
    ok = ok && expect_brush_mask("brush_mask_diamond_zero_radius", BRUSH_SHAPE_DIAMOND, 0, 0, 0, 1);
    ok = ok && expect_brush_mask("brush_mask_diamond_negative_symmetry", BRUSH_SHAPE_DIAMOND, -1, -1, 2, 1);
    ok = ok && expect_brush_mask("brush_mask_default", (BrushShape)999, 0, 0, 2, 0);
    ok = ok && expect_sampled_brush_color("sampled_brush_color_opaque", 0xFF445566u, TOOL_ERASER, 0, 0, 0, TOOL_BRUSH, 0xFF445566u, 0x00445566u, 100);
    ok = ok && expect_sampled_brush_color("sampled_brush_color_black", 0xFF000000u, TOOL_ERASER, 0, 0, 0, TOOL_BRUSH, 0xFF000000u, 0x00000000u, 100);
    ok = ok && expect_sampled_brush_color("sampled_brush_color_white", 0xFFFFFFFFu, TOOL_ERASER, 0, 0, 0, TOOL_BRUSH, 0xFFFFFFFFu, 0x00FFFFFFu, 100);
    ok = ok && expect_sampled_brush_color("sampled_brush_color_translucent", 0x80445566u, TOOL_LINE, 0, 0, 0, TOOL_BRUSH, 0x80445566u, 0x00445566u, 50);
    ok = ok && expect_sampled_brush_color("sampled_brush_color_low_alpha", 0x05445566u, TOOL_LINE, 0, 0, 0, TOOL_BRUSH, 0x05445566u, 0x00445566u, 2);
    ok = ok && expect_sampled_brush_color("sampled_brush_color_next_low_step", 0x07445566u, TOOL_LINE, 0, 0, 0, TOOL_BRUSH, 0x08445566u, 0x00445566u, 3);
    ok = ok && expect_sampled_brush_color("sampled_brush_color_rounds_down", 0x7F445566u, TOOL_LINE, 0, 0, 0, TOOL_BRUSH, 0x80445566u, 0x00445566u, 50);
    ok = ok && expect_sampled_brush_color("sampled_brush_color_rounds_up", 0x81445566u, TOOL_LINE, 0, 0, 0, TOOL_BRUSH, 0x82445566u, 0x00445566u, 51);
    ok = ok && expect_sampled_brush_color("sampled_brush_color_transparent_clamp", 0x00445566u, TOOL_RECT, 0, 0, 0, TOOL_BRUSH, 0x03445566u, 0x00445566u, 1);
    ok = ok && expect_sampled_brush_color_noop("sampled_brush_color_null_tool", 0xFF778899u, NULL, &sampled_brush_color, &sampled_brush_color_rgb, &sampled_brush_opacity, TOOL_BRUSH, 0xAA112233u, 0x00112233u, 42);
    ok = ok && expect_sampled_brush_color_noop("sampled_brush_color_null_color", 0xFF778899u, &sampled_tool, NULL, &sampled_brush_color_rgb, &sampled_brush_opacity, TOOL_ERASER, 0, 0x00112233u, 42);
    ok = ok && expect_brush_color("brush_color_low_clamp", 0x00123456u, 0, 0x03123456u);
    ok = ok && expect_brush_color("brush_color_mid_round", 0x00ABCDEFu, 50, 0x80ABCDEFu);
    ok = ok && expect_brush_color("brush_color_high_clamp", 0x00FEDCBAu, 150, 0xFFFEDCBAu);
    ok = ok && expect_layer_clear_color("layer_clear_color_background", 0, 0xFFFFFFFFu);
    ok = ok && expect_layer_clear_color("layer_clear_color_foreground", 3, 0x00000000u);
    ok = ok && expect_layer_editable("layer_editable_true", &editable_layer, 1);
    ok = ok && expect_layer_editable("layer_editable_locked", &locked_layer, 0);
    ok = ok && expect_layer_editable("layer_editable_missing_pixels", &empty_layer, 0);
    ok = ok && expect_layer_editable("layer_editable_null", NULL, 0);
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
    ok = ok && expect_constrained_shape_end("shape_filled_rect_square_snap", TOOL_FILLED_RECT, 10, 10, 14, 18, 1, 18, 18);
    ok = ok && expect_constrained_shape_end("shape_filled_ellipse_square_snap", TOOL_FILLED_ELLIPSE, 10, 10, 4, 18, 1, 2, 18);
    ok = ok && expect_constrained_shape_end("shape_no_shift_passthrough", TOOL_ELLIPSE, 10, 10, 14, 18, 0, 14, 18);
    ok = ok && expect_constrained_shape_end("shape_unknown_tool_passthrough", (Tool)999, 10, 10, 25, 13, 1, 25, 13);
    ok = ok && expect_constrained_shape_end_no_output("shape_null_out_x", TOOL_LINE, 10, 10, 25, 13, 1, NULL, &sentinel_y, 0, 654);
    ok = ok && expect_constrained_shape_end_no_output("shape_null_out_y", TOOL_LINE, 10, 10, 25, 13, 1, &sentinel_x, NULL, 321, 0);
    {
        const BeginShapePreviewCase begin_shape_preview_cases[] = {
            {"begin_shape_preview_copy", 12, 34, 0, -1, -1, preview_copy, preview_source, 4, 1, 12, 34, preview_source},
            {"begin_shape_preview_no_copy_without_source", 20, 30, 5, 7, 9, preview_copy, NULL, 4, 1, 20, 30, preview_sentinel},
            {"begin_shape_preview_zero_length_no_copy", 22, 33, 4, 2, 3, preview_copy, preview_source, 0, 1, 22, 33, preview_sentinel},
            {"begin_shape_preview_null_source_zero_length", 120, 130, 14, 17, 18, preview_copy, NULL, 0, 1, 120, 130, preview_sentinel},
        };
        size_t i;

        memset(preview_copy, 0, sizeof(preview_copy));
        ok = ok && run_begin_shape_preview_case(&begin_shape_preview_cases[0], &shaping, &shape_start_x, &shape_start_y);

        for (i = 1; i < sizeof(begin_shape_preview_cases) / sizeof(begin_shape_preview_cases[0]); i++) {
            memcpy(preview_copy, preview_sentinel, sizeof(preview_copy));
            ok = ok && run_begin_shape_preview_case(&begin_shape_preview_cases[i], &shaping, &shape_start_x, &shape_start_y);
        }
    }
    {
        uint32_t preview_partial[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};

        shaping = 2;
        shape_start_x = -2;
        shape_start_y = -3;
        app_begin_shape_preview(14, 24, &shaping, &shape_start_x, &shape_start_y, preview_partial, preview_source, 2);
        if (shaping != 1 ||
            shape_start_x != 14 ||
            shape_start_y != 24 ||
            preview_partial[0] != preview_source[0] ||
            preview_partial[1] != preview_source[1] ||
            preview_partial[2] != 0xCCCCCCCCu ||
            preview_partial[3] != 0xDDDDDDDDu) {
            fprintf(stderr, "begin_shape_preview_partial_copy failed\n");
            ok = 0;
        }
    }
    shaping = 6;
    shape_start_x = 8;
    shape_start_y = 10;
    memcpy(preview_copy, preview_sentinel, sizeof(preview_copy));
    ok = ok && expect_begin_shape_preview(
        "begin_shape_preview_null_shaping_noop",
        40,
        50,
        NULL,
        &shape_start_x,
        &shape_start_y,
        preview_copy,
        preview_source,
        4,
        6,
        8,
        10,
        preview_sentinel
    );
    shaping = 11;
    shape_start_y = 13;
    memcpy(preview_copy, preview_sentinel, sizeof(preview_copy));
    ok = ok && expect_begin_shape_preview(
        "begin_shape_preview_null_start_x_noop",
        60,
        70,
        &shaping,
        NULL,
        &shape_start_y,
        preview_copy,
        preview_source,
        4,
        11,
        0,
        13,
        preview_sentinel
    );
    shaping = 12;
    shape_start_x = 14;
    memcpy(preview_copy, preview_sentinel, sizeof(preview_copy));
    ok = ok && expect_begin_shape_preview(
        "begin_shape_preview_null_start_y_noop",
        80,
        90,
        &shaping,
        &shape_start_x,
        NULL,
        preview_copy,
        preview_source,
        4,
        12,
        14,
        0,
        preview_sentinel
    );
    shaping = 13;
    shape_start_x = 15;
    shape_start_y = 16;
    ok = ok && expect_begin_shape_preview(
        "begin_shape_preview_null_destination_no_copy",
        100,
        110,
        &shaping,
        &shape_start_x,
        &shape_start_y,
        NULL,
        preview_source,
        4,
        1,
        100,
        110,
        NULL
    );
    shaping = 15;
    shape_start_x = 19;
    shape_start_y = 20;
    ok = ok && expect_begin_shape_preview(
        "begin_shape_preview_null_source_and_destination",
        140,
        150,
        &shaping,
        &shape_start_x,
        &shape_start_y,
        NULL,
        NULL,
        4,
        1,
        140,
        150,
        NULL
    );
    shaping = 16;
    shape_start_x = 21;
    shape_start_y = 22;
    ok = ok && expect_begin_shape_preview(
        "begin_shape_preview_null_everything_copy_path",
        160,
        170,
        &shaping,
        &shape_start_x,
        &shape_start_y,
        NULL,
        NULL,
        0,
        1,
        160,
        170,
        NULL
    );
    ok = ok && expect_cancel_shape_preview("cancel_shape_preview_both", &shaping, &preview_active, 0, 0);
    shaping = 7;
    preview_active = 9;
    ok = ok && expect_cancel_shape_preview("cancel_shape_preview_null_shape", NULL, &preview_active, 0, 0);
    ok = ok && expect_cancel_shape_preview("cancel_shape_preview_null_preview", &shaping, NULL, 0, 0);
    ok = ok && expect_preview_canvas_selection("preview_canvas_active", &composite_canvas, &preview_canvas, 1, &preview_canvas);
    ok = ok && expect_preview_canvas_selection("preview_canvas_inactive", &composite_canvas, &preview_canvas, 0, &composite_canvas);
    ok = ok && expect_preview_canvas_selection(
        "preview_canvas_missing_pixels_falls_back",
        &composite_canvas,
        &preview_canvas_without_pixels,
        1,
        &composite_canvas
    );
    ok = ok && expect_preview_canvas_selection("preview_canvas_null_preview_falls_back", &composite_canvas, NULL, 1, &composite_canvas);
    ok = ok && expect_preview_canvas_selection("preview_canvas_null_composite_allowed", NULL, &preview_canvas, 0, NULL);
    preview_active = 0;
    memcpy(preview_restore_copy, preview_restore_sentinel, sizeof(preview_restore_copy));
    ok = ok && expect_restore_shape_preview(
        "restore_shape_preview_copy",
        preview_restore_copy,
        preview_restore_source,
        4,
        &preview_active,
        1,
        preview_restore_source,
        4
    );
    preview_active = 0;
    memcpy(preview_restore_copy, preview_restore_sentinel, sizeof(preview_restore_copy));
    ok = ok && expect_restore_shape_preview(
        "restore_shape_preview_partial_copy",
        preview_restore_copy,
        preview_restore_source,
        2,
        &preview_active,
        1,
        (const uint32_t[]){0x01010101u, 0x02020202u, 0xCCCCCCCCu, 0xDDDDDDDDu},
        4
    );
    preview_active = 0;
    memcpy(preview_restore_copy, preview_restore_sentinel, sizeof(preview_restore_copy));
    ok = ok && expect_restore_shape_preview(
        "restore_shape_preview_zero_length",
        preview_restore_copy,
        preview_restore_source,
        0,
        &preview_active,
        1,
        preview_restore_sentinel,
        4
    );
    preview_active = 0;
    memcpy(preview_restore_copy, preview_restore_sentinel, sizeof(preview_restore_copy));
    ok = ok && expect_restore_shape_preview(
        "restore_shape_preview_null_destination",
        NULL,
        preview_restore_source,
        4,
        &preview_active,
        1,
        NULL,
        0
    );
    preview_active = 0;
    memcpy(preview_restore_copy, preview_restore_sentinel, sizeof(preview_restore_copy));
    ok = ok && expect_restore_shape_preview(
        "restore_shape_preview_null_source",
        preview_restore_copy,
        NULL,
        4,
        &preview_active,
        1,
        preview_restore_sentinel,
        4
    );
    preview_active = 9;
    memcpy(preview_restore_copy, preview_restore_sentinel, sizeof(preview_restore_copy));
    ok = ok && expect_restore_shape_preview(
        "restore_shape_preview_null_flag",
        preview_restore_copy,
        preview_restore_source,
        4,
        NULL,
        0,
        preview_restore_sentinel,
        4
    );
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
