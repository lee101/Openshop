#include "../src/canvas.h"
#include "../src/image_io.h"

#include <SDL2/SDL.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static int expect_pixel_eq(const char *label, uint32_t got, uint32_t want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

static int mkdir_if_missing(const char *path) {
    if (mkdir(path, 0777) == 0) {
        return 1;
    }
    return errno == EEXIST;
}

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    Canvas src = {0};
    Canvas dst = {0};
    int ok = 1;

    if (!canvas_init(&src, 2, 2) || !canvas_init(&dst, 4, 3)) {
        fprintf(stderr, "canvas_init failed\n");
        ok = 0;
        goto cleanup;
    }

    canvas_clear(&src, 0x00000000);
    canvas_set_pixel(&src, 0, 0, 0xFF112233);
    canvas_set_pixel(&src, 1, 0, 0xFF445566);
    canvas_set_pixel(&src, 0, 1, 0xFF778899);
    canvas_set_pixel(&src, 1, 1, 0xFFAABBCC);

    if (!mkdir_if_missing("test-artifacts")) {
        fprintf(stderr, "mkdir_if_missing failed: %s\n", strerror(errno));
        ok = 0;
        goto cleanup;
    }

    if (!canvas_save_bmp(&src, "test-artifacts/image-io-roundtrip.bmp")) {
        fprintf(stderr, "canvas_save_bmp failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!canvas_save_auto(&src, "test-artifacts/image-io-roundtrip.png")) {
        fprintf(stderr, "canvas_save_auto(.png) failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!canvas_save_png(&src, "test-artifacts/image-io-roundtrip.bin")) {
        fprintf(stderr, "canvas_save_png(.bin) failed\n");
        ok = 0;
        goto cleanup;
    }

    canvas_clear(&dst, 0xFFDEADBE);
    if (!canvas_load_bmp(&dst, "test-artifacts/image-io-roundtrip.bmp", 0xFFFFFFFF)) {
        fprintf(stderr, "canvas_load_bmp failed\n");
        ok = 0;
        goto cleanup;
    }

    ok = ok && expect_pixel_eq("loaded_00", canvas_get_pixel(&dst, 0, 0), 0xFF112233);
    ok = ok && expect_pixel_eq("loaded_10", canvas_get_pixel(&dst, 1, 0), 0xFF445566);
    ok = ok && expect_pixel_eq("loaded_01", canvas_get_pixel(&dst, 0, 1), 0xFF778899);
    ok = ok && expect_pixel_eq("loaded_11", canvas_get_pixel(&dst, 1, 1), 0xFFAABBCC);
    ok = ok && expect_pixel_eq("cleared_extra_col", canvas_get_pixel(&dst, 3, 0), 0xFFFFFFFF);
    ok = ok && expect_pixel_eq("cleared_extra_row", canvas_get_pixel(&dst, 0, 2), 0xFFFFFFFF);

    canvas_clear(&dst, 0xFF010203);
    if (!canvas_load_auto(&dst, "test-artifacts/image-io-roundtrip.png", 0xFFFFFFFF)) {
        fprintf(stderr, "canvas_load_auto(.png) failed\n");
        ok = 0;
        goto cleanup;
    }
    ok = ok && expect_pixel_eq("auto_png_00", canvas_get_pixel(&dst, 0, 0), 0xFF112233);
    ok = ok && expect_pixel_eq("auto_png_11", canvas_get_pixel(&dst, 1, 1), 0xFFAABBCC);

    canvas_clear(&dst, 0xFF010203);
    if (!canvas_load_auto(&dst, "test-artifacts/image-io-roundtrip.bin", 0xFFFFFFFF)) {
        fprintf(stderr, "canvas_load_auto(.bin fallback) failed\n");
        ok = 0;
        goto cleanup;
    }
    ok = ok && expect_pixel_eq("auto_bin_00", canvas_get_pixel(&dst, 0, 0), 0xFF112233);
    ok = ok && expect_pixel_eq("auto_bin_11", canvas_get_pixel(&dst, 1, 1), 0xFFAABBCC);

cleanup:
    canvas_free(&src);
    canvas_free(&dst);
    SDL_Quit();
    if (!ok) {
        return 1;
    }

    printf("ok\n");
    return 0;
}
