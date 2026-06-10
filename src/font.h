#ifndef FONT_H
#define FONT_H

#include "canvas.h"
#include <stdint.h>

int font_text_width(const char *text, int scale);
int font_text_height(int scale);
void canvas_draw_text(Canvas *c, int x, int y, const char *text, int scale, uint32_t argb);

#endif
