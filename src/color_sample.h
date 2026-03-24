#ifndef COLOR_SAMPLE_H
#define COLOR_SAMPLE_H

#include "brush_state.h"
#include "canvas.h"

int sample_canvas_brush_state(const Canvas *sample, int x, int y,
                              uint32_t *brush_color_rgb, uint32_t *brush_color,
                              int *brush_opacity, Tool *tool);

#endif
