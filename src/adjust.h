#ifndef ADJUST_H
#define ADJUST_H

#include "canvas.h"

void canvas_adjust_brightness_contrast(Canvas *c, int brightness, int contrast);
void canvas_adjust_hue_saturation(Canvas *c, int hue_degrees, int saturation, int lightness);
void canvas_adjust_levels(Canvas *c, int in_black, int in_white, double gamma, int out_black, int out_white);
void canvas_desaturate(Canvas *c);
void canvas_posterize(Canvas *c, int levels);
void canvas_threshold(Canvas *c, int level);
void canvas_gaussian_blur(Canvas *c, int radius);
void canvas_sharpen(Canvas *c, int amount_percent);

#endif
