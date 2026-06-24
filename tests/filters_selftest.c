#include "../src/filters.h"

#include <assert.h>
#include <stdio.h>

static void test_rgb_grayscale(void) {
    unsigned char pixels[] = {
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        10, 20, 30
    };

    os_grayscale(pixels, 2, 2, 3);

    assert(pixels[0] == 76 && pixels[1] == 76 && pixels[2] == 76);
    assert(pixels[3] == 150 && pixels[4] == 150 && pixels[5] == 150);
    assert(pixels[6] == 29 && pixels[7] == 29 && pixels[8] == 29);
    assert(pixels[9] == 18 && pixels[10] == 18 && pixels[11] == 18);
}

static void test_rgba_preserves_alpha(void) {
    unsigned char pixels[] = {
        100, 150, 200, 7,
        30, 20, 10, 250
    };

    os_grayscale(pixels, 2, 1, 4);

    assert(pixels[0] == 141 && pixels[1] == 141 && pixels[2] == 141 && pixels[3] == 7);
    assert(pixels[4] == 22 && pixels[5] == 22 && pixels[6] == 22 && pixels[7] == 250);
}

static void test_invalid_args_are_noop(void) {
    unsigned char pixels[] = {1, 2, 3, 4};

    os_grayscale(pixels, 1, 1, 1);
    assert(pixels[0] == 1 && pixels[1] == 2 && pixels[2] == 3 && pixels[3] == 4);

    os_grayscale(pixels, 0, 1, 4);
    assert(pixels[0] == 1 && pixels[1] == 2 && pixels[2] == 3 && pixels[3] == 4);

    os_grayscale(NULL, 1, 1, 4);
}

int main(void) {
    test_rgb_grayscale();
    test_rgba_preserves_alpha();
    test_invalid_args_are_noop();
    puts("filters selftest ok");
    return 0;
}
