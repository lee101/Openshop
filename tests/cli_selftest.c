#include "../src/cli.h"

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
    int matches = 0;
    if (!expected) {
        matches = (actual == NULL);
    } else if (actual) {
        matches = strcmp(actual, expected) == 0;
    }
    if (!matches) {
        fprintf(stderr, "%s: expected %s got %s\n", label, expected ? expected : "(null)", actual ? actual : "(null)");
        return 0;
    }
    return 1;
}

int main(void) {
    CliOptions options = {0};

    if (!expect_str("usage_suffix", cli_usage_suffix(),
                    "[input_path] [width height]\n"
                    "       or: WIDTH HEIGHT")) {
        return 1;
    }

    char *argv_default[] = {"openshop"};
    if (!expect_int("default_ok", parse_cli_args(1, argv_default, &options), 1) ||
        !expect_str("default_input", options.input_path, NULL) ||
        !expect_int("default_w", options.canvas_w, 0) ||
        !expect_int("default_h", options.canvas_h, 0)) {
        return 1;
    }

    char *argv_input[] = {"openshop", "art/scene.png"};
    if (!expect_int("input_ok", parse_cli_args(2, argv_input, &options), 1) ||
        !expect_str("input_path", options.input_path, "art/scene.png") ||
        !expect_int("input_w", options.canvas_w, 0) ||
        !expect_int("input_h", options.canvas_h, 0)) {
        return 1;
    }

    char *argv_numeric_input[] = {"openshop", "640"};
    if (!expect_int("numeric_input_ok", parse_cli_args(2, argv_numeric_input, &options), 1) ||
        !expect_str("numeric_input_path", options.input_path, "640") ||
        !expect_int("numeric_input_w", options.canvas_w, 0) ||
        !expect_int("numeric_input_h", options.canvas_h, 0)) {
        return 1;
    }

    char *argv_size_only[] = {"openshop", "1024", "768"};
    if (!expect_int("size_only_ok", parse_cli_args(3, argv_size_only, &options), 1) ||
        !expect_str("size_only_input", options.input_path, NULL) ||
        !expect_int("size_only_w", options.canvas_w, 1024) ||
        !expect_int("size_only_h", options.canvas_h, 768)) {
        return 1;
    }

    char *argv_size[] = {"openshop", "art/scene.png", "1024", "768"};
    if (!expect_int("size_ok", parse_cli_args(4, argv_size, &options), 1) ||
        !expect_str("size_input", options.input_path, "art/scene.png") ||
        !expect_int("size_w", options.canvas_w, 1024) ||
        !expect_int("size_h", options.canvas_h, 768)) {
        return 1;
    }

    char *argv_bad_w[] = {"openshop", "art/scene.png", "0", "768"};
    if (!expect_int("bad_w", parse_cli_args(4, argv_bad_w, &options), 0)) {
        return 1;
    }

    char *argv_bad_h[] = {"openshop", "art/scene.png", "640", "-1"};
    if (!expect_int("bad_h", parse_cli_args(4, argv_bad_h, &options), 0)) {
        return 1;
    }

    char *argv_missing_h[] = {"openshop", "art/scene.png", "640"};
    if (!expect_int("missing_h", parse_cli_args(3, argv_missing_h, &options), 0)) {
        return 1;
    }

    char *argv_bad_size_only[] = {"openshop", "art/scene.png", "768"};
    if (!expect_int("bad_size_only", parse_cli_args(3, argv_bad_size_only, &options), 0)) {
        return 1;
    }

    char *argv_extra[] = {"openshop", "art/scene.png", "640", "480", "extra"};
    if (!expect_int("extra_args", parse_cli_args(5, argv_extra, &options), 0)) {
        return 1;
    }

    char *argv_bad_width_token[] = {"openshop", "art/scene.png", "12x", "480"};
    if (!expect_int("bad_width_token", parse_cli_args(4, argv_bad_width_token, &options), 0)) {
        return 1;
    }

    char *argv_bad_height_token[] = {"openshop", "art/scene.png", "640", "48px"};
    if (!expect_int("bad_height_token", parse_cli_args(4, argv_bad_height_token, &options), 0)) {
        return 1;
    }

    char *argv_empty_width[] = {"openshop", "art/scene.png", "", "480"};
    if (!expect_int("empty_width", parse_cli_args(4, argv_empty_width, &options), 0)) {
        return 1;
    }

    char *argv_overflow_width[] = {"openshop", "art/scene.png", "2147483648", "480"};
    if (!expect_int("overflow_width", parse_cli_args(4, argv_overflow_width, &options), 0)) {
        return 1;
    }

    char *argv_overflow_height[] = {"openshop", "art/scene.png", "640", "999999999999999999999"};
    if (!expect_int("overflow_height", parse_cli_args(4, argv_overflow_height, &options), 0)) {
        return 1;
    }

    char *argv_leading_space_width[] = {"openshop", "art/scene.png", " 640", "480"};
    if (!expect_int("leading_space_width", parse_cli_args(4, argv_leading_space_width, &options), 0)) {
        return 1;
    }

    char *argv_leading_tab_height[] = {"openshop", "art/scene.png", "640", "\t480"};
    if (!expect_int("leading_tab_height", parse_cli_args(4, argv_leading_tab_height, &options), 0)) {
        return 1;
    }

    char *argv_plus_prefixed[] = {"openshop", "art/scene.png", "+640", "480"};
    if (!expect_int("plus_prefixed", parse_cli_args(4, argv_plus_prefixed, &options), 1) ||
        !expect_int("plus_prefixed_w", options.canvas_w, 640) ||
        !expect_int("plus_prefixed_h", options.canvas_h, 480)) {
        return 1;
    }

    if (!expect_int("null_options", parse_cli_args(1, argv_default, NULL), 0)) {
        return 1;
    }

    printf("cli selftest ok\n");
    return 0;
}
