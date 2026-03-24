#ifndef DISPLAY_CANVAS_H
#define DISPLAY_CANVAS_H

#include "canvas.h"

#include <stdint.h>

const Canvas *current_display_canvas(int preview_active,
                                     const Canvas *preview_canvas,
                                     const Canvas *composite);

const uint32_t *current_display_pixels(int preview_active,
                                       const Canvas *preview_canvas,
                                       const Canvas *composite);

#endif
