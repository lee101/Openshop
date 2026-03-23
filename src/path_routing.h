#ifndef PATH_ROUTING_H
#define PATH_ROUTING_H

#include <stddef.h>

enum {
    ROUTED_PATH_MAX = 1024
};

typedef struct RoutedPath {
    const char *path;
    int used_alternate;
} RoutedPath;

int path_has_extension_ci(const char *path, const char *ext);
int path_exists(const char *path);
int build_routed_paths(
    const char *seed_path,
    char *bmp_path,
    size_t bmp_size,
    char *png_path,
    size_t png_size
);
const char *default_routed_path(
    const char *bmp_path,
    const char *png_path,
    int prefer_png,
    int bmp_exists,
    int png_exists
);
RoutedPath resolve_routed_choice(const char *bmp_path, const char *png_path, int prefer_png);
const char *default_input_path(int prefer_png, int bmp_exists, int png_exists);
const char *default_output_path(int prefer_png, int bmp_exists, int png_exists);
RoutedPath resolve_default_input_choice(int prefer_png);
RoutedPath resolve_default_output_choice(int prefer_png);
const char *resolve_default_input_path(int prefer_png);
const char *resolve_default_output_path(int prefer_png);

#endif
