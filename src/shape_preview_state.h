#ifndef SHAPE_PREVIEW_STATE_H
#define SHAPE_PREVIEW_STATE_H

#include "canvas.h"
#include "layers.h"

#include <stdint.h>

void shape_preview_cancel(int *shaping, int *preview_active);
int shape_preview_begin_if_editable(const LayerStack *layers,
                                    int x, int y,
                                    const Canvas *composite,
                                    uint32_t *shape_base_pixels,
                                    int *shaping,
                                    int *shape_start_x,
                                    int *shape_start_y);

#endif
