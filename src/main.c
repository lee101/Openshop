#include "app.h"
#include <stdio.h>

int main(int argc, char **argv) {
    const char *input = NULL;
    if (argc > 1) {
        input = argv[1];
    }
    int code = app_run(input);
    if (code != 0) {
        fprintf(stderr, "App exited with code %d\n", code);
    }
    return code;
}
