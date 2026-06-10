#include "psd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_u16(FILE *f, uint16_t value) {
    uint8_t bytes[2] = {(uint8_t)(value >> 8), (uint8_t)value};
    fwrite(bytes, 1, 2, f);
}

static void write_u32(FILE *f, uint32_t value) {
    uint8_t bytes[4] = {(uint8_t)(value >> 24), (uint8_t)(value >> 16), (uint8_t)(value >> 8), (uint8_t)value};
    fwrite(bytes, 1, 4, f);
}

static int read_u16(FILE *f, uint16_t *value) {
    uint8_t bytes[2];
    if (fread(bytes, 1, 2, f) != 2) {
        return 0;
    }
    *value = (uint16_t)((bytes[0] << 8) | bytes[1]);
    return 1;
}

static int read_u32(FILE *f, uint32_t *value) {
    uint8_t bytes[4];
    if (fread(bytes, 1, 4, f) != 4) {
        return 0;
    }
    *value = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | bytes[3];
    return 1;
}

int psd_save_canvas(const Canvas *c, const char *path) {
    FILE *f;
    static const int shifts[3] = {16, 8, 0};

    if (!c || !c->pixels || !path) {
        return 0;
    }
    f = fopen(path, "wb");
    if (!f) {
        return 0;
    }

    fwrite("8BPS", 1, 4, f);
    write_u16(f, 1);
    for (int i = 0; i < 6; i++) {
        fputc(0, f);
    }
    write_u16(f, 3);
    write_u32(f, (uint32_t)c->height);
    write_u32(f, (uint32_t)c->width);
    write_u16(f, 8);
    write_u16(f, 3);

    write_u32(f, 0);
    write_u32(f, 0);
    write_u32(f, 0);

    write_u16(f, 0);
    for (int channel = 0; channel < 3; channel++) {
        for (int y = 0; y < c->height; y++) {
            for (int x = 0; x < c->width; x++) {
                uint32_t p = c->pixels[(size_t)y * (size_t)c->width + (size_t)x];
                fputc((int)((p >> shifts[channel]) & 0xFF), f);
            }
        }
    }

    if (fclose(f) != 0) {
        return 0;
    }
    return 1;
}

static int decode_rle_row(FILE *f, uint8_t *row, int width, int byte_count) {
    int written = 0;
    int consumed = 0;

    while (consumed < byte_count && written < width) {
        int control = fgetc(f);
        if (control == EOF) {
            return 0;
        }
        consumed++;
        if (control == 128) {
            continue;
        }
        if (control < 128) {
            int run = control + 1;
            for (int i = 0; i < run; i++) {
                int byte = fgetc(f);
                if (byte == EOF) {
                    return 0;
                }
                consumed++;
                if (written < width) {
                    row[written++] = (uint8_t)byte;
                }
            }
        } else {
            int run = 257 - control;
            int byte = fgetc(f);
            if (byte == EOF) {
                return 0;
            }
            consumed++;
            for (int i = 0; i < run && written < width; i++) {
                row[written++] = (uint8_t)byte;
            }
        }
    }
    while (consumed < byte_count) {
        if (fgetc(f) == EOF) {
            return 0;
        }
        consumed++;
    }
    return written == width;
}

int psd_load_canvas(Canvas *out, const char *path) {
    FILE *f;
    char signature[4];
    uint16_t version;
    uint16_t channels;
    uint32_t height;
    uint32_t width;
    uint16_t depth;
    uint16_t mode;
    uint32_t section_length;
    uint16_t compression;
    int ok = 0;
    uint16_t *rle_counts = NULL;
    uint8_t *row = NULL;

    if (!out || !path) {
        return 0;
    }
    f = fopen(path, "rb");
    if (!f) {
        return 0;
    }

    if (fread(signature, 1, 4, f) != 4 || memcmp(signature, "8BPS", 4) != 0) {
        goto done;
    }
    if (!read_u16(f, &version) || version != 1) {
        goto done;
    }
    if (fseek(f, 6, SEEK_CUR) != 0) {
        goto done;
    }
    if (!read_u16(f, &channels) || channels < 3 || channels > 4) {
        goto done;
    }
    if (!read_u32(f, &height) || !read_u32(f, &width)) {
        goto done;
    }
    if (width == 0 || height == 0 || width > 16384 || height > 16384) {
        goto done;
    }
    if (!read_u16(f, &depth) || depth != 8) {
        goto done;
    }
    if (!read_u16(f, &mode) || mode != 3) {
        goto done;
    }

    for (int section = 0; section < 3; section++) {
        if (!read_u32(f, &section_length) || fseek(f, (long)section_length, SEEK_CUR) != 0) {
            goto done;
        }
    }

    if (!read_u16(f, &compression) || compression > 1) {
        goto done;
    }

    if (!canvas_init(out, (int)width, (int)height)) {
        goto done;
    }
    canvas_clear(out, 0xFF000000);

    row = (uint8_t *)malloc(width);
    if (!row) {
        goto fail_canvas;
    }

    if (compression == 1) {
        size_t total_rows = (size_t)channels * (size_t)height;
        rle_counts = (uint16_t *)malloc(total_rows * sizeof(uint16_t));
        if (!rle_counts) {
            goto fail_canvas;
        }
        for (size_t i = 0; i < total_rows; i++) {
            if (!read_u16(f, &rle_counts[i])) {
                goto fail_canvas;
            }
        }
    }

    for (int channel = 0; channel < (int)channels; channel++) {
        int shift = channel == 0 ? 16 : channel == 1 ? 8 : channel == 2 ? 0 : 24;
        for (uint32_t y = 0; y < height; y++) {
            if (compression == 1) {
                if (!decode_rle_row(f, row, (int)width, rle_counts[(size_t)channel * height + y])) {
                    goto fail_canvas;
                }
            } else {
                if (fread(row, 1, width, f) != width) {
                    goto fail_canvas;
                }
            }
            for (uint32_t x = 0; x < width; x++) {
                size_t idx = (size_t)y * (size_t)width + (size_t)x;
                out->pixels[idx] = (out->pixels[idx] & ~((uint32_t)0xFF << shift)) | ((uint32_t)row[x] << shift);
            }
        }
    }

    ok = 1;
    goto done;

fail_canvas:
    canvas_free(out);
done:
    free(rle_counts);
    free(row);
    fclose(f);
    return ok;
}
