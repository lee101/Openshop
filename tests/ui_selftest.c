#include "../src/ui_sdf.h"
#include "../src/ui_shell.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_sdf_math(void) {
    UiRectF box = {10, 10, 20, 20};

    assert(ui_sdf_rounded_rect(20, 20, box, 0) < 0.0f);
    assert(ui_sdf_rounded_rect(0, 0, box, 0) > 0.0f);
    assert(fabsf(ui_sdf_rounded_rect(10, 20, box, 0)) < 0.6f);
    assert(ui_sdf_rounded_rect(11, 11, box, 8) > 0.0f);
    assert(ui_sdf_segment(5, 5, 0, 5, 10, 5) < 0.001f);
    assert(fabsf(ui_sdf_segment(5, 8, 0, 5, 10, 5) - 3.0f) < 0.001f);
    assert(fabsf(ui_sdf_segment(-3, 5, 0, 5, 10, 5) - 3.0f) < 0.001f);
}

static void test_rasterize_rect(void) {
    UiDrawList list;
    Canvas c = {0};

    assert(canvas_init(&c, 40, 40));
    canvas_clear(&c, 0xFF000000);
    ui_draw_list_reset(&list);
    assert(ui_draw_rect(&list, (UiRectF){8, 8, 16, 16}, 0xFFFFFFFF, 4));
    ui_draw_list_rasterize(&list, &c);

    assert(canvas_get_pixel(&c, 16, 16) == 0xFFFFFFFF);
    assert(canvas_get_pixel(&c, 2, 2) == 0xFF000000);
    assert(canvas_get_pixel(&c, 9, 9) != 0xFFFFFFFF);
    assert(canvas_get_pixel(&c, 9, 9) != 0xFF000000);
    canvas_free(&c);
}

static void test_rasterize_border_and_line(void) {
    UiDrawList list;
    Canvas c = {0};

    assert(canvas_init(&c, 40, 40));
    canvas_clear(&c, 0xFF000000);
    ui_draw_list_reset(&list);
    assert(ui_draw_border(&list, (UiRectF){5, 5, 30, 30}, 0xFFFF0000, 0, 2));
    assert(ui_draw_line(&list, 10, 20, 30, 20, 3, 0xFF00FF00));
    ui_draw_list_rasterize(&list, &c);

    assert((canvas_get_pixel(&c, 6, 20) & 0x00FF0000) != 0);
    assert((canvas_get_pixel(&c, 20, 6) & 0x00FF0000) != 0);
    assert(canvas_get_pixel(&c, 20, 12) == 0xFF000000);
    assert((canvas_get_pixel(&c, 20, 20) & 0x0000FF00) != 0);
    assert(canvas_get_pixel(&c, 2, 2) == 0xFF000000);
    canvas_free(&c);
}

static void test_scissor_and_checkerboard(void) {
    UiDrawList list;
    Canvas c = {0};

    assert(canvas_init(&c, 40, 40));
    canvas_clear(&c, 0xFF000000);
    ui_draw_list_reset(&list);
    assert(ui_scissor_start(&list, (UiRectF){0, 0, 20, 40}));
    assert(ui_draw_rect(&list, (UiRectF){0, 0, 40, 40}, 0xFFFFFFFF, 0));
    assert(ui_scissor_end(&list));
    assert(ui_draw_checkerboard(&list, (UiRectF){0, 0, 8, 8}, 4, 0xFF111111, 0xFF222222));
    ui_draw_list_rasterize(&list, &c);

    assert(canvas_get_pixel(&c, 10, 10) == 0xFFFFFFFF);
    assert(canvas_get_pixel(&c, 30, 10) == 0xFF000000);
    assert(canvas_get_pixel(&c, 1, 1) == 0xFF111111);
    assert(canvas_get_pixel(&c, 5, 1) == 0xFF222222);
    canvas_free(&c);
}

static UiShellState sample_state(void) {
    UiShellState state;

    memset(&state, 0, sizeof(state));
    state.doc_width = 800;
    state.doc_height = 600;
    state.tool_count = 8;
    state.active_tool = 1;
    state.tool_labels[0] = "M";
    state.tool_labels[1] = "B";
    state.tool_labels[2] = "E";
    state.tool_labels[3] = "L";
    state.tool_labels[4] = "R";
    state.tool_labels[5] = "O";
    state.tool_labels[6] = "W";
    state.tool_labels[7] = "G";
    state.layer_count = 2;
    state.active_layer = 1;
    snprintf(state.layer_names[0], UI_SHELL_NAME_MAX, "Background");
    snprintf(state.layer_names[1], UI_SHELL_NAME_MAX, "Paint");
    state.layer_visible[0] = 1;
    state.layer_visible[1] = 1;
    state.layer_opacity[0] = 100;
    state.layer_opacity[1] = 80;
    state.blend_label = "Brush  Normal  100%";
    state.status_text = "fit";
    state.foreground_color = 0xFF1B1F24;
    state.background_color = 0xFFFDD835;
    return state;
}

static void test_shell_layout(void) {
    UiShellState state = sample_state();
    static UiShellFrame frame;

    assert(ui_shell_init());
    ui_shell_build(1280, 800, &state, &frame);

    assert(frame.draw_list.count > 10);
    assert(frame.well.x > 0.0f);
    assert(frame.well.width < 1280.0f);
    assert(frame.scale > 0.0);
    assert(frame.viewport.width > 100.0f);
    assert(frame.viewport.x >= frame.well.x - 1.0f);
    assert(frame.viewport.x + frame.viewport.width <= frame.well.x + frame.well.width + 1.0f);

    {
        static UiShellFrame wide;
        ui_shell_build(1920, 1080, &state, &wide);
        assert(wide.well.width > frame.well.width);
        assert(fabs(wide.scale - 1.0) < 0.0001);
        assert(fabsf(wide.viewport.width - 800.0f) < 1.0f);
    }

    {
        static UiShellFrame compact;
        state.compact = 1;
        ui_shell_build(1280, 800, &state, &compact);
        assert(compact.well.x < 1.0f);
        assert(compact.well.width > 1278.0f);
        state.compact = 0;
    }

    {
        static UiShellFrame zoomed;
        state.zoom = 2.0;
        ui_shell_build(1280, 800, &state, &zoomed);
        assert(fabs(zoomed.scale - 2.0) < 0.0001);
        assert(fabsf(zoomed.viewport.width - 1600.0f) < 1.0f);
        state.zoom = 0.0;
    }
}

static void test_shell_mapping(void) {
    UiShellState state = sample_state();
    static UiShellFrame frame;
    int doc_x = -1;
    int doc_y = -1;
    int inside = 0;

    ui_shell_build(1280, 800, &state, &frame);

    ui_shell_screen_to_doc(&frame, (int)frame.viewport.x, (int)frame.viewport.y, &doc_x, &doc_y, &inside);
    assert(inside);
    assert(doc_x >= 0 && doc_x <= 1);
    assert(doc_y >= 0 && doc_y <= 1);

    ui_shell_screen_to_doc(&frame, 0, 0, &doc_x, &doc_y, &inside);
    assert(!inside);

    ui_shell_screen_to_doc_clamped(&frame, 0, 0, state.doc_width, state.doc_height, &doc_x, &doc_y);
    assert(doc_x == 0 && doc_y == 0);
    ui_shell_screen_to_doc_clamped(&frame, 5000, 5000, state.doc_width, state.doc_height, &doc_x, &doc_y);
    assert(doc_x == state.doc_width - 1 && doc_y == state.doc_height - 1);
}

static void test_shell_rasterize(void) {
    UiShellState state = sample_state();
    static UiShellFrame frame;
    Canvas c = {0};
    int non_background = 0;

    ui_shell_build(640, 480, &state, &frame);
    assert(canvas_init(&c, 640, 480));
    canvas_clear(&c, 0xFF000000);
    ui_draw_list_rasterize(&frame.draw_list, &c);
    for (int y = 0; y < c.height; y += 7) {
        for (int x = 0; x < c.width; x += 7) {
            if (canvas_get_pixel(&c, x, y) != 0xFF000000) {
                non_background++;
            }
        }
    }
    assert(non_background > 500);
    canvas_free(&c);
}

int main(void) {
    test_sdf_math();
    test_rasterize_rect();
    test_rasterize_border_and_line();
    test_scissor_and_checkerboard();
    test_shell_layout();
    test_shell_mapping();
    test_shell_rasterize();
    ui_shell_shutdown();
    printf("ui selftest ok\n");
    return 0;
}
