#include "cli.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

static int parse_positive_int(const char *text, int *value) {
    char *end = NULL;
    long parsed = 0;

    if (!text || !text[0] || !value) {
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

int parse_cli_args(int argc, char **argv, CliOptions *options) {
    if (!options) {
        return 0;
    }

    if (argc < 1 || argc == 3 || argc > 4) {
        return 0;
    }

    options->input_path = NULL;
    options->canvas_w = 0;
    options->canvas_h = 0;

    if (argc > 1) {
        options->input_path = argv[1];
    }
    if (argc > 3) {
        if (!parse_positive_int(argv[2], &options->canvas_w) ||
            !parse_positive_int(argv[3], &options->canvas_h)) {
            return 0;
        }
    }
    return 1;
}
