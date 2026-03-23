#include "../src/path_routing.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
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
    remove_if_exists("draft.bmp");
    remove_if_exists("draft.png");
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
    RoutedPath choice = {0};
    char bmp_path[ROUTED_PATH_MAX] = {0};
    char png_path[ROUTED_PATH_MAX] = {0};

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

    if (!build_routed_paths("shots/scene.png", bmp_path, sizeof(bmp_path), png_path, sizeof(png_path)) ||
        !expect_str("build_from_png_bmp", bmp_path, "shots/scene.bmp") ||
        !expect_str("build_from_png_png", png_path, "shots/scene.png")) {
        return 1;
    }

    if (!build_routed_paths("shots/scene.bmp", bmp_path, sizeof(bmp_path), png_path, sizeof(png_path)) ||
        !expect_str("build_from_bmp_bmp", bmp_path, "shots/scene.bmp") ||
        !expect_str("build_from_bmp_png", png_path, "shots/scene.png")) {
        return 1;
    }

    if (!build_routed_paths("shots/scene", bmp_path, sizeof(bmp_path), png_path, sizeof(png_path)) ||
        !expect_str("build_no_ext_bmp", bmp_path, "shots/scene.bmp") ||
        !expect_str("build_no_ext_png", png_path, "shots/scene.png")) {
        return 1;
    }

    if (!build_routed_paths("shots/archive.data", bmp_path, sizeof(bmp_path), png_path, sizeof(png_path)) ||
        !expect_str("build_unknown_ext_bmp", bmp_path, "shots/archive.bmp") ||
        !expect_str("build_unknown_ext_png", png_path, "shots/archive.png")) {
        return 1;
    }

    if (!build_routed_paths("shots/v1.2/scene.final.PNG", bmp_path, sizeof(bmp_path), png_path, sizeof(png_path)) ||
        !expect_str("build_dotted_dir_bmp", bmp_path, "shots/v1.2/scene.final.bmp") ||
        !expect_str("build_dotted_dir_png", png_path, "shots/v1.2/scene.final.png")) {
        return 1;
    }

    if (!build_routed_paths("C:\\art\\scene.BmP", bmp_path, sizeof(bmp_path), png_path, sizeof(png_path)) ||
        !expect_str("build_windows_bmp", bmp_path, "C:\\art\\scene.bmp") ||
        !expect_str("build_windows_png", png_path, "C:\\art\\scene.png")) {
        return 1;
    }

    if (!build_routed_paths(".hiddenfile", bmp_path, sizeof(bmp_path), png_path, sizeof(png_path)) ||
        !expect_str("build_hidden_bmp", bmp_path, ".hiddenfile.bmp") ||
        !expect_str("build_hidden_png", png_path, ".hiddenfile.png")) {
        return 1;
    }

    if (!expect_int("build_null_seed", build_routed_paths(NULL, bmp_path, sizeof(bmp_path), png_path, sizeof(png_path)), 0) ||
        !expect_int("build_empty_seed", build_routed_paths("", bmp_path, sizeof(bmp_path), png_path, sizeof(png_path)), 0) ||
        !expect_int("build_null_bmp", build_routed_paths("scene.png", NULL, sizeof(bmp_path), png_path, sizeof(png_path)), 0) ||
        !expect_int("build_null_png", build_routed_paths("scene.png", bmp_path, sizeof(bmp_path), NULL, sizeof(png_path)), 0) ||
        !expect_int("build_zero_bmp_size", build_routed_paths("scene.png", bmp_path, 0, png_path, sizeof(png_path)), 0) ||
        !expect_int("build_zero_png_size", build_routed_paths("scene.png", bmp_path, sizeof(bmp_path), png_path, 0), 0) ||
        !expect_int("build_small_bmp_buffer", build_routed_paths("scene.png", bmp_path, 4, png_path, sizeof(png_path)), 0) ||
        !expect_int("build_small_png_buffer", build_routed_paths("scene.png", bmp_path, sizeof(bmp_path), png_path, 4), 0)) {
        return 1;
    }

    if (!expect_str("generic_prefer_png", default_routed_path("draft.bmp", "draft.png", 1, 1, 1), "draft.png") ||
        !expect_str("generic_prefer_bmp_existing", default_routed_path("draft.bmp", "draft.png", 0, 1, 1), "draft.bmp") ||
        !expect_str("generic_fallback_png", default_routed_path("draft.bmp", "draft.png", 0, 0, 1), "draft.png") ||
        !expect_str("generic_default_bmp", default_routed_path("draft.bmp", "draft.png", 0, 0, 0), "draft.bmp")) {
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

    choice = resolve_default_input_choice(0);
    if (!expect_int("path_exists_missing", path_exists("input.bmp"), 0) ||
        !expect_str("resolve_input_default_bmp", choice.path, "input.bmp") ||
        !expect_int("resolve_input_default_bmp_alternate", choice.used_alternate, 0)) {
        return 1;
    }

    choice = resolve_default_output_choice(0);
    if (!expect_str("resolve_output_default_bmp", choice.path, "output.bmp") ||
        !expect_int("resolve_output_default_bmp_alternate", choice.used_alternate, 0)) {
        return 1;
    }

    if (!touch_file("input.png") || !touch_file("output.png")) {
        fprintf(stderr, "touch png failed\n");
        return 1;
    }
    choice = resolve_default_input_choice(0);
    if (!expect_int("path_exists_png", path_exists("input.png"), 1) ||
        !expect_str("resolve_input_fallback_png", choice.path, "input.png") ||
        !expect_int("resolve_input_fallback_png_alternate", choice.used_alternate, 1)) {
        return 1;
    }

    choice = resolve_default_output_choice(0);
    if (!expect_str("resolve_output_reuse_png", choice.path, "output.png") ||
        !expect_int("resolve_output_reuse_png_alternate", choice.used_alternate, 1)) {
        return 1;
    }

    if (!touch_file("input.bmp") || !touch_file("output.bmp")) {
        fprintf(stderr, "touch bmp failed\n");
        return 1;
    }
    choice = resolve_default_input_choice(0);
    if (!expect_str("resolve_input_prefer_bmp", choice.path, "input.bmp") ||
        !expect_int("resolve_input_prefer_bmp_alternate", choice.used_alternate, 0)) {
        return 1;
    }

    choice = resolve_default_output_choice(0);
    if (!expect_str("resolve_output_prefer_bmp", choice.path, "output.bmp") ||
        !expect_int("resolve_output_prefer_bmp_alternate", choice.used_alternate, 0)) {
        return 1;
    }

    choice = resolve_default_input_choice(1);
    if (!expect_str("resolve_input_prefer_png", choice.path, "input.png") ||
        !expect_int("resolve_input_prefer_png_alternate", choice.used_alternate, 0)) {
        return 1;
    }

    choice = resolve_default_output_choice(1);
    if (!expect_str("resolve_output_prefer_png", choice.path, "output.png") ||
        !expect_int("resolve_output_prefer_png_alternate", choice.used_alternate, 0)) {
        return 1;
    }

    cleanup_artifacts();
    if (!touch_file("draft.png")) {
        fprintf(stderr, "touch draft png failed\n");
        return 1;
    }
    choice = resolve_routed_choice("draft.bmp", "draft.png", 0);
    if (!expect_str("resolve_generic_fallback_png", choice.path, "draft.png") ||
        !expect_int("resolve_generic_fallback_png_alternate", choice.used_alternate, 1)) {
        return 1;
    }

    if (!touch_file("draft.bmp")) {
        fprintf(stderr, "touch draft bmp failed\n");
        return 1;
    }
    choice = resolve_routed_choice("draft.bmp", "draft.png", 0);
    if (!expect_str("resolve_generic_prefer_bmp", choice.path, "draft.bmp") ||
        !expect_int("resolve_generic_prefer_bmp_alternate", choice.used_alternate, 0)) {
        return 1;
    }

    cleanup_artifacts();
    printf("path routing selftest ok\n");
    return 0;
}
