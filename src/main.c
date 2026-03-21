#include "app.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *input = NULL;
    int canvas_w = 0;
    int canvas_h = 0;

    if (argc > 1) {
        input = argv[1];
    }
    if (argc > 3) {
        canvas_w = atoi(argv[2]);
        canvas_h = atoi(argv[3]);
        if (canvas_w <= 0 || canvas_h <= 0) {
            fprintf(stderr, "Usage: %s [input_path] [width height]\n", argv[0]);
            return 1;
        }
    }

    int code = app_run(input, canvas_w, canvas_h);
    if (code != 0) {
        fprintf(stderr, "App exited with code %d\n", code);
    }
    return code;
}
