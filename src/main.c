#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_size(const char *arg, int *width, int *height) {
    const char *sep = strchr(arg, 'x');
    char *end = NULL;
    long w;
    long h;

    if (!sep || sep == arg || !sep[1]) {
        return 0;
    }
    w = strtol(arg, &end, 10);
    if (end != sep) {
        return 0;
    }
    h = strtol(sep + 1, &end, 10);
    if (*end != '\0') {
        return 0;
    }
    if (w < 8 || h < 8 || w > 16384 || h > 16384) {
        return 0;
    }
    *width = (int)w;
    *height = (int)h;
    return 1;
}

int main(int argc, char **argv) {
    const char *input = NULL;
    int doc_width = 0;
    int doc_height = 0;

    for (int i = 1; i < argc; i++) {
        int w = 0;
        int h = 0;
        if (parse_size(argv[i], &w, &h)) {
            doc_width = w;
            doc_height = h;
        } else {
            input = argv[i];
        }
    }

    int code = app_run(input, doc_width, doc_height);
    if (code != 0) {
        fprintf(stderr, "App exited with code %d\n", code);
    }
    return code;
}
