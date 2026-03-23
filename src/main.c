#include "app.h"
#include "cli.h"
#include <stdio.h>

int main(int argc, char **argv) {
    CliOptions options = {0};

    if (!parse_cli_args(argc, argv, &options)) {
        fprintf(stderr, "Usage: %s %s\n", cli_program_name(argv), cli_usage_suffix());
        return 1;
    }

    int code = app_run(options.input_path, options.canvas_w, options.canvas_h);
    if (code != 0) {
        fprintf(stderr, "App exited with code %d\n", code);
    }
    return code;
}
