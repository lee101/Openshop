#ifndef FILTERS_H
#define FILTERS_H

#include "canvas.h"

/* ---------------------------------------------------------------
 * CPU convolution kernel effects.
 * All filters operate in-place on the canvas RGB channels.
 * Alpha is preserved unless noted.
 * --------------------------------------------------------------- */

/* 3x3 box blur (fast, uniform) */
void filter_box_blur(Canvas *c);

/* 5x5 Gaussian blur (sigma~1.0, smoother) */
void filter_gaussian_blur(Canvas *c);

/* Sharpen (unsharp-style 3x3 laplacian boost) */
void filter_sharpen(Canvas *c);

/* Emboss / relief effect */
void filter_emboss(Canvas *c);

/* Edge detection via Sobel operator (magnitude of XY gradients) */
void filter_edge_detect(Canvas *c);

/* Invert all RGB channels */
void filter_invert(Canvas *c);

/* Luminosity-weighted grayscale */
void filter_grayscale(Canvas *c);

/* Brightness [-255,255] and contrast [0.25,4.0], neutral=0,1.0 */
void filter_brightness_contrast(Canvas *c, int brightness, float contrast);

/* Pixelate: average colour within each (block_size x block_size) cell */
void filter_pixelate(Canvas *c, int block_size);

/* Posterize: reduce each channel to (levels) distinct values */
void filter_posterize(Canvas *c, int levels);

/* Vignette: darken corners with a smooth radial falloff */
void filter_vignette(Canvas *c, float strength);

#endif /* FILTERS_H */
