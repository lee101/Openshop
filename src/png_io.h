#ifndef PNG_IO_H
#define PNG_IO_H

#include "canvas.h"

/* Save a canvas as a PNG file.  Returns 1 on success, 0 on failure.
 * Pixels are stored as ARGB8888 internally; output is RGBA8888 PNG. */
int canvas_save_png(const Canvas *c, const char *path);

#endif
