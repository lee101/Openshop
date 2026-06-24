#ifndef FILTERS_H
#define FILTERS_H

unsigned char os_luminance_rgb(unsigned char r, unsigned char g, unsigned char b);
void os_grayscale(unsigned char *pixels, int width, int height, int channels);

#endif
