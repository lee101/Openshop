#include "filters.h"

#include <stddef.h>

unsigned char os_luminance_rgb(unsigned char r, unsigned char g, unsigned char b) {
    return (unsigned char)((299 * (int)r + 587 * (int)g + 114 * (int)b + 500) / 1000);
}

void os_grayscale(unsigned char *pixels, int width, int height, int channels) {
    size_t pixel_count;

    if (!pixels || width <= 0 || height <= 0 || (channels != 3 && channels != 4)) {
        return;
    }

    pixel_count = (size_t)width * (size_t)height;
    for (size_t i = 0; i < pixel_count; i++) {
        unsigned char *p = pixels + i * (size_t)channels;
        unsigned char gray = os_luminance_rgb(p[0], p[1], p[2]);
        p[0] = gray;
        p[1] = gray;
        p[2] = gray;
    }
}
