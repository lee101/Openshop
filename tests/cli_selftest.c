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

static int expect_stream_text(const char *label, FILE *stream, const char *expected) {
    char buffer[128] = {0};
    size_t bytes = 0;

    if (!stream || fflush(stream) != 0 || fseek(stream, 0, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to prepare stream\n", label);
        return 0;
    }

    bytes = fread(buffer, 1, sizeof(buffer) - 1, stream);
    buffer[bytes] = '\0';
    return expect_str(label, buffer, expected);
}

int main(void) {
    CliOptions options = {0};
    char *argv_default[] = {"openshop"};
    char *argv_custom_program[] = {"./bin/openshop-dev"};
    char usage[CLI_USAGE_BUFFER_SIZE] = {0};
    char exact_usage[96] = {0};
    int usage_size = 0;
    int custom_usage_size = 0;

    if (!expect_str("usage_suffix", cli_usage_suffix(),
                    "[input_path] [width height]\n"
                    "       or: WIDTH HEIGHT")) {
        return 1;
    }
    if (!expect_str("program_name_default", cli_program_name(argv_default), "openshop")) {
        return 1;
    }
    if (!expect_str("program_name_null_argv", cli_program_name(NULL), "openshop")) {
        return 1;
    }
    char *argv_null_program_name[] = {NULL};
    if (!expect_str("program_name_null_slot", cli_program_name(argv_null_program_name), "openshop")) {
        return 1;
    }
    char *argv_empty_program_name[] = {""};
    if (!expect_str("program_name_empty_slot", cli_program_name(argv_empty_program_name), "openshop")) {
        return 1;
    }
    usage_size = cli_usage_size(argv_default);
    if (!expect_int("usage_default_size_positive", usage_size > 0, 1)) {
        return 1;
    }
    if (!expect_int("usage_null_argv_size", cli_usage_size(NULL), usage_size)) {
        return 1;
    }
    custom_usage_size = cli_usage_size(argv_custom_program);
    if (!expect_int("usage_custom_size_greater", custom_usage_size > usage_size, 1)) {
        return 1;
    }
    if (!expect_int("usage_default_ok", format_cli_usage(usage, (int)sizeof(usage), argv_default), 1) ||
        !expect_str("usage_default_text", usage,
                    "Usage: openshop [input_path] [width height]\n"
                    "       or: WIDTH HEIGHT\n")) {
        return 1;
    }
    if (!expect_int("usage_default_size_matches", cli_usage_size(argv_default), (int)strlen(usage) + 1)) {
        return 1;
    }
    if (!expect_int("usage_null_argv_ok", format_cli_usage(usage, (int)sizeof(usage), NULL), 1) ||
        !expect_str("usage_null_argv_text", usage,
                    "Usage: openshop [input_path] [width height]\n"
                    "       or: WIDTH HEIGHT\n")) {
        return 1;
    }
    if (!expect_int("usage_null_buffer", format_cli_usage(NULL, (int)sizeof(usage), argv_default), 0)) {
        return 1;
    }
    if (!expect_int("usage_zero_size", format_cli_usage(usage, 0, argv_default), 0)) {
        return 1;
    }
    if (!expect_int("usage_tiny_buffer", format_cli_usage(usage, 8, argv_default), 0)) {
        return 1;
    }
    if (!expect_int("usage_exact_buffer_ok",
                    format_cli_usage(exact_usage, usage_size, argv_default), 1) ||
        !expect_str("usage_exact_buffer_text", exact_usage,
                    "Usage: openshop [input_path] [width height]\n"
                    "       or: WIDTH HEIGHT\n")) {
        return 1;
    }
    if (!expect_int("usage_custom_buffer_ok",
                    format_cli_usage(exact_usage, custom_usage_size, argv_custom_program), 1) ||
        !expect_str("usage_custom_buffer_text", exact_usage,
                    "Usage: ./bin/openshop-dev [input_path] [width height]\n"
                    "       or: WIDTH HEIGHT\n")) {
        return 1;
    }
    if (!expect_int("usage_custom_size_matches", cli_usage_size(argv_custom_program), (int)strlen(exact_usage) + 1)) {
        return 1;
    }
    if (!expect_int("usage_short_buffer",
                    format_cli_usage(exact_usage, usage_size - 1, argv_default), 0)) {
        return 1;
    }
    FILE *usage_stream = tmpfile();
    if (!expect_int("usage_stream_open", usage_stream != NULL, 1)) {
        return 1;
    }
    if (!expect_int("usage_write_default_ok", write_cli_usage(usage_stream, argv_default), 1) ||
        !expect_stream_text("usage_write_default_text", usage_stream,
                            "Usage: openshop [input_path] [width height]\n"
                            "       or: WIDTH HEIGHT\n")) {
        fclose(usage_stream);
        return 1;
    }
    fclose(usage_stream);
    FILE *custom_usage_stream = tmpfile();
    if (!expect_int("usage_custom_stream_open", custom_usage_stream != NULL, 1)) {
        return 1;
    }
    if (!expect_int("usage_write_custom_ok", write_cli_usage(custom_usage_stream, argv_custom_program), 1) ||
        !expect_stream_text("usage_write_custom_text", custom_usage_stream,
                            "Usage: ./bin/openshop-dev [input_path] [width height]\n"
                            "       or: WIDTH HEIGHT\n")) {
        fclose(custom_usage_stream);
        return 1;
    }
    fclose(custom_usage_stream);
    if (!expect_int("usage_write_null_stream", write_cli_usage(NULL, argv_default), 0)) {
        return 1;
    }

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
    char *argv_empty_input[] = {"openshop", ""};
    if (!expect_int("empty_input", parse_cli_args(2, argv_empty_input, &options), 0)) {
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
    char *argv_trailing_space_width[] = {"openshop", "art/scene.png", "640 ", "480"};
    if (!expect_int("trailing_space_width", parse_cli_args(4, argv_trailing_space_width, &options), 0)) {
        return 1;
    }
    char *argv_trailing_newline_height[] = {"openshop", "art/scene.png", "640", "480\n"};
    if (!expect_int("trailing_newline_height", parse_cli_args(4, argv_trailing_newline_height, &options), 0)) {
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
    if (!expect_int("zero_argc", parse_cli_args(0, argv_default, &options), 0)) {
        return 1;
    }
    if (!expect_int("null_argv", parse_cli_args(1, NULL, &options), 0)) {
        return 1;
    }
    char *argv_null_program[] = {NULL};
    if (!expect_int("null_program", parse_cli_args(1, argv_null_program, &options), 0)) {
        return 1;
    }
    char *argv_empty_program[] = {""};
    if (!expect_int("empty_program", parse_cli_args(1, argv_empty_program, &options), 0)) {
        return 1;
    }
    char *argv_null_input[] = {"openshop", NULL};
    if (!expect_int("null_input", parse_cli_args(2, argv_null_input, &options), 0)) {
        return 1;
    }
    char *argv_null_size_only_height[] = {"openshop", "640", NULL};
    if (!expect_int("null_size_only_height", parse_cli_args(3, argv_null_size_only_height, &options), 0)) {
        return 1;
    }
    char *argv_null_width[] = {"openshop", "art/scene.png", NULL, "480"};
    if (!expect_int("null_width", parse_cli_args(4, argv_null_width, &options), 0)) {
        return 1;
    }
    char *argv_null_height[] = {"openshop", "art/scene.png", "640", NULL};
    if (!expect_int("null_height", parse_cli_args(4, argv_null_height, &options), 0)) {
        return 1;
    }

    printf("cli selftest ok\n");
    return 0;
}
