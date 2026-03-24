#ifndef IMAGE_IO_H
#define IMAGE_IO_H

#include "canvas.h"
#include <stdint.h>

typedef enum {
    IMAGE_LOAD_FAILED = 0,
    IMAGE_LOAD_UNCHANGED,
    IMAGE_LOAD_CHANGED
} ImageLoadResult;

ImageLoadResult canvas_load_bmp_action_result(Canvas *c, const char *path, uint32_t background_color);
int canvas_load_bmp(Canvas *c, const char *path, uint32_t background_color);
int canvas_load_bmp_with_result(Canvas *c, const char *path, uint32_t background_color, int *changed);
int canvas_save_bmp(const Canvas *c, const char *path);

#endif
