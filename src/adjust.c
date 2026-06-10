#include "adjust.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint8_t clamp_u8(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

static double clamp_d(double value, double min_value, double max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void apply_lut(Canvas *c, const uint8_t lut[256]) {
    size_t count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        c->pixels[i] = (p & 0xFF000000u) |
                       ((uint32_t)lut[(p >> 16) & 0xFF] << 16) |
                       ((uint32_t)lut[(p >> 8) & 0xFF] << 8) |
                       lut[p & 0xFF];
    }
}

void canvas_adjust_brightness_contrast(Canvas *c, int brightness, int contrast) {
    uint8_t lut[256];
    double slope;

    if (!c || !c->pixels) {
        return;
    }
    if (brightness < -100) brightness = -100;
    if (brightness > 100) brightness = 100;
    if (contrast < -100) contrast = -100;
    if (contrast > 100) contrast = 100;

    slope = tan((contrast + 99.9) / 200.0 * 3.14159265358979 / 2.0);
    for (int i = 0; i < 256; i++) {
        double value = i + brightness * 1.275;
        value = (value - 127.5) * slope + 127.5;
        lut[i] = clamp_u8((int)(clamp_d(value, -1.0, 256.0) + 0.5));
    }
    apply_lut(c, lut);
}

static void rgb_to_hsl(uint8_t r, uint8_t g, uint8_t b, double *h, double *s, double *l) {
    double rd = r / 255.0;
    double gd = g / 255.0;
    double bd = b / 255.0;
    double max = rd > gd ? (rd > bd ? rd : bd) : (gd > bd ? gd : bd);
    double min = rd < gd ? (rd < bd ? rd : bd) : (gd < bd ? gd : bd);
    double delta = max - min;

    *l = (max + min) / 2.0;
    if (delta <= 0.0) {
        *h = 0.0;
        *s = 0.0;
        return;
    }
    *s = *l > 0.5 ? delta / (2.0 - max - min) : delta / (max + min);
    if (max == rd) {
        *h = fmod((gd - bd) / delta + 6.0, 6.0);
    } else if (max == gd) {
        *h = (bd - rd) / delta + 2.0;
    } else {
        *h = (rd - gd) / delta + 4.0;
    }
    *h *= 60.0;
}

static double hue_to_channel(double p, double q, double t) {
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0 / 2.0) return q;
    if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
    return p;
}

static void hsl_to_rgb(double h, double s, double l, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (s <= 0.0) {
        uint8_t v = clamp_u8((int)(l * 255.0 + 0.5));
        *r = v;
        *g = v;
        *b = v;
        return;
    }
    double q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
    double p = 2.0 * l - q;
    double hn = h / 360.0;
    *r = clamp_u8((int)(hue_to_channel(p, q, hn + 1.0 / 3.0) * 255.0 + 0.5));
    *g = clamp_u8((int)(hue_to_channel(p, q, hn) * 255.0 + 0.5));
    *b = clamp_u8((int)(hue_to_channel(p, q, hn - 1.0 / 3.0) * 255.0 + 0.5));
}

void canvas_adjust_hue_saturation(Canvas *c, int hue_degrees, int saturation, int lightness) {
    size_t count;

    if (!c || !c->pixels) {
        return;
    }
    if (hue_degrees < -180) hue_degrees = -180;
    if (hue_degrees > 180) hue_degrees = 180;
    if (saturation < -100) saturation = -100;
    if (saturation > 100) saturation = 100;
    if (lightness < -100) lightness = -100;
    if (lightness > 100) lightness = 100;

    count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        double h, s, l;
        uint8_t r, g, b;

        rgb_to_hsl((uint8_t)((p >> 16) & 0xFF), (uint8_t)((p >> 8) & 0xFF), (uint8_t)(p & 0xFF), &h, &s, &l);
        h = fmod(h + hue_degrees + 360.0, 360.0);
        if (saturation >= 0) {
            s = clamp_d(s * (1.0 + saturation / 100.0), 0.0, 1.0);
        } else {
            s = s * (1.0 + saturation / 100.0);
        }
        if (lightness >= 0) {
            l = l + (1.0 - l) * (lightness / 100.0);
        } else {
            l = l * (1.0 + lightness / 100.0);
        }
        hsl_to_rgb(h, s, l, &r, &g, &b);
        c->pixels[i] = (p & 0xFF000000u) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
}

void canvas_adjust_levels(Canvas *c, int in_black, int in_white, double gamma, int out_black, int out_white) {
    uint8_t lut[256];

    if (!c || !c->pixels) {
        return;
    }
    if (in_black < 0) in_black = 0;
    if (in_white > 255) in_white = 255;
    if (in_white <= in_black) in_white = in_black + 1;
    if (out_black < 0) out_black = 0;
    if (out_white > 255) out_white = 255;
    if (gamma < 0.01) gamma = 0.01;
    if (gamma > 9.99) gamma = 9.99;

    for (int i = 0; i < 256; i++) {
        double value = clamp_d((double)(i - in_black) / (double)(in_white - in_black), 0.0, 1.0);
        value = pow(value, 1.0 / gamma);
        lut[i] = clamp_u8((int)(out_black + value * (out_white - out_black) + 0.5));
    }
    apply_lut(c, lut);
}

void canvas_desaturate(Canvas *c) {
    size_t count;

    if (!c || !c->pixels) {
        return;
    }
    count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        int r = (p >> 16) & 0xFF;
        int g = (p >> 8) & 0xFF;
        int b = p & 0xFF;
        uint8_t gray = clamp_u8((r * 77 + g * 151 + b * 28) >> 8);
        c->pixels[i] = (p & 0xFF000000u) | ((uint32_t)gray << 16) | ((uint32_t)gray << 8) | gray;
    }
}

void canvas_posterize(Canvas *c, int levels) {
    uint8_t lut[256];

    if (!c || !c->pixels) {
        return;
    }
    if (levels < 2) levels = 2;
    if (levels > 255) levels = 255;

    for (int i = 0; i < 256; i++) {
        int bucket = i * levels / 256;
        lut[i] = clamp_u8(bucket * 255 / (levels - 1));
    }
    apply_lut(c, lut);
}

void canvas_threshold(Canvas *c, int level) {
    size_t count;

    if (!c || !c->pixels) {
        return;
    }
    if (level < 1) level = 1;
    if (level > 255) level = 255;

    count = (size_t)c->width * (size_t)c->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        int r = (p >> 16) & 0xFF;
        int g = (p >> 8) & 0xFF;
        int b = p & 0xFF;
        int luma = (r * 77 + g * 151 + b * 28) >> 8;
        uint32_t value = luma >= level ? 0x00FFFFFFu : 0x00000000u;
        c->pixels[i] = (p & 0xFF000000u) | value;
    }
}

static void box_blur_pass(const uint32_t *src, uint32_t *dst, int width, int height, int radius) {
    int window = radius * 2 + 1;

    for (int y = 0; y < height; y++) {
        const uint32_t *row = src + (size_t)y * (size_t)width;
        uint32_t *out = dst + (size_t)y * (size_t)width;
        int sum_a = 0, sum_r = 0, sum_g = 0, sum_b = 0;

        for (int x = -radius; x <= radius; x++) {
            int cx = x < 0 ? 0 : (x >= width ? width - 1 : x);
            uint32_t p = row[cx];
            sum_a += (p >> 24) & 0xFF;
            sum_r += (p >> 16) & 0xFF;
            sum_g += (p >> 8) & 0xFF;
            sum_b += p & 0xFF;
        }
        for (int x = 0; x < width; x++) {
            out[x] = ((uint32_t)(sum_a / window) << 24) |
                     ((uint32_t)(sum_r / window) << 16) |
                     ((uint32_t)(sum_g / window) << 8) |
                     (uint32_t)(sum_b / window);
            {
                int add = x + radius + 1;
                int sub = x - radius;
                if (add >= width) add = width - 1;
                if (sub < 0) sub = 0;
                uint32_t pa = row[add];
                uint32_t ps = row[sub];
                sum_a += (int)((pa >> 24) & 0xFF) - (int)((ps >> 24) & 0xFF);
                sum_r += (int)((pa >> 16) & 0xFF) - (int)((ps >> 16) & 0xFF);
                sum_g += (int)((pa >> 8) & 0xFF) - (int)((ps >> 8) & 0xFF);
                sum_b += (int)(pa & 0xFF) - (int)(ps & 0xFF);
            }
        }
    }
}

static void transpose(const uint32_t *src, uint32_t *dst, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dst[(size_t)x * (size_t)height + (size_t)y] = src[(size_t)y * (size_t)width + (size_t)x];
        }
    }
}

void canvas_gaussian_blur(Canvas *c, int radius) {
    size_t count;
    uint32_t *a;
    uint32_t *b;

    if (!c || !c->pixels || radius <= 0) {
        return;
    }
    if (radius > 64) radius = 64;

    count = (size_t)c->width * (size_t)c->height;
    a = (uint32_t *)malloc(count * sizeof(uint32_t));
    b = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!a || !b) {
        free(a);
        free(b);
        return;
    }

    box_blur_pass(c->pixels, a, c->width, c->height, radius);
    box_blur_pass(a, b, c->width, c->height, radius);
    box_blur_pass(b, a, c->width, c->height, radius);
    transpose(a, b, c->width, c->height);
    box_blur_pass(b, a, c->height, c->width, radius);
    box_blur_pass(a, b, c->height, c->width, radius);
    box_blur_pass(b, a, c->height, c->width, radius);
    transpose(a, c->pixels, c->height, c->width);

    free(a);
    free(b);
}

void canvas_sharpen(Canvas *c, int amount_percent) {
    size_t count;
    uint32_t *blurred;

    if (!c || !c->pixels || amount_percent <= 0) {
        return;
    }
    if (amount_percent > 500) amount_percent = 500;

    count = (size_t)c->width * (size_t)c->height;
    blurred = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!blurred) {
        return;
    }
    memcpy(blurred, c->pixels, count * sizeof(uint32_t));
    {
        Canvas tmp = {c->width, c->height, blurred};
        canvas_gaussian_blur(&tmp, 1);
    }

    for (size_t i = 0; i < count; i++) {
        uint32_t p = c->pixels[i];
        uint32_t q = blurred[i];
        int r = (int)((p >> 16) & 0xFF);
        int g = (int)((p >> 8) & 0xFF);
        int b = (int)(p & 0xFF);
        r += (r - (int)((q >> 16) & 0xFF)) * amount_percent / 100;
        g += (g - (int)((q >> 8) & 0xFF)) * amount_percent / 100;
        b += (b - (int)(q & 0xFF)) * amount_percent / 100;
        c->pixels[i] = (p & 0xFF000000u) | ((uint32_t)clamp_u8(r) << 16) | ((uint32_t)clamp_u8(g) << 8) | clamp_u8(b);
    }
    free(blurred);
}
