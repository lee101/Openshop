#include "../src/path_routing.h"

#include <stdio.h>
#include <string.h>

static int expect_int(const char *label, int actual, int expected) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d got %d\n", label, expected, actual);
        return 0;
    }
    return 1;
}

static int expect_str(const char *label, const char *actual, const char *expected) {
    if (!actual || strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s got %s\n", label, expected, actual ? actual : "(null)");
        return 0;
    }
    return 1;
}

int main(void) {
    if (!expect_int("ext_png_upper", path_has_extension_ci("art/IMAGE.PNG", ".png"), 1) ||
        !expect_int("ext_bmp_mixed", path_has_extension_ci("scene.BmP", ".bmp"), 1) ||
        !expect_int("ext_short", path_has_extension_ci("png", ".png"), 0) ||
        !expect_int("ext_mismatch", path_has_extension_ci("scene.png", ".bmp"), 0)) {
        return 1;
    }

    if (!expect_str("input_prefer_png", default_input_path(1, 1, 1), "input.png") ||
        !expect_str("input_prefer_bmp_existing", default_input_path(0, 1, 1), "input.bmp") ||
        !expect_str("input_fallback_png", default_input_path(0, 0, 1), "input.png") ||
        !expect_str("input_default_bmp", default_input_path(0, 0, 0), "input.bmp")) {
        return 1;
    }

    if (!expect_str("output_prefer_png", default_output_path(1, 1, 1), "output.png") ||
        !expect_str("output_prefer_bmp_existing", default_output_path(0, 1, 1), "output.bmp") ||
        !expect_str("output_reuse_png", default_output_path(0, 0, 1), "output.png") ||
        !expect_str("output_default_bmp", default_output_path(0, 0, 0), "output.bmp")) {
        return 1;
    }

    printf("path routing selftest ok\n");
    return 0;
}
