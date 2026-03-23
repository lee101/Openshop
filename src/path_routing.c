#include "path_routing.h"

#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

int path_has_extension_ci(const char *path, const char *ext) {
    size_t path_len = 0;
    size_t ext_len = 0;
    if (!path || !ext) {
        return 0;
    }
    path_len = strlen(path);
    ext_len = strlen(ext);
    if (path_len < ext_len) {
        return 0;
    }
    path += path_len - ext_len;
    for (size_t i = 0; i < ext_len; i++) {
        if (tolower((unsigned char)path[i]) != tolower((unsigned char)ext[i])) {
            return 0;
        }
    }
    return 1;
}

int path_exists(const char *path) {
    FILE *f = NULL;
    if (!path || !path[0]) {
        return 0;
    }
    f = fopen(path, "rb");
    if (!f) {
        return 0;
    }
    fclose(f);
    return 1;
}

const char *default_input_path(int prefer_png, int bmp_exists, int png_exists) {
    if (prefer_png) {
        return "input.png";
    }
    if (bmp_exists || !png_exists) {
        return "input.bmp";
    }
    return "input.png";
}

const char *default_output_path(int prefer_png, int bmp_exists, int png_exists) {
    if (prefer_png) {
        return "output.png";
    }
    if (bmp_exists || !png_exists) {
        return "output.bmp";
    }
    return "output.png";
}

RoutedPath resolve_default_input_choice(int prefer_png) {
    int bmp_exists = path_exists("input.bmp");
    int png_exists = path_exists("input.png");
    RoutedPath choice = {default_input_path(prefer_png, bmp_exists, png_exists), 0};

    if (!prefer_png && !bmp_exists && png_exists) {
        choice.used_alternate = 1;
    }
    return choice;
}

RoutedPath resolve_default_output_choice(int prefer_png) {
    int bmp_exists = path_exists("output.bmp");
    int png_exists = path_exists("output.png");
    RoutedPath choice = {default_output_path(prefer_png, bmp_exists, png_exists), 0};

    if (!prefer_png && !bmp_exists && png_exists) {
        choice.used_alternate = 1;
    }
    return choice;
}

const char *resolve_default_input_path(int prefer_png) {
    return resolve_default_input_choice(prefer_png).path;
}

const char *resolve_default_output_path(int prefer_png) {
    return resolve_default_output_choice(prefer_png).path;
}
