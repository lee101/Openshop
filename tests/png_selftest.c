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
    canvas_set_pixel(&c, 1, 1, 0xFF0000FF);

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

    canvas_free(&c);
    printf("png selftest ok\n");
    return 0;
}
