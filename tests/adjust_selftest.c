#include "../src/adjust.h"
#include "../src/gradient.h"

#include <assert.h>
#include <stdio.h>

static Canvas make_canvas(int width, int height, uint32_t fill) {
    Canvas c = {0};
    assert(canvas_init(&c, width, height));
    canvas_clear(&c, fill);
    return c;
}

static void test_brightness_contrast(void) {
    Canvas c = make_canvas(4, 4, 0xFF808080);

    canvas_adjust_brightness_contrast(&c, 50, 0);
    assert(((canvas_get_pixel(&c, 0, 0) >> 16) & 0xFF) > 0x80);
    canvas_free(&c);

    c = make_canvas(4, 4, 0xFF808080);
    canvas_adjust_brightness_contrast(&c, -50, 0);
    assert(((canvas_get_pixel(&c, 0, 0) >> 16) & 0xFF) < 0x80);
    canvas_free(&c);

    c = make_canvas(4, 4, 0xFFC0C0C0);
    canvas_adjust_brightness_contrast(&c, 0, 100);
    assert(((canvas_get_pixel(&c, 0, 0) >> 16) & 0xFF) == 0xFF);
    canvas_free(&c);

    c = make_canvas(4, 4, 0xFF204060);
    canvas_adjust_brightness_contrast(&c, 0, 0);
    assert(canvas_get_pixel(&c, 0, 0) == 0xFF204060);
    canvas_free(&c);
}

static void test_hue_saturation(void) {
    Canvas c = make_canvas(2, 2, 0xFFFF0000);

    canvas_adjust_hue_saturation(&c, 120, 0, 0);
    {
        uint32_t p = canvas_get_pixel(&c, 0, 0);
        assert(((p >> 8) & 0xFF) > ((p >> 16) & 0xFF));
        assert(((p >> 8) & 0xFF) > (p & 0xFF));
    }
    canvas_free(&c);

    c = make_canvas(2, 2, 0xFFFF0000);
    canvas_adjust_hue_saturation(&c, 0, -100, 0);
    {
        uint32_t p = canvas_get_pixel(&c, 0, 0);
        assert(((p >> 16) & 0xFF) == ((p >> 8) & 0xFF));
        assert(((p >> 8) & 0xFF) == (p & 0xFF));
    }
    canvas_free(&c);

    c = make_canvas(2, 2, 0xFF808080);
    canvas_adjust_hue_saturation(&c, 0, 0, 100);
    assert((canvas_get_pixel(&c, 0, 0) & 0x00FFFFFF) == 0x00FFFFFF);
    canvas_free(&c);
}

static void test_levels(void) {
    Canvas c = make_canvas(2, 2, 0xFF808080);

    canvas_adjust_levels(&c, 0, 128, 1.0, 0, 255);
    assert((canvas_get_pixel(&c, 0, 0) & 0x00FFFFFF) == 0x00FFFFFF);
    canvas_free(&c);

    c = make_canvas(2, 2, 0xFF808080);
    canvas_adjust_levels(&c, 128, 255, 1.0, 0, 255);
    assert((canvas_get_pixel(&c, 0, 0) & 0x00FFFFFF) == 0x00000000);
    canvas_free(&c);

    c = make_canvas(2, 2, 0xFF000000);
    canvas_adjust_levels(&c, 0, 255, 1.0, 64, 255);
    assert(((canvas_get_pixel(&c, 0, 0) >> 16) & 0xFF) == 64);
    canvas_free(&c);
}

static void test_desaturate_posterize_threshold(void) {
    Canvas c = make_canvas(2, 2, 0xFF2080E0);

    canvas_desaturate(&c);
    {
        uint32_t p = canvas_get_pixel(&c, 0, 0);
        assert(((p >> 16) & 0xFF) == ((p >> 8) & 0xFF));
        assert(((p >> 8) & 0xFF) == (p & 0xFF));
    }
    canvas_free(&c);

    c = make_canvas(2, 2, 0xFF7F7F7F);
    canvas_posterize(&c, 2);
    {
        uint32_t p = canvas_get_pixel(&c, 0, 0) & 0xFF;
        assert(p == 0 || p == 255);
    }
    canvas_free(&c);

    c = make_canvas(2, 2, 0xFFC0C0C0);
    canvas_threshold(&c, 128);
    assert((canvas_get_pixel(&c, 0, 0) & 0x00FFFFFF) == 0x00FFFFFF);
    canvas_free(&c);

    c = make_canvas(2, 2, 0xFF202020);
    canvas_threshold(&c, 128);
    assert((canvas_get_pixel(&c, 0, 0) & 0x00FFFFFF) == 0x00000000);
    canvas_free(&c);
}

static void test_blur_and_sharpen(void) {
    Canvas c = make_canvas(9, 9, 0xFF000000);

    canvas_set_pixel_raw(&c, 4, 4, 0xFFFFFFFF);
    canvas_gaussian_blur(&c, 2);
    {
        uint32_t center = canvas_get_pixel(&c, 4, 4);
        uint32_t neighbor = canvas_get_pixel(&c, 5, 4);
        assert(((center >> 16) & 0xFF) < 0xFF);
        assert(((neighbor >> 16) & 0xFF) > 0);
    }
    canvas_free(&c);

    c = make_canvas(9, 9, 0xFF404040);
    canvas_draw_rect_filled(&c, 0, 0, 4, 8, 0xFFC0C0C0);
    {
        uint32_t before = (canvas_get_pixel(&c, 4, 4) >> 16) & 0xFF;
        canvas_sharpen(&c, 200);
        uint32_t after = (canvas_get_pixel(&c, 4, 4) >> 16) & 0xFF;
        assert(after >= before);
    }
    canvas_free(&c);
}

static void test_gradient(void) {
    Canvas c = make_canvas(16, 1, 0xFF000000);

    canvas_gradient_fill(&c, 0, 0, 15, 0, 0xFF000000, 0xFFFFFFFF, GRADIENT_LINEAR);
    assert((canvas_get_pixel(&c, 0, 0) & 0xFF) == 0);
    assert((canvas_get_pixel(&c, 15, 0) & 0xFF) == 0xFF);
    {
        uint32_t mid = canvas_get_pixel(&c, 8, 0) & 0xFF;
        assert(mid > 0x60 && mid < 0xA0);
    }
    canvas_free(&c);

    c = make_canvas(17, 17, 0xFF000000);
    canvas_gradient_fill(&c, 8, 8, 16, 8, 0xFFFFFFFF, 0xFF000000, GRADIENT_RADIAL);
    assert((canvas_get_pixel(&c, 8, 8) & 0xFF) == 0xFF);
    assert((canvas_get_pixel(&c, 16, 8) & 0xFF) == 0);
    assert((canvas_get_pixel(&c, 8, 16) & 0xFF) == 0);
    canvas_free(&c);

    assert(gradient_lerp_argb(0xFF000000, 0xFFFFFFFF, 0.5) == 0xFF808080);
    assert(gradient_type_valid(GRADIENT_LINEAR));
    assert(!gradient_type_valid(99));
}

static void test_canvas_resize(void) {
    Canvas c = make_canvas(4, 4, 0xFF404040);

    canvas_set_pixel_raw(&c, 3, 3, 0xFFC0C0C0);
    assert(canvas_resize_bilinear(&c, 8, 8));
    assert(c.width == 8 && c.height == 8);
    assert(canvas_get_pixel(&c, 0, 0) == 0xFF404040);
    assert(canvas_get_pixel(&c, 7, 7) == 0xFFC0C0C0);
    canvas_free(&c);

    c = make_canvas(4, 4, 0xFF112233);
    assert(canvas_resize_extent(&c, 8, 8, 2, 2, 0x00000000));
    assert(canvas_get_pixel(&c, 0, 0) == 0x00000000);
    assert(canvas_get_pixel(&c, 3, 3) == 0xFF112233);
    assert(canvas_get_pixel(&c, 7, 7) == 0x00000000);
    canvas_free(&c);
}

int main(void) {
    test_gradient();
    test_canvas_resize();
    test_brightness_contrast();
    test_hue_saturation();
    test_levels();
    test_desaturate_posterize_threshold();
    test_blur_and_sharpen();
    printf("adjust selftest ok\n");
    return 0;
}
