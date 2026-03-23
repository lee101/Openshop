#ifndef CLI_H
#define CLI_H

typedef struct CliOptions {
    const char *input_path;
    int canvas_w;
    int canvas_h;
} CliOptions;

int parse_cli_args(int argc, char **argv, CliOptions *options);
const char *cli_program_name(char **argv);
const char *cli_usage_suffix(void);
int format_cli_usage(char *buffer, int buffer_size, char **argv);

#endif
