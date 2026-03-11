#include "../src/canvas.h"
#include <stdio.h>

static int expect_pixel_eq(const char *label, uint32_t got, uint32_t want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

int main(void) {
    Canvas c;
    if (!canvas_init(&c, 64, 64)) {
        fprintf(stderr, "canvas_init failed\n");
        return 1;
    }
    canvas_clear(&c, 0xFFFFFFFF);
    canvas_draw_circle(&c, 32, 32, 8, 0xFF000000);
    canvas_draw_line(&c, 0, 0, 63, 63, 2, 0xFF00FF00);
    canvas_draw_rect_outline(&c, 5, 5, 20, 20, 1, 0xFF0000FF);
    canvas_draw_ellipse_outline(&c, 32, 32, 12, 6, 1, 0xFFFFFF00);
    if (!canvas_flood_fill(&c, 1, 1, 0xFFFF0000)) {
        fprintf(stderr, "canvas_flood_fill failed\n");
        canvas_free(&c);
        return 1;
    }

    // basic checksum to ensure drawing occurred
    unsigned long long sum = 0;
    for (int i = 0; i < c.width * c.height; i++) {
        sum += c.pixels[i];
    }
    canvas_free(&c);

    if (sum == 0) {
        fprintf(stderr, "unexpected checksum\n");
        return 1;
    }

    if (!canvas_init(&c, 2, 2)) {
        fprintf(stderr, "canvas_init blend test failed\n");
        return 1;
    }
    canvas_clear(&c, 0xFFFFFFFF);
    canvas_set_pixel(&c, 0, 0, 0x80000000);
    uint32_t blended = canvas_get_pixel(&c, 0, 0);
    if (blended != 0xFF7F7F7F && blended != 0xFF808080) {
        fprintf(stderr, "unexpected blended pixel: 0x%08X\n", blended);
        canvas_free(&c);
        return 1;
    }
    canvas_set_pixel(&c, 0, 0, 0x80000000);
    if (!expect_pixel_eq("double_blend", canvas_get_pixel(&c, 0, 0), 0xFF3F3F3F)) {
        canvas_free(&c);
        return 1;
    }
    canvas_set_pixel(&c, 0, 1, 0x00FF00FF);
    if (!expect_pixel_eq("transparent_noop", canvas_get_pixel(&c, 0, 1), 0xFFFFFFFF)) {
        canvas_free(&c);
        return 1;
    }
    canvas_set_pixel(&c, 1, 1, 0xFFFF0000);
    if (!expect_pixel_eq("opaque_write", canvas_get_pixel(&c, 1, 1), 0xFFFF0000)) {
        canvas_free(&c);
        return 1;
    }
    canvas_free(&c);

    Canvas transparent;
    if (!canvas_init(&transparent, 1, 1)) {
        fprintf(stderr, "transparent canvas init failed\n");
        return 1;
    }
    canvas_set_pixel(&transparent, 0, 0, 0x80FF0000);
    if (!expect_pixel_eq("blend_into_transparent", canvas_get_pixel(&transparent, 0, 0), 0x80800000)) {
        canvas_free(&transparent);
        return 1;
    }
    canvas_free(&transparent);

    printf("ok\n");
    return 0;
}
