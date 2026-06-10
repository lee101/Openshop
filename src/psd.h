#ifndef PSD_H
#define PSD_H

#include "canvas.h"

int psd_save_canvas(const Canvas *c, const char *path);
int psd_load_canvas(Canvas *out, const char *path);

#endif
