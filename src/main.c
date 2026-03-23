#include "app.h"
#include "cli.h"
#include <stdio.h>

int main(int argc, char **argv) {
    CliOptions options = {0};
    char usage[CLI_USAGE_BUFFER_SIZE] = {0};

    if (!parse_cli_args(argc, argv, &options)) {
        if (format_cli_usage(usage, (int)sizeof(usage), argv)) {
            fputs(usage, stderr);
        }
        return 1;
    }

    int code = app_run(options.input_path, options.canvas_w, options.canvas_h);
    if (code != 0) {
        fprintf(stderr, "App exited with code %d\n", code);
    }
    return code;
}
