#include "filters.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------- */

static inline uint8_t clamp255(int v) {
    if (v < 0)   return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

/*
 * General 2-D convolution.
 * kernel  : row-major float array, (ksize x ksize)
 * border  : clamp-to-edge
 * Alpha channel is copied verbatim from the source pixel.
 */
static void apply_kernel(Canvas *c, const float *kernel, int ksize) {
    if (!c || !c->pixels || ksize < 1) return;
    int w = c->width, h = c->height;
    uint32_t *tmp = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    if (!tmp) return;
    int half = ksize / 2;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float r = 0.0f, g = 0.0f, b = 0.0f;
            uint8_t src_a = (uint8_t)((c->pixels[y * w + x] >> 24) & 0xFF);
            for (int ky = 0; ky < ksize; ky++) {
                int sy = y + ky - half;
                if (sy < 0)  sy = 0;
                if (sy >= h) sy = h - 1;
                for (int kx = 0; kx < ksize; kx++) {
                    int sx = x + kx - half;
                    if (sx < 0)  sx = 0;
                    if (sx >= w) sx = w - 1;
                    uint32_t p = c->pixels[sy * w + sx];
                    float k = kernel[ky * ksize + kx];
                    r += k * (float)((p >> 16) & 0xFF);
                    g += k * (float)((p >>  8) & 0xFF);
                    b += k * (float)(p & 0xFF);
                }
            }
            tmp[y * w + x] = ((uint32_t)src_a       << 24)
                            | ((uint32_t)clamp255((int)r) << 16)
                            | ((uint32_t)clamp255((int)g) <<  8)
                            |  (uint32_t)clamp255((int)b);
        }
    }
    memcpy(c->pixels, tmp, (size_t)w * (size_t)h * sizeof(uint32_t));
    free(tmp);
}

/* ---------------------------------------------------------------
 * Public filter implementations
 * --------------------------------------------------------------- */

void filter_box_blur(Canvas *c) {
    static const float k[9] = {
        1.0f/9, 1.0f/9, 1.0f/9,
        1.0f/9, 1.0f/9, 1.0f/9,
        1.0f/9, 1.0f/9, 1.0f/9
    };
    apply_kernel(c, k, 3);
}

void filter_gaussian_blur(Canvas *c) {
    /* Pascal-triangle 5x5, sigma ~ 1.0 */
    static const float k[25] = {
        1/256.0f,  4/256.0f,  6/256.0f,  4/256.0f, 1/256.0f,
        4/256.0f, 16/256.0f, 24/256.0f, 16/256.0f, 4/256.0f,
        6/256.0f, 24/256.0f, 36/256.0f, 24/256.0f, 6/256.0f,
        4/256.0f, 16/256.0f, 24/256.0f, 16/256.0f, 4/256.0f,
        1/256.0f,  4/256.0f,  6/256.0f,  4/256.0f, 1/256.0f
    };
    apply_kernel(c, k, 5);
}

void filter_sharpen(Canvas *c) {
    static const float k[9] = {
         0.0f, -1.0f,  0.0f,
        -1.0f,  5.0f, -1.0f,
         0.0f, -1.0f,  0.0f
    };
    apply_kernel(c, k, 3);
}

void filter_emboss(Canvas *c) {
    static const float k[9] = {
        -2.0f, -1.0f,  0.0f,
        -1.0f,  1.0f,  1.0f,
         0.0f,  1.0f,  2.0f
    };
    apply_kernel(c, k, 3);
}

void filter_edge_detect(Canvas *c) {
    if (!c || !c->pixels) return;
    int w = c->width, h = c->height;
    uint32_t *tmp = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    if (!tmp) return;

    /* Sobel kernels */
    static const float kx[9] = { -1,  0,  1,  -2, 0, 2,  -1,  0,  1 };
    static const float ky[9] = { -1, -2, -1,   0, 0, 0,   1,  2,  1 };

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float gxr = 0, gxg = 0, gxb = 0;
            float gyr = 0, gyg = 0, gyb = 0;
            for (int j = 0; j < 3; j++) {
                int sy = y + j - 1;
                if (sy < 0)  sy = 0;
                if (sy >= h) sy = h - 1;
                for (int i = 0; i < 3; i++) {
                    int sx = x + i - 1;
                    if (sx < 0)  sx = 0;
                    if (sx >= w) sx = w - 1;
                    uint32_t p = c->pixels[sy * w + sx];
                    float pr = (float)((p >> 16) & 0xFF);
                    float pg = (float)((p >>  8) & 0xFF);
                    float pb = (float)(p & 0xFF);
                    float fkx = kx[j * 3 + i];
                    float fky = ky[j * 3 + i];
                    gxr += fkx * pr;  gxg += fkx * pg;  gxb += fkx * pb;
                    gyr += fky * pr;  gyg += fky * pg;   gyb += fky * pb;
                }
            }
            uint8_t or2 = clamp255((int)sqrtf(gxr*gxr + gyr*gyr));
            uint8_t og2 = clamp255((int)sqrtf(gxg*gxg + gyg*gyg));
            uint8_t ob2 = clamp255((int)sqrtf(gxb*gxb + gyb*gyb));
            uint8_t src_a = (uint8_t)((c->pixels[y * w + x] >> 24) & 0xFF);
            tmp[y * w + x] = ((uint32_t)src_a << 24)
                           | ((uint32_t)or2 << 16)
                           | ((uint32_t)og2 <<  8)
                           |  (uint32_t)ob2;
        }
    }
    memcpy(c->pixels, tmp, (size_t)w * (size_t)h * sizeof(uint32_t));
    free(tmp);
}

void filter_invert(Canvas *c) {
    if (!c || !c->pixels) return;
    size_t n = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < n; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)(p >> 24);
        uint8_t r = (uint8_t)((p >> 16) & 0xFF);
        uint8_t g = (uint8_t)((p >>  8) & 0xFF);
        uint8_t b = (uint8_t)(p & 0xFF);
        c->pixels[i] = ((uint32_t)a << 24)
                     | ((uint32_t)(255 - r) << 16)
                     | ((uint32_t)(255 - g) <<  8)
                     |  (uint32_t)(255 - b);
    }
}

void filter_grayscale(Canvas *c) {
    if (!c || !c->pixels) return;
    size_t n = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < n; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)(p >> 24);
        uint8_t r = (uint8_t)((p >> 16) & 0xFF);
        uint8_t g = (uint8_t)((p >>  8) & 0xFF);
        uint8_t b = (uint8_t)(p & 0xFF);
        /* ITU-R BT.601 luminosity weights */
        uint8_t gray = clamp255((int)(0.299f * r + 0.587f * g + 0.114f * b));
        c->pixels[i] = ((uint32_t)a    << 24)
                     | ((uint32_t)gray << 16)
                     | ((uint32_t)gray <<  8)
                     |  (uint32_t)gray;
    }
}

void filter_brightness_contrast(Canvas *c, int brightness, float contrast) {
    if (!c || !c->pixels) return;
    size_t n = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < n; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)(p >> 24);
        int r = (int)((p >> 16) & 0xFF);
        int g = (int)((p >>  8) & 0xFF);
        int b = (int)(p & 0xFF);
        /* contrast around midpoint 128, then brightness offset */
        r = (int)((r - 128) * contrast) + 128 + brightness;
        g = (int)((g - 128) * contrast) + 128 + brightness;
        b = (int)((b - 128) * contrast) + 128 + brightness;
        c->pixels[i] = ((uint32_t)a           << 24)
                     | ((uint32_t)clamp255(r)  << 16)
                     | ((uint32_t)clamp255(g)  <<  8)
                     |  (uint32_t)clamp255(b);
    }
}

void filter_pixelate(Canvas *c, int block_size) {
    if (!c || !c->pixels || block_size < 2) return;
    int w = c->width, h = c->height;
    for (int y = 0; y < h; y += block_size) {
        for (int x = 0; x < w; x += block_size) {
            long sr = 0, sg = 0, sb = 0, sa = 0;
            int cnt = 0;
            for (int dy = 0; dy < block_size && y + dy < h; dy++) {
                for (int dx = 0; dx < block_size && x + dx < w; dx++) {
                    uint32_t p = c->pixels[(y + dy) * w + (x + dx)];
                    sa += (p >> 24) & 0xFF;
                    sr += (p >> 16) & 0xFF;
                    sg += (p >>  8) & 0xFF;
                    sb +=  p & 0xFF;
                    cnt++;
                }
            }
            if (cnt == 0) continue;
            uint32_t avg = ((uint32_t)(sa / cnt) << 24)
                         | ((uint32_t)(sr / cnt) << 16)
                         | ((uint32_t)(sg / cnt) <<  8)
                         |  (uint32_t)(sb / cnt);
            for (int dy = 0; dy < block_size && y + dy < h; dy++) {
                for (int dx = 0; dx < block_size && x + dx < w; dx++) {
                    c->pixels[(y + dy) * w + (x + dx)] = avg;
                }
            }
        }
    }
}

void filter_posterize(Canvas *c, int levels) {
    if (!c || !c->pixels || levels < 2) return;
    /* quantize each channel to `levels` steps */
    size_t n = (size_t)c->width * (size_t)c->height;
    float step = 255.0f / (float)(levels - 1);
    for (size_t i = 0; i < n; i++) {
        uint32_t p = c->pixels[i];
        uint8_t a = (uint8_t)(p >> 24);
        int r = (int)((p >> 16) & 0xFF);
        int g = (int)((p >>  8) & 0xFF);
        int b = (int)(p & 0xFF);
        r = (int)(roundf(r / step) * step);
        g = (int)(roundf(g / step) * step);
        b = (int)(roundf(b / step) * step);
        c->pixels[i] = ((uint32_t)a          << 24)
                     | ((uint32_t)clamp255(r) << 16)
                     | ((uint32_t)clamp255(g) <<  8)
                     |  (uint32_t)clamp255(b);
    }
}

void filter_vignette(Canvas *c, float strength) {
    if (!c || !c->pixels) return;
    int w = c->width, h = c->height;
    float cx = w * 0.5f, cy = h * 0.5f;
    /* normalise so the corner distance = 1.0 */
    float max_dist = sqrtf(cx * cx + cy * cy);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx * dx + dy * dy) / max_dist;
            /* smooth cosine falloff: 1 at centre, 0 at corner */
            float factor = 1.0f - strength * dist * dist;
            if (factor < 0.0f) factor = 0.0f;
            uint32_t p = c->pixels[y * w + x];
            uint8_t a = (uint8_t)(p >> 24);
            int r = (int)((p >> 16) & 0xFF);
            int g = (int)((p >>  8) & 0xFF);
            int b = (int)(p & 0xFF);
            c->pixels[y * w + x] = ((uint32_t)a                      << 24)
                                  | ((uint32_t)clamp255((int)(r * factor)) << 16)
                                  | ((uint32_t)clamp255((int)(g * factor)) <<  8)
                                  |  (uint32_t)clamp255((int)(b * factor));
        }
    }
}
