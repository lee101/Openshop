#include "../src/path_routing.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

static int mkdir_if_missing(const char *path) {
    if (mkdir(path, 0777) == 0) {
        return 1;
    }
    return errno == EEXIST;
}

static int touch_file(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return 0;
    }
    fclose(f);
    return 1;
}

static void remove_if_exists(const char *path) {
    remove(path);
}

static void cleanup_artifacts(void) {
    remove_if_exists("input.bmp");
    remove_if_exists("input.png");
    remove_if_exists("output.bmp");
    remove_if_exists("output.png");
}

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

    if (!mkdir_if_missing("test-artifacts")) {
        fprintf(stderr, "mkdir_if_missing failed\n");
        return 1;
    }
    if (chdir("test-artifacts") != 0) {
        fprintf(stderr, "chdir test-artifacts failed\n");
        return 1;
    }

    cleanup_artifacts();

    if (!expect_int("path_exists_missing", path_exists("input.bmp"), 0) ||
        !expect_str("resolve_input_default_bmp", resolve_default_input_path(0), "input.bmp") ||
        !expect_str("resolve_output_default_bmp", resolve_default_output_path(0), "output.bmp")) {
        return 1;
    }

    if (!touch_file("input.png") || !touch_file("output.png")) {
        fprintf(stderr, "touch png failed\n");
        return 1;
    }
    if (!expect_int("path_exists_png", path_exists("input.png"), 1) ||
        !expect_str("resolve_input_fallback_png", resolve_default_input_path(0), "input.png") ||
        !expect_str("resolve_output_reuse_png", resolve_default_output_path(0), "output.png")) {
        return 1;
    }

    if (!touch_file("input.bmp") || !touch_file("output.bmp")) {
        fprintf(stderr, "touch bmp failed\n");
        return 1;
    }
    if (!expect_str("resolve_input_prefer_bmp", resolve_default_input_path(0), "input.bmp") ||
        !expect_str("resolve_output_prefer_bmp", resolve_default_output_path(0), "output.bmp") ||
        !expect_str("resolve_input_prefer_png", resolve_default_input_path(1), "input.png") ||
        !expect_str("resolve_output_prefer_png", resolve_default_output_path(1), "output.png")) {
        return 1;
    }

    cleanup_artifacts();
    printf("path routing selftest ok\n");
    return 0;
}
