#ifndef BRUSH_RENDER_H
#define BRUSH_RENDER_H

#include "brush_state.h"
#include "canvas.h"

void stamp_brush(Canvas *c, int cx, int cy, int radius, uint32_t color, BrushShape shape);
void erase_stamp(Canvas *c, int cx, int cy, int radius, uint32_t clear_color, BrushShape shape);
void draw_brush_line(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t color, BrushShape shape);
void erase_line(Canvas *c, int x0, int y0, int x1, int y1, int radius, uint32_t clear_color, BrushShape shape);

#endif
