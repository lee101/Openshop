#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int app_run_called = 0;
static const char *last_input_path = NULL;
static int last_canvas_w = 0;
static int last_canvas_h = 0;
static int app_run_result = 0;
static const char *default_scene_path = "art/scene.png";
static const char *default_input_path = "art/input.png";
static const char *custom_program_name = "./bin/openshop-dev";
static const char *custom_input_path = "art/custom.png";
static const char *custom_numeric_input = "640";
static const char *default_numeric_input = "640";
static const char *default_size_only_width = "640";
static const char *default_size_only_height = "480";
static const char *default_plus_prefixed_size_only_width = "+640";
static const char *default_input_size_width = "320";
static const char *default_input_size_height = "240";
static const char *default_plus_prefixed_input_size_width = "+640";
static const char *custom_size_only_width = "800";
static const char *custom_size_only_height = "600";
static const char *custom_plus_prefixed_size_only_width = "+800";
static const char *custom_input_size_width = "320";
static const char *custom_input_size_height = "240";
static const char *custom_plus_prefixed_input_size_width = "+320";

int app_run(const char *input_path, int canvas_w, int canvas_h) {
    app_run_called += 1;
    last_input_path = input_path;
    last_canvas_w = canvas_w;
    last_canvas_h = canvas_h;
    return app_run_result;
}

#define main openshop_main
#include "../src/main.c"
#undef main

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

static int capture_main_stderr(int argc, char **argv, char *buffer, size_t buffer_size, int *exit_code) {
    FILE *tmp = NULL;
    int saved_stderr = -1;
    size_t bytes = 0;

    if (!buffer || buffer_size == 0 || !exit_code) {
        return 0;
    }

    tmp = tmpfile();
    if (!tmp) {
        return 0;
    }

    fflush(stderr);
    saved_stderr = dup(fileno(stderr));
    if (saved_stderr < 0 || dup2(fileno(tmp), fileno(stderr)) < 0) {
        if (saved_stderr >= 0) {
            close(saved_stderr);
        }
        fclose(tmp);
        return 0;
    }

    *exit_code = openshop_main(argc, argv);

    fflush(stderr);
    if (dup2(saved_stderr, fileno(stderr)) < 0) {
        close(saved_stderr);
        fclose(tmp);
        return 0;
    }
    close(saved_stderr);

    if (fseek(tmp, 0, SEEK_SET) != 0) {
        fclose(tmp);
        return 0;
    }

    bytes = fread(buffer, 1, buffer_size - 1, tmp);
    buffer[bytes] = '\0';
    fclose(tmp);
    return 1;
}

static void reset_app_state(int result, const char *input_path, int canvas_w, int canvas_h, char *stderr_text) {
    app_run_result = result;
    app_run_called = 0;
    last_input_path = input_path;
    last_canvas_w = canvas_w;
    last_canvas_h = canvas_h;
    if (stderr_text) {
        stderr_text[0] = '\0';
    }
}

static int expect_successful_run(const char *label_prefix, int exit_code, int expected_exit_code,
                                 const char *expected_input_path, int expected_canvas_w,
                                 int expected_canvas_h, const char *actual_stderr,
                                 const char *expected_stderr) {
    char label[64] = {0};

    snprintf(label, sizeof(label), "%s_exit", label_prefix);
    if (!expect_int(label, exit_code, expected_exit_code)) {
        return 0;
    }
    snprintf(label, sizeof(label), "%s_app_run_called", label_prefix);
    if (!expect_int(label, app_run_called, 1)) {
        return 0;
    }
    snprintf(label, sizeof(label), "%s_input_path", label_prefix);
    if (!expect_str(label, last_input_path, expected_input_path)) {
        return 0;
    }
    snprintf(label, sizeof(label), "%s_canvas_w", label_prefix);
    if (!expect_int(label, last_canvas_w, expected_canvas_w)) {
        return 0;
    }
    snprintf(label, sizeof(label), "%s_canvas_h", label_prefix);
    if (!expect_int(label, last_canvas_h, expected_canvas_h)) {
        return 0;
    }
    snprintf(label, sizeof(label), "%s_stderr", label_prefix);
    if (!expect_str(label, actual_stderr, expected_stderr)) {
        return 0;
    }
    return 1;
}

static int expect_invalid_run(const char *label_prefix, int exit_code, const char *actual_stderr) {
    char label[64] = {0};
    char expected_usage_text[CLI_USAGE_BUFFER_SIZE] = {0};

    snprintf(label, sizeof(label), "%s_exit", label_prefix);
    if (!expect_int(label, exit_code, 1)) {
        return 0;
    }
    snprintf(label, sizeof(label), "%s_app_run_called", label_prefix);
    if (!expect_int(label, app_run_called, 0)) {
        return 0;
    }
    if (!format_cli_usage(expected_usage_text, sizeof(expected_usage_text), NULL)) {
        fprintf(stderr, "%s_usage_build: failed to format usage text\n", label_prefix);
        return 0;
    }
    snprintf(label, sizeof(label), "%s_usage_text", label_prefix);
    if (!expect_str(label, actual_stderr, expected_usage_text)) {
        return 0;
    }
    return 1;
}

static int expect_invalid_run_with_usage(const char *label_prefix, int exit_code,
                                         const char *actual_stderr, const char *expected_stderr) {
    char label[64] = {0};

    snprintf(label, sizeof(label), "%s_exit", label_prefix);
    if (!expect_int(label, exit_code, 1)) {
        return 0;
    }
    snprintf(label, sizeof(label), "%s_app_run_called", label_prefix);
    if (!expect_int(label, app_run_called, 0)) {
        return 0;
    }
    snprintf(label, sizeof(label), "%s_usage_text", label_prefix);
    if (!expect_str(label, actual_stderr, expected_stderr)) {
        return 0;
    }
    return 1;
}

static int format_custom_usage_text(char *buffer, size_t buffer_size) {
    char *argv[] = {(char *)custom_program_name};

    if (!buffer || buffer_size == 0) {
        return 0;
    }
    return format_cli_usage(buffer, (int)buffer_size, argv);
}

int main(void) {
    char stderr_text[256] = {0};
    char custom_usage_text[256] = {0};
    int exit_code = 0;

    if (!format_custom_usage_text(custom_usage_text, sizeof(custom_usage_text))) {
        return 1;
    }

    char *argv_invalid[] = {"openshop", (char *)default_scene_path, (char *)default_size_only_width};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(3, argv_invalid, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("invalid", exit_code, stderr_text)) {
        return 1;
    }

    char *argv_bad_size_only[] = {"openshop", (char *)default_scene_path, "768"};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(3, argv_bad_size_only, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("bad_size_only", exit_code, stderr_text)) {
        return 1;
    }

    char *argv_custom_program_invalid[] = {(char *)custom_program_name, (char *)default_scene_path, "768"};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(3, argv_custom_program_invalid, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run_with_usage("custom_program_invalid", exit_code, stderr_text, custom_usage_text)) {
        return 1;
    }

    char *argv_missing_h[] = {"openshop", (char *)default_scene_path, (char *)default_size_only_width};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(3, argv_missing_h, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("missing_h", exit_code, stderr_text)) {
        return 1;
    }

    char *argv_null_size_only_height[] = {"openshop", (char *)default_size_only_width, NULL};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(3, argv_null_size_only_height, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("null_size_only_height", exit_code, stderr_text)) {
        return 1;
    }

    char *argv_extra[] = {"openshop", (char *)default_scene_path, (char *)default_size_only_width, (char *)default_size_only_height, "extra"};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(5, argv_extra, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("extra_args", exit_code, stderr_text)) {
        return 1;
    }

    char *argv_custom_program_extra[] = {
        (char *)custom_program_name, (char *)default_scene_path,
        (char *)default_size_only_width, (char *)default_size_only_height, "extra"};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(5, argv_custom_program_extra, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run_with_usage("custom_program_extra_args", exit_code, stderr_text, custom_usage_text)) {
        return 1;
    }

    char *argv_custom_program_bad_width[] = {
        (char *)custom_program_name, (char *)default_scene_path, "12x", (char *)default_size_only_height};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(4, argv_custom_program_bad_width, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run_with_usage("custom_program_bad_width", exit_code, stderr_text, custom_usage_text)) {
        return 1;
    }

    char *argv_custom_program_overflow_width[] = {
        (char *)custom_program_name, (char *)default_scene_path, "2147483648", (char *)default_size_only_height};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(4, argv_custom_program_overflow_width, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run_with_usage("custom_program_overflow_width", exit_code, stderr_text, custom_usage_text)) {
        return 1;
    }

    char *argv_custom_program_bad_height[] = {
        (char *)custom_program_name, (char *)default_scene_path, (char *)default_size_only_width, "48px"};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(4, argv_custom_program_bad_height, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run_with_usage("custom_program_bad_height", exit_code, stderr_text, custom_usage_text)) {
        return 1;
    }

    char *argv_custom_program_null_height[] = {
        (char *)custom_program_name, (char *)default_scene_path, (char *)default_size_only_width, NULL};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(4, argv_custom_program_null_height, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run_with_usage("custom_program_null_height", exit_code, stderr_text, custom_usage_text)) {
        return 1;
    }

    char *argv_custom_program_null_width[] = {
        (char *)custom_program_name, (char *)default_scene_path, NULL, (char *)default_size_only_height};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(4, argv_custom_program_null_width, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run_with_usage("custom_program_null_width", exit_code, stderr_text, custom_usage_text)) {
        return 1;
    }

    char *argv_custom_program_overflow_height[] = {
        (char *)custom_program_name, (char *)default_scene_path,
        (char *)default_size_only_width, "999999999999999999999"};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(4, argv_custom_program_overflow_height, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run_with_usage("custom_program_overflow_height", exit_code, stderr_text, custom_usage_text)) {
        return 1;
    }

    char *argv_custom_program_leading_space_width[] = {
        (char *)custom_program_name, (char *)default_scene_path, " 640", (char *)default_size_only_height};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(4, argv_custom_program_leading_space_width, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run_with_usage("custom_program_leading_space_width", exit_code, stderr_text, custom_usage_text)) {
        return 1;
    }

    char *argv_custom_program_trailing_newline_height[] = {
        (char *)custom_program_name, (char *)default_scene_path, (char *)default_size_only_width, "480\n"};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(4, argv_custom_program_trailing_newline_height, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run_with_usage("custom_program_trailing_newline_height", exit_code, stderr_text, custom_usage_text)) {
        return 1;
    }

    char *argv_custom_program_leading_tab_height[] = {
        (char *)custom_program_name, (char *)default_scene_path, (char *)default_size_only_width, "\t480"};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(4, argv_custom_program_leading_tab_height, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run_with_usage("custom_program_leading_tab_height", exit_code, stderr_text, custom_usage_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(1, NULL, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("null_argv", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_null_program[] = {NULL};
    if (!capture_main_stderr(1, argv_null_program, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("null_program", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_zero_argc[] = {"openshop"};
    if (!capture_main_stderr(0, argv_zero_argc, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("zero_argc", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_empty_program[] = {""};
    if (!capture_main_stderr(1, argv_empty_program, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("empty_program", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_empty_input[] = {"openshop", ""};
    if (!capture_main_stderr(2, argv_empty_input, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("empty_input", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_null_input[] = {"openshop", NULL};
    if (!capture_main_stderr(2, argv_null_input, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("null_input", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_bad_width_value[] = {"openshop", (char *)default_scene_path, "0", "768"};
    if (!capture_main_stderr(4, argv_bad_width_value, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("bad_width_value", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_bad_width_token[] = {"openshop", (char *)default_scene_path, "12x", "480"};
    if (!capture_main_stderr(4, argv_bad_width_token, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("bad_width_token", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_bad_height_value[] = {"openshop", (char *)default_scene_path, (char *)default_size_only_width, "-1"};
    if (!capture_main_stderr(4, argv_bad_height_value, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("bad_height_value", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_empty_width[] = {"openshop", (char *)default_scene_path, "", "480"};
    if (!capture_main_stderr(4, argv_empty_width, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("empty_width", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_null_width[] = {"openshop", (char *)default_scene_path, NULL, "480"};
    if (!capture_main_stderr(4, argv_null_width, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("null_width", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_null_height[] = {"openshop", (char *)default_scene_path, (char *)default_size_only_width, NULL};
    if (!capture_main_stderr(4, argv_null_height, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("null_height", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_overflow_width[] = {"openshop", (char *)default_scene_path, "2147483648", "480"};
    if (!capture_main_stderr(4, argv_overflow_width, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("overflow_width", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_bad_height_token[] = {"openshop", (char *)default_scene_path, (char *)default_size_only_width, "48px"};
    if (!capture_main_stderr(4, argv_bad_height_token, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("bad_height_token", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_overflow_height[] = {"openshop", (char *)default_scene_path, (char *)default_size_only_width, "999999999999999999999"};
    if (!capture_main_stderr(4, argv_overflow_height, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("overflow_height", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_leading_space_width[] = {"openshop", (char *)default_scene_path, " 640", "480"};
    if (!capture_main_stderr(4, argv_leading_space_width, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("leading_space_width", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_trailing_newline_height[] = {"openshop", (char *)default_scene_path, (char *)default_size_only_width, "480\n"};
    if (!capture_main_stderr(4, argv_trailing_newline_height, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("trailing_newline_height", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_leading_tab_height[] = {"openshop", (char *)default_scene_path, (char *)default_size_only_width, "\t480"};
    if (!capture_main_stderr(4, argv_leading_tab_height, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("leading_tab_height", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_trailing_space_width[] = {"openshop", (char *)default_scene_path, "640 ", "480"};
    if (!capture_main_stderr(4, argv_trailing_space_width, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_invalid_run("trailing_space_width", exit_code, stderr_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 123, 456, stderr_text);
    char *argv_default[] = {"openshop"};
    if (!capture_main_stderr(1, argv_default, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("default", exit_code, 0, NULL, 0, 0, stderr_text, "")) {
        return 1;
    }

    reset_app_state(7, NULL, 0, 0, stderr_text);
    char *argv_size_only[] = {"openshop", (char *)default_size_only_width, (char *)default_size_only_height};
    if (!capture_main_stderr(3, argv_size_only, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("size_only", exit_code, 7, NULL, 640, 480, stderr_text,
                               "App exited with code 7\n")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_plus_prefixed_size_only[] = {"openshop", (char *)default_plus_prefixed_size_only_width, (char *)default_size_only_height};
    if (!capture_main_stderr(3, argv_plus_prefixed_size_only, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("plus_prefixed_size_only", exit_code, 0, NULL, 640, 480, stderr_text, "")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_custom_program_input[] = {(char *)custom_program_name, (char *)custom_input_path};
    if (!capture_main_stderr(2, argv_custom_program_input, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("custom_program_input", exit_code, 0, custom_input_path, 0, 0, stderr_text, "")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_custom_program_default[] = {(char *)custom_program_name};
    if (!capture_main_stderr(1, argv_custom_program_default, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("custom_program_default", exit_code, 0, NULL, 0, 0, stderr_text, "")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_custom_program_numeric_input[] = {(char *)custom_program_name, (char *)custom_numeric_input};
    if (!capture_main_stderr(2, argv_custom_program_numeric_input, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("custom_program_numeric_input", exit_code, 0, custom_numeric_input, 0, 0, stderr_text, "")) {
        return 1;
    }

    reset_app_state(5, NULL, 0, 0, stderr_text);
    char *argv_custom_program_nonzero[] = {(char *)custom_program_name, (char *)custom_input_path};
    if (!capture_main_stderr(2, argv_custom_program_nonzero, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("custom_program_nonzero", exit_code, 5, custom_input_path, 0, 0, stderr_text,
                               "App exited with code 5\n")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_custom_program_size_only[] = {
        (char *)custom_program_name, (char *)custom_size_only_width, (char *)custom_size_only_height};
    if (!capture_main_stderr(3, argv_custom_program_size_only, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("custom_program_size_only", exit_code, 0, NULL, 800, 600, stderr_text, "")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_custom_program_plus_prefixed_size_only[] = {
        (char *)custom_program_name, (char *)custom_plus_prefixed_size_only_width, (char *)custom_size_only_height};
    if (!capture_main_stderr(3, argv_custom_program_plus_prefixed_size_only, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("custom_program_plus_prefixed_size_only", exit_code, 0, NULL, 800, 600, stderr_text, "")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_custom_program_input_size[] = {
        (char *)custom_program_name, (char *)custom_input_path,
        (char *)custom_input_size_width, (char *)custom_input_size_height};
    if (!capture_main_stderr(4, argv_custom_program_input_size, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("custom_program_input_size", exit_code, 0, custom_input_path, 320, 240, stderr_text, "")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_custom_program_plus_prefixed[] = {
        (char *)custom_program_name, (char *)custom_input_path,
        (char *)custom_plus_prefixed_input_size_width, (char *)custom_input_size_height};
    if (!capture_main_stderr(4, argv_custom_program_plus_prefixed, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("custom_program_plus_prefixed", exit_code, 0, custom_input_path, 320, 240, stderr_text, "")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_input_size[] = {"openshop", (char *)default_scene_path, (char *)default_input_size_width, (char *)default_input_size_height};
    if (!capture_main_stderr(4, argv_input_size, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("input_size", exit_code, 0, default_scene_path, 320, 240, stderr_text, "")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_plus_prefixed[] = {"openshop", (char *)default_scene_path, (char *)default_plus_prefixed_input_size_width, (char *)default_size_only_height};
    if (!capture_main_stderr(4, argv_plus_prefixed, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("plus_prefixed", exit_code, 0, default_scene_path, 640, 480, stderr_text, "")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_input_only[] = {"openshop", (char *)default_input_path};
    if (!capture_main_stderr(2, argv_input_only, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("input_only", exit_code, 0, default_input_path, 0, 0, stderr_text, "")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_numeric_input[] = {"openshop", (char *)default_numeric_input};
    if (!capture_main_stderr(2, argv_numeric_input, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_successful_run("numeric_input", exit_code, 0, default_numeric_input, 0, 0, stderr_text, "")) {
        return 1;
    }

    printf("main usage smoke ok\n");
    return 0;
}
