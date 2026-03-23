#include "png_io.h"

#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#pragma GCC diagnostic pop

int canvas_save_png(const Canvas *c, const char *path) {
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0 || !path) {
        return 0;
    }
    /* Canvas stores pixels as ARGB8888; stb_image_write needs RGBA8888. */
    int count = c->width * c->height;
    unsigned char *rgba = (unsigned char *)malloc((size_t)count * 4);
    if (!rgba) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        rgba[i * 4 + 0] = (unsigned char)((p >> 16) & 0xFF); /* R */
        rgba[i * 4 + 1] = (unsigned char)((p >> 8)  & 0xFF); /* G */
        rgba[i * 4 + 2] = (unsigned char)(p          & 0xFF); /* B */
        rgba[i * 4 + 3] = (unsigned char)((p >> 24) & 0xFF); /* A */
    }
    int ok = stbi_write_png(path, c->width, c->height, 4, rgba, c->width * 4);
    free(rgba);
    return ok != 0;
}
