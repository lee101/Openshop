#include "cli.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define CLI_DEFAULT_PROGRAM_NAME "openshop"

static int parse_positive_int(const char *text, int *value) {
    char *end = NULL;
    long parsed = 0;

    if (!text || !text[0] || !value) {
        return 0;
    }
    if (isspace((unsigned char)text[0])) {
        return 0;
    }

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (!end || end == text || end[0] != '\0') {
        return 0;
    }
    if (errno == ERANGE) {
        return 0;
    }
    if (parsed <= 0 || parsed > INT_MAX) {
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

const char *cli_usage_suffix(void) {
    return "[input_path] [width height]\n"
           "       or: WIDTH HEIGHT";
}

const char *cli_program_name(char **argv) {
    if (!argv || !argv[0] || !argv[0][0]) {
        return CLI_DEFAULT_PROGRAM_NAME;
    }
    return argv[0];
}

int cli_usage_size(char **argv) {
    int written = snprintf(NULL, 0, "Usage: %s %s\n",
                           cli_program_name(argv), cli_usage_suffix());
    return written > 0 ? written + 1 : 0;
}

int format_cli_usage(char *buffer, int buffer_size, char **argv) {
    int written = 0;
    int required = 0;

    if (!buffer || buffer_size <= 0) {
        return 0;
    }
    required = cli_usage_size(argv);
    if (required <= 0 || buffer_size < required) {
        return 0;
    }

    written = snprintf(buffer, (size_t)buffer_size, "Usage: %s %s\n",
                       cli_program_name(argv), cli_usage_suffix());
    return written > 0 && written < buffer_size;
}

int parse_cli_args(int argc, char **argv, CliOptions *options) {
    if (!argv || !options) {
        return 0;
    }

    if (argc < 1 || argc > 4) {
        return 0;
    }
    if (!argv[0] || !argv[0][0]) {
        return 0;
    }

    options->input_path = NULL;
    options->canvas_w = 0;
    options->canvas_h = 0;

    if (argc > 1) {
        if (!argv[1] || !argv[1][0]) {
            return 0;
        }
        options->input_path = argv[1];
    }
    if (argc == 3) {
        if (!argv[2] ||
            !parse_positive_int(argv[1], &options->canvas_w) ||
            !parse_positive_int(argv[2], &options->canvas_h)) {
            return 0;
        }
        options->input_path = NULL;
    } else if (argc > 3) {
        if (!argv[2] || !argv[3] ||
            !parse_positive_int(argv[2], &options->canvas_w) ||
            !parse_positive_int(argv[3], &options->canvas_h)) {
            return 0;
        }
    }
    return 1;
}
