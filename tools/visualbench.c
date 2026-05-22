#include "../src/app_brush.h"
#include "../src/app_shape.h"
#include "../src/canvas.h"
#include "../src/image_io.h"
#include "../src/layers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BENCH_WIDTH 800
#define BENCH_HEIGHT 600
#define BENCH_BG 0xFFFFFFFF

static int save_composite(const LayerStack *stack, const char *path) {
    Canvas composite;
    int ok = 0;

    if (!canvas_init(&composite, BENCH_WIDTH, BENCH_HEIGHT)) {
        fprintf(stderr, "visualbench: failed to allocate composite for %s\n", path);
        return 0;
    }

    layer_stack_composite(stack, &composite, BENCH_BG);
    ok = canvas_save_bmp(&composite, path);
    canvas_free(&composite);

    if (!ok) {
        fprintf(stderr, "visualbench: failed to save %s\n", path);
    }
    return ok;
}

static void draw_gradient(Canvas *canvas) {
    for (int y = 0; y < canvas->height; y++) {
        for (int x = 0; x < canvas->width; x++) {
            int r = 28 + (x * 165) / canvas->width;
            int g = 36 + (y * 118) / canvas->height;
            int b = 62 + ((canvas->width - x) * 128) / canvas->width;
            canvas_set_pixel_raw(canvas, x, y, 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
        }
    }
}

static void draw_sample_photo(Canvas *canvas) {
    draw_gradient(canvas);
    canvas_draw_ellipse_filled(canvas, 628, 132, 76, 76, 0xFFFFD36A);
    canvas_draw_ellipse_filled(canvas, 636, 124, 52, 52, 0xFFFFEAB0);
    canvas_draw_rect_filled(canvas, 0, 398, BENCH_WIDTH - 1, BENCH_HEIGHT - 1, 0xFF174F49);
    canvas_draw_ellipse_filled(canvas, 260, 424, 260, 94, 0xFF256F61);
    canvas_draw_ellipse_filled(canvas, 596, 430, 316, 120, 0xFF143D45);
    canvas_draw_rect_filled(canvas, 132, 268, 312, 452, 0xFFF5E7C8);
    canvas_draw_rect_filled(canvas, 196, 356, 244, 452, 0xFF3A2D32);
    app_draw_shape(canvas, TOOL_LINE, 102, 268, 222, 174, 5, 0xFFB34B3C);
    app_draw_shape(canvas, TOOL_LINE, 222, 174, 344, 268, 5, 0xFFB34B3C);
    app_draw_shape(canvas, TOOL_LINE, 102, 268, 344, 268, 5, 0xFFB34B3C);
    canvas_draw_rect_filled(canvas, 150, 310, 188, 350, 0xFF94BBD0);
    canvas_draw_rect_filled(canvas, 260, 310, 294, 350, 0xFF94BBD0);
}

static void draw_ui_overlay(LayerStack *stack, Canvas *overlay) {
    canvas_draw_rect_filled(overlay, 0, 0, BENCH_WIDTH - 1, 42, 0xEE18181B);
    canvas_draw_rect_filled(overlay, 0, 42, 58, BENCH_HEIGHT - 1, 0xDD1F1F24);
    canvas_draw_rect_filled(overlay, BENCH_WIDTH - 184, 42, BENCH_WIDTH - 1, BENCH_HEIGHT - 1, 0xE6232329);

    for (int i = 0; i < 7; i++) {
        int y = 64 + i * 46;
        uint32_t color = i == 0 ? 0xFFFFD36A : 0xFF3A3A42;
        canvas_draw_rect_outline(overlay, 13, y, 45, y + 31, 2, color);
    }

    for (int i = 0; i < stack->layer_count; i++) {
        int y = 74 + i * 58;
        uint32_t border = i == stack->active_layer ? 0xFFFFD36A : 0xFF6B7280;
        canvas_draw_rect_outline(overlay, 642, y, 776, y + 40, 2, border);
        canvas_draw_rect_filled(overlay, 650, y + 10, 688, y + 30, stack->layers[i].visible ? 0xAA93C5FD : 0xAA555555);
        if (stack->layers[i].locked) {
            canvas_draw_rect_filled(overlay, 716, y + 12, 746, y + 28, 0xAAEF4444);
        }
    }

    canvas_draw_rect_filled(overlay, 642, 412, 776, 418, 0xFFFFD36A);
    canvas_draw_rect_filled(overlay, 642, 444, 736, 450, 0xFF93C5FD);
    canvas_draw_rect_filled(overlay, 642, 476, 760, 482, 0xFFE879F9);
}

static int make_editor_overview(void) {
    LayerStack stack;
    int ok = 0;

    if (!layer_stack_init(&stack, BENCH_WIDTH, BENCH_HEIGHT, BENCH_BG)) {
        return 0;
    }

    draw_sample_photo(&stack.layers[0].canvas);
    int retouch_index = layer_stack_add(&stack, "Retouch", 0x00000000);
    int shape_index = layer_stack_add(&stack, "Vector shapes", 0x00000000);
    int ui_index = layer_stack_add(&stack, "UI chrome", 0x00000000);
    if (retouch_index < 0 || shape_index < 0 || ui_index < 0) {
        goto done;
    }

    app_draw_brush_line(&stack.layers[retouch_index].canvas, 424, 228, 548, 280, 17, 0xCCFFD36A, BRUSH_SHAPE_ROUND);
    app_draw_brush_line(&stack.layers[retouch_index].canvas, 440, 304, 592, 360, 12, 0xBB38BDF8, BRUSH_SHAPE_DIAMOND);
    app_draw_shape(&stack.layers[shape_index].canvas, TOOL_RECT, 386, 118, 700, 392, 4, 0xCCFFFFFF);
    app_draw_shape(&stack.layers[shape_index].canvas, TOOL_ELLIPSE, 468, 172, 624, 326, 4, 0xCCF472B6);
    draw_ui_overlay(&stack, &stack.layers[ui_index].canvas);

    ok = save_composite(&stack, "visualbench/editor-overview.bmp");

done:
    layer_stack_free(&stack);
    return ok;
}

static int make_layer_states(void) {
    LayerStack stack;
    int ok = 0;

    if (!layer_stack_init(&stack, BENCH_WIDTH, BENCH_HEIGHT, 0xFF101820)) {
        return 0;
    }
    canvas_draw_rect_filled(&stack.layers[0].canvas, 80, 82, 720, 516, 0xFF243B55);

    int red = layer_stack_add(&stack, "Red paint", 0x00000000);
    int blue = layer_stack_add(&stack, "Blue paint", 0x00000000);
    int locked = layer_stack_add(&stack, "Locked guide", 0x00000000);
    if (red < 0 || blue < 0 || locked < 0) {
        goto done;
    }

    canvas_draw_ellipse_filled(&stack.layers[red].canvas, 316, 300, 164, 164, 0xDDEF4444);
    canvas_draw_ellipse_filled(&stack.layers[blue].canvas, 480, 300, 164, 164, 0xDD38BDF8);
    app_draw_shape(&stack.layers[locked].canvas, TOOL_LINE, 120, 110, 680, 490, 6, 0xEEFFFFFF);
    app_draw_shape(&stack.layers[locked].canvas, TOOL_LINE, 680, 110, 120, 490, 6, 0xEEFFFFFF);
    layer_stack_set_opacity(&stack, red, 78);
    layer_stack_set_opacity(&stack, blue, 62);
    layer_stack_toggle_lock(&stack, locked);
    stack.active_layer = blue;

    ok = save_composite(&stack, "visualbench/layer-states.bmp");

done:
    layer_stack_free(&stack);
    return ok;
}

static int make_tools_gallery(void) {
    LayerStack stack;
    int ok = 0;

    if (!layer_stack_init(&stack, BENCH_WIDTH, BENCH_HEIGHT, 0xFFF8FAFC)) {
        return 0;
    }

    int layer = layer_stack_add(&stack, "Tools", 0x00000000);
    if (layer < 0) {
        goto done;
    }

    Canvas *canvas = &stack.layers[layer].canvas;
    app_draw_brush_line(canvas, 70, 112, 330, 180, 18, 0xFF111827, BRUSH_SHAPE_ROUND);
    app_draw_brush_line(canvas, 90, 244, 338, 244, 14, 0xFFE11D48, BRUSH_SHAPE_SQUARE);
    app_draw_brush_line(canvas, 94, 330, 336, 430, 16, 0xFF7C3AED, BRUSH_SHAPE_DIAMOND);
    app_draw_shape(canvas, TOOL_LINE, 430, 96, 726, 174, 8, 0xFF2563EB);
    app_draw_shape(canvas, TOOL_RECT, 430, 220, 724, 330, 5, 0xFF059669);
    app_draw_shape(canvas, TOOL_FILLED_RECT, 448, 386, 596, 512, 1, 0xBBF59E0B);
    app_draw_shape(canvas, TOOL_ELLIPSE, 596, 366, 746, 526, 5, 0xFFDB2777);

    ok = save_composite(&stack, "visualbench/tools-gallery.bmp");

done:
    layer_stack_free(&stack);
    return ok;
}

int main(void) {
    if (!make_editor_overview()) {
        return 1;
    }
    if (!make_layer_states()) {
        return 1;
    }
    if (!make_tools_gallery()) {
        return 1;
    }

    puts("visualbench wrote visualbench/editor-overview.bmp");
    puts("visualbench wrote visualbench/layer-states.bmp");
    puts("visualbench wrote visualbench/tools-gallery.bmp");
    return 0;
}
