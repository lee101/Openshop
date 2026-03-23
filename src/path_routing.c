#include "path_routing.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *const DEFAULT_INPUT_BMP_PATH = "input.bmp";
static const char *const DEFAULT_INPUT_PNG_PATH = "input.png";
static const char *const DEFAULT_OUTPUT_BMP_PATH = "output.bmp";
static const char *const DEFAULT_OUTPUT_PNG_PATH = "output.png";

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

int build_routed_paths(
    const char *seed_path,
    char *bmp_path,
    size_t bmp_size,
    char *png_path,
    size_t png_size
) {
    const char *name = NULL;
    const char *ext = NULL;
    const char *slash = NULL;
    const char *backslash = NULL;
    size_t stem_len = 0;

    if (!seed_path || !seed_path[0] || !bmp_path || !png_path || bmp_size == 0 || png_size == 0) {
        return 0;
    }

    slash = strrchr(seed_path, '/');
    backslash = strrchr(seed_path, '\\');
    name = slash;
    if (!name || (backslash && backslash > name)) {
        name = backslash;
    }
    name = name ? name + 1 : seed_path;

    ext = strrchr(name, '.');
    if (ext && ext != name) {
        stem_len = (size_t)(ext - seed_path);
    } else {
        stem_len = strlen(seed_path);
    }

    if (snprintf(bmp_path, bmp_size, "%.*s.bmp", (int)stem_len, seed_path) >= (int)bmp_size) {
        return 0;
    }
    if (snprintf(png_path, png_size, "%.*s.png", (int)stem_len, seed_path) >= (int)png_size) {
        return 0;
    }
    return 1;
}

void init_routed_path_pair(RoutedPathPair *paths, const char *seed_path, const char *fallback_stem) {
    if (!paths) {
        return;
    }
    if (seed_path && seed_path[0] &&
        build_routed_paths(seed_path, paths->bmp, sizeof(paths->bmp), paths->png, sizeof(paths->png))) {
        return;
    }
    if (!fallback_stem) {
        paths->bmp[0] = '\0';
        paths->png[0] = '\0';
        return;
    }
    if (snprintf(paths->bmp, sizeof(paths->bmp), "%s.bmp", fallback_stem) >= (int)sizeof(paths->bmp) ||
        snprintf(paths->png, sizeof(paths->png), "%s.png", fallback_stem) >= (int)sizeof(paths->png)) {
        paths->bmp[0] = '\0';
        paths->png[0] = '\0';
    }
}

const char *default_routed_path(
    const char *bmp_path,
    const char *png_path,
    int prefer_png,
    int bmp_exists,
    int png_exists
) {
    if (!bmp_path || !png_path) {
        return NULL;
    }
    if (prefer_png) {
        return png_path;
    }
    if (bmp_exists || !png_exists) {
        return bmp_path;
    }
    return png_path;
}

const char *default_input_path(int prefer_png, int bmp_exists, int png_exists) {
    return default_routed_path(DEFAULT_INPUT_BMP_PATH, DEFAULT_INPUT_PNG_PATH, prefer_png, bmp_exists, png_exists);
}

const char *default_output_path(int prefer_png, int bmp_exists, int png_exists) {
    return default_routed_path(DEFAULT_OUTPUT_BMP_PATH, DEFAULT_OUTPUT_PNG_PATH, prefer_png, bmp_exists, png_exists);
}

RoutedPath resolve_routed_choice(const char *bmp_path, const char *png_path, int prefer_png) {
    int bmp_exists = path_exists(bmp_path);
    int png_exists = path_exists(png_path);
    RoutedPath choice = {default_routed_path(bmp_path, png_path, prefer_png, bmp_exists, png_exists), 0};

    if (!prefer_png && !bmp_exists && png_exists) {
        choice.used_alternate = 1;
    }
    return choice;
}

RoutedPath resolve_routed_pair_choice(
    const RoutedPathPair *paths,
    const char *fallback_bmp_path,
    const char *fallback_png_path,
    int prefer_png
) {
    const char *bmp_path = (paths && paths->bmp[0]) ? paths->bmp : fallback_bmp_path;
    const char *png_path = (paths && paths->png[0]) ? paths->png : fallback_png_path;
    return resolve_routed_choice(bmp_path, png_path, prefer_png);
}

RoutedPath resolve_default_input_pair_choice(const RoutedPathPair *paths, int prefer_png) {
    return resolve_routed_pair_choice(paths, DEFAULT_INPUT_BMP_PATH, DEFAULT_INPUT_PNG_PATH, prefer_png);
}

RoutedPath resolve_default_output_pair_choice(const RoutedPathPair *paths, int prefer_png) {
    return resolve_routed_pair_choice(paths, DEFAULT_OUTPUT_BMP_PATH, DEFAULT_OUTPUT_PNG_PATH, prefer_png);
}

RoutedPath resolve_default_input_choice(int prefer_png) {
    return resolve_routed_choice(DEFAULT_INPUT_BMP_PATH, DEFAULT_INPUT_PNG_PATH, prefer_png);
}

RoutedPath resolve_default_output_choice(int prefer_png) {
    return resolve_routed_choice(DEFAULT_OUTPUT_BMP_PATH, DEFAULT_OUTPUT_PNG_PATH, prefer_png);
}

const char *resolve_default_input_path(int prefer_png) {
    return resolve_default_input_choice(prefer_png).path;
}

const char *resolve_default_output_path(int prefer_png) {
    return resolve_default_output_choice(prefer_png).path;
}
