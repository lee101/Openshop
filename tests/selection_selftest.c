#include "../src/selection.h"

#include <assert.h>
#include <stdio.h>

static void test_basic_ops(void) {
    Selection sel;

    assert(selection_init(&sel, 16, 16));
    assert(!sel.active);
    assert(selection_coverage(&sel, 4, 4) == 255);
    assert(selection_is_empty(&sel));

    selection_select_all(&sel);
    assert(sel.active);
    assert(selection_coverage(&sel, 0, 0) == 255);
    assert(!selection_is_empty(&sel));

    selection_deselect(&sel);
    assert(!sel.active);
    assert(selection_coverage(&sel, 0, 0) == 255);

    selection_select_rect(&sel, 2, 2, 5, 5, SELECTION_REPLACE);
    assert(selection_coverage(&sel, 3, 3) == 255);
    assert(selection_coverage(&sel, 10, 10) == 0);
    {
        int x0, y0, x1, y1;
        assert(selection_bounds(&sel, &x0, &y0, &x1, &y1));
        assert(x0 == 2 && y0 == 2 && x1 == 5 && y1 == 5);
    }

    selection_select_rect(&sel, 10, 10, 12, 12, SELECTION_ADD);
    assert(selection_coverage(&sel, 3, 3) == 255);
    assert(selection_coverage(&sel, 11, 11) == 255);

    selection_select_rect(&sel, 2, 2, 3, 3, SELECTION_SUBTRACT);
    assert(selection_coverage(&sel, 2, 2) == 0);
    assert(selection_coverage(&sel, 5, 5) == 255);

    selection_invert(&sel);
    assert(selection_coverage(&sel, 2, 2) == 255);
    assert(selection_coverage(&sel, 5, 5) == 0);

    selection_free(&sel);
}

static void test_ellipse(void) {
    Selection sel;

    assert(selection_init(&sel, 32, 32));
    selection_select_ellipse(&sel, 4, 4, 27, 27, SELECTION_REPLACE);
    assert(selection_coverage(&sel, 16, 16) == 255);
    assert(selection_coverage(&sel, 5, 5) == 0);
    assert(selection_coverage(&sel, 16, 5) == 255);
    selection_free(&sel);
}

static void test_magic_wand(void) {
    Selection sel;
    Canvas c = {0};

    assert(selection_init(&sel, 16, 16));
    assert(canvas_init(&c, 16, 16));
    canvas_clear(&c, 0xFFFFFFFF);
    canvas_draw_rect_filled(&c, 4, 4, 8, 8, 0xFFFF0000);

    assert(selection_magic_wand(&sel, &c, 5, 5, 16, SELECTION_REPLACE));
    assert(selection_coverage(&sel, 6, 6) == 255);
    assert(selection_coverage(&sel, 0, 0) == 0);

    assert(selection_magic_wand(&sel, &c, 0, 0, 16, SELECTION_REPLACE));
    assert(selection_coverage(&sel, 0, 0) == 255);
    assert(selection_coverage(&sel, 6, 6) == 0);
    assert(selection_coverage(&sel, 15, 15) == 255);

    assert(!selection_magic_wand(&sel, &c, 50, 50, 16, SELECTION_REPLACE));

    canvas_free(&c);
    selection_free(&sel);
}

static void test_feather(void) {
    Selection sel;

    assert(selection_init(&sel, 32, 32));
    selection_select_rect(&sel, 12, 12, 19, 19, SELECTION_REPLACE);
    selection_feather(&sel, 3);
    assert(selection_coverage(&sel, 15, 15) > 200);
    {
        uint8_t edge = selection_coverage(&sel, 11, 15);
        assert(edge > 0 && edge < 255);
    }
    assert(selection_coverage(&sel, 0, 0) == 0);
    selection_free(&sel);
}

static void test_clamp_edit(void) {
    Selection sel;
    Canvas c = {0};
    uint32_t original[16 * 16];

    assert(selection_init(&sel, 16, 16));
    assert(canvas_init(&c, 16, 16));
    canvas_clear(&c, 0xFF000000);
    for (int i = 0; i < 16 * 16; i++) {
        original[i] = 0xFF000000;
    }

    selection_select_rect(&sel, 0, 0, 7, 15, SELECTION_REPLACE);
    canvas_clear(&c, 0xFFFFFFFF);
    selection_clamp_edit(&sel, &c, original);

    assert(canvas_get_pixel(&c, 3, 3) == 0xFFFFFFFF);
    assert(canvas_get_pixel(&c, 12, 3) == 0xFF000000);

    canvas_free(&c);
    selection_free(&sel);
}

int main(void) {
    test_basic_ops();
    test_ellipse();
    test_magic_wand();
    test_feather();
    test_clamp_edit();
    printf("selection selftest ok\n");
    return 0;
}
