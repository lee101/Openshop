#include "../src/app_brush.h"
#include "../src/app_brush_mask.h"
#include "../src/app_canvas_click.h"
#include "../src/app_color.h"
#include "../src/app_layer_state.h"
#include "../src/app_preview.h"
#include "../src/app_runtime_shortcuts.h"
#include "../src/app_sampled_color.h"
#include "../src/app_shape.h"
#include "../src/app_shape_cancel.h"
#include "../src/app_title.h"
#include "../src/brush_shortcuts.h"
#include "../src/canvas_shortcuts.h"
#include "../src/direct_layer_shortcuts.h"
#include "../src/file_shortcuts.h"
#include "../src/history_shortcuts.h"
#include "../src/history_state.h"
#include "../src/layer_name_shortcuts.h"
#include "../src/merge_shortcuts.h"
#include "../src/paint_shortcuts.h"
#include "../src/view_shortcuts.h"
#include <stdio.h>
#include <string.h>

static void init_single_layer_stack(
    LayerStack *stack,
    Canvas *canvas,
    uint32_t *pixels,
    int width,
    int height,
    uint32_t fill,
    int locked
);

static int expect_shortcut(const char *label, int ctrl, int alt, int shift, LayerNameResetShortcut want) {
    LayerNameResetShortcut got = layer_name_reset_shortcut_from_modifiers(ctrl, alt, shift);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int ctrl;
    int alt;
    int shift;
    LayerNameResetShortcut want;
} LayerNameResetShortcutCase;

static int run_layer_name_reset_shortcut_case(const LayerNameResetShortcutCase *test_case) {
    return expect_shortcut(
        test_case->label,
        test_case->ctrl,
        test_case->alt,
        test_case->shift,
        test_case->want
    );
}

static int expect_direct_action(const char *label, int ctrl, int alt, int shift, DirectLayerShortcutAction want) {
    DirectLayerShortcutAction got = direct_layer_shortcut_action_from_modifiers(ctrl, alt, shift);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int ctrl;
    int alt;
    int shift;
    DirectLayerShortcutAction want;
} DirectLayerShortcutCase;

static int run_direct_layer_shortcut_case(const DirectLayerShortcutCase *test_case) {
    return expect_direct_action(
        test_case->label,
        test_case->ctrl,
        test_case->alt,
        test_case->shift,
        test_case->want
    );
}

static int expect_history_action(const char *label, int ctrl, int key, HistoryShortcutAction want) {
    HistoryShortcutAction got = history_shortcut_action(ctrl, key);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int ctrl;
    int key;
    HistoryShortcutAction want;
} HistoryShortcutCase;

static int run_history_shortcut_case(const HistoryShortcutCase *test_case) {
    return expect_history_action(
        test_case->label,
        test_case->ctrl,
        test_case->key,
        test_case->want
    );
}

static int expect_history_navigation_shortcut(
    const char *label,
    int ctrl,
    int key,
    uint32_t initial_fill,
    uint32_t mutated_fill,
    int prepare_redo,
    int want_handled,
    int want_needs_composite,
    int want_undo_count,
    int want_redo_count,
    uint32_t want_pixel
) {
    LayerStack stack;
    Canvas canvas;
    uint32_t pixels[4];
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int needs_composite = 0;
    int handled = 0;

    init_single_layer_stack(&stack, &canvas, pixels, 2, 2, initial_fill, 0);
    snapshot_push(&stack, undo_stack, &undo_count, 2, redo_stack, &redo_count);
    pixels[0] = mutated_fill;
    if (prepare_redo) {
        snapshot_undo(&stack, undo_stack, &undo_count, 2, redo_stack, &redo_count);
        needs_composite = 0;
    }

    handled = app_handle_history_navigation_shortcut(
        key,
        ctrl,
        &stack,
        undo_stack,
        &undo_count,
        2,
        redo_stack,
        &redo_count,
        &needs_composite
    );

    if (handled != want_handled) {
        fprintf(stderr, "%s handled mismatch: got %d want %d\n", label, handled, want_handled);
        return 0;
    }
    if (needs_composite != want_needs_composite) {
        fprintf(stderr, "%s needs_composite mismatch: got %d want %d\n", label, needs_composite, want_needs_composite);
        return 0;
    }
    if (undo_count != want_undo_count || redo_count != want_redo_count) {
        fprintf(stderr, "%s history count mismatch: undo %d/%d redo %d/%d\n", label, undo_count, want_undo_count, redo_count, want_redo_count);
        return 0;
    }
    if (pixels[0] != want_pixel) {
        fprintf(stderr, "%s pixel mismatch: got 0x%08X want 0x%08X\n", label, pixels[0], want_pixel);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int ctrl;
    int key;
    uint32_t initial_fill;
    uint32_t mutated_fill;
    int prepare_redo;
    int want_handled;
    int want_needs_composite;
    int want_undo_count;
    int want_redo_count;
    uint32_t want_pixel;
} HistoryNavigationShortcutCase;

static int run_history_navigation_shortcut_case(const HistoryNavigationShortcutCase *test_case) {
    return expect_history_navigation_shortcut(
        test_case->label,
        test_case->ctrl,
        test_case->key,
        test_case->initial_fill,
        test_case->mutated_fill,
        test_case->prepare_redo,
        test_case->want_handled,
        test_case->want_needs_composite,
        test_case->want_undo_count,
        test_case->want_redo_count,
        test_case->want_pixel
    );
}

static int expect_file_action(const char *label, int ctrl, int key, FileShortcutAction want) {
    FileShortcutAction got = file_shortcut_action(ctrl, key);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int ctrl;
    int key;
    FileShortcutAction want;
} FileShortcutCase;

static int run_file_shortcut_case(const FileShortcutCase *test_case) {
    return expect_file_action(
        test_case->label,
        test_case->ctrl,
        test_case->key,
        test_case->want
    );
}

static int expect_merge_action(const char *label, int ctrl, int key, MergeShortcutAction want) {
    MergeShortcutAction got = merge_shortcut_action(ctrl, key);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int ctrl;
    int key;
    MergeShortcutAction want;
} MergeShortcutCase;

static int run_merge_shortcut_case(const MergeShortcutCase *test_case) {
    return expect_merge_action(
        test_case->label,
        test_case->ctrl,
        test_case->key,
        test_case->want
    );
}

static int expect_paint_action(const char *label, int key, PaintShortcutAction want) {
    PaintShortcutAction got = paint_shortcut_action(key);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int key;
    PaintShortcutAction want;
} PaintShortcutCase;

static int run_paint_shortcut_case(const PaintShortcutCase *test_case) {
    return expect_paint_action(test_case->label, test_case->key, test_case->want);
}

static int expect_brush_action(const char *label, int key, BrushShortcutAction want) {
    BrushShortcutAction got = brush_shortcut_action(key);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int key;
    BrushShortcutAction want;
} BrushShortcutCase;

static int run_brush_shortcut_case(const BrushShortcutCase *test_case) {
    return expect_brush_action(test_case->label, test_case->key, test_case->want);
}

static int expect_direct_draw_tool(const char *label, Tool tool, int want) {
    int got = app_tool_draws_directly(tool);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    Tool tool;
    int want;
} DirectDrawToolCase;

static int run_direct_draw_tool_case(const DirectDrawToolCase *test_case) {
    return expect_direct_draw_tool(test_case->label, test_case->tool, test_case->want);
}

static int expect_stroke_mark(const char *label, Tool tool, AppStrokeMark want) {
    AppStrokeMark got = app_tool_stroke_mark(tool);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    Tool tool;
    AppStrokeMark want;
} StrokeMarkCase;

static int run_stroke_mark_case(const StrokeMarkCase *test_case) {
    return expect_stroke_mark(test_case->label, test_case->tool, test_case->want);
}

static int expect_brush_stamp_pixel(
    const char *label,
    void (*apply_stamp)(Canvas *, int, int, int, uint32_t, BrushShape),
    uint32_t initial_color,
    uint32_t stamp_color,
    BrushShape shape,
    uint32_t want_center,
    uint32_t want_edge
) {
    Canvas canvas = {3, 3, NULL};
    uint32_t pixels[9];
    size_t i;

    for (i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++) {
        pixels[i] = initial_color;
    }
    canvas.width = 3;
    canvas.height = 3;
    canvas.pixels = pixels;

    apply_stamp(&canvas, 1, 1, 1, stamp_color, shape);
    if (pixels[4] != want_center || pixels[0] != want_edge) {
        fprintf(stderr, "%s mismatch: center 0x%08X want 0x%08X edge 0x%08X want 0x%08X\n", label, pixels[4], want_center, pixels[0], want_edge);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    void (*apply_stamp)(Canvas *, int, int, int, uint32_t, BrushShape);
    uint32_t initial_color;
    uint32_t stamp_color;
    BrushShape shape;
    uint32_t want_center;
    uint32_t want_edge;
} BrushStampCase;

static int run_brush_stamp_case(const BrushStampCase *test_case) {
    return expect_brush_stamp_pixel(
        test_case->label,
        test_case->apply_stamp,
        test_case->initial_color,
        test_case->stamp_color,
        test_case->shape,
        test_case->want_center,
        test_case->want_edge
    );
}

static int expect_brush_line_pixel(
    const char *label,
    void (*apply_line)(Canvas *, int, int, int, int, int, uint32_t, BrushShape),
    uint32_t initial_color,
    uint32_t stroke_color,
    BrushShape shape,
    size_t changed_index,
    uint32_t want_changed,
    size_t unchanged_index,
    uint32_t want_unchanged
) {
    Canvas canvas = {5, 5, NULL};
    uint32_t pixels[25];
    size_t i;

    for (i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++) {
        pixels[i] = initial_color;
    }
    canvas.width = 5;
    canvas.height = 5;
    canvas.pixels = pixels;

    apply_line(&canvas, 1, 2, 3, 2, 1, stroke_color, shape);
    if (pixels[changed_index] != want_changed || pixels[unchanged_index] != want_unchanged) {
        fprintf(
            stderr,
            "%s mismatch: changed 0x%08X want 0x%08X unchanged 0x%08X want 0x%08X\n",
            label,
            pixels[changed_index],
            want_changed,
            pixels[unchanged_index],
            want_unchanged
        );
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    void (*apply_line)(Canvas *, int, int, int, int, int, uint32_t, BrushShape);
    uint32_t initial_color;
    uint32_t stroke_color;
    BrushShape shape;
    size_t changed_index;
    uint32_t want_changed;
    size_t unchanged_index;
    uint32_t want_unchanged;
} BrushLineCase;

static int run_brush_line_case(const BrushLineCase *test_case) {
    return expect_brush_line_pixel(
        test_case->label,
        test_case->apply_line,
        test_case->initial_color,
        test_case->stroke_color,
        test_case->shape,
        test_case->changed_index,
        test_case->want_changed,
        test_case->unchanged_index,
        test_case->want_unchanged
    );
}

static int expect_begin_direct_stroke(
    const char *label,
    LayerStack *stack,
    int x,
    int y,
    Tool tool,
    BrushShape shape,
    int radius,
    uint32_t brush_color,
    int initial_drawing,
    int initial_needs_composite,
    int want_started,
    int want_drawing,
    int want_needs_composite,
    size_t changed_index,
    uint32_t want_changed,
    size_t snapshot_index,
    uint32_t want_snapshot
) {
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int drawing = initial_drawing;
    int needs_composite = initial_needs_composite;
    int ok = 1;
    int started = app_begin_direct_stroke(
        stack,
        x,
        y,
        tool,
        shape,
        radius,
        brush_color,
        undo_stack,
        &undo_count,
        2,
        redo_stack,
        &redo_count,
        &drawing,
        &needs_composite
    );

    if (started != want_started) {
        fprintf(stderr, "%s start mismatch: got %d want %d\n", label, started, want_started);
        ok = 0;
    }
    if (drawing != want_drawing) {
        fprintf(stderr, "%s drawing mismatch: got %d want %d\n", label, drawing, want_drawing);
        ok = 0;
    }
    if (needs_composite != want_needs_composite) {
        fprintf(stderr, "%s needs_composite mismatch: got %d want %d\n", label, needs_composite, want_needs_composite);
        ok = 0;
    }
    if (stack && stack->layer_count > 0 && stack->layers[stack->active_layer].canvas.pixels) {
        if (stack->layers[stack->active_layer].canvas.pixels[changed_index] != want_changed) {
            fprintf(
                stderr,
                "%s pixel mismatch: got 0x%08X want 0x%08X\n",
                label,
                stack->layers[stack->active_layer].canvas.pixels[changed_index],
                want_changed
            );
            ok = 0;
        }
    }
    if (want_started) {
        if (undo_count != 1 || redo_count != 0 || !undo_stack[0].pixels || undo_stack[0].pixels[snapshot_index] != want_snapshot) {
            fprintf(
                stderr,
                "%s snapshot mismatch: undo_count=%d redo_count=%d pixel=0x%08X want 0x%08X\n",
                label,
                undo_count,
                redo_count,
                undo_stack[0].pixels ? undo_stack[0].pixels[snapshot_index] : 0u,
                want_snapshot
            );
            ok = 0;
        }
    } else if (undo_count != 0 || redo_count != 0) {
        fprintf(stderr, "%s no-op snapshot mismatch: undo_count=%d redo_count=%d\n", label, undo_count, redo_count);
        ok = 0;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    return ok;
}

static int expect_handle_left_canvas_press(
    const char *label,
    LayerStack *stack,
    int x,
    int y,
    Tool tool,
    BrushShape shape,
    int radius,
    uint32_t brush_color,
    const Canvas *composite,
    int initial_drawing,
    int initial_shaping,
    int initial_shape_start_x,
    int initial_shape_start_y,
    int initial_last_x,
    int initial_last_y,
    int initial_needs_composite,
    uint32_t *shape_base_pixels,
    const uint32_t *want_shape_base_pixels,
    size_t want_pixel_count,
    AppCanvasClickResult want_result,
    int want_drawing,
    int want_shaping,
    int want_shape_start_x,
    int want_shape_start_y,
    int want_last_x,
    int want_last_y,
    int want_needs_composite,
    size_t changed_index,
    uint32_t want_changed,
    size_t snapshot_index,
    uint32_t want_snapshot
) {
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int drawing = initial_drawing;
    int shaping = initial_shaping;
    int shape_start_x = initial_shape_start_x;
    int shape_start_y = initial_shape_start_y;
    int last_x = initial_last_x;
    int last_y = initial_last_y;
    int needs_composite = initial_needs_composite;
    AppCanvasClickResult got = app_handle_left_canvas_press(
        stack,
        x,
        y,
        &last_x,
        &last_y,
        tool,
        shape,
        radius,
        brush_color,
        composite,
        undo_stack,
        &undo_count,
        2,
        redo_stack,
        &redo_count,
        &drawing,
        &shaping,
        &shape_start_x,
        &shape_start_y,
        shape_base_pixels,
        &needs_composite
    );
    int ok = 1;
    size_t i;

    if (got != want_result) {
        fprintf(stderr, "%s result mismatch: got %d want %d\n", label, got, want_result);
        ok = 0;
    }
    if (drawing != want_drawing || shaping != want_shaping || shape_start_x != want_shape_start_x || shape_start_y != want_shape_start_y) {
        fprintf(
            stderr,
            "%s state mismatch: drawing=%d/%d shaping=%d/%d start={%d,%d}/{%d,%d}\n",
            label,
            drawing,
            want_drawing,
            shaping,
            want_shaping,
            shape_start_x,
            shape_start_y,
            want_shape_start_x,
            want_shape_start_y
        );
        ok = 0;
    }
    if (last_x != want_last_x || last_y != want_last_y || needs_composite != want_needs_composite) {
        fprintf(
            stderr,
            "%s pointer/composite mismatch: last={%d,%d} want {%d,%d} composite=%d want %d\n",
            label,
            last_x,
            last_y,
            want_last_x,
            want_last_y,
            needs_composite,
            want_needs_composite
        );
        ok = 0;
    }
    if (shape_base_pixels && want_shape_base_pixels) {
        for (i = 0; i < want_pixel_count; i++) {
            if (shape_base_pixels[i] != want_shape_base_pixels[i]) {
                fprintf(stderr, "%s shape_base_pixels[%zu] mismatch: got 0x%08X want 0x%08X\n", label, i, shape_base_pixels[i], want_shape_base_pixels[i]);
                ok = 0;
                break;
            }
        }
    }
    if (stack && stack->layer_count > 0 && stack->layers[stack->active_layer].canvas.pixels[changed_index] != want_changed) {
        fprintf(stderr, "%s pixel mismatch: got 0x%08X want 0x%08X\n", label, stack->layers[stack->active_layer].canvas.pixels[changed_index], want_changed);
        ok = 0;
    }
    if (want_result == APP_CANVAS_CLICK_DIRECT_STROKE) {
        if (undo_count != 1 || redo_count != 0 || !undo_stack[0].pixels || undo_stack[0].pixels[snapshot_index] != want_snapshot) {
            fprintf(stderr, "%s snapshot mismatch: undo=%d redo=%d pixel=0x%08X want 0x%08X\n", label, undo_count, redo_count, undo_stack[0].pixels ? undo_stack[0].pixels[snapshot_index] : 0u, want_snapshot);
            ok = 0;
        }
    } else if (undo_count != 0 || redo_count != 0) {
        fprintf(stderr, "%s history mismatch: undo=%d redo=%d want 0/0\n", label, undo_count, redo_count);
        ok = 0;
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    return ok;
}

static int expect_handle_right_canvas_press(
    const char *label,
    int *shaping,
    int *preview_active,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_canvas_active,
    int x,
    int y,
    Tool initial_tool,
    unsigned int initial_brush_color,
    unsigned int initial_brush_color_rgb,
    int initial_brush_opacity,
    AppCanvasClickResult want_result,
    int want_shaping,
    int want_preview_active,
    Tool want_tool,
    unsigned int want_brush_color,
    unsigned int want_brush_color_rgb,
    int want_brush_opacity
) {
    Tool tool = initial_tool;
    unsigned int brush_color = initial_brush_color;
    unsigned int brush_color_rgb = initial_brush_color_rgb;
    int brush_opacity = initial_brush_opacity;
    AppCanvasClickResult got = app_handle_right_canvas_press(
        shaping,
        preview_active,
        composite,
        preview_canvas,
        preview_canvas_active,
        x,
        y,
        &tool,
        &brush_color,
        &brush_color_rgb,
        &brush_opacity
    );

    if (got != want_result) {
        fprintf(stderr, "%s result mismatch: got %d want %d\n", label, got, want_result);
        return 0;
    }
    if (shaping && *shaping != want_shaping) {
        fprintf(stderr, "%s shaping mismatch: got %d want %d\n", label, *shaping, want_shaping);
        return 0;
    }
    if (preview_active && *preview_active != want_preview_active) {
        fprintf(stderr, "%s preview_active mismatch: got %d want %d\n", label, *preview_active, want_preview_active);
        return 0;
    }
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

static int expect_handle_left_canvas_release(
    const char *label,
    int *drawing,
    LayerStack *layers,
    int *shaping,
    int *preview_active,
    int shape_start_x,
    int shape_start_y,
    int x,
    int y,
    int shift,
    Tool tool,
    int brush_radius,
    uint32_t brush_color,
    int undo_capacity,
    AppCanvasClickResult want_result,
    int want_drawing,
    int want_shaping,
    int want_preview_active,
    int want_needs_composite,
    int want_undo_count,
    size_t snapshot_index,
    uint32_t want_snapshot,
    size_t pixel_index,
    uint32_t want_pixel
) {
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int needs_composite = 0;
    AppCanvasClickResult got = app_handle_left_canvas_release(
        drawing,
        layers,
        shaping,
        preview_active,
        shape_start_x,
        shape_start_y,
        x,
        y,
        shift,
        tool,
        brush_radius,
        brush_color,
        undo_stack,
        &undo_count,
        undo_capacity,
        redo_stack,
        &redo_count,
        &needs_composite
    );

    if (got != want_result) {
        fprintf(stderr, "%s result mismatch: got %d want %d\n", label, got, want_result);
        return 0;
    }
    if (drawing && *drawing != want_drawing) {
        fprintf(stderr, "%s drawing mismatch: got %d want %d\n", label, *drawing, want_drawing);
        return 0;
    }
    if (shaping && *shaping != want_shaping) {
        fprintf(stderr, "%s shaping mismatch: got %d want %d\n", label, *shaping, want_shaping);
        return 0;
    }
    if (preview_active && *preview_active != want_preview_active) {
        fprintf(stderr, "%s preview_active mismatch: got %d want %d\n", label, *preview_active, want_preview_active);
        return 0;
    }
    if (needs_composite != want_needs_composite) {
        fprintf(stderr, "%s needs_composite mismatch: got %d want %d\n", label, needs_composite, want_needs_composite);
        return 0;
    }
    if (undo_count != want_undo_count || redo_count != 0) {
        fprintf(stderr, "%s history mismatch: undo=%d want %d redo=%d want 0\n", label, undo_count, want_undo_count, redo_count);
        return 0;
    }
    if (want_undo_count > 0) {
        if (!undo_stack[0].pixels || undo_stack[0].pixels[snapshot_index] != want_snapshot) {
            fprintf(stderr, "%s snapshot mismatch: got 0x%08X want 0x%08X\n", label, undo_stack[0].pixels ? undo_stack[0].pixels[snapshot_index] : 0u, want_snapshot);
            return 0;
        }
    }
    if (layers && pixel_index < (size_t)layers->width * (size_t)layers->height) {
        uint32_t got_pixel = layers->layers[layers->active_layer].canvas.pixels[pixel_index];
        if (got_pixel != want_pixel) {
            fprintf(stderr, "%s pixel mismatch: got 0x%08X want 0x%08X\n", label, got_pixel, want_pixel);
            return 0;
        }
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    return 1;
}

static int expect_handle_canvas_motion(
    const char *label,
    LayerStack *layers,
    int x,
    int y,
    int *drawing,
    int *last_x,
    int *last_y,
    int *shaping,
    int shape_start_x,
    int shape_start_y,
    int shift,
    Tool tool,
    BrushShape brush_shape,
    int brush_radius,
    uint32_t brush_color,
    uint32_t *shape_base_pixels,
    uint32_t *preview_pixels,
    Canvas *preview_canvas,
    int *preview_active,
    int *needs_composite,
    size_t pixel_count,
    AppCanvasClickResult want_result,
    int want_last_x,
    int want_last_y,
    int want_preview_active,
    int want_needs_composite,
    size_t changed_index,
    uint32_t want_changed,
    const uint32_t *want_preview_pixels,
    size_t want_preview_pixel_count
) {
    AppCanvasClickResult got = app_handle_canvas_motion(
        x,
        y,
        drawing,
        last_x,
        last_y,
        shaping,
        shape_start_x,
        shape_start_y,
        shift,
        layers,
        tool,
        brush_shape,
        brush_radius,
        brush_color,
        shape_base_pixels,
        preview_pixels,
        preview_canvas,
        preview_active,
        needs_composite,
        pixel_count
    );
    size_t i;

    if (got != want_result) {
        fprintf(stderr, "%s result mismatch: got %d want %d\n", label, got, want_result);
        return 0;
    }
    if (last_x && *last_x != want_last_x) {
        fprintf(stderr, "%s last_x mismatch: got %d want %d\n", label, *last_x, want_last_x);
        return 0;
    }
    if (last_y && *last_y != want_last_y) {
        fprintf(stderr, "%s last_y mismatch: got %d want %d\n", label, *last_y, want_last_y);
        return 0;
    }
    if (preview_active && *preview_active != want_preview_active) {
        fprintf(stderr, "%s preview_active mismatch: got %d want %d\n", label, *preview_active, want_preview_active);
        return 0;
    }
    if (needs_composite && *needs_composite != want_needs_composite) {
        fprintf(stderr, "%s needs_composite mismatch: got %d want %d\n", label, *needs_composite, want_needs_composite);
        return 0;
    }
    if (layers && changed_index < (size_t)layers->width * (size_t)layers->height) {
        uint32_t got_pixel = layers->layers[layers->active_layer].canvas.pixels[changed_index];
        if (got_pixel != want_changed) {
            fprintf(stderr, "%s pixel mismatch: got 0x%08X want 0x%08X\n", label, got_pixel, want_changed);
            return 0;
        }
    }
    if (preview_pixels && want_preview_pixels) {
        for (i = 0; i < want_preview_pixel_count; i++) {
            if (preview_pixels[i] != want_preview_pixels[i]) {
                fprintf(stderr, "%s preview_pixels[%zu] mismatch: got 0x%08X want 0x%08X\n", label, i, preview_pixels[i], want_preview_pixels[i]);
                return 0;
            }
        }
    }
    return 1;
}

typedef struct {
    const char *label;
    int initial_drawing;
    int initial_last_x;
    int initial_last_y;
    int initial_shaping;
    int initial_preview_active;
    int initial_needs_composite;
    int x;
    int y;
    int shape_start_x;
    int shape_start_y;
    int shift;
    Tool tool;
    BrushShape brush_shape;
    int brush_radius;
    uint32_t brush_color;
    AppCanvasClickResult want_result;
    int want_last_x;
    int want_last_y;
    int want_preview_active;
    int want_needs_composite;
    size_t changed_index;
    uint32_t want_changed;
    const uint32_t *want_preview_pixels;
    size_t want_preview_pixel_count;
} HandleCanvasMotionCase;

static int run_handle_canvas_motion_case(const HandleCanvasMotionCase *test_case) {
    LayerStack stack;
    Canvas canvas;
    uint32_t pixels[25];
    uint32_t preview_pixels_local[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
    uint32_t shape_base_local[4] = {0x01010101u, 0x02020202u, 0x03030303u, 0x04040404u};
    Canvas preview_canvas_local = {2, 2, preview_pixels_local};
    int drawing = test_case->initial_drawing;
    int last_x = test_case->initial_last_x;
    int last_y = test_case->initial_last_y;
    int shaping = test_case->initial_shaping;
    int preview_active = test_case->initial_preview_active;
    int needs_composite = test_case->initial_needs_composite;

    init_single_layer_stack(&stack, &canvas, pixels, 5, 5, 0x00000000u, 0);
    return expect_handle_canvas_motion(
        test_case->label,
        &stack,
        test_case->x,
        test_case->y,
        &drawing,
        &last_x,
        &last_y,
        &shaping,
        test_case->shape_start_x,
        test_case->shape_start_y,
        test_case->shift,
        test_case->tool,
        test_case->brush_shape,
        test_case->brush_radius,
        test_case->brush_color,
        shape_base_local,
        preview_pixels_local,
        &preview_canvas_local,
        &preview_active,
        &needs_composite,
        4,
        test_case->want_result,
        test_case->want_last_x,
        test_case->want_last_y,
        test_case->want_preview_active,
        test_case->want_needs_composite,
        test_case->changed_index,
        test_case->want_changed,
        test_case->want_preview_pixels,
        test_case->want_preview_pixel_count
    );
}

typedef struct {
    const char *label;
    int width;
    int height;
    uint32_t initial_fill;
    int locked;
    size_t preset_pixel_index;
    uint32_t preset_pixel_value;
    int initial_drawing;
    int initial_shaping;
    int initial_preview_active;
    int shape_start_x;
    int shape_start_y;
    int x;
    int y;
    int shift;
    Tool tool;
    int brush_radius;
    uint32_t brush_color;
    int undo_capacity;
    AppCanvasClickResult want_result;
    int want_drawing;
    int want_shaping;
    int want_preview_active;
    int want_needs_composite;
    int want_undo_count;
    size_t snapshot_index;
    uint32_t want_snapshot;
    size_t pixel_index;
    uint32_t want_pixel;
} HandleLeftCanvasReleaseCase;

static int run_handle_left_canvas_release_case(const HandleLeftCanvasReleaseCase *test_case) {
    LayerStack stack;
    Canvas canvas;
    uint32_t pixels[9];
    int drawing = test_case->initial_drawing;
    int shaping = test_case->initial_shaping;
    int preview_active = test_case->initial_preview_active;

    init_single_layer_stack(
        &stack,
        &canvas,
        pixels,
        test_case->width,
        test_case->height,
        test_case->initial_fill,
        test_case->locked
    );
    if (test_case->preset_pixel_index < (size_t)test_case->width * (size_t)test_case->height) {
        pixels[test_case->preset_pixel_index] = test_case->preset_pixel_value;
    }

    return expect_handle_left_canvas_release(
        test_case->label,
        &drawing,
        &stack,
        &shaping,
        &preview_active,
        test_case->shape_start_x,
        test_case->shape_start_y,
        test_case->x,
        test_case->y,
        test_case->shift,
        test_case->tool,
        test_case->brush_radius,
        test_case->brush_color,
        test_case->undo_capacity,
        test_case->want_result,
        test_case->want_drawing,
        test_case->want_shaping,
        test_case->want_preview_active,
        test_case->want_needs_composite,
        test_case->want_undo_count,
        test_case->snapshot_index,
        test_case->want_snapshot,
        test_case->pixel_index,
        test_case->want_pixel
    );
}

typedef struct {
    const char *label;
    int initial_shaping;
    int initial_preview_active;
    const Canvas *composite;
    const Canvas *preview_canvas;
    int preview_canvas_active;
    int x;
    int y;
    Tool initial_tool;
    unsigned int initial_brush_color;
    unsigned int initial_brush_color_rgb;
    int initial_brush_opacity;
    AppCanvasClickResult want_result;
    int want_shaping;
    int want_preview_active;
    Tool want_tool;
    unsigned int want_brush_color;
    unsigned int want_brush_color_rgb;
    int want_brush_opacity;
} HandleRightCanvasPressCase;

static int run_handle_right_canvas_press_case(
    const HandleRightCanvasPressCase *test_case,
    int *shaping,
    int *preview_active
) {
    *shaping = test_case->initial_shaping;
    *preview_active = test_case->initial_preview_active;
    return expect_handle_right_canvas_press(
        test_case->label,
        shaping,
        preview_active,
        test_case->composite,
        test_case->preview_canvas,
        test_case->preview_canvas_active,
        test_case->x,
        test_case->y,
        test_case->initial_tool,
        test_case->initial_brush_color,
        test_case->initial_brush_color_rgb,
        test_case->initial_brush_opacity,
        test_case->want_result,
        test_case->want_shaping,
        test_case->want_preview_active,
        test_case->want_tool,
        test_case->want_brush_color,
        test_case->want_brush_color_rgb,
        test_case->want_brush_opacity
    );
}

static int expect_canvas_click_title_refresh(
    const char *label,
    AppCanvasClickResult result,
    int want
) {
    int got = app_canvas_click_result_refreshes_title(result);

    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    AppCanvasClickResult result;
    int want;
} CanvasClickTitleRefreshCase;

static int run_canvas_click_title_refresh_case(
    const CanvasClickTitleRefreshCase *test_case
) {
    return expect_canvas_click_title_refresh(
        test_case->label,
        test_case->result,
        test_case->want
    );
}

typedef struct {
    const char *label;
    int width;
    int height;
    uint32_t initial_fill;
    int locked;
    size_t preset_pixel_index;
    uint32_t preset_pixel_value;
    int x;
    int y;
    Tool tool;
    BrushShape shape;
    int radius;
    uint32_t brush_color;
    int initial_drawing;
    int initial_shaping;
    int initial_shape_start_x;
    int initial_shape_start_y;
    int initial_last_x;
    int initial_last_y;
    int initial_needs_composite;
    uint32_t initial_shape_base_fill;
    AppCanvasClickResult want_result;
    int want_drawing;
    int want_shaping;
    int want_shape_start_x;
    int want_shape_start_y;
    int want_last_x;
    int want_last_y;
    int want_needs_composite;
    size_t changed_index;
    uint32_t want_changed;
    size_t snapshot_index;
    uint32_t want_snapshot;
    int expect_shape_base_copy;
} HandleLeftCanvasPressCase;

static int run_handle_left_canvas_press_case(const HandleLeftCanvasPressCase *test_case) {
    LayerStack stack;
    Canvas canvas;
    uint32_t pixels[9];
    uint32_t shape_base_pixels[9];
    uint32_t want_shape_base_pixels[9];
    size_t pixel_count = (size_t)test_case->width * (size_t)test_case->height;

    init_single_layer_stack(&stack, &canvas, pixels, test_case->width, test_case->height, test_case->initial_fill, test_case->locked);
    if (test_case->preset_pixel_index < pixel_count) {
        pixels[test_case->preset_pixel_index] = test_case->preset_pixel_value;
    }
    memset(shape_base_pixels, (unsigned char)test_case->initial_shape_base_fill, pixel_count * sizeof(*shape_base_pixels));
    memcpy(want_shape_base_pixels, shape_base_pixels, pixel_count * sizeof(*want_shape_base_pixels));
    if (test_case->expect_shape_base_copy) {
        memcpy(want_shape_base_pixels, pixels, pixel_count * sizeof(*want_shape_base_pixels));
    }

    return expect_handle_left_canvas_press(
        test_case->label,
        &stack,
        test_case->x,
        test_case->y,
        test_case->tool,
        test_case->shape,
        test_case->radius,
        test_case->brush_color,
        &canvas,
        test_case->initial_drawing,
        test_case->initial_shaping,
        test_case->initial_shape_start_x,
        test_case->initial_shape_start_y,
        test_case->initial_last_x,
        test_case->initial_last_y,
        test_case->initial_needs_composite,
        shape_base_pixels,
        want_shape_base_pixels,
        pixel_count,
        test_case->want_result,
        test_case->want_drawing,
        test_case->want_shaping,
        test_case->want_shape_start_x,
        test_case->want_shape_start_y,
        test_case->want_last_x,
        test_case->want_last_y,
        test_case->want_needs_composite,
        test_case->changed_index,
        test_case->want_changed,
        test_case->snapshot_index,
        test_case->want_snapshot
    );
}

static int expect_continue_direct_stroke(
    const char *label,
    LayerStack *stack,
    int initial_last_x,
    int initial_last_y,
    int x,
    int y,
    Tool tool,
    BrushShape shape,
    int radius,
    uint32_t brush_color,
    int initial_needs_composite,
    int want_continued,
    int want_last_x,
    int want_last_y,
    int want_needs_composite,
    size_t changed_index,
    uint32_t want_changed
) {
    int last_x = initial_last_x;
    int last_y = initial_last_y;
    int needs_composite = initial_needs_composite;
    int continued = app_continue_direct_stroke(
        stack,
        &last_x,
        &last_y,
        x,
        y,
        tool,
        shape,
        radius,
        brush_color,
        &needs_composite
    );

    if (continued != want_continued) {
        fprintf(stderr, "%s continue mismatch: got %d want %d\n", label, continued, want_continued);
        return 0;
    }
    if (last_x != want_last_x || last_y != want_last_y) {
        fprintf(stderr, "%s last mismatch: got {%d,%d} want {%d,%d}\n", label, last_x, last_y, want_last_x, want_last_y);
        return 0;
    }
    if (needs_composite != want_needs_composite) {
        fprintf(stderr, "%s needs_composite mismatch: got %d want %d\n", label, needs_composite, want_needs_composite);
        return 0;
    }
    if (stack && stack->layer_count > 0 && stack->layers[stack->active_layer].canvas.pixels) {
        if (stack->layers[stack->active_layer].canvas.pixels[changed_index] != want_changed) {
            fprintf(
                stderr,
                "%s pixel mismatch: got 0x%08X want 0x%08X\n",
                label,
                stack->layers[stack->active_layer].canvas.pixels[changed_index],
                want_changed
            );
            return 0;
        }
    }
    return 1;
}

static void init_single_layer_stack(
    LayerStack *stack,
    Canvas *canvas,
    uint32_t *pixels,
    int width,
    int height,
    uint32_t fill,
    int locked
) {
    size_t i;
    size_t pixel_count = (size_t)width * (size_t)height;

    *stack = (LayerStack){0};
    stack->width = width;
    stack->height = height;
    stack->layer_count = 1;
    stack->active_layer = 0;
    stack->solo_index = -1;
    stack->layers[0].canvas.width = width;
    stack->layers[0].canvas.height = height;
    stack->layers[0].canvas.pixels = pixels;
    stack->layers[0].visible = 1;
    stack->layers[0].locked = locked;
    stack->layers[0].opacity_percent = 100;
    if (canvas) {
        *canvas = stack->layers[0].canvas;
    }
    for (i = 0; i < pixel_count; i++) {
        pixels[i] = fill;
    }
}

typedef struct {
    const char *label;
    int x;
    int y;
    Tool tool;
    BrushShape shape;
    int radius;
    uint32_t brush_color;
    uint32_t initial_fill;
    int locked;
    int initial_drawing;
    int initial_needs_composite;
    int want_started;
    int want_drawing;
    int want_needs_composite;
    size_t changed_index;
    uint32_t want_changed;
    size_t snapshot_index;
    uint32_t want_snapshot;
} BeginDirectStrokeCase;

static int run_begin_direct_stroke_case(const BeginDirectStrokeCase *test_case) {
    LayerStack stack;
    Canvas canvas;
    uint32_t pixels[9];

    init_single_layer_stack(&stack, &canvas, pixels, 3, 3, test_case->initial_fill, test_case->locked);
    return expect_begin_direct_stroke(
        test_case->label,
        &stack,
        test_case->x,
        test_case->y,
        test_case->tool,
        test_case->shape,
        test_case->radius,
        test_case->brush_color,
        test_case->initial_drawing,
        test_case->initial_needs_composite,
        test_case->want_started,
        test_case->want_drawing,
        test_case->want_needs_composite,
        test_case->changed_index,
        test_case->want_changed,
        test_case->snapshot_index,
        test_case->want_snapshot
    );
}

typedef struct {
    const char *label;
    int initial_last_x;
    int initial_last_y;
    int x;
    int y;
    Tool tool;
    BrushShape shape;
    int radius;
    uint32_t brush_color;
    uint32_t initial_fill;
    int locked;
    int initial_needs_composite;
    int want_continued;
    int want_last_x;
    int want_last_y;
    int want_needs_composite;
    size_t changed_index;
    uint32_t want_changed;
} ContinueDirectStrokeCase;

static int run_continue_direct_stroke_case(const ContinueDirectStrokeCase *test_case) {
    LayerStack stack;
    Canvas canvas;
    uint32_t pixels[25];

    init_single_layer_stack(&stack, &canvas, pixels, 5, 5, test_case->initial_fill, test_case->locked);
    return expect_continue_direct_stroke(
        test_case->label,
        &stack,
        test_case->initial_last_x,
        test_case->initial_last_y,
        test_case->x,
        test_case->y,
        test_case->tool,
        test_case->shape,
        test_case->radius,
        test_case->brush_color,
        test_case->initial_needs_composite,
        test_case->want_continued,
        test_case->want_last_x,
        test_case->want_last_y,
        test_case->want_needs_composite,
        test_case->changed_index,
        test_case->want_changed
    );
}

static int expect_canvas_action(const char *label, int key, CanvasShortcutAction want) {
    CanvasShortcutAction got = canvas_shortcut_action(key);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int key;
    CanvasShortcutAction want;
} CanvasActionCase;

static int run_canvas_action_case(const CanvasActionCase *test_case) {
    return expect_canvas_action(test_case->label, test_case->key, test_case->want);
}

static int expect_canvas_mutation_shortcut(
    const char *label,
    CanvasShortcutAction action,
    int locked,
    const uint32_t *initial_pixels,
    int want_handled,
    int want_needs_composite,
    int want_undo_count,
    const uint32_t *want_pixels
) {
    LayerStack stack;
    Canvas canvas;
    uint32_t pixels[4];
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int needs_composite = 0;
    int handled = 0;
    size_t i;

    init_single_layer_stack(&stack, &canvas, pixels, 2, 2, 0x00000000u, locked);
    for (i = 0; i < 4; i++) {
        pixels[i] = initial_pixels[i];
    }

    handled = app_handle_canvas_mutation_shortcut(
        action,
        &stack,
        undo_stack,
        &undo_count,
        2,
        redo_stack,
        &redo_count,
        &needs_composite
    );

    if (handled != want_handled) {
        fprintf(stderr, "%s handled mismatch: got %d want %d\n", label, handled, want_handled);
        return 0;
    }
    if (needs_composite != want_needs_composite) {
        fprintf(stderr, "%s needs_composite mismatch: got %d want %d\n", label, needs_composite, want_needs_composite);
        return 0;
    }
    if (undo_count != want_undo_count || redo_count != 0) {
        fprintf(stderr, "%s history count mismatch: undo %d/%d redo %d/0\n", label, undo_count, want_undo_count, redo_count);
        return 0;
    }
    for (i = 0; i < 4; i++) {
        if (pixels[i] != want_pixels[i]) {
            fprintf(stderr, "%s pixels[%zu] mismatch: got 0x%08X want 0x%08X\n", label, i, pixels[i], want_pixels[i]);
            return 0;
        }
    }
    return 1;
}

typedef struct {
    const char *label;
    CanvasShortcutAction action;
    int locked;
    uint32_t initial_pixels[4];
    int want_handled;
    int want_needs_composite;
    int want_undo_count;
    uint32_t want_pixels[4];
} CanvasMutationShortcutCase;

static int run_canvas_mutation_shortcut_case(const CanvasMutationShortcutCase *test_case) {
    return expect_canvas_mutation_shortcut(
        test_case->label,
        test_case->action,
        test_case->locked,
        test_case->initial_pixels,
        test_case->want_handled,
        test_case->want_needs_composite,
        test_case->want_undo_count,
        test_case->want_pixels
    );
}

static int expect_brush_and_paint_shortcut_runtime(
    const char *label,
    PaintShortcutAction paint_action,
    BrushShortcutAction brush_action,
    Tool initial_tool,
    BrushShape initial_brush_shape,
    int initial_brush_radius,
    uint32_t initial_brush_color,
    uint32_t initial_brush_color_rgb,
    int initial_brush_opacity,
    int want_handled,
    Tool want_tool,
    BrushShape want_brush_shape,
    int want_brush_radius,
    uint32_t want_brush_color,
    uint32_t want_brush_color_rgb,
    int want_brush_opacity
) {
    Tool tool = initial_tool;
    BrushShape brush_shape = initial_brush_shape;
    int brush_radius = initial_brush_radius;
    uint32_t brush_color = initial_brush_color;
    uint32_t brush_color_rgb = initial_brush_color_rgb;
    int brush_opacity = initial_brush_opacity;
    int handled = app_handle_brush_and_paint_shortcut(
        paint_action,
        brush_action,
        &tool,
        &brush_shape,
        &brush_radius,
        &brush_color,
        &brush_color_rgb,
        &brush_opacity
    );

    if (handled != want_handled) {
        fprintf(stderr, "%s handled mismatch: got %d want %d\n", label, handled, want_handled);
        return 0;
    }
    if (tool != want_tool || brush_shape != want_brush_shape || brush_radius != want_brush_radius ||
        brush_color != want_brush_color || brush_color_rgb != want_brush_color_rgb || brush_opacity != want_brush_opacity) {
        fprintf(
            stderr,
            "%s state mismatch: got {%d,%d,%d,0x%08X,0x%08X,%d} want {%d,%d,%d,0x%08X,0x%08X,%d}\n",
            label,
            tool,
            brush_shape,
            brush_radius,
            brush_color,
            brush_color_rgb,
            brush_opacity,
            want_tool,
            want_brush_shape,
            want_brush_radius,
            want_brush_color,
            want_brush_color_rgb,
            want_brush_opacity
        );
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    PaintShortcutAction paint_action;
    BrushShortcutAction brush_action;
    Tool initial_tool;
    BrushShape initial_brush_shape;
    int initial_brush_radius;
    uint32_t initial_brush_color;
    uint32_t initial_brush_color_rgb;
    int initial_brush_opacity;
    int want_handled;
    Tool want_tool;
    BrushShape want_brush_shape;
    int want_brush_radius;
    uint32_t want_brush_color;
    uint32_t want_brush_color_rgb;
    int want_brush_opacity;
} BrushAndPaintShortcutRuntimeCase;

static int run_brush_and_paint_shortcut_runtime_case(const BrushAndPaintShortcutRuntimeCase *test_case) {
    return expect_brush_and_paint_shortcut_runtime(
        test_case->label,
        test_case->paint_action,
        test_case->brush_action,
        test_case->initial_tool,
        test_case->initial_brush_shape,
        test_case->initial_brush_radius,
        test_case->initial_brush_color,
        test_case->initial_brush_color_rgb,
        test_case->initial_brush_opacity,
        test_case->want_handled,
        test_case->want_tool,
        test_case->want_brush_shape,
        test_case->want_brush_radius,
        test_case->want_brush_color,
        test_case->want_brush_color_rgb,
        test_case->want_brush_opacity
    );
}

static int expect_layer_opacity_reset_shortcut(
    const char *label,
    int key,
    int ctrl,
    int initial_opacity,
    int want_handled,
    int want_needs_composite,
    int want_undo_count,
    int want_opacity
) {
    LayerStack stack = {0};
    uint32_t pixels[4] = {0};
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int needs_composite = 0;
    int handled = 0;

    init_single_layer_stack(&stack, &stack.layers[0].canvas, pixels, 2, 2, 0xFF112233u, 0);
    stack.layers[0].opacity_percent = initial_opacity;

    handled = app_handle_layer_opacity_reset_shortcut(
        key,
        ctrl,
        &stack,
        undo_stack,
        &undo_count,
        2,
        redo_stack,
        &redo_count,
        &needs_composite
    );

    if (handled != want_handled) {
        fprintf(stderr, "%s handled mismatch: got %d want %d\n", label, handled, want_handled);
        return 0;
    }
    if (needs_composite != want_needs_composite) {
        fprintf(stderr, "%s needs_composite mismatch: got %d want %d\n", label, needs_composite, want_needs_composite);
        return 0;
    }
    if (undo_count != want_undo_count || redo_count != 0) {
        fprintf(stderr, "%s history count mismatch: undo %d/%d redo %d/0\n", label, undo_count, want_undo_count, redo_count);
        return 0;
    }
    if (stack.layers[0].opacity_percent != want_opacity) {
        fprintf(stderr, "%s opacity mismatch: got %d want %d\n", label, stack.layers[0].opacity_percent, want_opacity);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int key;
    int ctrl;
    int initial_opacity;
    int want_handled;
    int want_needs_composite;
    int want_undo_count;
    int want_opacity;
} LayerOpacityResetShortcutCase;

static int run_layer_opacity_reset_shortcut_case(const LayerOpacityResetShortcutCase *test_case) {
    return expect_layer_opacity_reset_shortcut(
        test_case->label,
        test_case->key,
        test_case->ctrl,
        test_case->initial_opacity,
        test_case->want_handled,
        test_case->want_needs_composite,
        test_case->want_undo_count,
        test_case->want_opacity
    );
}

static int expect_layer_visibility_shortcut_runtime(
    const char *label,
    int key,
    int ctrl,
    int shift,
    int layer_count,
    int active_layer,
    int solo_index,
    int active_visible,
    int second_visible,
    int want_handled,
    int want_needs_composite,
    int want_undo_count,
    int want_active_visible,
    int want_second_visible,
    int want_solo_index
) {
    LayerStack stack = {0};
    uint32_t pixels0[4] = {0};
    uint32_t pixels1[4] = {0};
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int needs_composite = 0;
    int handled = 0;

    stack.width = 2;
    stack.height = 2;
    stack.layer_count = layer_count;
    stack.active_layer = active_layer;
    stack.solo_index = solo_index;

    stack.layers[0].canvas.width = 2;
    stack.layers[0].canvas.height = 2;
    stack.layers[0].canvas.pixels = pixels0;
    stack.layers[0].visible = active_visible;
    stack.layers[0].opacity_percent = 100;

    stack.layers[1].canvas.width = 2;
    stack.layers[1].canvas.height = 2;
    stack.layers[1].canvas.pixels = pixels1;
    stack.layers[1].visible = second_visible;
    stack.layers[1].opacity_percent = 100;

    handled = app_handle_layer_visibility_shortcut(
        key,
        ctrl,
        shift,
        &stack,
        undo_stack,
        &undo_count,
        2,
        redo_stack,
        &redo_count,
        &needs_composite
    );

    if (handled != want_handled) {
        fprintf(stderr, "%s handled mismatch: got %d want %d\n", label, handled, want_handled);
        return 0;
    }
    if (needs_composite != want_needs_composite) {
        fprintf(stderr, "%s needs_composite mismatch: got %d want %d\n", label, needs_composite, want_needs_composite);
        return 0;
    }
    if (undo_count != want_undo_count || redo_count != 0) {
        fprintf(stderr, "%s history count mismatch: undo %d/%d redo %d/0\n", label, undo_count, want_undo_count, redo_count);
        return 0;
    }
    if (stack.layers[0].visible != want_active_visible || stack.layers[1].visible != want_second_visible || stack.solo_index != want_solo_index) {
        fprintf(
            stderr,
            "%s visibility mismatch: got {%d,%d,%d} want {%d,%d,%d}\n",
            label,
            stack.layers[0].visible,
            stack.layers[1].visible,
            stack.solo_index,
            want_active_visible,
            want_second_visible,
            want_solo_index
        );
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int key;
    int ctrl;
    int shift;
    int layer_count;
    int active_layer;
    int solo_index;
    int active_visible;
    int second_visible;
    int want_handled;
    int want_needs_composite;
    int want_undo_count;
    int want_active_visible;
    int want_second_visible;
    int want_solo_index;
} LayerVisibilityShortcutRuntimeCase;

static int run_layer_visibility_shortcut_runtime_case(const LayerVisibilityShortcutRuntimeCase *test_case) {
    return expect_layer_visibility_shortcut_runtime(
        test_case->label,
        test_case->key,
        test_case->ctrl,
        test_case->shift,
        test_case->layer_count,
        test_case->active_layer,
        test_case->solo_index,
        test_case->active_visible,
        test_case->second_visible,
        test_case->want_handled,
        test_case->want_needs_composite,
        test_case->want_undo_count,
        test_case->want_active_visible,
        test_case->want_second_visible,
        test_case->want_solo_index
    );
}

static int expect_active_layer_opacity_step_runtime(
    const char *label,
    int initial_opacity,
    int delta,
    int want_handled,
    int want_needs_composite,
    int want_undo_count,
    int want_opacity
) {
    LayerStack stack = {0};
    uint32_t pixels[4] = {0};
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int needs_composite = 0;
    int handled = 0;

    init_single_layer_stack(&stack, &stack.layers[0].canvas, pixels, 2, 2, 0xFF112233u, 0);
    stack.layers[0].opacity_percent = initial_opacity;

    handled = app_handle_active_layer_opacity_step(
        &stack,
        delta,
        undo_stack,
        &undo_count,
        2,
        redo_stack,
        &redo_count,
        &needs_composite
    );

    if (handled != want_handled) {
        fprintf(stderr, "%s handled mismatch: got %d want %d\n", label, handled, want_handled);
        return 0;
    }
    if (needs_composite != want_needs_composite) {
        fprintf(stderr, "%s needs_composite mismatch: got %d want %d\n", label, needs_composite, want_needs_composite);
        return 0;
    }
    if (undo_count != want_undo_count || redo_count != 0) {
        fprintf(stderr, "%s history count mismatch: undo %d/%d redo %d/0\n", label, undo_count, want_undo_count, redo_count);
        return 0;
    }
    if (stack.layers[0].opacity_percent != want_opacity) {
        fprintf(stderr, "%s opacity mismatch: got %d want %d\n", label, stack.layers[0].opacity_percent, want_opacity);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int initial_opacity;
    int delta;
    int want_handled;
    int want_needs_composite;
    int want_undo_count;
    int want_opacity;
} ActiveLayerOpacityStepRuntimeCase;

static int run_active_layer_opacity_step_runtime_case(const ActiveLayerOpacityStepRuntimeCase *test_case) {
    return expect_active_layer_opacity_step_runtime(
        test_case->label,
        test_case->initial_opacity,
        test_case->delta,
        test_case->want_handled,
        test_case->want_needs_composite,
        test_case->want_undo_count,
        test_case->want_opacity
    );
}

static int expect_canvas_sample_shortcut(
    const char *label,
    CanvasShortcutAction action,
    int locked,
    const uint32_t *initial_pixels,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_active,
    int x,
    int y,
    Tool initial_tool,
    uint32_t initial_brush_color,
    uint32_t initial_brush_color_rgb,
    int initial_brush_opacity,
    int want_handled,
    int want_needs_composite,
    int want_undo_count,
    const uint32_t *want_pixels,
    Tool want_tool,
    uint32_t want_brush_color,
    uint32_t want_brush_color_rgb,
    int want_brush_opacity
) {
    LayerStack stack;
    Canvas canvas;
    uint32_t pixels[4];
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int needs_composite = 0;
    int handled = 0;
    Tool tool = initial_tool;
    uint32_t brush_color = initial_brush_color;
    uint32_t brush_color_rgb = initial_brush_color_rgb;
    int brush_opacity = initial_brush_opacity;
    size_t i;

    init_single_layer_stack(&stack, &canvas, pixels, 2, 2, 0x00000000u, locked);
    for (i = 0; i < 4; i++) {
        pixels[i] = initial_pixels[i];
    }

    handled = app_handle_canvas_sample_shortcut_at(
        action,
        &stack,
        composite,
        preview_canvas,
        preview_active,
        x,
        y,
        2,
        2,
        undo_stack,
        &undo_count,
        2,
        redo_stack,
        &redo_count,
        &tool,
        &brush_color,
        &brush_color_rgb,
        &brush_opacity,
        &needs_composite
    );

    if (handled != want_handled) {
        fprintf(stderr, "%s handled mismatch: got %d want %d\n", label, handled, want_handled);
        return 0;
    }
    if (needs_composite != want_needs_composite) {
        fprintf(stderr, "%s needs_composite mismatch: got %d want %d\n", label, needs_composite, want_needs_composite);
        return 0;
    }
    if (undo_count != want_undo_count || redo_count != 0) {
        fprintf(stderr, "%s history count mismatch: undo %d/%d redo %d/0\n", label, undo_count, want_undo_count, redo_count);
        return 0;
    }
    for (i = 0; i < 4; i++) {
        if (pixels[i] != want_pixels[i]) {
            fprintf(stderr, "%s pixels[%zu] mismatch: got 0x%08X want 0x%08X\n", label, i, pixels[i], want_pixels[i]);
            return 0;
        }
    }
    if (tool != want_tool || brush_color != want_brush_color || brush_color_rgb != want_brush_color_rgb || brush_opacity != want_brush_opacity) {
        fprintf(
            stderr,
            "%s brush state mismatch: got {%d,0x%08X,0x%08X,%d} want {%d,0x%08X,0x%08X,%d}\n",
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

typedef struct {
    const char *label;
    CanvasShortcutAction action;
    int locked;
    uint32_t initial_pixels[4];
    const Canvas *composite;
    const Canvas *preview_canvas;
    int preview_active;
    int x;
    int y;
    Tool initial_tool;
    uint32_t initial_brush_color;
    uint32_t initial_brush_color_rgb;
    int initial_brush_opacity;
    int want_handled;
    int want_needs_composite;
    int want_undo_count;
    uint32_t want_pixels[4];
    Tool want_tool;
    uint32_t want_brush_color;
    uint32_t want_brush_color_rgb;
    int want_brush_opacity;
} CanvasSampleShortcutCase;

static int run_canvas_sample_shortcut_case(const CanvasSampleShortcutCase *test_case) {
    return expect_canvas_sample_shortcut(
        test_case->label,
        test_case->action,
        test_case->locked,
        test_case->initial_pixels,
        test_case->composite,
        test_case->preview_canvas,
        test_case->preview_active,
        test_case->x,
        test_case->y,
        test_case->initial_tool,
        test_case->initial_brush_color,
        test_case->initial_brush_color_rgb,
        test_case->initial_brush_opacity,
        test_case->want_handled,
        test_case->want_needs_composite,
        test_case->want_undo_count,
        test_case->want_pixels,
        test_case->want_tool,
        test_case->want_brush_color,
        test_case->want_brush_color_rgb,
        test_case->want_brush_opacity
    );
}

static int expect_view_shortcut_runtime(
    const char *label,
    ViewShortcutResult view_result,
    int layer_count,
    int active_layer,
    int locked,
    const uint32_t *initial_pixels,
    int want_handled,
    int want_needs_composite,
    int want_active_layer,
    int want_undo_count,
    const uint32_t *want_pixels
) {
    LayerStack stack = {0};
    uint32_t pixels0[4] = {0};
    uint32_t pixels1[4] = {0};
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int needs_composite = 0;
    int handled = 0;
    size_t i;

    stack.width = 2;
    stack.height = 2;
    stack.layer_count = layer_count;
    stack.active_layer = active_layer;
    stack.solo_index = -1;

    stack.layers[0].canvas.width = 2;
    stack.layers[0].canvas.height = 2;
    stack.layers[0].canvas.pixels = pixels0;
    stack.layers[0].visible = 1;
    stack.layers[0].opacity_percent = 100;
    stack.layers[0].locked = locked;

    stack.layers[1].canvas.width = 2;
    stack.layers[1].canvas.height = 2;
    stack.layers[1].canvas.pixels = pixels1;
    stack.layers[1].visible = 1;
    stack.layers[1].opacity_percent = 100;
    stack.layers[1].locked = 0;

    for (i = 0; i < 4; i++) {
        pixels0[i] = initial_pixels[i];
        pixels1[i] = 0xFF100000u + (uint32_t)i;
    }

    handled = app_handle_view_shortcut(
        view_result,
        &stack,
        undo_stack,
        &undo_count,
        2,
        redo_stack,
        &redo_count,
        &needs_composite
    );

    if (handled != want_handled) {
        fprintf(stderr, "%s handled mismatch: got %d want %d\n", label, handled, want_handled);
        return 0;
    }
    if (needs_composite != want_needs_composite) {
        fprintf(stderr, "%s needs_composite mismatch: got %d want %d\n", label, needs_composite, want_needs_composite);
        return 0;
    }
    if (stack.active_layer != want_active_layer) {
        fprintf(stderr, "%s active layer mismatch: got %d want %d\n", label, stack.active_layer, want_active_layer);
        return 0;
    }
    if (undo_count != want_undo_count || redo_count != 0) {
        fprintf(stderr, "%s history count mismatch: undo %d/%d redo %d/0\n", label, undo_count, want_undo_count, redo_count);
        return 0;
    }
    for (i = 0; i < 4; i++) {
        if (pixels0[i] != want_pixels[i]) {
            fprintf(stderr, "%s pixels[%zu] mismatch: got 0x%08X want 0x%08X\n", label, i, pixels0[i], want_pixels[i]);
            return 0;
        }
    }
    return 1;
}

typedef struct {
    const char *label;
    ViewShortcutResult view_result;
    int layer_count;
    int active_layer;
    int locked;
    uint32_t initial_pixels[4];
    int want_handled;
    int want_needs_composite;
    int want_active_layer;
    int want_undo_count;
    uint32_t want_pixels[4];
} ViewShortcutRuntimeCase;

static int run_view_shortcut_runtime_case(const ViewShortcutRuntimeCase *test_case) {
    return expect_view_shortcut_runtime(
        test_case->label,
        test_case->view_result,
        test_case->layer_count,
        test_case->active_layer,
        test_case->locked,
        test_case->initial_pixels,
        test_case->want_handled,
        test_case->want_needs_composite,
        test_case->want_active_layer,
        test_case->want_undo_count,
        test_case->want_pixels
    );
}

static int expect_shape_cancel(const char *label, int key, int ctrl, int want) {
    int got = app_should_cancel_shape_on_key(key, ctrl);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int key;
    int ctrl;
    int want;
} ShapeCancelCase;

static int run_shape_cancel_case(const ShapeCancelCase *test_case) {
    return expect_shape_cancel(
        test_case->label,
        test_case->key,
        test_case->ctrl,
        test_case->want
    );
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
    int use_shaping;
    int use_shape_start_x;
    int use_shape_start_y;
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
    int *shaping_ptr = NULL;
    int *shape_start_x_ptr = NULL;
    int *shape_start_y_ptr = NULL;

    if (test_case->use_shaping) {
        *shaping = test_case->initial_shaping;
        shaping_ptr = shaping;
    }
    if (test_case->use_shape_start_x) {
        *shape_start_x = test_case->initial_shape_start_x;
        shape_start_x_ptr = shape_start_x;
    }
    if (test_case->use_shape_start_y) {
        *shape_start_y = test_case->initial_shape_start_y;
        shape_start_y_ptr = shape_start_y;
    }

    return expect_begin_shape_preview(
        test_case->label,
        test_case->start_x,
        test_case->start_y,
        shaping_ptr,
        shape_start_x_ptr,
        shape_start_y_ptr,
        test_case->shape_base_pixels,
        test_case->composite_pixels,
        test_case->pixel_count,
        test_case->want_shaping,
        test_case->want_shape_start_x,
        test_case->want_shape_start_y,
        test_case->want_shape_base_pixels
    );
}

typedef struct {
    const char *label;
    int start_x;
    int start_y;
    int initial_shaping;
    int initial_shape_start_x;
    int initial_shape_start_y;
    uint32_t initial_shape_base_pixels[4];
    const uint32_t *composite_pixels;
    size_t pixel_count;
    int want_shaping;
    int want_shape_start_x;
    int want_shape_start_y;
    uint32_t want_shape_base_pixels[4];
} BeginShapePreviewPartialCopyCase;

static int run_begin_shape_preview_partial_copy_case(
    const BeginShapePreviewPartialCopyCase *test_case,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y
) {
    uint32_t shape_base_pixels[4];

    memcpy(shape_base_pixels, test_case->initial_shape_base_pixels, sizeof(shape_base_pixels));
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
        shape_base_pixels,
        test_case->composite_pixels,
        test_case->pixel_count,
        test_case->want_shaping,
        test_case->want_shape_start_x,
        test_case->want_shape_start_y,
        test_case->want_shape_base_pixels
    );
}

static int expect_begin_shape_preview_from_canvas(
    const char *label,
    int start_x,
    int start_y,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y,
    uint32_t *shape_base_pixels,
    const Canvas *composite,
    int want_shaping,
    int want_shape_start_x,
    int want_shape_start_y,
    const uint32_t *want_shape_base_pixels,
    size_t want_pixel_count
) {
    size_t i;

    app_begin_shape_preview_from_canvas(
        start_x,
        start_y,
        shaping,
        shape_start_x,
        shape_start_y,
        shape_base_pixels,
        composite
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
        for (i = 0; i < want_pixel_count; i++) {
            if (shape_base_pixels[i] != want_shape_base_pixels[i]) {
                fprintf(stderr, "%s shape_base_pixels[%zu] mismatch: got 0x%08X want 0x%08X\n", label, i, shape_base_pixels[i], want_shape_base_pixels[i]);
                return 0;
            }
        }
    }
    return 1;
}

static int expect_begin_shape_preview_to_active_layer(
    const char *label,
    LayerStack *layers,
    int start_x,
    int start_y,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y,
    uint32_t *shape_base_pixels,
    const Canvas *composite,
    int want_started,
    int want_shaping,
    int want_shape_start_x,
    int want_shape_start_y,
    const uint32_t *want_shape_base_pixels,
    size_t want_pixel_count
) {
    size_t i;
    int started = app_begin_shape_preview_to_active_layer(
        layers,
        start_x,
        start_y,
        shaping,
        shape_start_x,
        shape_start_y,
        shape_base_pixels,
        composite
    );

    if (started != want_started) {
        fprintf(stderr, "%s started mismatch: got %d want %d\n", label, started, want_started);
        return 0;
    }
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
        for (i = 0; i < want_pixel_count; i++) {
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
    int use_stack;
    int locked;
    int start_x;
    int start_y;
    int use_shaping;
    int use_shape_start_x;
    int use_shape_start_y;
    int initial_shaping;
    int initial_shape_start_x;
    int initial_shape_start_y;
    int want_started;
    int want_shaping;
    int want_shape_start_x;
    int want_shape_start_y;
    const uint32_t *want_shape_base_pixels;
    size_t want_pixel_count;
} BeginShapePreviewToActiveLayerCase;

static int run_begin_shape_preview_to_active_layer_case(
    const BeginShapePreviewToActiveLayerCase *test_case,
    uint32_t *shape_base_pixels,
    const Canvas *composite
) {
    LayerStack stack;
    Canvas canvas;
    uint32_t pixels[4];
    LayerStack *layers = NULL;
    int shaping = test_case->initial_shaping;
    int shape_start_x = test_case->initial_shape_start_x;
    int shape_start_y = test_case->initial_shape_start_y;
    int *shaping_ptr = NULL;
    int *shape_start_x_ptr = NULL;
    int *shape_start_y_ptr = NULL;

    if (test_case->use_stack) {
        init_single_layer_stack(&stack, &canvas, pixels, 2, 2, 0x00000000u, test_case->locked);
        layers = &stack;
    }
    if (test_case->use_shaping) {
        shaping_ptr = &shaping;
    }
    if (test_case->use_shape_start_x) {
        shape_start_x_ptr = &shape_start_x;
    }
    if (test_case->use_shape_start_y) {
        shape_start_y_ptr = &shape_start_y;
    }

    return expect_begin_shape_preview_to_active_layer(
        test_case->label,
        layers,
        test_case->start_x,
        test_case->start_y,
        shaping_ptr,
        shape_start_x_ptr,
        shape_start_y_ptr,
        shape_base_pixels,
        composite,
        test_case->want_started,
        test_case->want_shaping,
        test_case->want_shape_start_x,
        test_case->want_shape_start_y,
        test_case->want_shape_base_pixels,
        test_case->want_pixel_count
    );
}

typedef struct {
    const char *label;
    int start_x;
    int start_y;
    int use_shaping;
    int use_shape_start_x;
    int use_shape_start_y;
    int initial_shaping;
    int initial_shape_start_x;
    int initial_shape_start_y;
    uint32_t *shape_base_pixels;
    const Canvas *composite;
    int want_shaping;
    int want_shape_start_x;
    int want_shape_start_y;
    const uint32_t *want_shape_base_pixels;
    size_t want_pixel_count;
} BeginShapePreviewFromCanvasCase;

static int run_begin_shape_preview_from_canvas_case(
    const BeginShapePreviewFromCanvasCase *test_case,
    int *shaping,
    int *shape_start_x,
    int *shape_start_y
) {
    int *shaping_ptr = NULL;
    int *shape_start_x_ptr = NULL;
    int *shape_start_y_ptr = NULL;

    if (test_case->use_shaping) {
        *shaping = test_case->initial_shaping;
        shaping_ptr = shaping;
    }
    if (test_case->use_shape_start_x) {
        *shape_start_x = test_case->initial_shape_start_x;
        shape_start_x_ptr = shape_start_x;
    }
    if (test_case->use_shape_start_y) {
        *shape_start_y = test_case->initial_shape_start_y;
        shape_start_y_ptr = shape_start_y;
    }

    return expect_begin_shape_preview_from_canvas(
        test_case->label,
        test_case->start_x,
        test_case->start_y,
        shaping_ptr,
        shape_start_x_ptr,
        shape_start_y_ptr,
        test_case->shape_base_pixels,
        test_case->composite,
        test_case->want_shaping,
        test_case->want_shape_start_x,
        test_case->want_shape_start_y,
        test_case->want_shape_base_pixels,
        test_case->want_pixel_count
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

typedef struct {
    const char *label;
    ViewShortcutKey key;
    int shift;
    ViewShortcutAction want_action;
    int want_cycle_direction;
    int want_dx;
    int want_dy;
} ViewResultCase;

static int run_view_result_case(const ViewResultCase *test_case) {
    return expect_view_result(
        test_case->label,
        test_case->key,
        test_case->shift,
        test_case->want_action,
        test_case->want_cycle_direction,
        test_case->want_dx,
        test_case->want_dy
    );
}

static int expect_brush_color(const char *label, unsigned int rgb_color, int opacity_percent, unsigned int want) {
    unsigned int got = app_compose_brush_color(rgb_color, opacity_percent);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    unsigned int rgb_color;
    int opacity_percent;
    unsigned int want;
} BrushColorCase;

static int run_brush_color_case(const BrushColorCase *test_case) {
    return expect_brush_color(
        test_case->label,
        test_case->rgb_color,
        test_case->opacity_percent,
        test_case->want
    );
}

static int expect_brush_mask(const char *label, BrushShape shape, int x, int y, int radius, int want) {
    int got = app_brush_mask_contains(shape, x, y, radius);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    BrushShape shape;
    int x;
    int y;
    int radius;
    int want;
} BrushMaskCase;

static int run_brush_mask_case(const BrushMaskCase *test_case) {
    return expect_brush_mask(
        test_case->label,
        test_case->shape,
        test_case->x,
        test_case->y,
        test_case->radius,
        test_case->want
    );
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

typedef struct {
    const char *label;
    unsigned int sampled_color;
    Tool initial_tool;
    unsigned int initial_brush_color;
    unsigned int initial_brush_color_rgb;
    int initial_brush_opacity;
    Tool want_tool;
    unsigned int want_brush_color;
    unsigned int want_brush_color_rgb;
    int want_brush_opacity;
} SampledBrushColorCase;

static int run_sampled_brush_color_case(const SampledBrushColorCase *test_case) {
    return expect_sampled_brush_color(
        test_case->label,
        test_case->sampled_color,
        test_case->initial_tool,
        test_case->initial_brush_color,
        test_case->initial_brush_color_rgb,
        test_case->initial_brush_opacity,
        test_case->want_tool,
        test_case->want_brush_color,
        test_case->want_brush_color_rgb,
        test_case->want_brush_opacity
    );
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

typedef struct {
    const char *label;
    unsigned int sampled_color;
    int use_tool;
    int use_brush_color;
    int use_brush_color_rgb;
    int use_brush_opacity;
    Tool initial_tool;
    unsigned int initial_brush_color;
    unsigned int initial_brush_color_rgb;
    int initial_brush_opacity;
    Tool want_tool;
    unsigned int want_brush_color;
    unsigned int want_brush_color_rgb;
    int want_brush_opacity;
} SampledBrushColorNoopCase;

static int run_sampled_brush_color_noop_case(
    const SampledBrushColorNoopCase *test_case
) {
    Tool tool = test_case->initial_tool;
    unsigned int brush_color = test_case->initial_brush_color;
    unsigned int brush_color_rgb = test_case->initial_brush_color_rgb;
    int brush_opacity = test_case->initial_brush_opacity;

    return expect_sampled_brush_color_noop(
        test_case->label,
        test_case->sampled_color,
        test_case->use_tool ? &tool : NULL,
        test_case->use_brush_color ? &brush_color : NULL,
        test_case->use_brush_color_rgb ? &brush_color_rgb : NULL,
        test_case->use_brush_opacity ? &brush_opacity : NULL,
        test_case->want_tool,
        test_case->want_brush_color,
        test_case->want_brush_color_rgb,
        test_case->want_brush_opacity
    );
}

static int expect_sampled_brush_color_from_canvas(
    const char *label,
    const Canvas *canvas,
    int x,
    int y,
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

    app_apply_sampled_brush_color_from_canvas(canvas, x, y, &tool, &brush_color, &brush_color_rgb, &brush_opacity);
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

typedef struct {
    const char *label;
    const Canvas *canvas;
    int x;
    int y;
    Tool initial_tool;
    unsigned int initial_brush_color;
    unsigned int initial_brush_color_rgb;
    int initial_brush_opacity;
    Tool want_tool;
    unsigned int want_brush_color;
    unsigned int want_brush_color_rgb;
    int want_brush_opacity;
} SampledBrushColorFromCanvasCase;

static int run_sampled_brush_color_from_canvas_case(
    const SampledBrushColorFromCanvasCase *test_case
) {
    return expect_sampled_brush_color_from_canvas(
        test_case->label,
        test_case->canvas,
        test_case->x,
        test_case->y,
        test_case->initial_tool,
        test_case->initial_brush_color,
        test_case->initial_brush_color_rgb,
        test_case->initial_brush_opacity,
        test_case->want_tool,
        test_case->want_brush_color,
        test_case->want_brush_color_rgb,
        test_case->want_brush_opacity
    );
}

static int expect_sampled_brush_color_from_available_canvas(
    const char *label,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_active,
    int x,
    int y,
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

    app_apply_sampled_brush_color_from_available_canvas(
        composite,
        preview_canvas,
        preview_active,
        x,
        y,
        &tool,
        &brush_color,
        &brush_color_rgb,
        &brush_opacity
    );
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

typedef struct {
    const char *label;
    const Canvas *composite;
    const Canvas *preview_canvas;
    int preview_active;
    int x;
    int y;
    Tool initial_tool;
    unsigned int initial_brush_color;
    unsigned int initial_brush_color_rgb;
    int initial_brush_opacity;
    Tool want_tool;
    unsigned int want_brush_color;
    unsigned int want_brush_color_rgb;
    int want_brush_opacity;
} SampledBrushColorFromAvailableCanvasCase;

static int run_sampled_brush_color_from_available_canvas_case(
    const SampledBrushColorFromAvailableCanvasCase *test_case
) {
    return expect_sampled_brush_color_from_available_canvas(
        test_case->label,
        test_case->composite,
        test_case->preview_canvas,
        test_case->preview_active,
        test_case->x,
        test_case->y,
        test_case->initial_tool,
        test_case->initial_brush_color,
        test_case->initial_brush_color_rgb,
        test_case->initial_brush_opacity,
        test_case->want_tool,
        test_case->want_brush_color,
        test_case->want_brush_color_rgb,
        test_case->want_brush_opacity
    );
}

static int expect_handle_available_canvas_sample(
    const char *label,
    int *shaping,
    int *preview_active,
    const Canvas *composite,
    const Canvas *preview_canvas,
    int preview_canvas_active,
    int x,
    int y,
    Tool initial_tool,
    unsigned int initial_brush_color,
    unsigned int initial_brush_color_rgb,
    int initial_brush_opacity,
    AppSampleBrushColorResult want_result,
    int want_shaping,
    int want_preview_active,
    Tool want_tool,
    unsigned int want_brush_color,
    unsigned int want_brush_color_rgb,
    int want_brush_opacity
) {
    Tool tool = initial_tool;
    unsigned int brush_color = initial_brush_color;
    unsigned int brush_color_rgb = initial_brush_color_rgb;
    int brush_opacity = initial_brush_opacity;
    AppSampleBrushColorResult got = app_handle_available_canvas_sample(
        shaping,
        preview_active,
        composite,
        preview_canvas,
        preview_canvas_active,
        x,
        y,
        &tool,
        &brush_color,
        &brush_color_rgb,
        &brush_opacity
    );

    if (got != want_result) {
        fprintf(stderr, "%s result mismatch: got %d want %d\n", label, got, want_result);
        return 0;
    }
    if (shaping && *shaping != want_shaping) {
        fprintf(stderr, "%s shaping mismatch: got %d want %d\n", label, *shaping, want_shaping);
        return 0;
    }
    if (preview_active && *preview_active != want_preview_active) {
        fprintf(stderr, "%s preview_active mismatch: got %d want %d\n", label, *preview_active, want_preview_active);
        return 0;
    }
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

typedef struct {
    const char *label;
    int initial_shaping;
    int initial_preview_active;
    const Canvas *composite;
    const Canvas *preview_canvas;
    int preview_canvas_active;
    int x;
    int y;
    Tool initial_tool;
    unsigned int initial_brush_color;
    unsigned int initial_brush_color_rgb;
    int initial_brush_opacity;
    AppSampleBrushColorResult want_result;
    int want_shaping;
    int want_preview_active;
    Tool want_tool;
    unsigned int want_brush_color;
    unsigned int want_brush_color_rgb;
    int want_brush_opacity;
} HandleAvailableCanvasSampleCase;

static int run_handle_available_canvas_sample_case(
    const HandleAvailableCanvasSampleCase *test_case,
    int *shaping,
    int *preview_active
) {
    *shaping = test_case->initial_shaping;
    *preview_active = test_case->initial_preview_active;
    return expect_handle_available_canvas_sample(
        test_case->label,
        shaping,
        preview_active,
        test_case->composite,
        test_case->preview_canvas,
        test_case->preview_canvas_active,
        test_case->x,
        test_case->y,
        test_case->initial_tool,
        test_case->initial_brush_color,
        test_case->initial_brush_color_rgb,
        test_case->initial_brush_opacity,
        test_case->want_result,
        test_case->want_shaping,
        test_case->want_preview_active,
        test_case->want_tool,
        test_case->want_brush_color,
        test_case->want_brush_color_rgb,
        test_case->want_brush_opacity
    );
}

static int expect_layer_clear_color(const char *label, int active_layer_index, unsigned int want) {
    unsigned int got = app_active_layer_clear_color(active_layer_index);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int active_layer_index;
    unsigned int want;
} LayerClearColorCase;

static int run_layer_clear_color_case(const LayerClearColorCase *test_case) {
    return expect_layer_clear_color(
        test_case->label,
        test_case->active_layer_index,
        test_case->want
    );
}

static int expect_layer_editable(const char *label, Layer *layer, int want) {
    int got = app_layer_editable(layer);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    Layer *layer;
    int want;
} LayerEditableCase;

static int run_layer_editable_case(const LayerEditableCase *test_case) {
    return expect_layer_editable(test_case->label, test_case->layer, test_case->want);
}

static int expect_active_layer_editable(const char *label, LayerStack *stack, int want) {
    int got = app_active_layer_editable(stack);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    LayerStack *stack;
    int active_layer;
    int want;
} ActiveLayerEditableCase;

static int run_active_layer_editable_case(const ActiveLayerEditableCase *test_case) {
    if (test_case->stack) {
        test_case->stack->active_layer = test_case->active_layer;
    }
    return expect_active_layer_editable(test_case->label, test_case->stack, test_case->want);
}

static int expect_active_editable_layer(const char *label, LayerStack *stack, Layer *want) {
    Layer *got = app_active_editable_layer(stack);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %p want %p\n", label, (void *)got, (void *)want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    LayerStack *stack;
    int active_layer;
    Layer *want;
} ActiveEditableLayerCase;

static int run_active_editable_layer_case(const ActiveEditableLayerCase *test_case) {
    if (test_case->stack) {
        test_case->stack->active_layer = test_case->active_layer;
    }
    return expect_active_editable_layer(test_case->label, test_case->stack, test_case->want);
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

typedef struct {
    const char *label;
    const char *tool_label;
    const char *shape_label;
    int radius;
    int opacity_percent;
    int active_layer_index;
    int layer_count;
    const char *layer_name;
    int active_visible;
    int active_locked;
    int active_opacity_percent;
    int active_is_solo;
    int visible_layer_count;
    unsigned int color;
    const char *want;
} TitleCase;

static int run_title_case(const TitleCase *test_case) {
    return expect_title(
        test_case->label,
        test_case->tool_label,
        test_case->shape_label,
        test_case->radius,
        test_case->opacity_percent,
        test_case->active_layer_index,
        test_case->layer_count,
        test_case->layer_name,
        test_case->active_visible,
        test_case->active_locked,
        test_case->active_opacity_percent,
        test_case->active_is_solo,
        test_case->visible_layer_count,
        test_case->color,
        test_case->want
    );
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

typedef struct {
    const char *label;
    size_t title_size;
    const char *tool_label;
    const char *shape_label;
    int radius;
    int opacity_percent;
    int active_layer_index;
    int layer_count;
    const char *layer_name;
    int active_visible;
    int active_locked;
    int active_opacity_percent;
    int active_is_solo;
    int visible_layer_count;
    unsigned int color;
    const char *want_prefix;
} TitlePrefixCase;

static int run_title_prefix_case(const TitlePrefixCase *test_case) {
    return expect_title_prefix(
        test_case->label,
        test_case->title_size,
        test_case->tool_label,
        test_case->shape_label,
        test_case->radius,
        test_case->opacity_percent,
        test_case->active_layer_index,
        test_case->layer_count,
        test_case->layer_name,
        test_case->active_visible,
        test_case->active_locked,
        test_case->active_opacity_percent,
        test_case->active_is_solo,
        test_case->visible_layer_count,
        test_case->color,
        test_case->want_prefix
    );
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

typedef struct {
    const char *label;
    size_t title_size;
    const char *tool_label;
    const char *shape_label;
    int radius;
    int opacity_percent;
    int active_layer_index;
    int layer_count;
    const char *layer_name;
    int active_visible;
    int active_locked;
    int active_opacity_percent;
    int active_is_solo;
    int visible_layer_count;
    unsigned int color;
} TitleEmptyBufferCase;

static int run_title_empty_buffer_case(const TitleEmptyBufferCase *test_case) {
    return expect_title_empty_buffer(
        test_case->label,
        test_case->title_size,
        test_case->tool_label,
        test_case->shape_label,
        test_case->radius,
        test_case->opacity_percent,
        test_case->active_layer_index,
        test_case->layer_count,
        test_case->layer_name,
        test_case->active_visible,
        test_case->active_locked,
        test_case->active_opacity_percent,
        test_case->active_is_solo,
        test_case->visible_layer_count,
        test_case->color
    );
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

typedef struct {
    const char *label;
    size_t title_size;
    const char *tool_label;
    const char *shape_label;
    int radius;
    int opacity_percent;
    int active_layer_index;
    int layer_count;
    const char *layer_name;
    int active_visible;
    int active_locked;
    int active_opacity_percent;
    int active_is_solo;
    int visible_layer_count;
    unsigned int color;
} TitleUnchangedBufferCase;

static int run_title_unchanged_buffer_case(const TitleUnchangedBufferCase *test_case) {
    return expect_title_unchanged_buffer(
        test_case->label,
        test_case->title_size,
        test_case->tool_label,
        test_case->shape_label,
        test_case->radius,
        test_case->opacity_percent,
        test_case->active_layer_index,
        test_case->layer_count,
        test_case->layer_name,
        test_case->active_visible,
        test_case->active_locked,
        test_case->active_opacity_percent,
        test_case->active_is_solo,
        test_case->visible_layer_count,
        test_case->color
    );
}

static int expect_tool_label(const char *label, Tool tool, const char *want) {
    const char *got = app_tool_label(tool);
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "%s mismatch:\n got  %s\n want %s\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    Tool tool;
    const char *want;
} ToolLabelCase;

static int run_tool_label_case(const ToolLabelCase *test_case) {
    return expect_tool_label(test_case->label, test_case->tool, test_case->want);
}

static int expect_brush_shape_label(const char *label, BrushShape shape, const char *want) {
    const char *got = app_brush_shape_label(shape);
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "%s mismatch:\n got  %s\n want %s\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    BrushShape shape;
    const char *want;
} BrushShapeLabelCase;

static int run_brush_shape_label_case(const BrushShapeLabelCase *test_case) {
    return expect_brush_shape_label(test_case->label, test_case->shape, test_case->want);
}

static int expect_cycle_brush_shape(const char *label, BrushShape shape, int direction, BrushShape want) {
    BrushShape got = app_cycle_brush_shape(shape, direction);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    BrushShape shape;
    int direction;
    BrushShape want;
} CycleBrushShapeCase;

static int run_cycle_brush_shape_case(const CycleBrushShapeCase *test_case) {
    return expect_cycle_brush_shape(
        test_case->label,
        test_case->shape,
        test_case->direction,
        test_case->want
    );
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

typedef struct {
    const char *label;
    Tool tool;
    int x0;
    int y0;
    int x1;
    int y1;
    int shift;
    int want_x;
    int want_y;
} ConstrainedShapeEndCase;

static int run_constrained_shape_end_case(const ConstrainedShapeEndCase *test_case) {
    return expect_constrained_shape_end(
        test_case->label,
        test_case->tool,
        test_case->x0,
        test_case->y0,
        test_case->x1,
        test_case->y1,
        test_case->shift,
        test_case->want_x,
        test_case->want_y
    );
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

typedef struct {
    const char *label;
    Tool tool;
    int x0;
    int y0;
    int x1;
    int y1;
    int shift;
    int use_out_x;
    int use_out_y;
    int initial_x;
    int initial_y;
    int want_x;
    int want_y;
} ConstrainedShapeEndNoOutputCase;

static int run_constrained_shape_end_no_output_case(
    const ConstrainedShapeEndNoOutputCase *test_case
) {
    int out_x = test_case->initial_x;
    int out_y = test_case->initial_y;

    return expect_constrained_shape_end_no_output(
        test_case->label,
        test_case->tool,
        test_case->x0,
        test_case->y0,
        test_case->x1,
        test_case->y1,
        test_case->shift,
        test_case->use_out_x ? &out_x : NULL,
        test_case->use_out_y ? &out_y : NULL,
        test_case->want_x,
        test_case->want_y
    );
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

typedef struct {
    const char *label;
    int use_shaping;
    int use_preview_active;
    int initial_shaping;
    int initial_preview_active;
    int want_shaping;
    int want_preview_active;
} CancelShapePreviewCase;

static int run_cancel_shape_preview_case(const CancelShapePreviewCase *test_case) {
    int shaping = test_case->initial_shaping;
    int preview_active = test_case->initial_preview_active;

    return expect_cancel_shape_preview(
        test_case->label,
        test_case->use_shaping ? &shaping : NULL,
        test_case->use_preview_active ? &preview_active : NULL,
        test_case->want_shaping,
        test_case->want_preview_active
    );
}

static int expect_handle_shape_preview_key(
    const char *label,
    AppShapeCancelKey key,
    int ctrl,
    int *shaping,
    int *preview_active,
    int *running,
    AppPreviewKeyResult want_result,
    int want_shaping,
    int want_preview_active,
    int want_running
) {
    AppPreviewKeyResult got = app_handle_shape_preview_key(key, ctrl, shaping, preview_active, running);

    if (got != want_result) {
        fprintf(stderr, "%s result mismatch: got %d want %d\n", label, got, want_result);
        return 0;
    }
    if (shaping && *shaping != want_shaping) {
        fprintf(stderr, "%s shaping mismatch: got %d want %d\n", label, *shaping, want_shaping);
        return 0;
    }
    if (preview_active && *preview_active != want_preview_active) {
        fprintf(stderr, "%s preview_active mismatch: got %d want %d\n", label, *preview_active, want_preview_active);
        return 0;
    }
    if (running && *running != want_running) {
        fprintf(stderr, "%s running mismatch: got %d want %d\n", label, *running, want_running);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    AppShapeCancelKey key;
    int ctrl;
    int initial_shaping;
    int initial_preview_active;
    int initial_running;
    AppPreviewKeyResult want_result;
    int want_shaping;
    int want_preview_active;
    int want_running;
} HandleShapePreviewKeyCase;

static int run_handle_shape_preview_key_case(const HandleShapePreviewKeyCase *test_case) {
    int shaping = test_case->initial_shaping;
    int preview_active = test_case->initial_preview_active;
    int running = test_case->initial_running;

    return expect_handle_shape_preview_key(
        test_case->label,
        test_case->key,
        test_case->ctrl,
        &shaping,
        &preview_active,
        &running,
        test_case->want_result,
        test_case->want_shaping,
        test_case->want_preview_active,
        test_case->want_running
    );
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

typedef struct {
    const char *label;
    const Canvas *composite;
    const Canvas *preview_canvas;
    int preview_active;
    const Canvas *want;
} PreviewCanvasSelectionCase;

static int run_preview_canvas_selection_case(const PreviewCanvasSelectionCase *test_case) {
    return expect_preview_canvas_selection(
        test_case->label,
        test_case->composite,
        test_case->preview_canvas,
        test_case->preview_active,
        test_case->want
    );
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

typedef struct {
    const char *label;
    uint32_t *preview_pixels;
    const uint32_t *shape_base_pixels;
    size_t pixel_count;
    int initial_preview_active;
    int use_preview_active_flag;
    int want_preview_active;
    const uint32_t *want_preview_pixels;
    size_t want_pixel_count;
} RestoreShapePreviewCase;

static int run_restore_shape_preview_case(const RestoreShapePreviewCase *test_case) {
    int preview_active = test_case->initial_preview_active;
    int *preview_active_ptr = test_case->use_preview_active_flag ? &preview_active : NULL;

    return expect_restore_shape_preview(
        test_case->label,
        test_case->preview_pixels,
        test_case->shape_base_pixels,
        test_case->pixel_count,
        preview_active_ptr,
        test_case->want_preview_active,
        test_case->want_preview_pixels,
        test_case->want_pixel_count
    );
}

static int expect_prepare_shape_preview_motion(
    const char *label,
    Canvas *preview_canvas,
    uint32_t *preview_pixels,
    const uint32_t *shape_base_pixels,
    size_t pixel_count,
    int *preview_active,
    Tool tool,
    int shape_start_x,
    int shape_start_y,
    int x,
    int y,
    int shift,
    int want_prepared,
    int want_preview_active,
    int want_x,
    int want_y,
    const uint32_t *want_preview_pixels,
    size_t want_pixel_count
) {
    int got_x = -999;
    int got_y = -999;
    size_t i;
    int prepared = app_prepare_shape_preview_motion(
        preview_canvas,
        preview_pixels,
        shape_base_pixels,
        pixel_count,
        preview_active,
        tool,
        shape_start_x,
        shape_start_y,
        x,
        y,
        shift,
        &got_x,
        &got_y
    );

    if (prepared != want_prepared) {
        fprintf(stderr, "%s prepared mismatch: got %d want %d\n", label, prepared, want_prepared);
        return 0;
    }
    if (prepared) {
        if (got_x != want_x || got_y != want_y) {
            fprintf(stderr, "%s end mismatch: got {%d,%d} want {%d,%d}\n", label, got_x, got_y, want_x, want_y);
            return 0;
        }
    }
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

typedef struct {
    const char *label;
    int canvas_width;
    int canvas_height;
    uint32_t *preview_canvas_pixels;
    uint32_t *preview_pixels;
    const uint32_t *shape_base_pixels;
    size_t pixel_count;
    int initial_preview_active;
    Tool tool;
    int shape_start_x;
    int shape_start_y;
    int x;
    int y;
    int shift;
    int want_prepared;
    int want_preview_active;
    int want_x;
    int want_y;
    const uint32_t *want_preview_pixels;
    size_t want_preview_pixel_count;
} PrepareShapePreviewMotionCase;

static int run_prepare_shape_preview_motion_case(const PrepareShapePreviewMotionCase *test_case) {
    Canvas preview_canvas = {
        test_case->canvas_width,
        test_case->canvas_height,
        test_case->preview_canvas_pixels
    };
    int preview_active = test_case->initial_preview_active;

    return expect_prepare_shape_preview_motion(
        test_case->label,
        &preview_canvas,
        test_case->preview_pixels,
        test_case->shape_base_pixels,
        test_case->pixel_count,
        &preview_active,
        test_case->tool,
        test_case->shape_start_x,
        test_case->shape_start_y,
        test_case->x,
        test_case->y,
        test_case->shift,
        test_case->want_prepared,
        test_case->want_preview_active,
        test_case->want_x,
        test_case->want_y,
        test_case->want_preview_pixels,
        test_case->want_preview_pixel_count
    );
}

static int expect_prepare_shape_preview_motion_rejection(
    const char *label,
    Canvas *preview_canvas,
    uint32_t *preview_pixels,
    const uint32_t *shape_base_pixels,
    size_t pixel_count,
    int *preview_active,
    Tool tool,
    int shape_start_x,
    int shape_start_y,
    int x,
    int y,
    int shift,
    int *out_x,
    int *out_y,
    int want_preview_active,
    const uint32_t *want_preview_pixels,
    size_t want_pixel_count
) {
    size_t i;
    int prepared = app_prepare_shape_preview_motion(
        preview_canvas,
        preview_pixels,
        shape_base_pixels,
        pixel_count,
        preview_active,
        tool,
        shape_start_x,
        shape_start_y,
        x,
        y,
        shift,
        out_x,
        out_y
    );

    if (prepared != 0) {
        fprintf(stderr, "%s prepared mismatch: got %d want 0\n", label, prepared);
        return 0;
    }
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

static int expect_prepare_shape_commit(
    const char *label,
    const int *shaping,
    Tool tool,
    int shape_start_x,
    int shape_start_y,
    int x,
    int y,
    int shift,
    int *out_x,
    int *out_y,
    int want_prepared,
    int want_x,
    int want_y
) {
    int prepared = app_prepare_shape_commit(
        shaping,
        tool,
        shape_start_x,
        shape_start_y,
        x,
        y,
        shift,
        out_x,
        out_y
    );

    if (prepared != want_prepared) {
        fprintf(stderr, "%s prepared mismatch: got %d want %d\n", label, prepared, want_prepared);
        return 0;
    }
    if (out_x && *out_x != want_x) {
        fprintf(stderr, "%s out_x mismatch: got %d want %d\n", label, *out_x, want_x);
        return 0;
    }
    if (out_y && *out_y != want_y) {
        fprintf(stderr, "%s out_y mismatch: got %d want %d\n", label, *out_y, want_y);
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    int use_shaping;
    int shaping_value;
    Tool tool;
    int shape_start_x;
    int shape_start_y;
    int x;
    int y;
    int shift;
    int use_out_x;
    int initial_out_x;
    int use_out_y;
    int initial_out_y;
    int want_prepared;
    int want_x;
    int want_y;
} PrepareShapeCommitCase;

static int run_prepare_shape_commit_case(const PrepareShapeCommitCase *test_case) {
    int shaping = test_case->shaping_value;
    int out_x = test_case->initial_out_x;
    int out_y = test_case->initial_out_y;

    return expect_prepare_shape_commit(
        test_case->label,
        test_case->use_shaping ? &shaping : NULL,
        test_case->tool,
        test_case->shape_start_x,
        test_case->shape_start_y,
        test_case->x,
        test_case->y,
        test_case->shift,
        test_case->use_out_x ? &out_x : NULL,
        test_case->use_out_y ? &out_y : NULL,
        test_case->want_prepared,
        test_case->want_x,
        test_case->want_y
    );
}

static int expect_prepare_shape_commit_to_active_layer(
    const char *label,
    LayerStack *layers,
    const int *shaping,
    Tool tool,
    int shape_start_x,
    int shape_start_y,
    int x,
    int y,
    int shift,
    int undo_capacity,
    int want_x,
    int want_y,
    int want_undo_count,
    size_t snapshot_index,
    uint32_t want_snapshot,
    Layer *want_layer
) {
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int out_x = -999;
    int out_y = -999;
    int ok = 1;
    Layer *got = app_prepare_shape_commit_to_active_layer(
        layers,
        shaping,
        tool,
        shape_start_x,
        shape_start_y,
        x,
        y,
        shift,
        undo_stack,
        &undo_count,
        undo_capacity,
        redo_stack,
        &redo_count,
        &out_x,
        &out_y
    );

    if (got != want_layer) {
        fprintf(stderr, "%s layer mismatch: got %p want %p\n", label, (void *)got, (void *)want_layer);
        ok = 0;
    }
    if (out_x != want_x || out_y != want_y) {
        fprintf(stderr, "%s end mismatch: got {%d,%d} want {%d,%d}\n", label, out_x, out_y, want_x, want_y);
        ok = 0;
    }
    if (undo_count != want_undo_count || redo_count != 0) {
        fprintf(stderr, "%s history mismatch: undo=%d want %d redo=%d want 0\n", label, undo_count, want_undo_count, redo_count);
        ok = 0;
    }
    if (want_undo_count > 0) {
        if (!undo_stack[0].pixels || undo_stack[0].pixels[snapshot_index] != want_snapshot) {
            fprintf(
                stderr,
                "%s snapshot mismatch: pixel=0x%08X want 0x%08X\n",
                label,
                undo_stack[0].pixels ? undo_stack[0].pixels[snapshot_index] : 0u,
                want_snapshot
            );
            ok = 0;
        }
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    return ok;
}

typedef struct {
    const char *label;
    int use_stack;
    int locked;
    int use_shaping;
    int shaping_value;
    Tool tool;
    int shape_start_x;
    int shape_start_y;
    int x;
    int y;
    int shift;
    int undo_capacity;
    int want_x;
    int want_y;
    int want_undo_count;
    size_t snapshot_index;
    uint32_t want_snapshot;
    int want_layer_active;
} PrepareShapeCommitToActiveLayerCase;

static int run_prepare_shape_commit_to_active_layer_case(
    const PrepareShapeCommitToActiveLayerCase *test_case
) {
    LayerStack stack;
    Canvas canvas;
    uint32_t pixels[9];
    int shaping = test_case->shaping_value;
    LayerStack *layers = NULL;
    Layer *want_layer = NULL;

    if (test_case->use_stack) {
        init_single_layer_stack(&stack, &canvas, pixels, 3, 3, 0xFF123456u, test_case->locked);
        layers = &stack;
        if (test_case->want_layer_active) {
            want_layer = &stack.layers[0];
        }
    }

    return expect_prepare_shape_commit_to_active_layer(
        test_case->label,
        layers,
        test_case->use_shaping ? &shaping : NULL,
        test_case->tool,
        test_case->shape_start_x,
        test_case->shape_start_y,
        test_case->x,
        test_case->y,
        test_case->shift,
        test_case->undo_capacity,
        test_case->want_x,
        test_case->want_y,
        test_case->want_undo_count,
        test_case->snapshot_index,
        test_case->want_snapshot,
        want_layer
    );
}

static int expect_finalize_shape_preview(
    const char *label,
    LayerStack *layers,
    int *shaping,
    int *preview_active,
    int shape_start_x,
    int shape_start_y,
    int x,
    int y,
    int shift,
    Tool tool,
    int brush_radius,
    uint32_t brush_color,
    int undo_capacity,
    int want_finalized,
    int want_shaping,
    int want_preview_active,
    int want_needs_composite,
    int want_undo_count,
    size_t snapshot_index,
    uint32_t want_snapshot,
    size_t pixel_index,
    uint32_t want_pixel
) {
    Snapshot undo_stack[2] = {0};
    Snapshot redo_stack[2] = {0};
    int undo_count = 0;
    int redo_count = 0;
    int needs_composite = 0;
    int finalized = app_finalize_shape_preview(
        layers,
        shaping,
        preview_active,
        shape_start_x,
        shape_start_y,
        x,
        y,
        shift,
        tool,
        brush_radius,
        brush_color,
        undo_stack,
        &undo_count,
        undo_capacity,
        redo_stack,
        &redo_count,
        &needs_composite
    );
    int ok = 1;

    if (finalized != want_finalized) {
        fprintf(stderr, "%s finalized mismatch: got %d want %d\n", label, finalized, want_finalized);
        ok = 0;
    }
    if (shaping && *shaping != want_shaping) {
        fprintf(stderr, "%s shaping mismatch: got %d want %d\n", label, *shaping, want_shaping);
        ok = 0;
    }
    if (preview_active && *preview_active != want_preview_active) {
        fprintf(stderr, "%s preview_active mismatch: got %d want %d\n", label, *preview_active, want_preview_active);
        ok = 0;
    }
    if (needs_composite != want_needs_composite) {
        fprintf(stderr, "%s needs_composite mismatch: got %d want %d\n", label, needs_composite, want_needs_composite);
        ok = 0;
    }
    if (undo_count != want_undo_count || redo_count != 0) {
        fprintf(stderr, "%s history mismatch: undo=%d want %d redo=%d want 0\n", label, undo_count, want_undo_count, redo_count);
        ok = 0;
    }
    if (want_undo_count > 0) {
        if (!undo_stack[0].pixels || undo_stack[0].pixels[snapshot_index] != want_snapshot) {
            fprintf(
                stderr,
                "%s snapshot mismatch: pixel=0x%08X want 0x%08X\n",
                label,
                undo_stack[0].pixels ? undo_stack[0].pixels[snapshot_index] : 0u,
                want_snapshot
            );
            ok = 0;
        }
    }
    if (layers && pixel_index < (size_t)layers->width * (size_t)layers->height) {
        uint32_t got_pixel = layers->layers[layers->active_layer].canvas.pixels[pixel_index];
        if (got_pixel != want_pixel) {
            fprintf(stderr, "%s canvas pixel mismatch: got 0x%08X want 0x%08X\n", label, got_pixel, want_pixel);
            ok = 0;
        }
    }

    snapshot_stack_clear(undo_stack, &undo_count);
    snapshot_stack_clear(redo_stack, &redo_count);
    return ok;
}

typedef struct {
    const char *label;
    int width;
    int height;
    uint32_t initial_fill;
    int locked;
    size_t preset_pixel_index;
    uint32_t preset_pixel_value;
    int initial_shaping;
    int initial_preview_active;
    int shape_start_x;
    int shape_start_y;
    int x;
    int y;
    int shift;
    Tool tool;
    int brush_radius;
    uint32_t brush_color;
    int undo_capacity;
    int want_finalized;
    int want_shaping;
    int want_preview_active;
    int want_needs_composite;
    int want_undo_count;
    size_t snapshot_index;
    uint32_t want_snapshot;
    size_t pixel_index;
    uint32_t want_pixel;
} FinalizeShapePreviewCase;

static int run_finalize_shape_preview_case(const FinalizeShapePreviewCase *test_case) {
    LayerStack stack;
    Canvas canvas;
    uint32_t pixels[9];
    int shaping = test_case->initial_shaping;
    int preview_active = test_case->initial_preview_active;

    init_single_layer_stack(
        &stack,
        &canvas,
        pixels,
        test_case->width,
        test_case->height,
        test_case->initial_fill,
        test_case->locked
    );
    if (test_case->preset_pixel_index < (size_t)test_case->width * (size_t)test_case->height) {
        pixels[test_case->preset_pixel_index] = test_case->preset_pixel_value;
    }

    return expect_finalize_shape_preview(
        test_case->label,
        &stack,
        &shaping,
        &preview_active,
        test_case->shape_start_x,
        test_case->shape_start_y,
        test_case->x,
        test_case->y,
        test_case->shift,
        test_case->tool,
        test_case->brush_radius,
        test_case->brush_color,
        test_case->undo_capacity,
        test_case->want_finalized,
        test_case->want_shaping,
        test_case->want_preview_active,
        test_case->want_needs_composite,
        test_case->want_undo_count,
        test_case->snapshot_index,
        test_case->want_snapshot,
        test_case->pixel_index,
        test_case->want_pixel
    );
}

static int expect_draw_shape_pixel(
    const char *label,
    Tool tool,
    int x0,
    int y0,
    int x1,
    int y1,
    int radius,
    uint32_t initial_color,
    uint32_t draw_color,
    size_t changed_index,
    uint32_t want_changed,
    size_t unchanged_index,
    uint32_t want_unchanged
) {
    Canvas canvas = {5, 5, NULL};
    uint32_t pixels[25];
    size_t i;

    for (i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++) {
        pixels[i] = initial_color;
    }
    canvas.pixels = pixels;

    app_draw_shape(&canvas, tool, x0, y0, x1, y1, radius, draw_color);
    if (pixels[changed_index] != want_changed || pixels[unchanged_index] != want_unchanged) {
        fprintf(
            stderr,
            "%s mismatch: changed 0x%08X want 0x%08X unchanged 0x%08X want 0x%08X\n",
            label,
            pixels[changed_index],
            want_changed,
            pixels[unchanged_index],
            want_unchanged
        );
        return 0;
    }
    return 1;
}

typedef struct {
    const char *label;
    Tool tool;
    int x0;
    int y0;
    int x1;
    int y1;
    int radius;
    uint32_t initial_color;
    uint32_t draw_color;
    size_t changed_index;
    uint32_t want_changed;
    size_t unchanged_index;
    uint32_t want_unchanged;
} DrawShapeCase;

static int run_draw_shape_case(const DrawShapeCase *test_case) {
    return expect_draw_shape_pixel(
        test_case->label,
        test_case->tool,
        test_case->x0,
        test_case->y0,
        test_case->x1,
        test_case->y1,
        test_case->radius,
        test_case->initial_color,
        test_case->draw_color,
        test_case->changed_index,
        test_case->want_changed,
        test_case->unchanged_index,
        test_case->want_unchanged
    );
}

typedef struct {
    const char *label;
    Canvas *preview_canvas;
    uint32_t *preview_pixels;
    const uint32_t *shape_base_pixels;
    size_t pixel_count;
    int *preview_active;
    Tool tool;
    int shape_start_x;
    int shape_start_y;
    int x;
    int y;
    int shift;
    int *out_x;
    int *out_y;
    int want_preview_active;
    const uint32_t *want_preview_pixels;
    size_t want_pixel_count;
} PrepareShapePreviewMotionRejectionCase;

static int run_prepare_shape_preview_motion_rejection_case(
    const PrepareShapePreviewMotionRejectionCase *test_case
) {
    return expect_prepare_shape_preview_motion_rejection(
        test_case->label,
        test_case->preview_canvas,
        test_case->preview_pixels,
        test_case->shape_base_pixels,
        test_case->pixel_count,
        test_case->preview_active,
        test_case->tool,
        test_case->shape_start_x,
        test_case->shape_start_y,
        test_case->x,
        test_case->y,
        test_case->shift,
        test_case->out_x,
        test_case->out_y,
        test_case->want_preview_active,
        test_case->want_preview_pixels,
        test_case->want_pixel_count
    );
}

int main(void) {
    int ok = 1;
    int shaping = 1;
    int preview_active = 1;
    int shape_start_x = -1;
    int shape_start_y = -1;
    uint32_t preview_source[4] = {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
    uint32_t preview_copy[4] = {0u, 0u, 0u, 0u};
    uint32_t preview_sentinel[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
    Layer editable_layer = {0};
    Layer locked_layer = {0};
    Layer empty_layer = {0};
    LayerStack layer_stack = {0};
    Canvas composite_canvas = {4, 4, preview_source};
    Canvas preview_canvas = {4, 4, preview_copy};
    Canvas preview_canvas_without_pixels = {4, 4, NULL};
    Canvas begin_preview_canvas = {2, 2, preview_source};
    Canvas begin_preview_canvas_without_pixels = {2, 2, NULL};
    uint32_t sampled_canvas_pixels[4] = {0xFF102030u, 0x80445566u, 0x00000000u, 0xFFABCDEFu};
    Canvas sampled_canvas = {2, 2, sampled_canvas_pixels};
    Canvas sampled_canvas_without_pixels = {2, 2, NULL};
    uint32_t sampled_preview_pixels[4] = {0xFF010203u, 0xFF112233u, 0xFF223344u, 0xFF334455u};
    Canvas sampled_preview_canvas = {2, 2, sampled_preview_pixels};
    Canvas sampled_preview_without_pixels = {2, 2, NULL};
    uint32_t preview_restore_copy[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
    uint32_t preview_restore_source[4] = {0x01010101u, 0x02020202u, 0x03030303u, 0x04040404u};
    uint32_t preview_restore_sentinel[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};

    editable_layer.locked = 0;
    editable_layer.canvas.pixels = (uint32_t *)&editable_layer;
    locked_layer.locked = 1;
    locked_layer.canvas.pixels = (uint32_t *)&locked_layer;
    empty_layer.locked = 0;
    empty_layer.canvas.pixels = NULL;
    layer_stack.layer_count = 3;
    layer_stack.active_layer = 0;
    layer_stack.layers[0] = editable_layer;
    layer_stack.layers[1] = locked_layer;
    layer_stack.layers[2] = empty_layer;

    {
        const LayerNameResetShortcutCase layer_name_reset_cases[] = {
            {"plain_f2", 0, 0, 0, LAYER_NAME_RESET_SHORTCUT_ACTIVE},
            {"ctrl_f2", 1, 0, 0, LAYER_NAME_RESET_SHORTCUT_ALL},
            {"ctrl_shift_f2", 1, 0, 1, LAYER_NAME_RESET_SHORTCUT_UNLOCKED},
            {"ctrl_alt_f2", 1, 1, 0, LAYER_NAME_RESET_SHORTCUT_VISIBLE},
            {"ctrl_alt_shift_f2", 1, 1, 1, LAYER_NAME_RESET_SHORTCUT_NON_BACKGROUND_UNLOCKED},
            {"alt_f2", 0, 1, 0, LAYER_NAME_RESET_SHORTCUT_LOCKED},
            {"alt_shift_f2", 0, 1, 1, LAYER_NAME_RESET_SHORTCUT_NON_BACKGROUND_VISIBLE},
            {"shift_f2", 0, 0, 1, LAYER_NAME_RESET_SHORTCUT_NON_BACKGROUND_LOCKED},
        };
        const DirectLayerShortcutCase direct_layer_shortcut_cases[] = {
            {"plain_number", 1, 0, 0, DIRECT_LAYER_SHORTCUT_SELECT},
            {"shift_number", 1, 0, 1, DIRECT_LAYER_SHORTCUT_SOLO},
            {"alt_number", 1, 1, 0, DIRECT_LAYER_SHORTCUT_TOGGLE_VISIBILITY},
            {"alt_shift_number", 1, 1, 1, DIRECT_LAYER_SHORTCUT_TOGGLE_LOCK},
            {"missing_ctrl", 0, 0, 0, DIRECT_LAYER_SHORTCUT_NONE},
        };
        size_t i;

        for (i = 0; i < sizeof(layer_name_reset_cases) / sizeof(layer_name_reset_cases[0]); i++) {
            ok = ok && run_layer_name_reset_shortcut_case(&layer_name_reset_cases[i]);
        }
        for (i = 0; i < sizeof(direct_layer_shortcut_cases) / sizeof(direct_layer_shortcut_cases[0]); i++) {
            ok = ok && run_direct_layer_shortcut_case(&direct_layer_shortcut_cases[i]);
        }
    }
    {
        const HistoryShortcutCase history_shortcut_cases[] = {
            {"undo", 1, 'z', HISTORY_SHORTCUT_UNDO},
            {"redo", 1, 'y', HISTORY_SHORTCUT_REDO},
            {"missing_ctrl_history", 0, 'z', HISTORY_SHORTCUT_NONE},
            {"other_key_history", 1, 'x', HISTORY_SHORTCUT_NONE},
        };
        const FileShortcutCase file_shortcut_cases[] = {
            {"save", 1, 's', FILE_SHORTCUT_SAVE},
            {"load", 1, 'o', FILE_SHORTCUT_LOAD},
            {"missing_ctrl_file", 0, 's', FILE_SHORTCUT_NONE},
            {"other_key_file", 1, 'p', FILE_SHORTCUT_NONE},
        };
        const MergeShortcutCase merge_shortcut_cases[] = {
            {"merge_down", 1, 'm', MERGE_SHORTCUT_DOWN},
            {"merge_up", 1, 'u', MERGE_SHORTCUT_UP},
            {"missing_ctrl_merge", 0, 'm', MERGE_SHORTCUT_NONE},
            {"other_key_merge", 1, 'q', MERGE_SHORTCUT_NONE},
        };
        size_t i;

        for (i = 0; i < sizeof(history_shortcut_cases) / sizeof(history_shortcut_cases[0]); i++) {
            ok = ok && run_history_shortcut_case(&history_shortcut_cases[i]);
        }
        for (i = 0; i < sizeof(file_shortcut_cases) / sizeof(file_shortcut_cases[0]); i++) {
            ok = ok && run_file_shortcut_case(&file_shortcut_cases[i]);
        }
        for (i = 0; i < sizeof(merge_shortcut_cases) / sizeof(merge_shortcut_cases[0]); i++) {
            ok = ok && run_merge_shortcut_case(&merge_shortcut_cases[i]);
        }
    }
    {
        const HistoryNavigationShortcutCase history_navigation_cases[] = {
            {
                "history_navigation_undo",
                1, 'z',
                0xFF102030u, 0xFFABCDEFu,
                0,
                1, 1, 0, 1,
                0xFF102030u,
            },
            {
                "history_navigation_redo",
                1, 'y',
                0xFF102030u, 0xFFABCDEFu,
                1,
                1, 1, 1, 0,
                0xFFABCDEFu,
            },
            {
                "history_navigation_missing_ctrl_noop",
                0, 'z',
                0xFF102030u, 0xFFABCDEFu,
                0,
                0, 0, 1, 0,
                0xFFABCDEFu,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(history_navigation_cases) / sizeof(history_navigation_cases[0]); i++) {
            ok = ok && run_history_navigation_shortcut_case(&history_navigation_cases[i]);
        }
    }
    {
        const PaintShortcutCase paint_shortcut_cases[] = {
            {"tool_brush", 'b', PAINT_SHORTCUT_TOOL_BRUSH},
            {"tool_eraser", 'e', PAINT_SHORTCUT_TOOL_ERASER},
            {"tool_line", 'l', PAINT_SHORTCUT_TOOL_LINE},
            {"tool_rect", 'r', PAINT_SHORTCUT_TOOL_RECT},
            {"tool_filled_rect", 't', PAINT_SHORTCUT_TOOL_FILLED_RECT},
            {"tool_ellipse", 'o', PAINT_SHORTCUT_TOOL_ELLIPSE},
            {"tool_filled_ellipse", 'p', PAINT_SHORTCUT_TOOL_FILLED_ELLIPSE},
            {"color_brush", '1', PAINT_SHORTCUT_COLOR_BRUSH},
            {"color_red", '2', PAINT_SHORTCUT_COLOR_RED},
            {"color_green", '3', PAINT_SHORTCUT_COLOR_GREEN},
            {"color_blue", '4', PAINT_SHORTCUT_COLOR_BLUE},
            {"color_yellow", '5', PAINT_SHORTCUT_COLOR_YELLOW},
            {"color_purple", '6', PAINT_SHORTCUT_COLOR_PURPLE},
            {"paint_other_key", '7', PAINT_SHORTCUT_NONE},
        };
        const BrushShortcutCase brush_shortcut_cases[] = {
            {"radius_down", '[', BRUSH_SHORTCUT_RADIUS_DOWN},
            {"radius_up", ']', BRUSH_SHORTCUT_RADIUS_UP},
            {"shape_prev", ',', BRUSH_SHORTCUT_SHAPE_PREV},
            {"shape_next", '.', BRUSH_SHORTCUT_SHAPE_NEXT},
            {"opacity_down", '-', BRUSH_SHORTCUT_OPACITY_DOWN},
            {"opacity_up_equals", '=', BRUSH_SHORTCUT_OPACITY_UP},
            {"opacity_up_plus", '+', BRUSH_SHORTCUT_OPACITY_UP},
            {"brush_other_key", '/', BRUSH_SHORTCUT_NONE},
        };
        size_t i;

        for (i = 0; i < sizeof(paint_shortcut_cases) / sizeof(paint_shortcut_cases[0]); i++) {
            ok = ok && run_paint_shortcut_case(&paint_shortcut_cases[i]);
        }
        for (i = 0; i < sizeof(brush_shortcut_cases) / sizeof(brush_shortcut_cases[0]); i++) {
            ok = ok && run_brush_shortcut_case(&brush_shortcut_cases[i]);
        }
    }
    {
        const BrushAndPaintShortcutRuntimeCase brush_and_paint_cases[] = {
            {
                "runtime_shortcut_tool_brush",
                PAINT_SHORTCUT_TOOL_BRUSH, BRUSH_SHORTCUT_NONE,
                TOOL_LINE, BRUSH_SHAPE_ROUND, 5,
                0xAA123456u, 0x00123456u, 75,
                1,
                TOOL_BRUSH, BRUSH_SHAPE_ROUND, 5,
                app_compose_brush_color(0x001B1F24u, 75), 0x001B1F24u, 75,
            },
            {
                "runtime_shortcut_tool_eraser",
                PAINT_SHORTCUT_TOOL_ERASER, BRUSH_SHORTCUT_NONE,
                TOOL_BRUSH, BRUSH_SHAPE_SQUARE, 4,
                0x80112233u, 0x00112233u, 40,
                1,
                TOOL_ERASER, BRUSH_SHAPE_SQUARE, 4,
                app_compose_brush_color(0x00FFFFFFu, 40), 0x00FFFFFFu, 40,
            },
            {
                "runtime_shortcut_radius_clamps_down",
                PAINT_SHORTCUT_NONE, BRUSH_SHORTCUT_RADIUS_DOWN,
                TOOL_BRUSH, BRUSH_SHAPE_ROUND, 1,
                0xAA123456u, 0x00123456u, 60,
                1,
                TOOL_BRUSH, BRUSH_SHAPE_ROUND, 1,
                0xAA123456u, 0x00123456u, 60,
            },
            {
                "runtime_shortcut_shape_next",
                PAINT_SHORTCUT_NONE, BRUSH_SHORTCUT_SHAPE_NEXT,
                TOOL_BRUSH, BRUSH_SHAPE_ROUND, 6,
                0xAA123456u, 0x00123456u, 60,
                1,
                TOOL_BRUSH, BRUSH_SHAPE_SQUARE, 6,
                0xAA123456u, 0x00123456u, 60,
            },
            {
                "runtime_shortcut_opacity_up_clamps",
                PAINT_SHORTCUT_NONE, BRUSH_SHORTCUT_OPACITY_UP,
                TOOL_BRUSH, BRUSH_SHAPE_ROUND, 6,
                app_compose_brush_color(0x00123456u, 98), 0x00123456u, 98,
                1,
                TOOL_BRUSH, BRUSH_SHAPE_ROUND, 6,
                app_compose_brush_color(0x00123456u, 100), 0x00123456u, 100,
            },
            {
                "runtime_shortcut_color_purple_sets_brush",
                PAINT_SHORTCUT_COLOR_PURPLE, BRUSH_SHORTCUT_NONE,
                TOOL_ERASER, BRUSH_SHAPE_DIAMOND, 3,
                0xFF000000u, 0x00000000u, 55,
                1,
                TOOL_BRUSH, BRUSH_SHAPE_DIAMOND, 3,
                app_compose_brush_color(0x008E24AAu, 55), 0x008E24AAu, 55,
            },
            {
                "runtime_shortcut_none_noop",
                PAINT_SHORTCUT_NONE, BRUSH_SHORTCUT_NONE,
                TOOL_RECT, BRUSH_SHAPE_DIAMOND, 7,
                0xAA123456u, 0x00123456u, 65,
                0,
                TOOL_RECT, BRUSH_SHAPE_DIAMOND, 7,
                0xAA123456u, 0x00123456u, 65,
            },
        };
        Tool tool = TOOL_BRUSH;
        BrushShape brush_shape = BRUSH_SHAPE_ROUND;
        int brush_radius = 2;
        uint32_t brush_color = 0xAA123456u;
        uint32_t brush_color_rgb = 0x00123456u;
        int brush_opacity = 50;
        size_t i;

        for (i = 0; i < sizeof(brush_and_paint_cases) / sizeof(brush_and_paint_cases[0]); i++) {
            ok = ok && run_brush_and_paint_shortcut_runtime_case(&brush_and_paint_cases[i]);
        }

        if (app_handle_brush_and_paint_shortcut(
                PAINT_SHORTCUT_TOOL_BRUSH,
                BRUSH_SHORTCUT_NONE,
                NULL,
                &brush_shape,
                &brush_radius,
                &brush_color,
                &brush_color_rgb,
                &brush_opacity
            ) != 0) {
            fprintf(stderr, "runtime_shortcut_null_tool should reject null tool pointer\n");
            ok = 0;
        }
        if (tool != TOOL_BRUSH || brush_shape != BRUSH_SHAPE_ROUND || brush_radius != 2 ||
            brush_color != 0xAA123456u || brush_color_rgb != 0x00123456u || brush_opacity != 50) {
            fprintf(stderr, "runtime_shortcut_null_tool mutated state unexpectedly\n");
            ok = 0;
        }
    }
    {
        const LayerOpacityResetShortcutCase layer_opacity_reset_cases[] = {
            {"layer_opacity_reset_changes_active", '0', 1, 35, 1, 1, 1, 100},
            {"layer_opacity_reset_already_full", '0', 1, 100, 1, 0, 0, 100},
            {"layer_opacity_reset_missing_ctrl", '0', 0, 35, 0, 0, 0, 35},
            {"layer_opacity_reset_other_key", '9', 1, 35, 0, 0, 0, 35},
        };
        size_t i;

        for (i = 0; i < sizeof(layer_opacity_reset_cases) / sizeof(layer_opacity_reset_cases[0]); i++) {
            ok = ok && run_layer_opacity_reset_shortcut_case(&layer_opacity_reset_cases[i]);
        }
    }
    {
        const LayerVisibilityShortcutRuntimeCase layer_visibility_cases[] = {
            {"layer_visibility_show_all", 'a', 1, 0, 2, 0, -1, 1, 0, 1, 1, 1, 1, 1, -1},
            {"layer_visibility_show_all_from_solo", 'a', 1, 0, 2, 0, 0, 1, 1, 1, 1, 1, 1, 1, -1},
            {"layer_visibility_show_active", 'r', 1, 1, 2, 0, -1, 0, 1, 1, 1, 1, 1, 1, -1},
            {"layer_visibility_show_active_already_visible", 'r', 1, 1, 2, 0, -1, 1, 1, 1, 0, 0, 1, 1, -1},
            {"layer_visibility_missing_ctrl", 'a', 0, 0, 2, 0, -1, 1, 0, 0, 0, 0, 1, 0, -1},
            {"layer_visibility_other_key", 'x', 1, 0, 2, 0, -1, 1, 0, 0, 0, 0, 1, 0, -1},
        };
        size_t i;

        for (i = 0; i < sizeof(layer_visibility_cases) / sizeof(layer_visibility_cases[0]); i++) {
            ok = ok && run_layer_visibility_shortcut_runtime_case(&layer_visibility_cases[i]);
        }
    }
    {
        const ActiveLayerOpacityStepRuntimeCase active_layer_opacity_step_cases[] = {
            {"active_layer_opacity_step_down", 70, -10, 1, 1, 1, 60},
            {"active_layer_opacity_step_up", 70, 10, 1, 1, 1, 80},
            {"active_layer_opacity_step_down_clamps", 5, -10, 1, 1, 1, 0},
            {"active_layer_opacity_step_up_clamps", 95, 10, 1, 1, 1, 100},
            {"active_layer_opacity_step_at_floor_noop", 0, -10, 1, 0, 0, 0},
            {"active_layer_opacity_step_at_ceiling_noop", 100, 10, 1, 0, 0, 100},
        };
        size_t i;

        for (i = 0; i < sizeof(active_layer_opacity_step_cases) / sizeof(active_layer_opacity_step_cases[0]); i++) {
            ok = ok && run_active_layer_opacity_step_runtime_case(&active_layer_opacity_step_cases[i]);
        }

        if (app_handle_active_layer_opacity_step(NULL, 10, NULL, NULL, 0, NULL, NULL, NULL) != 0) {
            fprintf(stderr, "active_layer_opacity_step_null_layers should reject null layer stack\n");
            ok = 0;
        }
        if (app_handle_active_layer_opacity_step(&layer_stack, 0, NULL, NULL, 0, NULL, NULL, NULL) != 0) {
            fprintf(stderr, "active_layer_opacity_step_zero_delta should reject zero delta\n");
            ok = 0;
        }
    }
    {
        const CanvasMutationShortcutCase canvas_mutation_cases[] = {
            {
                "canvas_mutation_clear_editable",
                CANVAS_SHORTCUT_CLEAR,
                0,
                {0xFF111111u, 0xFF222222u, 0xFF333333u, 0xFF444444u},
                1,
                1,
                1,
                {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu},
            },
            {
                "canvas_mutation_clear_locked_noop",
                CANVAS_SHORTCUT_CLEAR,
                1,
                {0xFF111111u, 0xFF222222u, 0xFF333333u, 0xFF444444u},
                1,
                0,
                0,
                {0xFF111111u, 0xFF222222u, 0xFF333333u, 0xFF444444u},
            },
            {
                "canvas_mutation_flip_horizontal",
                CANVAS_SHORTCUT_FLIP_HORIZONTAL,
                0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                1,
                1,
                1,
                {0xFF000002u, 0xFF000001u, 0xFF000004u, 0xFF000003u},
            },
            {
                "canvas_mutation_unknown_noop",
                CANVAS_SHORTCUT_NONE,
                0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                0,
                0,
                0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
            },
        };
        size_t i;

        for (i = 0; i < sizeof(canvas_mutation_cases) / sizeof(canvas_mutation_cases[0]); i++) {
            ok = ok && run_canvas_mutation_shortcut_case(&canvas_mutation_cases[i]);
        }
    }
    {
        const BrushAndPaintShortcutRuntimeCase brush_and_paint_shortcut_cases[] = {
            {
                "brush_and_paint_tool_brush",
                PAINT_SHORTCUT_TOOL_BRUSH,
                BRUSH_SHORTCUT_NONE,
                TOOL_RECT,
                BRUSH_SHAPE_DIAMOND,
                8,
                0xAA556677u,
                0x00556677u,
                40,
                1,
                TOOL_BRUSH,
                BRUSH_SHAPE_DIAMOND,
                8,
                0x661B1F24u,
                0x001B1F24u,
                40,
            },
            {
                "brush_and_paint_radius_down_floor",
                PAINT_SHORTCUT_NONE,
                BRUSH_SHORTCUT_RADIUS_DOWN,
                TOOL_BRUSH,
                BRUSH_SHAPE_ROUND,
                1,
                0xFF112233u,
                0x00112233u,
                75,
                1,
                TOOL_BRUSH,
                BRUSH_SHAPE_ROUND,
                1,
                0xFF112233u,
                0x00112233u,
                75,
            },
            {
                "brush_and_paint_opacity_up_clamp",
                PAINT_SHORTCUT_NONE,
                BRUSH_SHORTCUT_OPACITY_UP,
                TOOL_BRUSH,
                BRUSH_SHAPE_SQUARE,
                4,
                0xF2112233u,
                0x00112233u,
                98,
                1,
                TOOL_BRUSH,
                BRUSH_SHAPE_SQUARE,
                4,
                0xFF112233u,
                0x00112233u,
                100,
            },
            {
                "brush_and_paint_color_purple",
                PAINT_SHORTCUT_COLOR_PURPLE,
                BRUSH_SHORTCUT_NONE,
                TOOL_ERASER,
                BRUSH_SHAPE_ROUND,
                5,
                0xAA112233u,
                0x00112233u,
                60,
                1,
                TOOL_BRUSH,
                BRUSH_SHAPE_ROUND,
                5,
                0x998E24AAu,
                0x008E24AAu,
                60,
            },
            {
                "brush_and_paint_none_noop",
                PAINT_SHORTCUT_NONE,
                BRUSH_SHORTCUT_NONE,
                TOOL_LINE,
                BRUSH_SHAPE_DIAMOND,
                7,
                0x80123456u,
                0x00123456u,
                50,
                0,
                TOOL_LINE,
                BRUSH_SHAPE_DIAMOND,
                7,
                0x80123456u,
                0x00123456u,
                50,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(brush_and_paint_shortcut_cases) / sizeof(brush_and_paint_shortcut_cases[0]); i++) {
            ok = ok && run_brush_and_paint_shortcut_runtime_case(&brush_and_paint_shortcut_cases[i]);
        }
    }
    {
        const CanvasSampleShortcutCase canvas_sample_cases[] = {
            {
                "canvas_sample_fill_editable",
                CANVAS_SHORTCUT_FILL,
                0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                &sampled_canvas,
                &sampled_preview_canvas,
                0,
                0, 0,
                TOOL_ERASER,
                0xFF556677u,
                0x00556677u,
                50,
                1, 1, 1,
                {0xFF556677u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                TOOL_ERASER,
                0xFF556677u,
                0x00556677u,
                50,
            },
            {
                "canvas_sample_fill_locked_noop",
                CANVAS_SHORTCUT_FILL,
                1,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                &sampled_canvas,
                &sampled_preview_canvas,
                0,
                0, 0,
                TOOL_ERASER,
                0xFF556677u,
                0x00556677u,
                50,
                1, 0, 0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                TOOL_ERASER,
                0xFF556677u,
                0x00556677u,
                50,
            },
            {
                "canvas_sample_eyedropper_preview",
                CANVAS_SHORTCUT_EYEDROPPER,
                0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                &sampled_canvas,
                &sampled_preview_canvas,
                1,
                0, 1,
                TOOL_ERASER,
                0xAA112233u,
                0x00112233u,
                42,
                1, 0, 0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                TOOL_BRUSH,
                0xFF223344u,
                0x00223344u,
                100,
            },
            {
                "canvas_sample_oob_handled",
                CANVAS_SHORTCUT_EYEDROPPER,
                0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                &sampled_canvas,
                &sampled_preview_canvas,
                1,
                9, 9,
                TOOL_ERASER,
                0xAA112233u,
                0x00112233u,
                42,
                1, 0, 0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                TOOL_ERASER,
                0xAA112233u,
                0x00112233u,
                42,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(canvas_sample_cases) / sizeof(canvas_sample_cases[0]); i++) {
            ok = ok && run_canvas_sample_shortcut_case(&canvas_sample_cases[i]);
        }
    }
    {
        const ViewShortcutRuntimeCase view_shortcut_runtime_cases[] = {
            {
                "view_shortcut_cycle_forward",
                {VIEW_SHORTCUT_CYCLE, 1, 0, 0},
                2, 0, 0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                1, 0, 1, 0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
            },
            {
                "view_shortcut_translate_editable",
                {VIEW_SHORTCUT_TRANSLATE, 0, 1, 0},
                1, 0, 0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                1, 1, 0, 1,
                {0xFFFFFFFFu, 0xFF000001u, 0xFFFFFFFFu, 0xFF000003u},
            },
            {
                "view_shortcut_translate_locked_noop",
                {VIEW_SHORTCUT_TRANSLATE, 0, 1, 0},
                1, 0, 1,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                1, 0, 0, 0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
            },
            {
                "view_shortcut_none_noop",
                {VIEW_SHORTCUT_NONE, 0, 0, 0},
                1, 0, 0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
                0, 0, 0, 0,
                {0xFF000001u, 0xFF000002u, 0xFF000003u, 0xFF000004u},
            },
        };
        size_t i;

        for (i = 0; i < sizeof(view_shortcut_runtime_cases) / sizeof(view_shortcut_runtime_cases[0]); i++) {
            ok = ok && run_view_shortcut_runtime_case(&view_shortcut_runtime_cases[i]);
        }
    }
    {
        const DirectDrawToolCase direct_draw_tool_cases[] = {
            {"tool_draws_directly_brush", TOOL_BRUSH, 1},
            {"tool_draws_directly_eraser", TOOL_ERASER, 1},
            {"tool_draws_directly_line", TOOL_LINE, 0},
            {"tool_draws_directly_rect", TOOL_RECT, 0},
            {"tool_draws_directly_filled_rect", TOOL_FILLED_RECT, 0},
            {"tool_draws_directly_ellipse", TOOL_ELLIPSE, 0},
            {"tool_draws_directly_filled_ellipse", TOOL_FILLED_ELLIPSE, 0},
            {"tool_draws_directly_default", (Tool)999, 0},
        };
        const StrokeMarkCase stroke_mark_cases[] = {
            {"stroke_mark_brush", TOOL_BRUSH, APP_STROKE_MARK_BRUSH},
            {"stroke_mark_eraser", TOOL_ERASER, APP_STROKE_MARK_ERASE},
            {"stroke_mark_line_defaults_to_brush", TOOL_LINE, APP_STROKE_MARK_BRUSH},
            {"stroke_mark_default", (Tool)999, APP_STROKE_MARK_BRUSH},
        };
        size_t i;

        for (i = 0; i < sizeof(direct_draw_tool_cases) / sizeof(direct_draw_tool_cases[0]); i++) {
            ok = ok && run_direct_draw_tool_case(&direct_draw_tool_cases[i]);
        }
        for (i = 0; i < sizeof(stroke_mark_cases) / sizeof(stroke_mark_cases[0]); i++) {
            ok = ok && run_stroke_mark_case(&stroke_mark_cases[i]);
        }
    }
    {
        const BrushStampCase brush_stamp_cases[] = {
            {
                "stamp_brush_center_blends",
                app_stamp_brush,
                0xFF000000u,
                0x80FFFFFFu,
                BRUSH_SHAPE_ROUND,
                0xFF808080u,
                0xFF000000u,
            },
            {
                "erase_brush_center_replaces",
                app_erase_brush,
                0xFF112233u,
                0x00000000u,
                BRUSH_SHAPE_ROUND,
                0x00000000u,
                0xFF112233u,
            },
        };
        const BrushLineCase brush_line_cases[] = {
            {
                "draw_brush_line_changes_row",
                app_draw_brush_line,
                0x00000000u,
                0xFF556677u,
                BRUSH_SHAPE_ROUND,
                12,
                0xFF556677u,
                0,
                0x00000000u,
            },
            {
                "erase_brush_line_changes_row",
                app_erase_brush_line,
                0xFFFFFFFFu,
                0x00000000u,
                BRUSH_SHAPE_ROUND,
                12,
                0x00000000u,
                0,
                0xFFFFFFFFu,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(brush_stamp_cases) / sizeof(brush_stamp_cases[0]); i++) {
            ok = ok && run_brush_stamp_case(&brush_stamp_cases[i]);
        }
        for (i = 0; i < sizeof(brush_line_cases) / sizeof(brush_line_cases[0]); i++) {
            ok = ok && run_brush_line_case(&brush_line_cases[i]);
        }
    }
    {
        BeginDirectStrokeCase begin_cases[] = {
            {
                "begin_direct_stroke_brush", 1, 1, TOOL_BRUSH, BRUSH_SHAPE_ROUND, 1, 0x80FFFFFFu,
                0xFF000000u, 0, 0, 0, 1, 1, 1, 4, 0xFF808080u, 4, 0xFF000000u,
            },
            {
                "begin_direct_stroke_eraser", 1, 1, TOOL_ERASER, BRUSH_SHAPE_ROUND, 1, 0xFFFFFFFFu,
                0xFF123456u, 0, 0, 0, 1, 1, 1, 4, 0xFFFFFFFFu, 4, 0xFF123456u,
            },
            {
                "begin_direct_stroke_locked_noop", 1, 1, TOOL_BRUSH, BRUSH_SHAPE_ROUND, 1, 0xFFFFFFFFu,
                0xFF010203u, 1, 0, 0, 0, 0, 0, 4, 0xFF010203u, 4, 0xFF010203u,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(begin_cases) / sizeof(begin_cases[0]); i++) {
            ok = ok && run_begin_direct_stroke_case(&begin_cases[i]);
        }
    }
    {
        const HandleLeftCanvasPressCase click_cases[] = {
            {
                "handle_left_canvas_press_direct_stroke",
                3, 3, 0xFF000000u, 0,
                4, 0xFF112233u,
                1, 1, TOOL_BRUSH, BRUSH_SHAPE_ROUND, 1, 0x80FFFFFFu,
                0, 0, 7, 8, -1, -1, 0, 0xDEADBEEFu,
                APP_CANVAS_CLICK_DIRECT_STROKE,
                1, 0, 7, 8, 1, 1, 1,
                4, 0xFF889199u, 4, 0xFF112233u,
                0,
            },
            {
                "handle_left_canvas_press_shape_preview",
                2, 2, 0xFF000000u, 0,
                99, 0u,
                1, 0, TOOL_RECT, BRUSH_SHAPE_ROUND, 1, 0xFFFFFFFFu,
                0, 0, 7, 8, -1, -1, 0, 0x00000000u,
                APP_CANVAS_CLICK_SHAPE_PREVIEW,
                0, 1, 1, 0, 1, 0, 0,
                0, 0xFF000000u, 0, 0u,
                1,
            },
            {
                "handle_left_canvas_press_locked_noop",
                2, 2, 0xFF010203u, 1,
                99, 0u,
                1, 0, TOOL_RECT, BRUSH_SHAPE_ROUND, 1, 0xFFFFFFFFu,
                0, 0, 7, 8, -1, -1, 0, 0x5A5A5A5Au,
                APP_CANVAS_CLICK_NOOP,
                0, 0, 7, 8, 1, 0, 0,
                0, 0xFF010203u, 0, 0u,
                0,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(click_cases) / sizeof(click_cases[0]); i++) {
            ok = ok && run_handle_left_canvas_press_case(&click_cases[i]);
        }
    }
    {
        const HandleRightCanvasPressCase right_click_cases[] = {
            {
                "handle_right_canvas_press_cancels_preview",
                1, 1,
                &sampled_canvas, &sampled_preview_canvas, 1, 0, 0,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                APP_CANVAS_CLICK_PREVIEW_CANCELED,
                0, 0,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
            },
            {
                "handle_right_canvas_press_samples_color",
                0, 1,
                &sampled_canvas, &sampled_preview_canvas, 1, 0, 1,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                APP_CANVAS_CLICK_COLOR_SAMPLED,
                0, 1,
                TOOL_BRUSH, 0xFF223344u, 0x00223344u, 100,
            },
            {
                "handle_right_canvas_press_oob_noop",
                0, 1,
                &sampled_canvas, &sampled_preview_canvas, 1, 5, 5,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                APP_CANVAS_CLICK_NOOP,
                0, 1,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(right_click_cases) / sizeof(right_click_cases[0]); i++) {
            ok = ok && run_handle_right_canvas_press_case(
                &right_click_cases[i],
                &shaping,
                &preview_active
            );
        }
        {
            const CanvasClickTitleRefreshCase title_refresh_cases[] = {
                {"canvas_click_title_refresh_noop", APP_CANVAS_CLICK_NOOP, 0},
                {"canvas_click_title_refresh_preview_canceled", APP_CANVAS_CLICK_PREVIEW_CANCELED, 0},
                {"canvas_click_title_refresh_color_sampled", APP_CANVAS_CLICK_COLOR_SAMPLED, 1},
            };

            for (i = 0; i < sizeof(title_refresh_cases) / sizeof(title_refresh_cases[0]); i++) {
                ok = ok && run_canvas_click_title_refresh_case(&title_refresh_cases[i]);
            }
        }
    }
    {
        const HandleLeftCanvasReleaseCase release_cases[] = {
            {
                "handle_left_canvas_release_drawing_only",
                3, 3, 0xFF010203u, 0,
                4, 0xFF112233u,
                1, 0, 0,
                0, 0, 2, 2, 0,
                TOOL_BRUSH, 1, 0xFFFFFFFFu,
                2,
                APP_CANVAS_CLICK_NOOP,
                0, 0, 0, 0, 0,
                0, 0u,
                4, 0xFF112233u,
            },
            {
                "handle_left_canvas_release_finalizes_shape",
                3, 3, 0xFF123456u, 0,
                0, 0x01020304u,
                1, 1, 1,
                0, 0, 2, 0, 0,
                TOOL_LINE, 1, 0xFFAABBCCu,
                2,
                APP_CANVAS_CLICK_SHAPE_FINALIZED,
                0, 0, 0, 1, 1,
                0, 0x01020304u,
                2, 0xFFAABBCCu,
            },
            {
                "handle_left_canvas_release_locked_shape_noop",
                3, 3, 0xFF123456u, 1,
                4, 0x55667788u,
                1, 1, 1,
                0, 0, 2, 1, 0,
                TOOL_RECT, 1, 0xFFABCDEFu,
                2,
                APP_CANVAS_CLICK_NOOP,
                0, 0, 0, 0, 0,
                0, 0u,
                4, 0x55667788u,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(release_cases) / sizeof(release_cases[0]); i++) {
            ok = ok && run_handle_left_canvas_release_case(&release_cases[i]);
        }
    }
    {
        static const uint32_t unchanged_preview_pixels[4] = {
            0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu,
        };
        HandleCanvasMotionCase motion_cases[] = {
            {
                "handle_canvas_motion_direct_stroke",
                1, 1, 2, 0, 0, 0,
                3, 2, 0, 0, 0,
                TOOL_BRUSH, BRUSH_SHAPE_ROUND, 1, 0xFF556677u,
                APP_CANVAS_CLICK_DIRECT_STROKE,
                3, 2, 0, 1,
                99, 0u,
                unchanged_preview_pixels, 4,
            },
            {
                "handle_canvas_motion_shape_preview",
                0, 8, 9, 1, 0, 0,
                1, 0, 0, 0, 0,
                TOOL_LINE, BRUSH_SHAPE_ROUND, 0, 0xFFABCDEFu,
                APP_CANVAS_CLICK_SHAPE_PREVIEW,
                8, 9, 1, 0,
                0, 0x00000000u,
                (const uint32_t[]){0x01010101u, 0x02020202u, 0x03030303u, 0x04040404u}, 4,
            },
            {
                "handle_canvas_motion_noop_without_mode",
                0, 6, 7, 0, 0, 0,
                1, 1, 0, 0, 0,
                TOOL_RECT, BRUSH_SHAPE_ROUND, 1, 0xFFABCDEFu,
                APP_CANVAS_CLICK_NOOP,
                6, 7, 0, 0,
                0, 0x00000000u,
                unchanged_preview_pixels, 4,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(motion_cases) / sizeof(motion_cases[0]); i++) {
            ok = ok && run_handle_canvas_motion_case(&motion_cases[i]);
        }
    }
    {
        ContinueDirectStrokeCase continue_cases[] = {
            {
                "continue_direct_stroke_brush", 1, 2, 3, 2, TOOL_BRUSH, BRUSH_SHAPE_ROUND, 1, 0xFF556677u,
                0x00000000u, 0, 0, 1, 3, 2, 1, 12, 0xFF556677u,
            },
            {
                "continue_direct_stroke_eraser", 1, 2, 3, 2, TOOL_ERASER, BRUSH_SHAPE_ROUND, 1, 0xFF556677u,
                0xFF112233u, 0, 0, 1, 3, 2, 1, 12, 0xFFFFFFFFu,
            },
            {
                "continue_direct_stroke_locked_noop", 1, 2, 3, 2, TOOL_BRUSH, BRUSH_SHAPE_ROUND, 1, 0xFF556677u,
                0xFF010203u, 1, 0, 0, 1, 2, 0, 12, 0xFF010203u,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(continue_cases) / sizeof(continue_cases[0]); i++) {
            ok = ok && run_continue_direct_stroke_case(&continue_cases[i]);
        }
    }
    {
        const CanvasActionCase canvas_action_cases[] = {
            {"canvas_clear", 'c', CANVAS_SHORTCUT_CLEAR},
            {"canvas_flip_h", 'h', CANVAS_SHORTCUT_FLIP_HORIZONTAL},
            {"canvas_flip_v", 'v', CANVAS_SHORTCUT_FLIP_VERTICAL},
            {"canvas_rotate_180", 'j', CANVAS_SHORTCUT_ROTATE_180},
            {"canvas_invert_rgb", 'x', CANVAS_SHORTCUT_INVERT_RGB},
            {"canvas_fill", 'f', CANVAS_SHORTCUT_FILL},
            {"canvas_eyedropper", 'i', CANVAS_SHORTCUT_EYEDROPPER},
            {"canvas_other_key", 'k', CANVAS_SHORTCUT_NONE},
        };
        const ViewResultCase view_result_cases[] = {
            {"pageup", VIEW_SHORTCUT_KEY_PAGEUP, 0, VIEW_SHORTCUT_CYCLE, 1, 0, 0},
            {"pagedown", VIEW_SHORTCUT_KEY_PAGEDOWN, 1, VIEW_SHORTCUT_CYCLE, -1, 0, 0},
            {"up", VIEW_SHORTCUT_KEY_UP, 0, VIEW_SHORTCUT_TRANSLATE, 0, 0, -1},
            {"down_shift", VIEW_SHORTCUT_KEY_DOWN, 1, VIEW_SHORTCUT_TRANSLATE, 0, 0, 10},
            {"left", VIEW_SHORTCUT_KEY_LEFT, 0, VIEW_SHORTCUT_TRANSLATE, 0, -1, 0},
            {"right_shift", VIEW_SHORTCUT_KEY_RIGHT, 1, VIEW_SHORTCUT_TRANSLATE, 0, 10, 0},
            {"view_none", VIEW_SHORTCUT_KEY_NONE, 0, VIEW_SHORTCUT_NONE, 0, 0, 0},
        };
        size_t i;

        for (i = 0; i < sizeof(canvas_action_cases) / sizeof(canvas_action_cases[0]); i++) {
            ok = ok && run_canvas_action_case(&canvas_action_cases[i]);
        }
        for (i = 0; i < sizeof(view_result_cases) / sizeof(view_result_cases[0]); i++) {
            ok = ok && run_view_result_case(&view_result_cases[i]);
        }
    }
    {
        const ShapeCancelCase shape_cancel_cases[] = {
            {"shape_cancel_ctrl_save", APP_SHAPE_CANCEL_KEY_S, 1, 1},
            {"shape_cancel_ctrl_bracket", APP_SHAPE_CANCEL_KEY_LEFTBRACKET, 1, 1},
            {"shape_cancel_ctrl_slash", APP_SHAPE_CANCEL_KEY_SLASH, 1, 1},
            {"shape_cancel_ctrl_digit_1", APP_SHAPE_CANCEL_KEY_1, 1, 1},
            {"shape_cancel_ctrl_unmapped_not_cancel", APP_SHAPE_CANCEL_KEY_OTHER, 1, 0},
            {"shape_cancel_ctrl_digit_7", APP_SHAPE_CANCEL_KEY_7, 1, 1},
            {"shape_cancel_ctrl_digit_8", APP_SHAPE_CANCEL_KEY_8, 1, 1},
            {"shape_cancel_tool_switch", APP_SHAPE_CANCEL_KEY_B, 0, 1},
            {"shape_cancel_plain_bracket", APP_SHAPE_CANCEL_KEY_RIGHTBRACKET, 0, 1},
            {"shape_cancel_plain_digit_6", APP_SHAPE_CANCEL_KEY_6, 0, 1},
            {"shape_cancel_plain_slash_not_cancel", APP_SHAPE_CANCEL_KEY_SLASH, 0, 0},
            {"shape_cancel_plain_kp_plus", APP_SHAPE_CANCEL_KEY_KP_PLUS, 0, 1},
            {"shape_cancel_plain_kp_minus", APP_SHAPE_CANCEL_KEY_KP_MINUS, 0, 1},
            {"shape_cancel_arrow", APP_SHAPE_CANCEL_KEY_LEFT, 0, 1},
            {"shape_cancel_function_key", APP_SHAPE_CANCEL_KEY_F2, 0, 1},
            {"shape_cancel_delete", APP_SHAPE_CANCEL_KEY_DELETE, 0, 1},
            {"shape_cancel_backspace", APP_SHAPE_CANCEL_KEY_BACKSPACE, 0, 1},
            {"shape_cancel_escape_exempt", APP_SHAPE_CANCEL_KEY_ESCAPE, 0, 0},
            {"shape_cancel_shift_exempt", APP_SHAPE_CANCEL_KEY_LSHIFT, 0, 0},
            {"shape_cancel_plain_save_not_cancel", APP_SHAPE_CANCEL_KEY_S, 0, 0},
            {"shape_cancel_unmapped_key", APP_SHAPE_CANCEL_KEY_OTHER, 0, 0},
        };
        size_t i;

        for (i = 0; i < sizeof(shape_cancel_cases) / sizeof(shape_cancel_cases[0]); i++) {
            ok = ok && run_shape_cancel_case(&shape_cancel_cases[i]);
        }
    }
    {
        HandleShapePreviewKeyCase preview_key_cases[] = {
            {
                "shape_preview_key_cancel_continue",
                APP_SHAPE_CANCEL_KEY_B,
                0,
                1, 1, 1,
                APP_PREVIEW_KEY_RESULT_STATE_CHANGED,
                0, 0, 1,
            },
            {
                "shape_preview_key_escape_cancel_handled",
                APP_SHAPE_CANCEL_KEY_ESCAPE,
                0,
                1, 1, 1,
                APP_PREVIEW_KEY_RESULT_HANDLED,
                0, 0, 1,
            },
            {
                "shape_preview_key_escape_exit",
                APP_SHAPE_CANCEL_KEY_ESCAPE,
                0,
                0, 1, 1,
                APP_PREVIEW_KEY_RESULT_HANDLED,
                0, 1, 0,
            },
            {
                "shape_preview_key_unmapped_noop",
                APP_SHAPE_CANCEL_KEY_OTHER,
                0,
                0, 1, 1,
                APP_PREVIEW_KEY_RESULT_NONE,
                0, 1, 1,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(preview_key_cases) / sizeof(preview_key_cases[0]); i++) {
            ok = ok && run_handle_shape_preview_key_case(&preview_key_cases[i]);
        }
    }
    {
        const BrushMaskCase brush_mask_cases[] = {
            {"brush_mask_round_inside", BRUSH_SHAPE_ROUND, 1, 1, 2, 1},
            {"brush_mask_round_edge", BRUSH_SHAPE_ROUND, 2, 0, 2, 1},
            {"brush_mask_round_outside", BRUSH_SHAPE_ROUND, 2, 1, 2, 0},
            {"brush_mask_round_negative_symmetry", BRUSH_SHAPE_ROUND, -1, -1, 2, 1},
            {"brush_mask_round_zero_radius", BRUSH_SHAPE_ROUND, 0, 0, 0, 1},
            {"brush_mask_square_corner", BRUSH_SHAPE_SQUARE, 2, 2, 2, 1},
            {"brush_mask_square_zero_radius", BRUSH_SHAPE_SQUARE, 0, 0, 0, 1},
            {"brush_mask_square_negative_symmetry", BRUSH_SHAPE_SQUARE, -2, -2, 2, 1},
            {"brush_mask_diamond_edge", BRUSH_SHAPE_DIAMOND, 2, 0, 2, 1},
            {"brush_mask_diamond_outside", BRUSH_SHAPE_DIAMOND, 2, 1, 2, 0},
            {"brush_mask_diamond_zero_radius", BRUSH_SHAPE_DIAMOND, 0, 0, 0, 1},
            {"brush_mask_diamond_negative_symmetry", BRUSH_SHAPE_DIAMOND, -1, -1, 2, 1},
            {"brush_mask_default", (BrushShape)999, 0, 0, 2, 0},
        };
        size_t i;

        for (i = 0; i < sizeof(brush_mask_cases) / sizeof(brush_mask_cases[0]); i++) {
            ok = ok && run_brush_mask_case(&brush_mask_cases[i]);
        }
    }
    {
        const SampledBrushColorCase sampled_cases[] = {
            {
                "sampled_brush_color_opaque",
                0xFF445566u,
                TOOL_ERASER, 0u, 0u, 0,
                TOOL_BRUSH, 0xFF445566u, 0x00445566u, 100,
            },
            {
                "sampled_brush_color_black",
                0xFF000000u,
                TOOL_ERASER, 0u, 0u, 0,
                TOOL_BRUSH, 0xFF000000u, 0x00000000u, 100,
            },
            {
                "sampled_brush_color_white",
                0xFFFFFFFFu,
                TOOL_ERASER, 0u, 0u, 0,
                TOOL_BRUSH, 0xFFFFFFFFu, 0x00FFFFFFu, 100,
            },
            {
                "sampled_brush_color_translucent",
                0x80445566u,
                TOOL_LINE, 0u, 0u, 0,
                TOOL_BRUSH, 0x80445566u, 0x00445566u, 50,
            },
            {
                "sampled_brush_color_low_alpha",
                0x05445566u,
                TOOL_LINE, 0u, 0u, 0,
                TOOL_BRUSH, 0x05445566u, 0x00445566u, 2,
            },
            {
                "sampled_brush_color_next_low_step",
                0x07445566u,
                TOOL_LINE, 0u, 0u, 0,
                TOOL_BRUSH, 0x08445566u, 0x00445566u, 3,
            },
            {
                "sampled_brush_color_rounds_down",
                0x7F445566u,
                TOOL_LINE, 0u, 0u, 0,
                TOOL_BRUSH, 0x80445566u, 0x00445566u, 50,
            },
            {
                "sampled_brush_color_rounds_up",
                0x81445566u,
                TOOL_LINE, 0u, 0u, 0,
                TOOL_BRUSH, 0x82445566u, 0x00445566u, 51,
            },
            {
                "sampled_brush_color_transparent_clamp",
                0x00445566u,
                TOOL_RECT, 0u, 0u, 0,
                TOOL_BRUSH, 0x03445566u, 0x00445566u, 1,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(sampled_cases) / sizeof(sampled_cases[0]); i++) {
            ok = ok && run_sampled_brush_color_case(&sampled_cases[i]);
        }
    }
    {
        const SampledBrushColorFromCanvasCase sampled_from_canvas_cases[] = {
            {
                "sampled_brush_color_from_canvas",
                &sampled_canvas,
                1, 0,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                TOOL_BRUSH, 0x80445566u, 0x00445566u, 50,
            },
            {
                "sampled_brush_color_from_canvas_null_canvas_noop",
                NULL,
                0, 0,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
            },
            {
                "sampled_brush_color_from_canvas_missing_pixels_noop",
                &sampled_canvas_without_pixels,
                0, 0,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
            },
            {
                "sampled_brush_color_from_canvas_oob_noop",
                &sampled_canvas,
                2, 0,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
            },
        };
        const SampledBrushColorFromAvailableCanvasCase sampled_from_available_cases[] = {
            {
                "sampled_brush_color_from_available_preview_priority",
                &sampled_canvas,
                &sampled_preview_canvas,
                1,
                0, 1,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                TOOL_BRUSH, 0xFF223344u, 0x00223344u, 100,
            },
            {
                "sampled_brush_color_from_available_preview_fallback",
                &sampled_canvas,
                &sampled_preview_without_pixels,
                1,
                1, 1,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                TOOL_BRUSH, 0xFFABCDEFu, 0x00ABCDEFu, 100,
            },
            {
                "sampled_brush_color_from_available_null_composite_noop",
                NULL,
                NULL,
                0,
                0, 0,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(sampled_from_canvas_cases) / sizeof(sampled_from_canvas_cases[0]); i++) {
            ok = ok && run_sampled_brush_color_from_canvas_case(&sampled_from_canvas_cases[i]);
        }
        for (i = 0; i < sizeof(sampled_from_available_cases) / sizeof(sampled_from_available_cases[0]); i++) {
            ok = ok && run_sampled_brush_color_from_available_canvas_case(&sampled_from_available_cases[i]);
        }
    }
    {
        const HandleAvailableCanvasSampleCase handle_sample_cases[] = {
            {
                "handle_available_canvas_sample_cancels_preview",
                1, 1,
                &sampled_canvas, &sampled_preview_canvas, 1, 0, 0,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                APP_SAMPLE_BRUSH_COLOR_PREVIEW_CANCELED,
                0, 0,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
            },
            {
                "handle_available_canvas_sample_applies_preview_color",
                0, 1,
                &sampled_canvas, &sampled_preview_canvas, 1, 0, 1,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                APP_SAMPLE_BRUSH_COLOR_APPLIED,
                0, 1,
                TOOL_BRUSH, 0xFF223344u, 0x00223344u, 100,
            },
            {
                "handle_available_canvas_sample_oob_noop",
                0, 1,
                &sampled_canvas, &sampled_preview_canvas, 1, 5, 5,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
                APP_SAMPLE_BRUSH_COLOR_NOOP,
                0, 1,
                TOOL_ERASER, 0xAA112233u, 0x00112233u, 42,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(handle_sample_cases) / sizeof(handle_sample_cases[0]); i++) {
            ok = ok && run_handle_available_canvas_sample_case(
                &handle_sample_cases[i],
                &shaping,
                &preview_active
            );
        }
    }
    {
        const SampledBrushColorNoopCase sampled_noop_cases[] = {
            {
                "sampled_brush_color_null_tool",
                0xFF778899u,
                0, 1, 1, 1,
                TOOL_BRUSH, 0xAA112233u, 0x00112233u, 42,
                TOOL_BRUSH, 0xAA112233u, 0x00112233u, 42,
            },
            {
                "sampled_brush_color_null_color",
                0xFF778899u,
                1, 0, 1, 1,
                TOOL_ERASER, 0u, 0x00112233u, 42,
                TOOL_ERASER, 0u, 0x00112233u, 42,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(sampled_noop_cases) / sizeof(sampled_noop_cases[0]); i++) {
            ok = ok && run_sampled_brush_color_noop_case(&sampled_noop_cases[i]);
        }
    }
    {
        const BrushColorCase brush_color_cases[] = {
            {"brush_color_low_clamp", 0x00123456u, 0, 0x03123456u},
            {"brush_color_mid_round", 0x00ABCDEFu, 50, 0x80ABCDEFu},
            {"brush_color_high_clamp", 0x00FEDCBAu, 150, 0xFFFEDCBAu},
        };
        const LayerClearColorCase layer_clear_color_cases[] = {
            {"layer_clear_color_background", 0, 0xFFFFFFFFu},
            {"layer_clear_color_foreground", 3, 0x00000000u},
        };
        size_t i;

        for (i = 0; i < sizeof(brush_color_cases) / sizeof(brush_color_cases[0]); i++) {
            ok = ok && run_brush_color_case(&brush_color_cases[i]);
        }
        for (i = 0; i < sizeof(layer_clear_color_cases) / sizeof(layer_clear_color_cases[0]); i++) {
            ok = ok && run_layer_clear_color_case(&layer_clear_color_cases[i]);
        }
    }
    {
        const LayerEditableCase layer_editable_cases[] = {
            {"layer_editable_true", &editable_layer, 1},
            {"layer_editable_locked", &locked_layer, 0},
            {"layer_editable_missing_pixels", &empty_layer, 0},
            {"layer_editable_null", NULL, 0},
        };
        size_t i;

        for (i = 0; i < sizeof(layer_editable_cases) / sizeof(layer_editable_cases[0]); i++) {
            ok = ok && run_layer_editable_case(&layer_editable_cases[i]);
        }
    }
    {
        const ActiveLayerEditableCase active_layer_editable_cases[] = {
            {"active_layer_editable_true", &layer_stack, 0, 1},
            {"active_layer_editable_locked", &layer_stack, 1, 0},
            {"active_layer_editable_missing_pixels", &layer_stack, 2, 0},
            {"active_layer_editable_null_stack", NULL, 0, 0},
        };
        const ActiveEditableLayerCase active_editable_layer_cases[] = {
            {"active_editable_layer_true", &layer_stack, 0, &layer_stack.layers[0]},
            {"active_editable_layer_locked", &layer_stack, 1, NULL},
            {"active_editable_layer_missing_pixels", &layer_stack, 2, NULL},
            {"active_editable_layer_negative_index", &layer_stack, -1, NULL},
            {"active_editable_layer_oob_index", &layer_stack, 3, NULL},
            {"active_editable_layer_null_stack", NULL, 0, NULL},
        };
        size_t i;

        for (i = 0; i < sizeof(active_layer_editable_cases) / sizeof(active_layer_editable_cases[0]); i++) {
            ok = ok && run_active_layer_editable_case(&active_layer_editable_cases[i]);
        }
        for (i = 0; i < sizeof(active_editable_layer_cases) / sizeof(active_editable_layer_cases[0]); i++) {
            ok = ok && run_active_editable_layer_case(&active_editable_layer_cases[i]);
        }
        layer_stack.active_layer = 0;
    }
    {
        const ToolLabelCase tool_label_cases[] = {
            {"tool_brush_label", TOOL_BRUSH, "Brush"},
            {"tool_filled_ellipse_label", TOOL_FILLED_ELLIPSE, "Filled Ellipse"},
            {"tool_label_default", (Tool)999, "Brush"},
        };
        const BrushShapeLabelCase brush_shape_label_cases[] = {
            {"brush_round_label", BRUSH_SHAPE_ROUND, "Round"},
            {"brush_diamond_label", BRUSH_SHAPE_DIAMOND, "Diamond"},
            {"brush_shape_label_default", (BrushShape)999, "Round"},
        };
        size_t i;

        for (i = 0; i < sizeof(tool_label_cases) / sizeof(tool_label_cases[0]); i++) {
            ok = ok && run_tool_label_case(&tool_label_cases[i]);
        }
        for (i = 0; i < sizeof(brush_shape_label_cases) / sizeof(brush_shape_label_cases[0]); i++) {
            ok = ok && run_brush_shape_label_case(&brush_shape_label_cases[i]);
        }
    }
    {
        const CycleBrushShapeCase cycle_brush_shape_cases[] = {
            {"brush_shape_cycle_forward", BRUSH_SHAPE_ROUND, 1, BRUSH_SHAPE_SQUARE},
            {"brush_shape_cycle_wrap_forward", BRUSH_SHAPE_DIAMOND, 1, BRUSH_SHAPE_ROUND},
            {"brush_shape_cycle_wrap_backward", BRUSH_SHAPE_ROUND, -1, BRUSH_SHAPE_DIAMOND},
        };
        const ConstrainedShapeEndCase constrained_shape_end_cases[] = {
            {"shape_line_horizontal_snap", TOOL_LINE, 10, 10, 25, 13, 1, 25, 10},
            {"shape_line_vertical_snap", TOOL_LINE, 10, 10, 13, 25, 1, 10, 25},
            {"shape_line_diagonal_snap", TOOL_LINE, 10, 10, 18, 15, 1, 18, 18},
            {"shape_rect_square_snap", TOOL_RECT, 10, 10, 14, 18, 1, 18, 18},
            {"shape_filled_rect_square_snap", TOOL_FILLED_RECT, 10, 10, 14, 18, 1, 18, 18},
            {"shape_filled_ellipse_square_snap", TOOL_FILLED_ELLIPSE, 10, 10, 4, 18, 1, 2, 18},
            {"shape_no_shift_passthrough", TOOL_ELLIPSE, 10, 10, 14, 18, 0, 14, 18},
            {"shape_unknown_tool_passthrough", (Tool)999, 10, 10, 25, 13, 1, 25, 13},
        };
        size_t i;

        for (i = 0; i < sizeof(cycle_brush_shape_cases) / sizeof(cycle_brush_shape_cases[0]); i++) {
            ok = ok && run_cycle_brush_shape_case(&cycle_brush_shape_cases[i]);
        }
        for (i = 0; i < sizeof(constrained_shape_end_cases) / sizeof(constrained_shape_end_cases[0]); i++) {
            ok = ok && run_constrained_shape_end_case(&constrained_shape_end_cases[i]);
        }
    }
    {
        const ConstrainedShapeEndNoOutputCase constrained_shape_end_no_output_cases[] = {
            {"shape_null_out_x", TOOL_LINE, 10, 10, 25, 13, 1, 0, 1, 321, 654, 0, 654},
            {"shape_null_out_y", TOOL_LINE, 10, 10, 25, 13, 1, 1, 0, 321, 654, 321, 0},
        };
        size_t i;

        for (i = 0; i < sizeof(constrained_shape_end_no_output_cases) / sizeof(constrained_shape_end_no_output_cases[0]); i++) {
            ok = ok && run_constrained_shape_end_no_output_case(&constrained_shape_end_no_output_cases[i]);
        }
    }
    {
        const DrawShapeCase draw_shape_cases[] = {
            {
                "draw_shape_line_dispatch",
                TOOL_LINE,
                1, 2, 3, 2,
                1,
                0x00000000u,
                0xFF556677u,
                12,
                0xFF556677u,
                0,
                0x00000000u,
            },
            {
                "draw_shape_filled_rect_dispatch",
                TOOL_FILLED_RECT,
                1, 1, 3, 3,
                1,
                0x00000000u,
                0xFF112233u,
                12,
                0xFF112233u,
                0,
                0x00000000u,
            },
            {
                "draw_shape_unknown_tool_noop",
                (Tool)999,
                1, 1, 3, 3,
                1,
                0x00000000u,
                0xFF112233u,
                12,
                0x00000000u,
                0,
                0x00000000u,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(draw_shape_cases) / sizeof(draw_shape_cases[0]); i++) {
            ok = ok && run_draw_shape_case(&draw_shape_cases[i]);
        }
    }
    {
        const BeginShapePreviewCase begin_shape_preview_cases[] = {
            {"begin_shape_preview_copy", 12, 34, 1, 1, 1, 0, -1, -1, preview_copy, preview_source, 4, 1, 12, 34, preview_source},
            {"begin_shape_preview_no_copy_without_source", 20, 30, 1, 1, 1, 5, 7, 9, preview_copy, NULL, 4, 1, 20, 30, preview_sentinel},
            {"begin_shape_preview_zero_length_no_copy", 22, 33, 1, 1, 1, 4, 2, 3, preview_copy, preview_source, 0, 1, 22, 33, preview_sentinel},
            {"begin_shape_preview_null_source_zero_length", 120, 130, 1, 1, 1, 14, 17, 18, preview_copy, NULL, 0, 1, 120, 130, preview_sentinel},
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
        const BeginShapePreviewPartialCopyCase begin_shape_preview_partial_copy_cases[] = {
            {
                "begin_shape_preview_partial_copy",
                14,
                24,
                2,
                -2,
                -3,
                {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu},
                preview_source,
                2,
                1,
                14,
                24,
                {0x11111111u, 0x22222222u, 0xCCCCCCCCu, 0xDDDDDDDDu},
            },
        };
        size_t i;

        for (i = 0; i < sizeof(begin_shape_preview_partial_copy_cases) / sizeof(begin_shape_preview_partial_copy_cases[0]); i++) {
            ok = ok && run_begin_shape_preview_partial_copy_case(
                &begin_shape_preview_partial_copy_cases[i],
                &shaping,
                &shape_start_x,
                &shape_start_y
            );
        }
    }
    {
        const BeginShapePreviewCase begin_shape_preview_boundary_cases[] = {
            {"begin_shape_preview_null_shaping_noop", 40, 50, 0, 1, 1, 6, 8, 10, preview_copy, preview_source, 4, 6, 8, 10, preview_sentinel},
            {"begin_shape_preview_null_start_x_noop", 60, 70, 1, 0, 1, 11, 0, 13, preview_copy, preview_source, 4, 11, 0, 13, preview_sentinel},
            {"begin_shape_preview_null_start_y_noop", 80, 90, 1, 1, 0, 12, 14, 0, preview_copy, preview_source, 4, 12, 14, 0, preview_sentinel},
            {"begin_shape_preview_null_destination_no_copy", 100, 110, 1, 1, 1, 13, 15, 16, NULL, preview_source, 4, 1, 100, 110, NULL},
            {"begin_shape_preview_null_source_and_destination", 140, 150, 1, 1, 1, 15, 19, 20, NULL, NULL, 4, 1, 140, 150, NULL},
            {"begin_shape_preview_null_everything_copy_path", 160, 170, 1, 1, 1, 16, 21, 22, NULL, NULL, 0, 1, 160, 170, NULL},
        };
        size_t i;

        for (i = 0; i < sizeof(begin_shape_preview_boundary_cases) / sizeof(begin_shape_preview_boundary_cases[0]); i++) {
            if (begin_shape_preview_boundary_cases[i].shape_base_pixels == preview_copy) {
                memcpy(preview_copy, preview_sentinel, sizeof(preview_copy));
            }
            ok = ok && run_begin_shape_preview_case(
                &begin_shape_preview_boundary_cases[i],
                &shaping,
                &shape_start_x,
                &shape_start_y
            );
        }
    }
    {
        const BeginShapePreviewFromCanvasCase begin_shape_preview_from_canvas_cases[] = {
            {"begin_shape_preview_from_canvas_copy", 180, 190, 1, 1, 1, 17, 23, 24, preview_copy, &begin_preview_canvas, 1, 180, 190, preview_source, 4},
            {"begin_shape_preview_from_canvas_null_canvas", 200, 210, 1, 1, 1, 18, 25, 26, preview_copy, NULL, 1, 200, 210, preview_sentinel, 4},
            {"begin_shape_preview_from_canvas_missing_pixels", 220, 230, 1, 1, 1, 19, 27, 28, preview_copy, &begin_preview_canvas_without_pixels, 1, 220, 230, preview_sentinel, 4},
            {"begin_shape_preview_from_canvas_null_shaping_noop", 300, 310, 0, 1, 1, 23, 35, 36, preview_copy, &begin_preview_canvas, 23, 35, 36, preview_sentinel, 4},
            {"begin_shape_preview_from_canvas_null_start_x_noop", 320, 330, 1, 0, 1, 24, 0, 37, preview_copy, &begin_preview_canvas, 24, 0, 37, preview_sentinel, 4},
            {"begin_shape_preview_from_canvas_null_start_y_noop", 340, 350, 1, 1, 0, 25, 38, 0, preview_copy, &begin_preview_canvas, 25, 38, 0, preview_sentinel, 4},
        };
        size_t i;

        for (i = 0; i < sizeof(begin_shape_preview_from_canvas_cases) / sizeof(begin_shape_preview_from_canvas_cases[0]); i++) {
            memcpy(preview_copy, preview_sentinel, sizeof(preview_copy));
            ok = ok && run_begin_shape_preview_from_canvas_case(
                &begin_shape_preview_from_canvas_cases[i],
                &shaping,
                &shape_start_x,
                &shape_start_y
            );
        }
    }
    {
        const BeginShapePreviewToActiveLayerCase begin_shape_preview_to_active_layer_cases[] = {
            {
                "begin_shape_preview_to_active_layer_editable",
                1, 0,
                240, 250,
                1, 1, 1,
                20, 29, 30,
                1, 1, 240, 250,
                preview_source, 4,
            },
            {
                "begin_shape_preview_to_active_layer_locked_noop",
                1, 1,
                260, 270,
                1, 1, 1,
                21, 31, 32,
                0, 21, 31, 32,
                preview_sentinel, 4,
            },
            {
                "begin_shape_preview_to_active_layer_null_stack_noop",
                0, 0,
                280, 290,
                1, 1, 1,
                22, 33, 34,
                0, 22, 33, 34,
                preview_sentinel, 4,
            },
            {
                "begin_shape_preview_to_active_layer_null_shaping_started_noop",
                1, 0,
                360, 370,
                0, 1, 1,
                26, 39, 40,
                1, 26, 39, 40,
                preview_sentinel, 4,
            },
            {
                "begin_shape_preview_to_active_layer_null_start_x_started_noop",
                1, 0,
                380, 390,
                1, 0, 1,
                27, 0, 41,
                1, 27, 0, 41,
                preview_sentinel, 4,
            },
            {
                "begin_shape_preview_to_active_layer_null_start_y_started_noop",
                1, 0,
                400, 410,
                1, 1, 0,
                28, 42, 0,
                1, 28, 42, 0,
                preview_sentinel, 4,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(begin_shape_preview_to_active_layer_cases) / sizeof(begin_shape_preview_to_active_layer_cases[0]); i++) {
            memcpy(preview_copy, preview_sentinel, sizeof(preview_copy));
            ok = ok && run_begin_shape_preview_to_active_layer_case(
                &begin_shape_preview_to_active_layer_cases[i],
                preview_copy,
                &begin_preview_canvas
            );
        }
    }
    {
        const CancelShapePreviewCase cancel_shape_preview_cases[] = {
            {"cancel_shape_preview_both", 1, 1, 1, 1, 0, 0},
            {"cancel_shape_preview_null_shape", 0, 1, 7, 9, 0, 0},
            {"cancel_shape_preview_null_preview", 1, 0, 7, 9, 0, 0},
        };
        const PreviewCanvasSelectionCase preview_canvas_selection_cases[] = {
            {"preview_canvas_active", &composite_canvas, &preview_canvas, 1, &preview_canvas},
            {"preview_canvas_inactive", &composite_canvas, &preview_canvas, 0, &composite_canvas},
            {
                "preview_canvas_missing_pixels_falls_back",
                &composite_canvas,
                &preview_canvas_without_pixels,
                1,
                &composite_canvas,
            },
            {"preview_canvas_null_preview_falls_back", &composite_canvas, NULL, 1, &composite_canvas},
            {"preview_canvas_null_composite_allowed", NULL, &preview_canvas, 0, NULL},
        };
        size_t i;

        for (i = 0; i < sizeof(cancel_shape_preview_cases) / sizeof(cancel_shape_preview_cases[0]); i++) {
            ok = ok && run_cancel_shape_preview_case(&cancel_shape_preview_cases[i]);
        }
        for (i = 0; i < sizeof(preview_canvas_selection_cases) / sizeof(preview_canvas_selection_cases[0]); i++) {
            ok = ok && run_preview_canvas_selection_case(&preview_canvas_selection_cases[i]);
        }
    }
    {
        const RestoreShapePreviewCase restore_shape_preview_cases[] = {
            {
                "restore_shape_preview_copy",
                preview_restore_copy,
                preview_restore_source,
                4,
                0, 1, 1,
                preview_restore_source,
                4,
            },
            {
                "restore_shape_preview_partial_copy",
                preview_restore_copy,
                preview_restore_source,
                2,
                0, 1, 1,
                (const uint32_t[]){0x01010101u, 0x02020202u, 0xCCCCCCCCu, 0xDDDDDDDDu},
                4,
            },
            {
                "restore_shape_preview_zero_length",
                preview_restore_copy,
                preview_restore_source,
                0,
                0, 1, 1,
                preview_restore_sentinel,
                4,
            },
            {
                "restore_shape_preview_null_destination",
                NULL,
                preview_restore_source,
                4,
                0, 1, 1,
                NULL,
                0,
            },
            {
                "restore_shape_preview_null_source",
                preview_restore_copy,
                NULL,
                4,
                0, 1, 1,
                preview_restore_sentinel,
                4,
            },
            {
                "restore_shape_preview_null_flag",
                preview_restore_copy,
                preview_restore_source,
                4,
                9, 0, 0,
                preview_restore_sentinel,
                4,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(restore_shape_preview_cases) / sizeof(restore_shape_preview_cases[0]); i++) {
            memcpy(preview_restore_copy, preview_restore_sentinel, sizeof(preview_restore_copy));
            ok = ok && run_restore_shape_preview_case(&restore_shape_preview_cases[i]);
        }
    }
    {
        uint32_t prep_preview_pixels[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
        uint32_t prep_shape_base[4] = {0x01010101u, 0x02020202u, 0x03030303u, 0x04040404u};
        const PrepareShapePreviewMotionCase prepare_shape_preview_motion_cases[] = {
            {
                "prepare_shape_preview_motion_success",
                2, 2,
                prep_preview_pixels,
                prep_preview_pixels,
                prep_shape_base,
                4,
                0,
                TOOL_RECT,
                0, 0,
                1, 1,
                0,
                1, 1, 1, 1,
                prep_shape_base,
                4,
            },
            {
                "prepare_shape_preview_motion_shift_constrained",
                4, 4,
                prep_preview_pixels,
                prep_preview_pixels,
                prep_shape_base,
                4,
                0,
                TOOL_LINE,
                1, 1,
                3, 2,
                1,
                1, 1, 3, 3,
                prep_shape_base,
                4,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(prepare_shape_preview_motion_cases) / sizeof(prepare_shape_preview_motion_cases[0]); i++) {
            memcpy(prep_preview_pixels, (uint32_t[]){0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu}, sizeof(prep_preview_pixels));
            ok = ok && run_prepare_shape_preview_motion_case(&prepare_shape_preview_motion_cases[i]);
        }
    }
    {
        const PrepareShapeCommitCase prepare_shape_commit_cases[] = {
            {
                "prepare_shape_commit_success",
                1, 1,
                TOOL_RECT,
                0, 0, 3, 2, 0,
                1, -111,
                1, -222,
                1, 3, 2,
            },
            {
                "prepare_shape_commit_shift_constrained",
                1, 1,
                TOOL_LINE,
                1, 1, 3, 2, 1,
                1, -333,
                1, -444,
                1, 3, 3,
            },
            {
                "prepare_shape_commit_inactive_noop",
                1, 0,
                TOOL_RECT,
                0, 0, 3, 2, 0,
                1, 17,
                1, 18,
                0, 17, 18,
            },
            {
                "prepare_shape_commit_null_shaping_noop",
                0, 0,
                TOOL_RECT,
                0, 0, 3, 2, 0,
                1, 19,
                1, 20,
                0, 19, 20,
            },
            {
                "prepare_shape_commit_null_out_x_noop",
                1, 1,
                TOOL_RECT,
                0, 0, 3, 2, 0,
                0, 0,
                1, 21,
                0, 0, 21,
            },
            {
                "prepare_shape_commit_null_out_y_noop",
                1, 1,
                TOOL_RECT,
                0, 0, 3, 2, 0,
                1, 22,
                0, 0,
                0, 22, 0,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(prepare_shape_commit_cases) / sizeof(prepare_shape_commit_cases[0]); i++) {
            ok = ok && run_prepare_shape_commit_case(&prepare_shape_commit_cases[i]);
        }
    }
    {
        const PrepareShapeCommitToActiveLayerCase prepare_shape_commit_to_active_layer_cases[] = {
            {
                "prepare_shape_commit_to_active_layer_success",
                1, 0,
                1, 1,
                TOOL_LINE,
                0, 0, 2, 1, 1,
                2,
                2, 2,
                1,
                4,
                0xFF123456u,
                1,
            },
            {
                "prepare_shape_commit_to_active_layer_locked_noop",
                1, 1,
                1, 1,
                TOOL_RECT,
                0, 0, 2, 1, 0,
                2,
                2, 1,
                0,
                0,
                0,
                0,
            },
            {
                "prepare_shape_commit_to_active_layer_inactive_noop",
                1, 0,
                1, 0,
                TOOL_RECT,
                0, 0, 2, 1, 0,
                2,
                -999, -999,
                0,
                0,
                0,
                0,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(prepare_shape_commit_to_active_layer_cases) / sizeof(prepare_shape_commit_to_active_layer_cases[0]); i++) {
            ok = ok && run_prepare_shape_commit_to_active_layer_case(&prepare_shape_commit_to_active_layer_cases[i]);
        }
    }
    {
        const FinalizeShapePreviewCase finalize_cases[] = {
            {
                "finalize_shape_preview_success",
                3, 3, 0xFF123456u, 0,
                0, 0x01020304u,
                1, 1,
                0, 0, 2, 0, 0,
                TOOL_LINE, 1, 0xFFAABBCCu,
                2,
                1, 0, 0, 1, 1,
                0, 0x01020304u,
                2, 0xFFAABBCCu,
            },
            {
                "finalize_shape_preview_locked_noop",
                3, 3, 0xFF123456u, 1,
                4, 0x55667788u,
                1, 1,
                0, 0, 2, 1, 0,
                TOOL_RECT, 1, 0xFFABCDEFu,
                2,
                0, 0, 0, 0, 0,
                0, 0u,
                4, 0x55667788u,
            },
            {
                "finalize_shape_preview_inactive_noop",
                3, 3, 0xFF123456u, 0,
                8, 0x10203040u,
                0, 1,
                0, 0, 2, 2, 0,
                TOOL_FILLED_RECT, 1, 0xFFFFFFFFu,
                2,
                0, 0, 1, 0, 0,
                0, 0u,
                8, 0x10203040u,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(finalize_cases) / sizeof(finalize_cases[0]); i++) {
            ok = ok && run_finalize_shape_preview_case(&finalize_cases[i]);
        }
    }
    {
        Canvas rejection_preview_canvas = {4, 4, preview_restore_copy};
        int rejection_preview_active = 7;
        uint32_t rejection_preview_pixels[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
        uint32_t rejection_shape_base[4] = {0x01010101u, 0x02020202u, 0x03030303u, 0x04040404u};
        uint32_t rejection_preview_want[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
        int rejection_out_x = -999;
        int rejection_out_y = -999;
        PrepareShapePreviewMotionRejectionCase rejection_cases[] = {
            {
                "prepare_shape_preview_motion_oob_noop",
                &rejection_preview_canvas,
                rejection_preview_pixels,
                rejection_shape_base,
                4,
                &rejection_preview_active,
                TOOL_RECT,
                1,
                1,
                4,
                2,
                0,
                &rejection_out_x,
                &rejection_out_y,
                7,
                rejection_preview_want,
                4,
            },
            {
                "prepare_shape_preview_motion_null_out_x",
                &rejection_preview_canvas,
                rejection_preview_pixels,
                rejection_shape_base,
                4,
                &rejection_preview_active,
                TOOL_RECT,
                1,
                1,
                2,
                2,
                0,
                NULL,
                &rejection_out_y,
                7,
                rejection_preview_want,
                4,
            },
            {
                "prepare_shape_preview_motion_null_out_y",
                &rejection_preview_canvas,
                rejection_preview_pixels,
                rejection_shape_base,
                4,
                &rejection_preview_active,
                TOOL_RECT,
                1,
                1,
                2,
                2,
                0,
                &rejection_out_x,
                NULL,
                7,
                rejection_preview_want,
                4,
            },
            {
                "prepare_shape_preview_motion_null_flag",
                &rejection_preview_canvas,
                rejection_preview_pixels,
                rejection_shape_base,
                4,
                NULL,
                TOOL_RECT,
                1,
                1,
                2,
                2,
                0,
                &rejection_out_x,
                &rejection_out_y,
                0,
                rejection_preview_want,
                4,
            },
            {
                "prepare_shape_preview_motion_null_canvas",
                NULL,
                rejection_preview_pixels,
                rejection_shape_base,
                4,
                &rejection_preview_active,
                TOOL_RECT,
                1,
                1,
                2,
                2,
                0,
                &rejection_out_x,
                &rejection_out_y,
                7,
                rejection_preview_want,
                4,
            },
            {
                "prepare_shape_preview_motion_canvas_without_pixels",
                &preview_canvas_without_pixels,
                rejection_preview_pixels,
                rejection_shape_base,
                4,
                &rejection_preview_active,
                TOOL_RECT,
                1,
                1,
                2,
                2,
                0,
                &rejection_out_x,
                &rejection_out_y,
                7,
                rejection_preview_want,
                4,
            },
            {
                "prepare_shape_preview_motion_null_preview_pixels",
                &rejection_preview_canvas,
                NULL,
                rejection_shape_base,
                4,
                &rejection_preview_active,
                TOOL_RECT,
                1,
                1,
                2,
                2,
                0,
                &rejection_out_x,
                &rejection_out_y,
                7,
                NULL,
                0,
            },
            {
                "prepare_shape_preview_motion_null_base_pixels",
                &rejection_preview_canvas,
                rejection_preview_pixels,
                NULL,
                4,
                &rejection_preview_active,
                TOOL_RECT,
                1,
                1,
                2,
                2,
                0,
                &rejection_out_x,
                &rejection_out_y,
                7,
                rejection_preview_want,
                4,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(rejection_cases) / sizeof(rejection_cases[0]); i++) {
            rejection_preview_active = 7;
            rejection_out_x = -999;
            rejection_out_y = -999;
            memcpy(rejection_preview_pixels, rejection_preview_want, sizeof(rejection_preview_pixels));
            ok = ok && run_prepare_shape_preview_motion_rejection_case(&rejection_cases[i]);
        }
    }
    {
        const TitleCase title_cases[] = {
            {
                "title_visible_locked_solo",
                "Brush", "Round",
                6, 100, 0, 3, "Ink",
                1, 1, 75, 1, 2, 0xFF1B1F24u,
                "Openshop - Brush (Round) | size 6 | brush 100% | layer 1/3 Ink [visible, locked 75%] [solo] | visible 2/3 | #FF1B1F24",
            },
            {
                "title_hidden_default_name",
                "Line", "Square",
                12, 40, 2, 4, "",
                0, 0, 100, 0, 1, 0x80123456u,
                "Openshop - Line (Square) | size 12 | brush 40% | layer 3/4 Layer [hidden 100%] | visible 1/4 | #80123456",
            },
            {
                "title_null_labels_default",
                NULL, NULL,
                5, 20, 1, 2, NULL,
                1, 0, 100, 0, 2, 0x00000000u,
                "Openshop - Brush (Round) | size 5 | brush 20% | layer 2/2 Layer [visible 100%] | visible 2/2 | #00000000",
            },
            {
                "title_extreme_numeric_values",
                "Ellipse", "Diamond",
                0, 0, 98, 120, "Edge",
                0, 1, 0, 0, 0, 0xFFFFFFFFu,
                "Openshop - Ellipse (Diamond) | size 0 | brush 0% | layer 99/120 Edge [hidden, locked 0%] | visible 0/120 | #FFFFFFFF",
            },
        };
        const TitlePrefixCase title_prefix_cases[] = {
            {
                "title_truncates",
                16,
                "Filled Rectangle", "Diamond",
                99, 55, 8, 12, "Very Long Layer Name",
                1, 0, 42, 0, 7, 0xABCDEF12u,
                "Openshop - Fill",
            },
        };
        const TitleEmptyBufferCase title_empty_buffer_cases[] = {
            {
                "title_size_one",
                1,
                "Brush", "Round",
                3, 10, 0, 1, "Tiny",
                1, 0, 100, 0, 1, 0xFF000000u,
            },
        };
        const TitleUnchangedBufferCase title_unchanged_buffer_cases[] = {
            {
                "title_size_zero",
                0,
                "Brush", "Round",
                3, 10, 0, 1, "Tiny",
                1, 0, 100, 0, 1, 0xFF000000u,
            },
        };
        size_t i;

        for (i = 0; i < sizeof(title_cases) / sizeof(title_cases[0]); i++) {
            ok = ok && run_title_case(&title_cases[i]);
        }
        for (i = 0; i < sizeof(title_prefix_cases) / sizeof(title_prefix_cases[0]); i++) {
            ok = ok && run_title_prefix_case(&title_prefix_cases[i]);
        }
        for (i = 0; i < sizeof(title_empty_buffer_cases) / sizeof(title_empty_buffer_cases[0]); i++) {
            ok = ok && run_title_empty_buffer_case(&title_empty_buffer_cases[i]);
        }
        for (i = 0; i < sizeof(title_unchanged_buffer_cases) / sizeof(title_unchanged_buffer_cases[0]); i++) {
            ok = ok && run_title_unchanged_buffer_case(&title_unchanged_buffer_cases[i]);
        }
    }

    if (!ok) {
        return 1;
    }

    puts("shortcut selftest ok");
    return 0;
}
