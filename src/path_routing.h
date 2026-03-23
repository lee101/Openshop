#ifndef PATH_ROUTING_H
#define PATH_ROUTING_H

int path_has_extension_ci(const char *path, const char *ext);
int path_exists(const char *path);
const char *default_input_path(int prefer_png, int bmp_exists, int png_exists);
const char *default_output_path(int prefer_png, int bmp_exists, int png_exists);
const char *resolve_default_input_path(int prefer_png);
const char *resolve_default_output_path(int prefer_png);

#endif
