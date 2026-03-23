#include "../src/canvas.h"
#include "../src/png_io.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static int mkdir_if_missing(const char *path) {
    if (mkdir(path, 0777) == 0) {
        return 1;
    }
    if (errno == EEXIST) {
        return 1;
    }
    return 0;
}

int main(void) {
    Canvas c = {0};
    if (!canvas_init(&c, 4, 4)) {
        fprintf(stderr, "canvas_init failed\n");
        return 1;
    }

    /* Fill with a known solid color: opaque red ARGB = 0xFFFF0000 */
    canvas_clear(&c, 0xFFFF0000);
    /* Put a blue pixel at (1,1) to exercise non-uniform data */
    canvas_set_pixel_raw(&c, 1, 1, 0xFF0000FF);
    canvas_set_pixel_raw(&c, 2, 2, 0x8000FF00);

    if (!mkdir_if_missing("test-artifacts")) {
        fprintf(stderr, "failed to create test-artifacts: %s\n", strerror(errno));
        canvas_free(&c);
        return 1;
    }

    const char *out = "test-artifacts/test.png";
    int ok = canvas_save_png(&c, out);
    if (!ok) {
        fprintf(stderr, "canvas_save_png returned 0 (failure)\n");
        canvas_free(&c);
        return 1;
    }

    /* Verify the file exists and starts with PNG magic bytes */
    FILE *f = fopen(out, "rb");
    if (!f) {
        fprintf(stderr, "could not open %s after save\n", out);
        canvas_free(&c);
        return 1;
    }
    unsigned char magic[4];
    size_t n = fread(magic, 1, sizeof(magic), f);
    fclose(f);
    if (n < 4 || magic[0] != 0x89 || magic[1] != 'P' || magic[2] != 'N' || magic[3] != 'G') {
        fprintf(stderr, "PNG magic bytes missing in %s\n", out);
        canvas_free(&c);
        return 1;
    }

    {
        Canvas c2 = {0};
        int pass = 1;
        if (!canvas_init(&c2, 4, 4)) {
            fprintf(stderr, "canvas_init (c2) failed\n");
            canvas_free(&c);
            return 1;
        }
        if (!canvas_load_png(&c2, out, 0xFF000000)) {
            fprintf(stderr, "canvas_load_png returned 0 (failure)\n");
            canvas_free(&c);
            canvas_free(&c2);
            return 1;
        }
        if (canvas_get_pixel(&c2, 0, 0) != 0xFFFF0000) {
            fprintf(stderr, "round-trip (0,0): expected 0xFFFF0000 got 0x%08X\n", canvas_get_pixel(&c2, 0, 0));
            pass = 0;
        }
        if (canvas_get_pixel(&c2, 1, 1) != 0xFF0000FF) {
            fprintf(stderr, "round-trip (1,1): expected 0xFF0000FF got 0x%08X\n", canvas_get_pixel(&c2, 1, 1));
            pass = 0;
        }
        if (canvas_get_pixel(&c2, 2, 2) != 0x8000FF00) {
            fprintf(stderr, "round-trip (2,2): expected 0x8000FF00 got 0x%08X\n", canvas_get_pixel(&c2, 2, 2));
            pass = 0;
        }
        canvas_free(&c2);
        if (!pass) {
            canvas_free(&c);
            return 1;
        }
    }

    canvas_free(&c);
    printf("png selftest ok\n");
    return 0;
}
