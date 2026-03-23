#include "png_io.h"

#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"
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

int canvas_load_png(Canvas *c, const char *path, uint32_t background_color) {
    if (!c || !path) {
        return 0;
    }

    int img_w = 0;
    int img_h = 0;
    int channels = 0;
    unsigned char *data = stbi_load(path, &img_w, &img_h, &channels, 4);
    if (!data) {
        return 0;
    }

    canvas_clear(c, background_color);

    {
        int copy_w = img_w < c->width ? img_w : c->width;
        int copy_h = img_h < c->height ? img_h : c->height;
        for (int y = 0; y < copy_h; y++) {
            for (int x = 0; x < copy_w; x++) {
                unsigned char *px = data + ((y * img_w + x) * 4);
                uint32_t argb = ((uint32_t)px[3] << 24) |
                    ((uint32_t)px[0] << 16) |
                    ((uint32_t)px[1] << 8) |
                    (uint32_t)px[2];
                c->pixels[(size_t)y * (size_t)c->width + (size_t)x] = argb;
            }
        }
    }

    stbi_image_free(data);
    return 1;
}
