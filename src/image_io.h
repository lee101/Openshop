#ifndef IMAGE_IO_H
#define IMAGE_IO_H

#include "canvas.h"
#include "png_io.h"
#include <stdint.h>

int canvas_load_bmp(Canvas *c, const char *path, uint32_t background_color);
int canvas_save_bmp(const Canvas *c, const char *path);
int canvas_load_auto(Canvas *c, const char *path, uint32_t background_color);
int canvas_save_auto(const Canvas *c, const char *path);

#endif
