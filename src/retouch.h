#ifndef RETOUCH_H
#define RETOUCH_H

#include "canvas.h"
#include <stdint.h>

void canvas_clone_stroke(Canvas *c, int offset_x, int offset_y, int x0, int y0, int x1, int y1, int radius, int hardness_percent);
void canvas_dodge_burn_stroke(Canvas *c, int x0, int y0, int x1, int y1, int radius, int amount_percent, int burn);
void canvas_sponge_stroke(Canvas *c, int x0, int y0, int x1, int y1, int radius, int amount_percent, int desaturate);
void canvas_smudge_stroke(Canvas *c, int x0, int y0, int x1, int y1, int radius, int strength_percent);

#endif
