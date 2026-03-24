#include "../src/app_brush.h"
#include "../src/app_brush_mask.h"
#include "../src/app_canvas_click.h"
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
#include "../src/history_state.h"
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

static void init_single_layer_stack(
    LayerStack *stack,
    Canvas *canvas,
    uint32_t *pixels,
    int width,
    int height,
    uint32_t fill,
    int locked
);

static int expect_direct_draw_tool(const char *label, Tool tool, int want) {
    int got = app_tool_draws_directly(tool);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_stroke_mark(const char *label, Tool tool, AppStrokeMark want) {
    AppStrokeMark got = app_tool_stroke_mark(tool);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
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
    int start_x;
    int start_y;
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
    *shaping = test_case->initial_shaping;
    *shape_start_x = test_case->initial_shape_start_x;
    *shape_start_y = test_case->initial_shape_start_y;

    return expect_begin_shape_preview_from_canvas(
        test_case->label,
        test_case->start_x,
        test_case->start_y,
        shaping,
        shape_start_x,
        shape_start_y,
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

static int expect_layer_editable(const char *label, Layer *layer, int want) {
    int got = app_layer_editable(layer);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_active_layer_editable(const char *label, LayerStack *stack, int want) {
    int got = app_active_layer_editable(stack);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_active_editable_layer(const char *label, LayerStack *stack, Layer *want) {
    Layer *got = app_active_editable_layer(stack);
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %p want %p\n", label, (void *)got, (void *)want);
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
    ok = ok && expect_direct_draw_tool("tool_draws_directly_brush", TOOL_BRUSH, 1);
    ok = ok && expect_direct_draw_tool("tool_draws_directly_eraser", TOOL_ERASER, 1);
    ok = ok && expect_direct_draw_tool("tool_draws_directly_line", TOOL_LINE, 0);
    ok = ok && expect_direct_draw_tool("tool_draws_directly_rect", TOOL_RECT, 0);
    ok = ok && expect_direct_draw_tool("tool_draws_directly_filled_rect", TOOL_FILLED_RECT, 0);
    ok = ok && expect_direct_draw_tool("tool_draws_directly_ellipse", TOOL_ELLIPSE, 0);
    ok = ok && expect_direct_draw_tool("tool_draws_directly_filled_ellipse", TOOL_FILLED_ELLIPSE, 0);
    ok = ok && expect_direct_draw_tool("tool_draws_directly_default", (Tool)999, 0);
    ok = ok && expect_stroke_mark("stroke_mark_brush", TOOL_BRUSH, APP_STROKE_MARK_BRUSH);
    ok = ok && expect_stroke_mark("stroke_mark_eraser", TOOL_ERASER, APP_STROKE_MARK_ERASE);
    ok = ok && expect_stroke_mark("stroke_mark_line_defaults_to_brush", TOOL_LINE, APP_STROKE_MARK_BRUSH);
    ok = ok && expect_stroke_mark("stroke_mark_default", (Tool)999, APP_STROKE_MARK_BRUSH);
    ok = ok && expect_brush_stamp_pixel(
        "stamp_brush_center_blends",
        app_stamp_brush,
        0xFF000000u,
        0x80FFFFFFu,
        BRUSH_SHAPE_ROUND,
        0xFF808080u,
        0xFF000000u
    );
    ok = ok && expect_brush_stamp_pixel(
        "erase_brush_center_replaces",
        app_erase_brush,
        0xFF112233u,
        0x00000000u,
        BRUSH_SHAPE_ROUND,
        0x00000000u,
        0xFF112233u
    );
    ok = ok && expect_brush_line_pixel(
        "draw_brush_line_changes_row",
        app_draw_brush_line,
        0x00000000u,
        0xFF556677u,
        BRUSH_SHAPE_ROUND,
        12,
        0xFF556677u,
        0,
        0x00000000u
    );
    ok = ok && expect_brush_line_pixel(
        "erase_brush_line_changes_row",
        app_erase_brush_line,
        0xFFFFFFFFu,
        0x00000000u,
        BRUSH_SHAPE_ROUND,
        12,
        0x00000000u,
        0,
        0xFFFFFFFFu
    );
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
    ok = ok && expect_sampled_brush_color_from_canvas(
        "sampled_brush_color_from_canvas",
        &sampled_canvas,
        1,
        0,
        TOOL_ERASER,
        0xAA112233u,
        0x00112233u,
        42,
        TOOL_BRUSH,
        0x80445566u,
        0x00445566u,
        50
    );
    ok = ok && expect_sampled_brush_color_from_canvas(
        "sampled_brush_color_from_canvas_null_canvas_noop",
        NULL,
        0,
        0,
        TOOL_ERASER,
        0xAA112233u,
        0x00112233u,
        42,
        TOOL_ERASER,
        0xAA112233u,
        0x00112233u,
        42
    );
    ok = ok && expect_sampled_brush_color_from_canvas(
        "sampled_brush_color_from_canvas_missing_pixels_noop",
        &sampled_canvas_without_pixels,
        0,
        0,
        TOOL_ERASER,
        0xAA112233u,
        0x00112233u,
        42,
        TOOL_ERASER,
        0xAA112233u,
        0x00112233u,
        42
    );
    ok = ok && expect_sampled_brush_color_from_canvas(
        "sampled_brush_color_from_canvas_oob_noop",
        &sampled_canvas,
        2,
        0,
        TOOL_ERASER,
        0xAA112233u,
        0x00112233u,
        42,
        TOOL_ERASER,
        0xAA112233u,
        0x00112233u,
        42
    );
    ok = ok && expect_sampled_brush_color_from_available_canvas(
        "sampled_brush_color_from_available_preview_priority",
        &sampled_canvas,
        &sampled_preview_canvas,
        1,
        0,
        1,
        TOOL_ERASER,
        0xAA112233u,
        0x00112233u,
        42,
        TOOL_BRUSH,
        0xFF223344u,
        0x00223344u,
        100
    );
    ok = ok && expect_sampled_brush_color_from_available_canvas(
        "sampled_brush_color_from_available_preview_fallback",
        &sampled_canvas,
        &sampled_preview_without_pixels,
        1,
        1,
        1,
        TOOL_ERASER,
        0xAA112233u,
        0x00112233u,
        42,
        TOOL_BRUSH,
        0xFFABCDEFu,
        0x00ABCDEFu,
        100
    );
    ok = ok && expect_sampled_brush_color_from_available_canvas(
        "sampled_brush_color_from_available_null_composite_noop",
        NULL,
        NULL,
        0,
        0,
        0,
        TOOL_ERASER,
        0xAA112233u,
        0x00112233u,
        42,
        TOOL_ERASER,
        0xAA112233u,
        0x00112233u,
        42
    );
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
    ok = ok && expect_active_layer_editable("active_layer_editable_true", &layer_stack, 1);
    ok = ok && expect_active_editable_layer("active_editable_layer_true", &layer_stack, &layer_stack.layers[0]);
    layer_stack.active_layer = 1;
    ok = ok && expect_active_layer_editable("active_layer_editable_locked", &layer_stack, 0);
    ok = ok && expect_active_editable_layer("active_editable_layer_locked", &layer_stack, NULL);
    layer_stack.active_layer = 2;
    ok = ok && expect_active_layer_editable("active_layer_editable_missing_pixels", &layer_stack, 0);
    ok = ok && expect_active_editable_layer("active_editable_layer_missing_pixels", &layer_stack, NULL);
    layer_stack.active_layer = -1;
    ok = ok && expect_active_editable_layer("active_editable_layer_negative_index", &layer_stack, NULL);
    layer_stack.active_layer = 3;
    ok = ok && expect_active_editable_layer("active_editable_layer_oob_index", &layer_stack, NULL);
    ok = ok && expect_active_layer_editable("active_layer_editable_null_stack", NULL, 0);
    ok = ok && expect_active_editable_layer("active_editable_layer_null_stack", NULL, NULL);
    layer_stack.active_layer = 0;
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
    ok = ok && expect_draw_shape_pixel(
        "draw_shape_line_dispatch",
        TOOL_LINE,
        1,
        2,
        3,
        2,
        1,
        0x00000000u,
        0xFF556677u,
        12,
        0xFF556677u,
        0,
        0x00000000u
    );
    ok = ok && expect_draw_shape_pixel(
        "draw_shape_filled_rect_dispatch",
        TOOL_FILLED_RECT,
        1,
        1,
        3,
        3,
        1,
        0x00000000u,
        0xFF112233u,
        12,
        0xFF112233u,
        0,
        0x00000000u
    );
    ok = ok && expect_draw_shape_pixel(
        "draw_shape_unknown_tool_noop",
        (Tool)999,
        1,
        1,
        3,
        3,
        1,
        0x00000000u,
        0xFF112233u,
        12,
        0x00000000u,
        0,
        0x00000000u
    );
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
    {
        const BeginShapePreviewCase begin_shape_preview_boundary_cases[] = {
            {"begin_shape_preview_null_destination_no_copy", 100, 110, 13, 15, 16, NULL, preview_source, 4, 1, 100, 110, NULL},
            {"begin_shape_preview_null_source_and_destination", 140, 150, 15, 19, 20, NULL, NULL, 4, 1, 140, 150, NULL},
            {"begin_shape_preview_null_everything_copy_path", 160, 170, 16, 21, 22, NULL, NULL, 0, 1, 160, 170, NULL},
        };
        size_t i;

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
        for (i = 0; i < sizeof(begin_shape_preview_boundary_cases) / sizeof(begin_shape_preview_boundary_cases[0]); i++) {
            shaping = begin_shape_preview_boundary_cases[i].initial_shaping;
            shape_start_x = begin_shape_preview_boundary_cases[i].initial_shape_start_x;
            shape_start_y = begin_shape_preview_boundary_cases[i].initial_shape_start_y;
            ok = ok && expect_begin_shape_preview(
                begin_shape_preview_boundary_cases[i].label,
                begin_shape_preview_boundary_cases[i].start_x,
                begin_shape_preview_boundary_cases[i].start_y,
                &shaping,
                &shape_start_x,
                &shape_start_y,
                begin_shape_preview_boundary_cases[i].shape_base_pixels,
                begin_shape_preview_boundary_cases[i].composite_pixels,
                begin_shape_preview_boundary_cases[i].pixel_count,
                begin_shape_preview_boundary_cases[i].want_shaping,
                begin_shape_preview_boundary_cases[i].want_shape_start_x,
                begin_shape_preview_boundary_cases[i].want_shape_start_y,
                begin_shape_preview_boundary_cases[i].want_shape_base_pixels
            );
        }
    }
    {
        const BeginShapePreviewFromCanvasCase begin_shape_preview_from_canvas_cases[] = {
            {"begin_shape_preview_from_canvas_copy", 180, 190, 17, 23, 24, preview_copy, &begin_preview_canvas, 1, 180, 190, preview_source, 4},
            {"begin_shape_preview_from_canvas_null_canvas", 200, 210, 18, 25, 26, preview_copy, NULL, 1, 200, 210, preview_sentinel, 4},
            {"begin_shape_preview_from_canvas_missing_pixels", 220, 230, 19, 27, 28, preview_copy, &begin_preview_canvas_without_pixels, 1, 220, 230, preview_sentinel, 4},
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
        LayerStack stack;
        Canvas canvas;
        uint32_t pixels[4];

        init_single_layer_stack(&stack, &canvas, pixels, 2, 2, 0x00000000u, 0);
        memcpy(preview_copy, preview_sentinel, sizeof(preview_copy));
        shaping = 20;
        shape_start_x = 29;
        shape_start_y = 30;
        ok = ok && expect_begin_shape_preview_to_active_layer(
            "begin_shape_preview_to_active_layer_editable",
            &stack,
            240,
            250,
            &shaping,
            &shape_start_x,
            &shape_start_y,
            preview_copy,
            &begin_preview_canvas,
            1,
            1,
            240,
            250,
            preview_source,
            4
        );
    }
    {
        LayerStack stack;
        Canvas canvas;
        uint32_t pixels[4];

        init_single_layer_stack(&stack, &canvas, pixels, 2, 2, 0x00000000u, 1);
        memcpy(preview_copy, preview_sentinel, sizeof(preview_copy));
        shaping = 21;
        shape_start_x = 31;
        shape_start_y = 32;
        ok = ok && expect_begin_shape_preview_to_active_layer(
            "begin_shape_preview_to_active_layer_locked_noop",
            &stack,
            260,
            270,
            &shaping,
            &shape_start_x,
            &shape_start_y,
            preview_copy,
            &begin_preview_canvas,
            0,
            21,
            31,
            32,
            preview_sentinel,
            4
        );
    }
    memcpy(preview_copy, preview_sentinel, sizeof(preview_copy));
    shaping = 22;
    shape_start_x = 33;
    shape_start_y = 34;
    ok = ok && expect_begin_shape_preview_to_active_layer(
        "begin_shape_preview_to_active_layer_null_stack_noop",
        NULL,
        280,
        290,
        &shaping,
        &shape_start_x,
        &shape_start_y,
        preview_copy,
        &begin_preview_canvas,
        0,
        22,
        33,
        34,
        preview_sentinel,
        4
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
    {
        Canvas prep_preview_canvas = {2, 2, preview_restore_copy};
        int prep_preview_active = 0;
        uint32_t prep_preview_pixels[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
        uint32_t prep_shape_base[4] = {0x01010101u, 0x02020202u, 0x03030303u, 0x04040404u};

        ok = ok && expect_prepare_shape_preview_motion(
            "prepare_shape_preview_motion_success",
            &prep_preview_canvas,
            prep_preview_pixels,
            prep_shape_base,
            4,
            &prep_preview_active,
            TOOL_RECT,
            0,
            0,
            1,
            1,
            0,
            1,
            1,
            1,
            1,
            prep_shape_base,
            4
        );
    }
    shaping = 1;
    sentinel_x = -111;
    sentinel_y = -222;
    ok = ok && expect_prepare_shape_commit(
        "prepare_shape_commit_success",
        &shaping,
        TOOL_RECT,
        0,
        0,
        3,
        2,
        0,
        &sentinel_x,
        &sentinel_y,
        1,
        3,
        2
    );
    shaping = 1;
    sentinel_x = -333;
    sentinel_y = -444;
    ok = ok && expect_prepare_shape_commit(
        "prepare_shape_commit_shift_constrained",
        &shaping,
        TOOL_LINE,
        1,
        1,
        3,
        2,
        1,
        &sentinel_x,
        &sentinel_y,
        1,
        3,
        3
    );
    shaping = 0;
    sentinel_x = 17;
    sentinel_y = 18;
    ok = ok && expect_prepare_shape_commit(
        "prepare_shape_commit_inactive_noop",
        &shaping,
        TOOL_RECT,
        0,
        0,
        3,
        2,
        0,
        &sentinel_x,
        &sentinel_y,
        0,
        17,
        18
    );
    sentinel_x = 19;
    sentinel_y = 20;
    ok = ok && expect_prepare_shape_commit(
        "prepare_shape_commit_null_shaping_noop",
        NULL,
        TOOL_RECT,
        0,
        0,
        3,
        2,
        0,
        &sentinel_x,
        &sentinel_y,
        0,
        19,
        20
    );
    shaping = 1;
    sentinel_y = 21;
    ok = ok && expect_prepare_shape_commit(
        "prepare_shape_commit_null_out_x_noop",
        &shaping,
        TOOL_RECT,
        0,
        0,
        3,
        2,
        0,
        NULL,
        &sentinel_y,
        0,
        0,
        21
    );
    shaping = 1;
    sentinel_x = 22;
    ok = ok && expect_prepare_shape_commit(
        "prepare_shape_commit_null_out_y_noop",
        &shaping,
        TOOL_RECT,
        0,
        0,
        3,
        2,
        0,
        &sentinel_x,
        NULL,
        0,
        22,
        0
    );
    {
        LayerStack stack;
        Canvas canvas;
        uint32_t pixels[9];

        shaping = 1;
        init_single_layer_stack(&stack, &canvas, pixels, 3, 3, 0xFF123456u, 0);
        ok = ok && expect_prepare_shape_commit_to_active_layer(
            "prepare_shape_commit_to_active_layer_success",
            &stack,
            &shaping,
            TOOL_LINE,
            0,
            0,
            2,
            1,
            1,
            2,
            2,
            2,
            1,
            4,
            0xFF123456u,
            &stack.layers[0]
        );
    }
    {
        LayerStack stack;
        Canvas canvas;
        uint32_t pixels[9];

        shaping = 1;
        init_single_layer_stack(&stack, &canvas, pixels, 3, 3, 0xFF123456u, 1);
        ok = ok && expect_prepare_shape_commit_to_active_layer(
            "prepare_shape_commit_to_active_layer_locked_noop",
            &stack,
            &shaping,
            TOOL_RECT,
            0,
            0,
            2,
            1,
            0,
            2,
            2,
            1,
            0,
            0,
            0,
            NULL
        );
    }
    {
        LayerStack stack;
        Canvas canvas;
        uint32_t pixels[9];

        shaping = 0;
        init_single_layer_stack(&stack, &canvas, pixels, 3, 3, 0xFF123456u, 0);
        ok = ok && expect_prepare_shape_commit_to_active_layer(
            "prepare_shape_commit_to_active_layer_inactive_noop",
            &stack,
            &shaping,
            TOOL_RECT,
            0,
            0,
            2,
            1,
            0,
            2,
            -999,
            -999,
            0,
            0,
            0,
            NULL
        );
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
        Canvas prep_preview_canvas = {4, 4, preview_restore_copy};
        int prep_preview_active = 0;
        uint32_t prep_preview_pixels[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
        uint32_t prep_shape_base[4] = {0x01010101u, 0x02020202u, 0x03030303u, 0x04040404u};

        ok = ok && expect_prepare_shape_preview_motion(
            "prepare_shape_preview_motion_shift_constrained",
            &prep_preview_canvas,
            prep_preview_pixels,
            prep_shape_base,
            4,
            &prep_preview_active,
            TOOL_LINE,
            1,
            1,
            3,
            2,
            1,
            1,
            1,
            3,
            3,
            prep_shape_base,
            4
        );
    }
    {
        Canvas prep_preview_canvas = {4, 4, preview_restore_copy};
        int prep_preview_active = 0;
        uint32_t prep_preview_pixels[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
        uint32_t prep_shape_base[4] = {0x01010101u, 0x02020202u, 0x03030303u, 0x04040404u};
        uint32_t prep_preview_want[4] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};

        ok = ok && expect_prepare_shape_preview_motion(
            "prepare_shape_preview_motion_oob_noop",
            &prep_preview_canvas,
            prep_preview_pixels,
            prep_shape_base,
            4,
            &prep_preview_active,
            TOOL_RECT,
            1,
            1,
            4,
            2,
            0,
            0,
            0,
            -999,
            -999,
            prep_preview_want,
            4
        );
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
