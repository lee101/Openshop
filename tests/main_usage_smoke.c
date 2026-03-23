#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int app_run_called = 0;
static const char *last_input_path = NULL;
static int last_canvas_w = 0;
static int last_canvas_h = 0;
static int app_run_result = 0;
static const char *expected_usage_text =
    "Usage: openshop [input_path] [width height]\n"
    "       or: WIDTH HEIGHT\n";

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

int main(void) {
    char stderr_text[256] = {0};
    int exit_code = 0;

    char *argv_invalid[] = {"openshop", "art/scene.png", "640"};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(3, argv_invalid, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("invalid_exit", exit_code, 1) ||
        !expect_int("invalid_app_run_called", app_run_called, 0) ||
        !expect_str("invalid_usage_text", stderr_text, expected_usage_text)) {
        return 1;
    }

    char *argv_bad_size_only[] = {"openshop", "art/scene.png", "768"};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(3, argv_bad_size_only, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("bad_size_only_exit", exit_code, 1) ||
        !expect_int("bad_size_only_app_run_called", app_run_called, 0) ||
        !expect_str("bad_size_only_usage_text", stderr_text, expected_usage_text)) {
        return 1;
    }

    char *argv_extra[] = {"openshop", "art/scene.png", "640", "480", "extra"};
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(5, argv_extra, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("extra_args_exit", exit_code, 1) ||
        !expect_int("extra_args_app_run_called", app_run_called, 0) ||
        !expect_str("extra_args_usage_text", stderr_text, expected_usage_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(1, NULL, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("null_argv_exit", exit_code, 1) ||
        !expect_int("null_argv_app_run_called", app_run_called, 0) ||
        !expect_str("null_argv_usage_text", stderr_text, expected_usage_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_zero_argc[] = {"openshop"};
    if (!capture_main_stderr(0, argv_zero_argc, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("zero_argc_exit", exit_code, 1) ||
        !expect_int("zero_argc_app_run_called", app_run_called, 0) ||
        !expect_str("zero_argc_usage_text", stderr_text, expected_usage_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_empty_program[] = {""};
    if (!capture_main_stderr(1, argv_empty_program, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("empty_program_exit", exit_code, 1) ||
        !expect_int("empty_program_app_run_called", app_run_called, 0) ||
        !expect_str("empty_program_usage_text", stderr_text, expected_usage_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_empty_input[] = {"openshop", ""};
    if (!capture_main_stderr(2, argv_empty_input, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("empty_input_exit", exit_code, 1) ||
        !expect_int("empty_input_app_run_called", app_run_called, 0) ||
        !expect_str("empty_input_usage_text", stderr_text, expected_usage_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_bad_width_token[] = {"openshop", "art/scene.png", "12x", "480"};
    if (!capture_main_stderr(4, argv_bad_width_token, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("bad_width_token_exit", exit_code, 1) ||
        !expect_int("bad_width_token_app_run_called", app_run_called, 0) ||
        !expect_str("bad_width_token_usage_text", stderr_text, expected_usage_text)) {
        return 1;
    }

    reset_app_state(0, NULL, 123, 456, stderr_text);
    char *argv_default[] = {"openshop"};
    if (!capture_main_stderr(1, argv_default, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("default_exit", exit_code, 0) ||
        !expect_int("default_app_run_called", app_run_called, 1) ||
        !expect_str("default_input_path", last_input_path, NULL) ||
        !expect_int("default_canvas_w", last_canvas_w, 0) ||
        !expect_int("default_canvas_h", last_canvas_h, 0) ||
        !expect_str("default_exit_text", stderr_text, "")) {
        return 1;
    }

    reset_app_state(7, NULL, 0, 0, stderr_text);
    char *argv_size_only[] = {"openshop", "640", "480"};
    if (!capture_main_stderr(3, argv_size_only, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("valid_exit", exit_code, 7) ||
        !expect_int("valid_app_run_called", app_run_called, 1) ||
        !expect_str("valid_input_path", last_input_path, NULL) ||
        !expect_int("valid_canvas_w", last_canvas_w, 640) ||
        !expect_int("valid_canvas_h", last_canvas_h, 480) ||
        !expect_str("valid_exit_text", stderr_text, "App exited with code 7\n")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_input_size[] = {"openshop", "art/scene.png", "320", "240"};
    if (!capture_main_stderr(4, argv_input_size, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("input_exit", exit_code, 0) ||
        !expect_int("input_app_run_called", app_run_called, 1) ||
        !expect_str("input_path_forwarded", last_input_path, "art/scene.png") ||
        !expect_int("input_canvas_w", last_canvas_w, 320) ||
        !expect_int("input_canvas_h", last_canvas_h, 240) ||
        !expect_str("input_exit_text", stderr_text, "")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_input_only[] = {"openshop", "art/input.png"};
    if (!capture_main_stderr(2, argv_input_only, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("input_only_exit", exit_code, 0) ||
        !expect_int("input_only_app_run_called", app_run_called, 1) ||
        !expect_str("input_only_path_forwarded", last_input_path, "art/input.png") ||
        !expect_int("input_only_canvas_w", last_canvas_w, 0) ||
        !expect_int("input_only_canvas_h", last_canvas_h, 0) ||
        !expect_str("input_only_exit_text", stderr_text, "")) {
        return 1;
    }

    reset_app_state(0, NULL, 0, 0, stderr_text);
    char *argv_numeric_input[] = {"openshop", "640"};
    if (!capture_main_stderr(2, argv_numeric_input, stderr_text, sizeof(stderr_text), &exit_code) ||
        !expect_int("numeric_input_exit", exit_code, 0) ||
        !expect_int("numeric_input_app_run_called", app_run_called, 1) ||
        !expect_str("numeric_input_path_forwarded", last_input_path, "640") ||
        !expect_int("numeric_input_canvas_w", last_canvas_w, 0) ||
        !expect_int("numeric_input_canvas_h", last_canvas_h, 0) ||
        !expect_str("numeric_input_exit_text", stderr_text, "")) {
        return 1;
    }

    printf("main usage smoke ok\n");
    return 0;
}
