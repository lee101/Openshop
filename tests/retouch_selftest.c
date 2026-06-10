#include "../src/font.h"
#include "../src/retouch.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static Canvas make_canvas(int width, int height, uint32_t fill) {
    Canvas c = {0};
    assert(canvas_init(&c, width, height));
    canvas_clear(&c, fill);
    return c;
}

static void test_clone(void) {
    Canvas c = make_canvas(32, 32, 0xFF000000);

    canvas_draw_rect_filled(&c, 0, 0, 7, 31, 0xFFFF0000);
    canvas_clone_stroke(&c, -16, 0, 20, 4, 20, 27, 4, 90);
    assert((canvas_get_pixel(&c, 20, 16) & 0x00FFFFFF) == 0x00FF0000);
    canvas_free(&c);
}

static void test_dodge_burn(void) {
    Canvas c = make_canvas(16, 16, 0xFF808080);

    canvas_dodge_burn_stroke(&c, 8, 8, 8, 8, 5, 60, 0);
    assert(((canvas_get_pixel(&c, 8, 8) >> 16) & 0xFF) > 0x80);
    canvas_free(&c);

    c = make_canvas(16, 16, 0xFF808080);
    canvas_dodge_burn_stroke(&c, 8, 8, 8, 8, 5, 60, 1);
    assert(((canvas_get_pixel(&c, 8, 8) >> 16) & 0xFF) < 0x80);
    canvas_free(&c);
}

static void test_sponge(void) {
    Canvas c = make_canvas(16, 16, 0xFFFF2020);

    canvas_sponge_stroke(&c, 8, 8, 8, 8, 5, 80, 1);
    {
        uint32_t p = canvas_get_pixel(&c, 8, 8);
        int r = (p >> 16) & 0xFF;
        int g = (p >> 8) & 0xFF;
        assert(r - g < 0xFF - 0x20);
    }
    canvas_free(&c);
}

static void test_smudge(void) {
    Canvas c = make_canvas(48, 16, 0xFFFFFFFF);

    canvas_draw_rect_filled(&c, 0, 0, 7, 15, 0xFF0000FF);
    canvas_smudge_stroke(&c, 6, 8, 30, 8, 4, 80);
    {
        uint32_t p = canvas_get_pixel(&c, 14, 8);
        assert((p & 0xFF) > ((p >> 16) & 0xFF));
    }
    canvas_free(&c);
}

static void test_text(void) {
    Canvas c = make_canvas(120, 40, 0xFF000000);
    int painted = 0;

    assert(font_text_width("Hi", 2) > 0);
    assert(font_text_width("Hi", 2) == font_text_width("Hi", 1) * 2);
    assert(font_text_height(2) == 26);
    canvas_draw_text(&c, 4, 4, "Hi", 2, 0xFFFFFFFF);
    for (int y = 0; y < c.height; y++) {
        for (int x = 0; x < c.width; x++) {
            if (canvas_get_pixel(&c, x, y) == 0xFFFFFFFF) {
                painted++;
            }
        }
    }
    assert(painted > 20);
    canvas_free(&c);

    c = make_canvas(120, 60, 0xFF000000);
    canvas_draw_text(&c, 4, 4, "a\nb", 1, 0xFFFFFFFF);
    {
        int top = 0;
        int bottom = 0;
        for (int y = 0; y < 13; y++) {
            for (int x = 0; x < 20; x++) {
                if (canvas_get_pixel(&c, x, y) == 0xFFFFFFFF) top++;
                if (canvas_get_pixel(&c, x, y + 13) == 0xFFFFFFFF) bottom++;
            }
        }
        assert(top > 0 && bottom > 0);
    }
    canvas_free(&c);
}

int main(void) {
    test_clone();
    test_dodge_burn();
    test_sponge();
    test_smudge();
    test_text();
    printf("retouch selftest ok\n");
    return 0;
}
