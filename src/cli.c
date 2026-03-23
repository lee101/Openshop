#include "cli.h"

#include <stdlib.h>

int parse_cli_args(int argc, char **argv, CliOptions *options) {
    if (!options) {
        return 0;
    }

    options->input_path = NULL;
    options->canvas_w = 0;
    options->canvas_h = 0;

    if (argc > 1) {
        options->input_path = argv[1];
    }
    if (argc > 3) {
        options->canvas_w = atoi(argv[2]);
        options->canvas_h = atoi(argv[3]);
        if (options->canvas_w <= 0 || options->canvas_h <= 0) {
            return 0;
        }
    }
    return 1;
}
