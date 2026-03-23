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
static const char *zero_token = "0";
static const char *invalid_probe_size = "768";
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
static const char *app_exit_code_7_stderr = "App exited with code 7\n";
static const char *app_exit_code_5_stderr = "App exited with code 5\n";

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

static int expect_invalid_main_run(const char *label_prefix, int argc, char **argv,
                                   char *stderr_text, size_t stderr_size, int *exit_code) {
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(argc, argv, stderr_text, stderr_size, exit_code)) {
        return 0;
    }
    return expect_invalid_run(label_prefix, *exit_code, stderr_text);
}

static int expect_invalid_main_run_with_usage(const char *label_prefix, int argc, char **argv,
                                              char *stderr_text, size_t stderr_size,
                                              int *exit_code, const char *expected_stderr) {
    reset_app_state(0, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(argc, argv, stderr_text, stderr_size, exit_code)) {
        return 0;
    }
    return expect_invalid_run_with_usage(label_prefix, *exit_code, stderr_text, expected_stderr);
}

static int expect_successful_main_run(const char *label_prefix, int argc, char **argv,
                                      int result, char *stderr_text, size_t stderr_size,
                                      int *exit_code, int expected_exit_code,
                                      const char *expected_input_path, int expected_canvas_w,
                                      int expected_canvas_h, const char *expected_stderr) {
    reset_app_state(result, NULL, 0, 0, stderr_text);
    if (!capture_main_stderr(argc, argv, stderr_text, stderr_size, exit_code)) {
        return 0;
    }
    return expect_successful_run(label_prefix, *exit_code, expected_exit_code, expected_input_path,
                                 expected_canvas_w, expected_canvas_h, stderr_text, expected_stderr);
}

static int expect_custom_invalid_main_run(const char *label_prefix, int argc, char **argv,
                                          char *stderr_text, size_t stderr_size,
                                          int *exit_code, const char *custom_usage_text) {
    return expect_invalid_main_run_with_usage(label_prefix, argc, argv, stderr_text, stderr_size, exit_code,
                                              custom_usage_text);
}

struct invalid_size_token_case {
    const char *label_prefix;
    char *token;
};

struct invalid_input_size_case {
    const char *label_prefix;
    char *width_token;
    char *height_token;
};

struct invalid_argv_case {
    const char *label_prefix;
    int argc;
    int use_null_argv;
    const char *expected_usage_text;
    char *argv[5];
};

struct success_case {
    const char *label_prefix;
    int argc;
    char *program_name;
    char *input_token;
    char *width_token;
    char *height_token;
    int result;
    int expected_exit_code;
    const char *expected_input_path;
    int expected_canvas_w;
    int expected_canvas_h;
    const char *expected_stderr;
};

static int expect_invalid_size_only_height_cases(const struct invalid_size_token_case *cases, size_t case_count,
                                                 char *program_name, char *width_token,
                                                 char *stderr_text, size_t stderr_size, int *exit_code,
                                                 const char *custom_usage_text) {
    size_t i = 0;

    for (i = 0; i < case_count; i += 1) {
        char *argv[] = {program_name, width_token, cases[i].token};

        if (custom_usage_text) {
            if (!expect_custom_invalid_main_run(cases[i].label_prefix, 3, argv, stderr_text, stderr_size, exit_code,
                                                custom_usage_text)) {
                return 0;
            }
            continue;
        }

        if (!expect_invalid_main_run(cases[i].label_prefix, 3, argv, stderr_text, stderr_size, exit_code)) {
            return 0;
        }
    }
    return 1;
}

static int expect_invalid_size_only_width_cases(const struct invalid_size_token_case *cases, size_t case_count,
                                                char *program_name, char *height_token,
                                                char *stderr_text, size_t stderr_size, int *exit_code,
                                                const char *custom_usage_text) {
    size_t i = 0;

    for (i = 0; i < case_count; i += 1) {
        char *argv[] = {program_name, cases[i].token, height_token};

        if (custom_usage_text) {
            if (!expect_custom_invalid_main_run(cases[i].label_prefix, 3, argv, stderr_text, stderr_size, exit_code,
                                                custom_usage_text)) {
                return 0;
            }
            continue;
        }

        if (!expect_invalid_main_run(cases[i].label_prefix, 3, argv, stderr_text, stderr_size, exit_code)) {
            return 0;
        }
    }
    return 1;
}

static int expect_invalid_input_size_cases(const struct invalid_input_size_case *cases, size_t case_count,
                                           char *program_name, char *scene_token,
                                           char *stderr_text, size_t stderr_size, int *exit_code,
                                           const char *custom_usage_text) {
    size_t i = 0;

    for (i = 0; i < case_count; i += 1) {
        char *argv[] = {program_name, scene_token, cases[i].width_token, cases[i].height_token};

        if (custom_usage_text) {
            if (!expect_custom_invalid_main_run(cases[i].label_prefix, 4, argv, stderr_text, stderr_size, exit_code,
                                                custom_usage_text)) {
                return 0;
            }
            continue;
        }

        if (!expect_invalid_main_run(cases[i].label_prefix, 4, argv, stderr_text, stderr_size, exit_code)) {
            return 0;
        }
    }
    return 1;
}

static int expect_success_cases(const struct success_case *cases, size_t case_count,
                                char *stderr_text, size_t stderr_size, int *exit_code) {
    size_t i = 0;

    for (i = 0; i < case_count; i += 1) {
        char *argv[] = {cases[i].program_name, cases[i].input_token, cases[i].width_token, cases[i].height_token};

        if (!expect_successful_main_run(cases[i].label_prefix, cases[i].argc, argv, cases[i].result, stderr_text,
                                        stderr_size, exit_code, cases[i].expected_exit_code,
                                        cases[i].expected_input_path, cases[i].expected_canvas_w,
                                        cases[i].expected_canvas_h, cases[i].expected_stderr)) {
            return 0;
        }
    }
    return 1;
}

static int expect_invalid_argv_cases(const struct invalid_argv_case *cases, size_t case_count,
                                     char *stderr_text, size_t stderr_size, int *exit_code) {
    size_t i = 0;

    for (i = 0; i < case_count; i += 1) {
        char **argv = (char **)cases[i].argv;

        if (cases[i].use_null_argv) {
            argv = NULL;
        }

        if (cases[i].expected_usage_text) {
            if (!expect_custom_invalid_main_run(cases[i].label_prefix, cases[i].argc, argv,
                                                stderr_text, stderr_size, exit_code, cases[i].expected_usage_text)) {
                return 0;
            }
            continue;
        }

        if (!expect_invalid_main_run(cases[i].label_prefix, cases[i].argc, argv,
                                     stderr_text, stderr_size, exit_code)) {
            return 0;
        }
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

    struct invalid_size_token_case custom_invalid_size_only_width_cases[] = {
        {"custom_program_null_size_only_width", NULL},
        {"custom_program_empty_size_only_width", ""},
        {"custom_program_bad_size_only_width", (char *)bad_width_token},
        {"custom_program_leading_space_size_only_width", (char *)leading_space_width_token},
        {"custom_program_trailing_space_size_only_width", (char *)trailing_space_width_token},
        {"custom_program_zero_size_only_width", (char *)zero_token},
        {"custom_program_negative_size_only_width", (char *)negative_size_only_width},
        {"custom_program_overflow_size_only_width", (char *)overflow_width_token},
    };
    if (!expect_invalid_size_only_width_cases(custom_invalid_size_only_width_cases,
                                              ARRAY_LEN(custom_invalid_size_only_width_cases),
                                              (char *)custom_program_name, (char *)default_size_only_height,
                                              stderr_text, sizeof(stderr_text), &exit_code, custom_usage_text)) {
        return 1;
    }

    struct invalid_size_token_case custom_invalid_size_only_height_cases[] = {
        {"custom_program_null_size_only_height", NULL},
        {"custom_program_bad_size_only_height", (char *)bad_height_token},
        {"custom_program_zero_size_only_height", (char *)zero_token},
        {"custom_program_negative_size_only_height", (char *)negative_size_only_height},
        {"custom_program_trailing_newline_size_only_height", (char *)trailing_newline_height_token},
        {"custom_program_leading_tab_size_only_height", (char *)leading_tab_height_token},
        {"custom_program_empty_size_only_height", ""},
        {"custom_program_overflow_size_only_height", (char *)overflow_height_token},
    };
    if (!expect_invalid_size_only_height_cases(custom_invalid_size_only_height_cases,
                                               ARRAY_LEN(custom_invalid_size_only_height_cases),
                                               (char *)custom_program_name, (char *)default_size_only_width,
                                               stderr_text, sizeof(stderr_text), &exit_code, custom_usage_text)) {
        return 1;
    }

    struct invalid_argv_case invalid_argv_cases[] = {
        {"invalid", 3, 0, NULL, {"openshop", (char *)default_scene_path, (char *)default_size_only_width, NULL, NULL}},
        {"bad_size_only", 3, 0, NULL, {"openshop", (char *)default_scene_path, (char *)invalid_probe_size, NULL, NULL}},
        {"custom_program_invalid", 3, 0, custom_usage_text, {(char *)custom_program_name, (char *)default_scene_path, (char *)invalid_probe_size, NULL, NULL}},
        {"missing_h", 3, 0, NULL, {"openshop", (char *)default_scene_path, (char *)default_size_only_width, NULL, NULL}},
        {"extra_args", 5, 0, NULL, {"openshop", (char *)default_scene_path, (char *)default_size_only_width, (char *)default_size_only_height, "extra"}},
        {"custom_program_extra_args", 5, 0, custom_usage_text, {(char *)custom_program_name, (char *)default_scene_path, (char *)default_size_only_width, (char *)default_size_only_height, "extra"}},
        {"custom_program_empty_input", 2, 0, custom_usage_text, {(char *)custom_program_name, "", NULL, NULL, NULL}},
        {"custom_program_null_input", 2, 0, custom_usage_text, {(char *)custom_program_name, NULL, NULL, NULL, NULL}},
        {"null_argv", 1, 1, NULL, {NULL, NULL, NULL, NULL, NULL}},
        {"null_program", 1, 0, NULL, {NULL, NULL, NULL, NULL, NULL}},
        {"zero_argc", 0, 0, NULL, {"openshop", NULL, NULL, NULL, NULL}},
        {"empty_program", 1, 0, NULL, {"", NULL, NULL, NULL, NULL}},
        {"empty_input", 2, 0, NULL, {"openshop", "", NULL, NULL, NULL}},
        {"null_input", 2, 0, NULL, {"openshop", NULL, NULL, NULL, NULL}},
    };
    if (!expect_invalid_argv_cases(invalid_argv_cases,
                                   ARRAY_LEN(invalid_argv_cases),
                                   stderr_text, sizeof(stderr_text), &exit_code)) {
        return 1;
    }

    struct invalid_size_token_case invalid_size_only_height_cases[] = {
        {"null_size_only_height", NULL},
        {"empty_size_only_height", ""},
        {"zero_size_only_height", (char *)zero_token},
        {"negative_size_only_height", (char *)negative_size_only_height},
        {"bad_size_only_height", (char *)bad_height_token},
        {"overflow_size_only_height", (char *)overflow_height_token},
        {"trailing_newline_size_only_height", (char *)trailing_newline_height_token},
        {"leading_tab_size_only_height", (char *)leading_tab_height_token},
    };
    if (!expect_invalid_size_only_height_cases(invalid_size_only_height_cases,
                                               ARRAY_LEN(invalid_size_only_height_cases),
                                               "openshop", (char *)default_size_only_width,
                                               stderr_text, sizeof(stderr_text), &exit_code, NULL)) {
        return 1;
    }

    struct invalid_size_token_case invalid_size_only_width_cases[] = {
        {"null_size_only_width", NULL},
        {"empty_size_only_width", ""},
        {"zero_size_only_width", (char *)zero_token},
        {"negative_size_only_width", (char *)negative_size_only_width},
        {"bad_size_only_width", (char *)bad_width_token},
        {"overflow_size_only_width", (char *)overflow_width_token},
        {"leading_space_size_only_width", (char *)leading_space_width_token},
        {"trailing_space_size_only_width", (char *)trailing_space_width_token},
    };
    if (!expect_invalid_size_only_width_cases(invalid_size_only_width_cases,
                                              ARRAY_LEN(invalid_size_only_width_cases),
                                              "openshop", (char *)default_size_only_height,
                                              stderr_text, sizeof(stderr_text), &exit_code, NULL)) {
        return 1;
    }

    struct invalid_input_size_case custom_invalid_input_size_cases[] = {
        {"custom_program_bad_width", (char *)bad_width_token, (char *)default_size_only_height},
        {"custom_program_negative_width", (char *)negative_size_only_width, (char *)default_size_only_height},
        {"custom_program_overflow_width", (char *)overflow_width_token, (char *)default_size_only_height},
        {"custom_program_bad_height", (char *)default_size_only_width, (char *)bad_height_token},
        {"custom_program_negative_height", (char *)default_size_only_width, (char *)negative_size_only_height},
        {"custom_program_null_height", (char *)default_size_only_width, NULL},
        {"custom_program_null_width", NULL, (char *)default_size_only_height},
        {"custom_program_overflow_height", (char *)default_size_only_width, (char *)overflow_height_token},
        {"custom_program_leading_space_width", (char *)leading_space_width_token, (char *)default_size_only_height},
        {"custom_program_trailing_newline_height", (char *)default_size_only_width, (char *)trailing_newline_height_token},
        {"custom_program_leading_tab_height", (char *)default_size_only_width, (char *)leading_tab_height_token},
        {"custom_program_trailing_space_width", (char *)trailing_space_width_token, (char *)default_size_only_height},
        {"custom_program_zero_width", (char *)zero_token, (char *)default_size_only_height},
        {"custom_program_zero_height", (char *)default_size_only_width, (char *)zero_token},
    };
    if (!expect_invalid_input_size_cases(custom_invalid_input_size_cases,
                                         ARRAY_LEN(custom_invalid_input_size_cases),
                                         (char *)custom_program_name, (char *)default_scene_path,
                                         stderr_text, sizeof(stderr_text), &exit_code, custom_usage_text)) {
        return 1;
    }

    struct invalid_input_size_case invalid_input_size_cases[] = {
        {"bad_width_value", (char *)zero_token, (char *)invalid_probe_size},
        {"negative_width_value", (char *)negative_size_only_width, (char *)invalid_probe_size},
        {"bad_width_token", (char *)bad_width_token, (char *)default_size_only_height},
        {"bad_height_value", (char *)default_size_only_width, (char *)negative_input_size_height},
        {"negative_height_value", (char *)default_size_only_width, (char *)negative_size_only_height},
        {"empty_width", "", (char *)default_size_only_height},
        {"null_width", NULL, (char *)default_size_only_height},
        {"null_height", (char *)default_size_only_width, NULL},
        {"overflow_width", (char *)overflow_width_token, (char *)default_size_only_height},
        {"bad_height_token", (char *)default_size_only_width, (char *)bad_height_token},
        {"overflow_height", (char *)default_size_only_width, (char *)overflow_height_token},
        {"leading_space_width", (char *)leading_space_width_token, (char *)default_size_only_height},
        {"trailing_newline_height", (char *)default_size_only_width, (char *)trailing_newline_height_token},
        {"leading_tab_height", (char *)default_size_only_width, (char *)leading_tab_height_token},
        {"trailing_space_width", (char *)trailing_space_width_token, (char *)default_size_only_height},
    };
    if (!expect_invalid_input_size_cases(invalid_input_size_cases,
                                         ARRAY_LEN(invalid_input_size_cases),
                                         "openshop", (char *)default_scene_path,
                                         stderr_text, sizeof(stderr_text), &exit_code, NULL)) {
        return 1;
    }

    struct success_case success_cases[] = {
        {"default", 1, "openshop", NULL, NULL, NULL, 0, 0, NULL, 0, 0, ""},
        {"custom_program_input", 2, (char *)custom_program_name, (char *)custom_input_path, NULL, NULL, 0, 0, custom_input_path, 0, 0, ""},
        {"custom_program_default", 1, (char *)custom_program_name, NULL, NULL, NULL, 0, 0, NULL, 0, 0, ""},
        {"custom_program_numeric_input", 2, (char *)custom_program_name, (char *)custom_numeric_input, NULL, NULL, 0, 0, custom_numeric_input, 0, 0, ""},
        {"custom_program_nonzero", 2, (char *)custom_program_name, (char *)custom_input_path, NULL, NULL, 5, 5, custom_input_path, 0, 0, app_exit_code_5_stderr},
        {"input_only", 2, "openshop", (char *)default_input_path, NULL, NULL, 0, 0, default_input_path, 0, 0, ""},
        {"numeric_input", 2, "openshop", (char *)default_numeric_input, NULL, NULL, 0, 0, default_numeric_input, 0, 0, ""},
        {"size_only", 3, "openshop", (char *)default_size_only_width, (char *)default_size_only_height, NULL, 7, 7, NULL, 640, 480, app_exit_code_7_stderr},
        {"plus_prefixed_size_only", 3, "openshop", (char *)default_plus_prefixed_size_only_width, (char *)default_size_only_height, NULL, 0, 0, NULL, 640, 480, ""},
        {"custom_program_size_only", 3, (char *)custom_program_name, (char *)custom_size_only_width, (char *)custom_size_only_height, NULL, 0, 0, NULL, 800, 600, ""},
        {"custom_program_plus_prefixed_size_only", 3, (char *)custom_program_name, (char *)custom_plus_prefixed_size_only_width, (char *)custom_size_only_height, NULL, 0, 0, NULL, 800, 600, ""},
        {"custom_program_input_size", 4, (char *)custom_program_name, (char *)custom_input_path, (char *)custom_input_size_width, (char *)custom_input_size_height, 0, 0, custom_input_path, 320, 240, ""},
        {"custom_program_plus_prefixed", 4, (char *)custom_program_name, (char *)custom_input_path, (char *)custom_plus_prefixed_input_size_width, (char *)custom_input_size_height, 0, 0, custom_input_path, 320, 240, ""},
        {"input_size", 4, "openshop", (char *)default_scene_path, (char *)default_input_size_width, (char *)default_input_size_height, 0, 0, default_scene_path, 320, 240, ""},
        {"plus_prefixed", 4, "openshop", (char *)default_scene_path, (char *)default_plus_prefixed_input_size_width, (char *)default_size_only_height, 0, 0, default_scene_path, 640, 480, ""},
    };
    if (!expect_success_cases(success_cases,
                              ARRAY_LEN(success_cases),
                              stderr_text, sizeof(stderr_text), &exit_code)) {
        return 1;
    }

    printf("main usage smoke ok\n");
    return 0;
}
