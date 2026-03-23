#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char *const no_input_path = NULL;

static int app_run_called = 0;
static const char *last_input_path = no_input_path;
static int last_canvas_w = 0;
static int last_canvas_h = 0;
static int app_run_result = 0;
static const size_t main_smoke_buffer_size = 256;
static const int default_size_only_canvas_w = 640;
static const int default_size_only_canvas_h = 480;
static const int custom_size_only_canvas_w = 800;
static const int custom_size_only_canvas_h = 600;
static const int input_size_canvas_w = 320;
static const int input_size_canvas_h = 240;
static const int no_input_canvas_w = 0;
static const int no_input_canvas_h = 0;
static const int zero_argc = 0;
static const int startup_argc = 1;
static const int input_only_argc = 2;
static const int size_only_argc = 3;
static const int input_size_argc = 4;
static const int extra_argv_argc = 5;
static const int invalid_exit_code = 1;
static const int success_exit_code = 0;
static const int app_run_not_called = 0;
static const int app_run_called_once = 1;
static const int app_exit_code_7 = 7;
static const int app_exit_code_5 = 5;
static char **const default_usage_argv = NULL;
static const char *empty_token = "";
static const char *empty_stderr = "";
static const char *default_program_name = "openshop";
static const char *default_scene_path = "art/scene.png";
static const char *default_input_path = "art/input.png";
static const char *custom_program_name = "./bin/openshop-dev";
static const char *custom_input_path = "art/custom.png";
static const char *numeric_input_token = "640";
static const char *default_size_only_width = "640";
static const char *default_size_only_height = "480";
static const char *zero_token = "0";
static const char *invalid_probe_token = "768";
static const char *extra_arg_token = "extra";
static const char *negative_size_only_width = "-640";
static const char *negative_size_only_height = "-480";
static const char *negative_input_size_height = "-1";
static const char *bad_width_token = "12x";
static const char *bad_height_token = "48px";
static const char *overflow_width_token = "2147483648";
static const char *overflow_height_token = "999999999999999999999";
static const char *leading_space_width_token = " 640";
static const char *trailing_space_width_token = "640 ";
static const char *trailing_newline_height_token = "480\n";
static const char *leading_tab_height_token = "\t480";
static const char *default_plus_prefixed_width_token = "+640";
static const char *input_size_width_token = "320";
static const char *input_size_height_token = "240";
static const char *custom_size_only_width = "800";
static const char *custom_size_only_height = "600";
static const char *custom_plus_prefixed_size_only_width = "+800";
static const char *custom_plus_prefixed_input_size_width = "+320";
static const char *app_exit_code_7_stderr = "App exited with code 7\n";
static const char *app_exit_code_5_stderr = "App exited with code 5\n";
static const char *const null_token = NULL;

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

#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

enum { expectation_label_size = 64 };

static int expect_int(const char *label, int actual, int expected) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d got %d\n", label, expected, actual);
        return 0;
    }
    return 1;
}

static int expect_str(const char *label, const char *actual, const char *expected) {
    if (!expected) {
        if (actual == NULL) {
            return 1;
        }
    } else if (actual && strcmp(actual, expected) == 0) {
        return 1;
    }

    {
        fprintf(stderr, "%s: expected %s got %s\n", label, expected ? expected : "(null)", actual ? actual : "(null)");
        return 0;
    }
}

static int capture_main_stderr(int argc, char **argv, char *buffer, size_t buffer_size, int *exit_code) {
    FILE *tmp;
    int saved_stderr;
    size_t bytes;

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
    char label[expectation_label_size] = {0};

    snprintf(label, sizeof(label), "%s_exit", label_prefix);
    if (!expect_int(label, exit_code, expected_exit_code)) {
        return 0;
    }
    snprintf(label, sizeof(label), "%s_app_run_called", label_prefix);
    if (!expect_int(label, app_run_called, app_run_called_once)) {
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
    char label[expectation_label_size] = {0};
    char expected_usage_text[CLI_USAGE_BUFFER_SIZE] = {0};

    snprintf(label, sizeof(label), "%s_exit", label_prefix);
    if (!expect_int(label, exit_code, invalid_exit_code)) {
        return 0;
    }
    snprintf(label, sizeof(label), "%s_app_run_called", label_prefix);
    if (!expect_int(label, app_run_called, app_run_not_called)) {
        return 0;
    }
    if (!format_cli_usage(expected_usage_text, sizeof(expected_usage_text), default_usage_argv)) {
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
    char label[expectation_label_size] = {0};

    snprintf(label, sizeof(label), "%s_exit", label_prefix);
    if (!expect_int(label, exit_code, invalid_exit_code)) {
        return 0;
    }
    snprintf(label, sizeof(label), "%s_app_run_called", label_prefix);
    if (!expect_int(label, app_run_called, app_run_not_called)) {
        return 0;
    }
    snprintf(label, sizeof(label), "%s_usage_text", label_prefix);
    if (!expect_str(label, actual_stderr, expected_stderr)) {
        return 0;
    }
    return 1;
}

static int expect_invalid_main_result(const char *label_prefix, int argc, char **argv,
                                      char *stderr_text, size_t stderr_size, int *exit_code,
                                      const char *expected_stderr) {
    reset_app_state(success_exit_code, no_input_path, no_input_canvas_w, no_input_canvas_h, stderr_text);
    if (!capture_main_stderr(argc, argv, stderr_text, stderr_size, exit_code)) {
        return 0;
    }

    if (expected_stderr) {
        return expect_invalid_run_with_usage(label_prefix, *exit_code, stderr_text, expected_stderr);
    }

    return expect_invalid_run(label_prefix, *exit_code, stderr_text);
}

struct invalid_size_token_case {
    const char *label_prefix;
    const char *token;
};

enum size_only_argv_index {
    size_only_width_argv_index = 1,
    size_only_height_argv_index = 2,
};

#define INVALID_SIZE_TOKEN_CASE(label, token) {label, token}

struct invalid_input_size_case {
    const char *label_prefix;
    const char *width_token;
    const char *height_token;
};

#define INVALID_INPUT_WIDTH_CASE(label, width) {label, width, default_size_only_height}
#define INVALID_INPUT_HEIGHT_CASE(label, height) {label, default_size_only_width, height}
#define INVALID_INPUT_WIDTH_VALUE_CASE(label, width) {label, width, invalid_probe_token}

enum invalid_argv_state {
    argv_is_present = 0,
    argv_is_null = 1,
};

struct invalid_argv_case {
    const char *label_prefix;
    int argc;
    enum invalid_argv_state argv_state;
    const char *expected_usage_text;
    const char *argv[5];
};

static const char *const empty_argv_tail_2[] = {NULL, NULL};
static const char *const empty_argv_tail_3[] = {NULL, NULL, NULL};
static const char *const empty_argv_tail_4[] = {NULL, NULL, NULL, NULL};
static const char *const empty_argv_tail_5[] = {NULL, NULL, NULL, NULL, NULL};
static const char *const no_size_tokens[] = {NULL, NULL};
static const char *const no_usage_text = NULL;
static char **const null_argv = NULL;

#define INVALID_NULL_ARGV_ONLY_CASE(label, argc_value, usage_text) \
    {label, argc_value, argv_is_null, usage_text, {empty_argv_tail_5[0], empty_argv_tail_5[1], empty_argv_tail_5[2], empty_argv_tail_5[3], empty_argv_tail_5[4]}}
#define INVALID_SIZE_ONLY_ARGV_CASE(label, usage_text, program, width) \
    {label, size_only_argc, argv_is_present, usage_text, {program, default_scene_path, width, empty_argv_tail_2[0], empty_argv_tail_2[1]}}
#define INVALID_EXTRA_ARGV_CASE(label, usage_text, program) \
    {label, extra_argv_argc, argv_is_present, usage_text, {program, default_scene_path, default_size_only_width, default_size_only_height, extra_arg_token}}
#define INVALID_INPUT_ARGV_CASE(label, usage_text, program, input) \
    {label, input_only_argc, argv_is_present, usage_text, {program, input, empty_argv_tail_3[0], empty_argv_tail_3[1], empty_argv_tail_3[2]}}
#define INVALID_PROGRAM_ONLY_ARGV_CASE(label, argc_value, usage_text, program) \
    {label, argc_value, argv_is_present, usage_text, {program, empty_argv_tail_4[0], empty_argv_tail_4[1], empty_argv_tail_4[2], empty_argv_tail_4[3]}}

struct success_case {
    const char *label_prefix;
    int argc;
    const char *program_name;
    const char *input_token;
    const char *width_token;
    const char *height_token;
    int result;
    int expected_exit_code;
    const char *expected_input_path;
    int expected_canvas_w;
    int expected_canvas_h;
    const char *expected_stderr;
};

#define SUCCESS_INPUT_CASE(label, program, input, result, expected_exit, expected_stderr) \
    {label, input_only_argc, program, input, no_size_tokens[0], no_size_tokens[1], result, expected_exit, input, no_input_canvas_w, no_input_canvas_h, expected_stderr}
#define SUCCESS_INPUT_CASE_EMPTY_STDERR(label, program, input, result, expected_exit) \
    SUCCESS_INPUT_CASE(label, program, input, result, expected_exit, empty_stderr)
#define SUCCESS_STARTUP_CASE_EMPTY_STDERR(label, program, result, expected_exit) \
    {label, startup_argc, program, no_input_path, no_size_tokens[0], no_size_tokens[1], result, expected_exit, no_input_path, no_input_canvas_w, no_input_canvas_h, empty_stderr}
#define SUCCESS_SIZE_ONLY_CASE(label, program, width, height, result, expected_exit, canvas_w, canvas_h, expected_stderr) \
    {label, size_only_argc, program, width, height, no_input_path, result, expected_exit, no_input_path, canvas_w, canvas_h, expected_stderr}
#define SUCCESS_SIZE_ONLY_CASE_EMPTY_STDERR(label, program, width, height, result, expected_exit, canvas_w, canvas_h) \
    SUCCESS_SIZE_ONLY_CASE(label, program, width, height, result, expected_exit, canvas_w, canvas_h, empty_stderr)
#define SUCCESS_INPUT_SIZE_CASE(label, program, input, width, height, result, expected_exit, canvas_w, canvas_h, expected_stderr) \
    {label, input_size_argc, program, input, width, height, result, expected_exit, input, canvas_w, canvas_h, expected_stderr}
#define SUCCESS_INPUT_SIZE_CASE_EMPTY_STDERR(label, program, input, width, height, result, expected_exit, canvas_w, canvas_h) \
    SUCCESS_INPUT_SIZE_CASE(label, program, input, width, height, result, expected_exit, canvas_w, canvas_h, empty_stderr)

static void fill_argv(char **dst, const char *const *src, size_t count) {
    size_t i;

    for (i = 0; i < count; i += 1) {
        dst[i] = (char *)src[i];
    }
}

static void fill_argv_with_override(char **dst, const char *const *src, size_t count,
                                    enum size_only_argv_index override_index, const char *override_value) {
    fill_argv(dst, src, count);
    dst[override_index] = (char *)override_value;
}

static int build_custom_usage_text(char *usage_text, size_t usage_text_size) {
    const char *src[] = {custom_program_name};
    char *argv[ARRAY_LEN(src)] = {0};

    if (!usage_text || usage_text_size == 0) {
        return 0;
    }

    usage_text[0] = '\0';
    fill_argv(argv, src, ARRAY_LEN(src));
    return format_cli_usage(usage_text, usage_text_size, argv);
}

static int expect_invalid_size_only_cases(const struct invalid_size_token_case *cases, size_t case_count,
                                          const char *program_name, const char *fixed_token,
                                          enum size_only_argv_index variable_token_index,
                                          char *stderr_text, size_t stderr_size,
                                          int *exit_code, const char *custom_usage_text) {
    size_t i;

    for (i = 0; i < case_count; i += 1) {
        const char *src[] = {program_name, fixed_token, fixed_token};
        char *argv[ARRAY_LEN(src)] = {0};

        fill_argv_with_override(argv, src, ARRAY_LEN(src), variable_token_index, cases[i].token);

        if (!expect_invalid_main_result(cases[i].label_prefix, size_only_argc, argv, stderr_text, stderr_size, exit_code,
                                        custom_usage_text)) {
            return 0;
        }
    }
    return 1;
}

static int expect_invalid_size_only_width_cases(const struct invalid_size_token_case *cases, size_t case_count,
                                                const char *program_name, const char *height_token,
                                                char *stderr_text, size_t stderr_size, int *exit_code,
                                                const char *custom_usage_text) {
    return expect_invalid_size_only_cases(cases, case_count, program_name, height_token,
                                          size_only_width_argv_index,
                                          stderr_text, stderr_size, exit_code, custom_usage_text);
}

static int expect_invalid_size_only_height_cases(const struct invalid_size_token_case *cases, size_t case_count,
                                                 const char *program_name, const char *width_token,
                                                 char *stderr_text, size_t stderr_size, int *exit_code,
                                                 const char *custom_usage_text) {
    return expect_invalid_size_only_cases(cases, case_count, program_name, width_token,
                                          size_only_height_argv_index,
                                          stderr_text, stderr_size, exit_code, custom_usage_text);
}

static int expect_main_invalid_size_only_case_groups(const struct invalid_size_token_case *custom_width_cases,
                                                     size_t custom_width_case_count,
                                                     const struct invalid_size_token_case *custom_height_cases,
                                                     size_t custom_height_case_count,
                                                     const struct invalid_size_token_case *default_width_cases,
                                                     size_t default_width_case_count,
                                                     const struct invalid_size_token_case *default_height_cases,
                                                     size_t default_height_case_count,
                                                     char *stderr_text, size_t stderr_size, int *exit_code,
                                                     const char *custom_usage_text) {
    if (!expect_invalid_size_only_width_cases(custom_width_cases, custom_width_case_count,
                                              custom_program_name, default_size_only_height,
                                              stderr_text, stderr_size, exit_code, custom_usage_text)) {
        return 0;
    }

    if (!expect_invalid_size_only_height_cases(custom_height_cases, custom_height_case_count,
                                               custom_program_name, default_size_only_width,
                                               stderr_text, stderr_size, exit_code, custom_usage_text)) {
        return 0;
    }

    if (!expect_invalid_size_only_height_cases(default_height_cases, default_height_case_count,
                                               default_program_name, default_size_only_width,
                                               stderr_text, stderr_size, exit_code, no_usage_text)) {
        return 0;
    }

    return expect_invalid_size_only_width_cases(default_width_cases, default_width_case_count,
                                                default_program_name, default_size_only_height,
                                                stderr_text, stderr_size, exit_code, no_usage_text);
}

static int expect_invalid_input_size_cases(const struct invalid_input_size_case *cases, size_t case_count,
                                           const char *program_name, const char *scene_token,
                                           char *stderr_text, size_t stderr_size, int *exit_code,
                                           const char *custom_usage_text) {
    size_t i;

    for (i = 0; i < case_count; i += 1) {
        const char *src[] = {program_name, scene_token, cases[i].width_token, cases[i].height_token};
        char *argv[ARRAY_LEN(src)] = {0};

        fill_argv(argv, src, ARRAY_LEN(src));

        if (!expect_invalid_main_result(cases[i].label_prefix, input_size_argc, argv, stderr_text, stderr_size, exit_code,
                                        custom_usage_text)) {
            return 0;
        }
    }
    return 1;
}

static int expect_main_invalid_input_size_case_groups(const struct invalid_input_size_case *custom_cases,
                                                      size_t custom_case_count,
                                                      const struct invalid_input_size_case *default_cases,
                                                      size_t default_case_count,
                                                      char *stderr_text, size_t stderr_size, int *exit_code,
                                                      const char *custom_usage_text) {
    if (!expect_invalid_input_size_cases(custom_cases, custom_case_count,
                                         custom_program_name, default_scene_path,
                                         stderr_text, stderr_size, exit_code, custom_usage_text)) {
        return 0;
    }

    return expect_invalid_input_size_cases(default_cases, default_case_count,
                                           default_program_name, default_scene_path,
                                           stderr_text, stderr_size, exit_code, no_usage_text);
}

static int expect_success_cases(const struct success_case *cases, size_t case_count,
                                char *stderr_text, size_t stderr_size, int *exit_code) {
    size_t i;

    for (i = 0; i < case_count; i += 1) {
        const char *src[] = {cases[i].program_name, cases[i].input_token, cases[i].width_token, cases[i].height_token};
        char *argv[ARRAY_LEN(src)] = {0};

        fill_argv(argv, src, ARRAY_LEN(src));

        reset_app_state(cases[i].result, no_input_path, no_input_canvas_w, no_input_canvas_h, stderr_text);
        if (!capture_main_stderr(cases[i].argc, argv, stderr_text, stderr_size, exit_code)) {
            return 0;
        }

        if (!expect_successful_run(cases[i].label_prefix, *exit_code, cases[i].expected_exit_code,
                                   cases[i].expected_input_path, cases[i].expected_canvas_w,
                                   cases[i].expected_canvas_h, stderr_text, cases[i].expected_stderr)) {
            return 0;
        }
    }
    return 1;
}

static int expect_invalid_argv_cases(const struct invalid_argv_case *cases, size_t case_count,
                                     char *stderr_text, size_t stderr_size, int *exit_code) {
    size_t i;

    for (i = 0; i < case_count; i += 1) {
        char *argv[ARRAY_LEN(cases[i].argv)] = {0};
        char **argv_ptr = cases[i].argv_state == argv_is_null ? null_argv : argv;

        if (cases[i].argv_state == argv_is_present) {
            fill_argv(argv, cases[i].argv, ARRAY_LEN(cases[i].argv));
        }

        if (!expect_invalid_main_result(cases[i].label_prefix, cases[i].argc, argv_ptr,
                                        stderr_text, stderr_size, exit_code,
                                        cases[i].expected_usage_text)) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    char stderr_text[main_smoke_buffer_size];
    char custom_usage_text[main_smoke_buffer_size];
    int exit_code = 0;

    stderr_text[0] = '\0';
    if (!build_custom_usage_text(custom_usage_text, sizeof(custom_usage_text))) {
        return 1;
    }

    const struct invalid_size_token_case custom_invalid_size_only_width_cases[] = {
        INVALID_SIZE_TOKEN_CASE("custom_program_null_size_only_width", null_token),
        INVALID_SIZE_TOKEN_CASE("custom_program_empty_size_only_width", empty_token),
        INVALID_SIZE_TOKEN_CASE("custom_program_bad_size_only_width", bad_width_token),
        INVALID_SIZE_TOKEN_CASE("custom_program_leading_space_size_only_width", leading_space_width_token),
        INVALID_SIZE_TOKEN_CASE("custom_program_trailing_space_size_only_width", trailing_space_width_token),
        INVALID_SIZE_TOKEN_CASE("custom_program_zero_size_only_width", zero_token),
        INVALID_SIZE_TOKEN_CASE("custom_program_negative_size_only_width", negative_size_only_width),
        INVALID_SIZE_TOKEN_CASE("custom_program_overflow_size_only_width", overflow_width_token),
    };
    const struct invalid_size_token_case custom_invalid_size_only_height_cases[] = {
        INVALID_SIZE_TOKEN_CASE("custom_program_null_size_only_height", null_token),
        INVALID_SIZE_TOKEN_CASE("custom_program_bad_size_only_height", bad_height_token),
        INVALID_SIZE_TOKEN_CASE("custom_program_zero_size_only_height", zero_token),
        INVALID_SIZE_TOKEN_CASE("custom_program_negative_size_only_height", negative_size_only_height),
        INVALID_SIZE_TOKEN_CASE("custom_program_trailing_newline_size_only_height", trailing_newline_height_token),
        INVALID_SIZE_TOKEN_CASE("custom_program_leading_tab_size_only_height", leading_tab_height_token),
        INVALID_SIZE_TOKEN_CASE("custom_program_empty_size_only_height", empty_token),
        INVALID_SIZE_TOKEN_CASE("custom_program_overflow_size_only_height", overflow_height_token),
    };
    const struct invalid_argv_case invalid_argv_cases[] = {
        INVALID_SIZE_ONLY_ARGV_CASE("invalid", no_usage_text, default_program_name, default_size_only_width),
        INVALID_SIZE_ONLY_ARGV_CASE("bad_size_only", no_usage_text, default_program_name, invalid_probe_token),
        INVALID_SIZE_ONLY_ARGV_CASE("custom_program_invalid", custom_usage_text, custom_program_name, invalid_probe_token),
        INVALID_SIZE_ONLY_ARGV_CASE("missing_h", no_usage_text, default_program_name, default_size_only_width),
        INVALID_EXTRA_ARGV_CASE("extra_args", no_usage_text, default_program_name),
        INVALID_EXTRA_ARGV_CASE("custom_program_extra_args", custom_usage_text, custom_program_name),
        INVALID_INPUT_ARGV_CASE("custom_program_empty_input", custom_usage_text, custom_program_name, empty_token),
        INVALID_INPUT_ARGV_CASE("custom_program_null_input", custom_usage_text, custom_program_name, no_input_path),
        INVALID_NULL_ARGV_ONLY_CASE("null_argv", startup_argc, no_usage_text),
        INVALID_PROGRAM_ONLY_ARGV_CASE("null_program", startup_argc, no_usage_text, no_input_path),
        INVALID_PROGRAM_ONLY_ARGV_CASE("zero_argc", zero_argc, no_usage_text, default_program_name),
        INVALID_PROGRAM_ONLY_ARGV_CASE("empty_program", startup_argc, no_usage_text, empty_token),
        INVALID_INPUT_ARGV_CASE("empty_input", no_usage_text, default_program_name, empty_token),
        INVALID_INPUT_ARGV_CASE("null_input", no_usage_text, default_program_name, no_input_path),
    };
    if (!expect_invalid_argv_cases(invalid_argv_cases,
                                   ARRAY_LEN(invalid_argv_cases),
                                   stderr_text, sizeof(stderr_text), &exit_code)) {
        return 1;
    }

    const struct invalid_size_token_case invalid_size_only_height_cases[] = {
        INVALID_SIZE_TOKEN_CASE("null_size_only_height", null_token),
        INVALID_SIZE_TOKEN_CASE("empty_size_only_height", empty_token),
        INVALID_SIZE_TOKEN_CASE("zero_size_only_height", zero_token),
        INVALID_SIZE_TOKEN_CASE("negative_size_only_height", negative_size_only_height),
        INVALID_SIZE_TOKEN_CASE("bad_size_only_height", bad_height_token),
        INVALID_SIZE_TOKEN_CASE("overflow_size_only_height", overflow_height_token),
        INVALID_SIZE_TOKEN_CASE("trailing_newline_size_only_height", trailing_newline_height_token),
        INVALID_SIZE_TOKEN_CASE("leading_tab_size_only_height", leading_tab_height_token),
    };
    const struct invalid_size_token_case invalid_size_only_width_cases[] = {
        INVALID_SIZE_TOKEN_CASE("null_size_only_width", null_token),
        INVALID_SIZE_TOKEN_CASE("empty_size_only_width", empty_token),
        INVALID_SIZE_TOKEN_CASE("zero_size_only_width", zero_token),
        INVALID_SIZE_TOKEN_CASE("negative_size_only_width", negative_size_only_width),
        INVALID_SIZE_TOKEN_CASE("bad_size_only_width", bad_width_token),
        INVALID_SIZE_TOKEN_CASE("overflow_size_only_width", overflow_width_token),
        INVALID_SIZE_TOKEN_CASE("leading_space_size_only_width", leading_space_width_token),
        INVALID_SIZE_TOKEN_CASE("trailing_space_size_only_width", trailing_space_width_token),
    };
    if (!expect_main_invalid_size_only_case_groups(custom_invalid_size_only_width_cases,
                                                   ARRAY_LEN(custom_invalid_size_only_width_cases),
                                                   custom_invalid_size_only_height_cases,
                                                   ARRAY_LEN(custom_invalid_size_only_height_cases),
                                                   invalid_size_only_width_cases,
                                                   ARRAY_LEN(invalid_size_only_width_cases),
                                                   invalid_size_only_height_cases,
                                                   ARRAY_LEN(invalid_size_only_height_cases),
                                                   stderr_text, sizeof(stderr_text), &exit_code, custom_usage_text)) {
        return 1;
    }

    const struct invalid_input_size_case custom_invalid_input_size_cases[] = {
        INVALID_INPUT_WIDTH_CASE("custom_program_bad_width", bad_width_token),
        INVALID_INPUT_WIDTH_CASE("custom_program_negative_width", negative_size_only_width),
        INVALID_INPUT_WIDTH_CASE("custom_program_overflow_width", overflow_width_token),
        INVALID_INPUT_HEIGHT_CASE("custom_program_bad_height", bad_height_token),
        INVALID_INPUT_HEIGHT_CASE("custom_program_negative_height", negative_size_only_height),
        INVALID_INPUT_HEIGHT_CASE("custom_program_null_height", null_token),
        INVALID_INPUT_WIDTH_CASE("custom_program_null_width", null_token),
        INVALID_INPUT_HEIGHT_CASE("custom_program_overflow_height", overflow_height_token),
        INVALID_INPUT_WIDTH_CASE("custom_program_leading_space_width", leading_space_width_token),
        INVALID_INPUT_HEIGHT_CASE("custom_program_trailing_newline_height", trailing_newline_height_token),
        INVALID_INPUT_HEIGHT_CASE("custom_program_leading_tab_height", leading_tab_height_token),
        INVALID_INPUT_WIDTH_CASE("custom_program_trailing_space_width", trailing_space_width_token),
        INVALID_INPUT_WIDTH_CASE("custom_program_zero_width", zero_token),
        INVALID_INPUT_HEIGHT_CASE("custom_program_zero_height", zero_token),
    };
    const struct invalid_input_size_case invalid_input_size_cases[] = {
        INVALID_INPUT_WIDTH_VALUE_CASE("bad_width_value", zero_token),
        INVALID_INPUT_WIDTH_VALUE_CASE("negative_width_value", negative_size_only_width),
        INVALID_INPUT_WIDTH_CASE("bad_width_token", bad_width_token),
        INVALID_INPUT_HEIGHT_CASE("bad_height_value", negative_input_size_height),
        INVALID_INPUT_HEIGHT_CASE("negative_height_value", negative_size_only_height),
        INVALID_INPUT_WIDTH_CASE("empty_width", empty_token),
        INVALID_INPUT_WIDTH_CASE("null_width", null_token),
        INVALID_INPUT_HEIGHT_CASE("null_height", null_token),
        INVALID_INPUT_WIDTH_CASE("overflow_width", overflow_width_token),
        INVALID_INPUT_HEIGHT_CASE("bad_height_token", bad_height_token),
        INVALID_INPUT_HEIGHT_CASE("overflow_height", overflow_height_token),
        INVALID_INPUT_WIDTH_CASE("leading_space_width", leading_space_width_token),
        INVALID_INPUT_HEIGHT_CASE("trailing_newline_height", trailing_newline_height_token),
        INVALID_INPUT_HEIGHT_CASE("leading_tab_height", leading_tab_height_token),
        INVALID_INPUT_WIDTH_CASE("trailing_space_width", trailing_space_width_token),
    };
    if (!expect_main_invalid_input_size_case_groups(custom_invalid_input_size_cases,
                                                    ARRAY_LEN(custom_invalid_input_size_cases),
                                                    invalid_input_size_cases,
                                                    ARRAY_LEN(invalid_input_size_cases),
                                                    stderr_text, sizeof(stderr_text), &exit_code, custom_usage_text)) {
        return 1;
    }

    const struct success_case success_cases[] = {
        SUCCESS_STARTUP_CASE_EMPTY_STDERR("default", default_program_name, success_exit_code, success_exit_code),
        SUCCESS_INPUT_CASE_EMPTY_STDERR("custom_program_input", custom_program_name, custom_input_path, success_exit_code, success_exit_code),
        SUCCESS_STARTUP_CASE_EMPTY_STDERR("custom_program_default", custom_program_name, success_exit_code, success_exit_code),
        SUCCESS_INPUT_CASE_EMPTY_STDERR("custom_program_numeric_input", custom_program_name, numeric_input_token, success_exit_code, success_exit_code),
        SUCCESS_INPUT_CASE("custom_program_nonzero", custom_program_name, custom_input_path, app_exit_code_5, app_exit_code_5, app_exit_code_5_stderr),
        SUCCESS_INPUT_CASE_EMPTY_STDERR("input_only", default_program_name, default_input_path, success_exit_code, success_exit_code),
        SUCCESS_INPUT_CASE_EMPTY_STDERR("numeric_input", default_program_name, numeric_input_token, success_exit_code, success_exit_code),
        SUCCESS_SIZE_ONLY_CASE("size_only", default_program_name, default_size_only_width, default_size_only_height, app_exit_code_7, app_exit_code_7, default_size_only_canvas_w, default_size_only_canvas_h, app_exit_code_7_stderr),
        SUCCESS_SIZE_ONLY_CASE_EMPTY_STDERR("plus_prefixed_size_only", default_program_name, default_plus_prefixed_width_token, default_size_only_height, success_exit_code, success_exit_code, default_size_only_canvas_w, default_size_only_canvas_h),
        SUCCESS_SIZE_ONLY_CASE_EMPTY_STDERR("custom_program_size_only", custom_program_name, custom_size_only_width, custom_size_only_height, success_exit_code, success_exit_code, custom_size_only_canvas_w, custom_size_only_canvas_h),
        SUCCESS_SIZE_ONLY_CASE_EMPTY_STDERR("custom_program_plus_prefixed_size_only", custom_program_name, custom_plus_prefixed_size_only_width, custom_size_only_height, success_exit_code, success_exit_code, custom_size_only_canvas_w, custom_size_only_canvas_h),
        SUCCESS_INPUT_SIZE_CASE_EMPTY_STDERR("custom_program_input_size", custom_program_name, custom_input_path, input_size_width_token, input_size_height_token, success_exit_code, success_exit_code, input_size_canvas_w, input_size_canvas_h),
        SUCCESS_INPUT_SIZE_CASE_EMPTY_STDERR("custom_program_plus_prefixed", custom_program_name, custom_input_path, custom_plus_prefixed_input_size_width, input_size_height_token, success_exit_code, success_exit_code, input_size_canvas_w, input_size_canvas_h),
        SUCCESS_INPUT_SIZE_CASE_EMPTY_STDERR("input_size", default_program_name, default_scene_path, input_size_width_token, input_size_height_token, success_exit_code, success_exit_code, input_size_canvas_w, input_size_canvas_h),
        SUCCESS_INPUT_SIZE_CASE_EMPTY_STDERR("plus_prefixed", default_program_name, default_scene_path, default_plus_prefixed_width_token, default_size_only_height, success_exit_code, success_exit_code, default_size_only_canvas_w, default_size_only_canvas_h),
    };
    if (!expect_success_cases(success_cases,
                              ARRAY_LEN(success_cases),
                              stderr_text, sizeof(stderr_text), &exit_code)) {
        return 1;
    }

    printf("main usage smoke ok\n");
    return 0;
}
