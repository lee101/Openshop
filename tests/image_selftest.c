#include "../src/canvas.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static uint64_t fnv1a64(const uint32_t *pixels, size_t count) {
    uint64_t hash = 1469598103934665603ULL;
    for (size_t i = 0; i < count; i++) {
        uint32_t px = pixels[i];
        hash ^= (uint8_t)((px >> 0) & 0xFF);
        hash *= 1099511628211ULL;
        hash ^= (uint8_t)((px >> 8) & 0xFF);
        hash *= 1099511628211ULL;
        hash ^= (uint8_t)((px >> 16) & 0xFF);
        hash *= 1099511628211ULL;
        hash ^= (uint8_t)((px >> 24) & 0xFF);
        hash *= 1099511628211ULL;
    }
    return hash;
}

static int mkdir_if_missing(const char *path) {
    if (mkdir(path, 0777) == 0) {
        return 1;
    }
    if (errno == EEXIST) {
        return 1;
    }
    return 0;
}

static int write_u16_le(FILE *f, uint16_t v) {
    uint8_t b[2] = {(uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF)};
    return fwrite(b, 1, sizeof(b), f) == sizeof(b);
}

static int write_u32_le(FILE *f, uint32_t v) {
    uint8_t b[4] = {
        (uint8_t)(v & 0xFF),
        (uint8_t)((v >> 8) & 0xFF),
        (uint8_t)((v >> 16) & 0xFF),
        (uint8_t)((v >> 24) & 0xFF),
    };
    return fwrite(b, 1, sizeof(b), f) == sizeof(b);
}

static int canvas_write_bmp32(const Canvas *c, const char *path) {
    if (!c || !c->pixels || !path || c->width <= 0 || c->height <= 0) {
        return 0;
    }

    const uint32_t pixel_bytes = (uint32_t)c->width * (uint32_t)c->height * 4U;
    const uint32_t file_size = 54U + pixel_bytes;

    FILE *f = fopen(path, "wb");
    if (!f) {
        return 0;
    }

    int ok = 1;
    ok = ok && fwrite("BM", 1, 2, f) == 2;
    ok = ok && write_u32_le(f, file_size);
    ok = ok && write_u16_le(f, 0);
    ok = ok && write_u16_le(f, 0);
    ok = ok && write_u32_le(f, 54);

    ok = ok && write_u32_le(f, 40);
    ok = ok && write_u32_le(f, (uint32_t)c->width);
    ok = ok && write_u32_le(f, (uint32_t)c->height);
    ok = ok && write_u16_le(f, 1);
    ok = ok && write_u16_le(f, 32);
    ok = ok && write_u32_le(f, 0);
    ok = ok && write_u32_le(f, pixel_bytes);
    ok = ok && write_u32_le(f, 2835);
    ok = ok && write_u32_le(f, 2835);
    ok = ok && write_u32_le(f, 0);
    ok = ok && write_u32_le(f, 0);

    for (int y = c->height - 1; ok && y >= 0; y--) {
        for (int x = 0; x < c->width; x++) {
            uint32_t p = canvas_get_pixel(c, x, y);
            uint8_t bgra[4] = {
                (uint8_t)((p >> 0) & 0xFF),
                (uint8_t)((p >> 8) & 0xFF),
                (uint8_t)((p >> 16) & 0xFF),
                (uint8_t)((p >> 24) & 0xFF),
            };
            if (fwrite(bgra, 1, sizeof(bgra), f) != sizeof(bgra)) {
                ok = 0;
                break;
            }
        }
    }

    if (fclose(f) != 0) {
        ok = 0;
    }
    return ok;
}

static int assert_pixel(const Canvas *c, int x, int y, uint32_t expected, const char *label) {
    uint32_t actual = canvas_get_pixel(c, x, y);
    if (actual != expected) {
        fprintf(stderr, "%s: pixel (%d,%d) expected 0x%08X got 0x%08X\n", label, x, y, expected, actual);
        return 0;
    }
    return 1;
}

int main(void) {
    Canvas c = {0};
    if (!canvas_init(&c, 160, 120)) {
        fprintf(stderr, "canvas_init failed\n");
        return 1;
    }

    const uint32_t white = 0xFFFFFFFF;
    const uint32_t black = 0xFF111111;
    const uint32_t blue = 0xFF1E88E5;
    const uint32_t red = 0xFFE53935;
    const uint32_t green = 0xFF43A047;
    const uint32_t yellow = 0xFFFDD835;

    canvas_clear(&c, white);
    canvas_draw_rect_outline(&c, 6, 6, 153, 113, 2, black);
    canvas_draw_line(&c, 0, 0, 159, 119, 2, blue);
    canvas_draw_line(&c, 159, 0, 0, 119, 2, red);
    canvas_draw_circle(&c, 80, 60, 18, green);
    canvas_draw_ellipse_outline(&c, 80, 60, 50, 20, 1, yellow);
    if (!canvas_flood_fill(&c, 80, 60, 0xFF2222CC)) {
        fprintf(stderr, "canvas_flood_fill(center) failed\n");
        canvas_free(&c);
        return 1;
    }
    if (!canvas_flood_fill(&c, 10, 10, 0xFFEFEFEF)) {
        fprintf(stderr, "canvas_flood_fill(border area) failed\n");
        canvas_free(&c);
        return 1;
    }

    if (!mkdir_if_missing("test-artifacts")) {
        fprintf(stderr, "failed to create test-artifacts: %s\n", strerror(errno));
        canvas_free(&c);
        return 1;
    }

    if (!canvas_write_bmp32(&c, "test-artifacts/scene.bmp")) {
        fprintf(stderr, "failed writing test-artifacts/scene.bmp\n");
        canvas_free(&c);
        return 1;
    }

    int ok = 1;
    ok = ok && assert_pixel(&c, 80, 60, 0xFF2222CC, "center fill");
    ok = ok && assert_pixel(&c, 10, 10, 0xFFEFEFEF, "outer fill");
    ok = ok && assert_pixel(&c, 80, 40, 0xFFFDD835, "ellipse stroke");

    uint64_t hash = fnv1a64(c.pixels, (size_t)c.width * (size_t)c.height);
    const uint64_t expected_hash = 0x5F341D9AC3C3684EULL;
    if (hash != expected_hash) {
        fprintf(stderr, "scene hash mismatch expected=0x%016llX actual=0x%016llX\n",
                (unsigned long long)expected_hash,
                (unsigned long long)hash);
        ok = 0;
    }

    canvas_clear(&c, white);
    canvas_draw_rect_outline(&c, 12, 12, 148, 108, 1, black);
    canvas_draw_line(&c, 20, 20, 140, 100, 1, blue);
    if (!canvas_flood_fill(&c, 15, 15, red)) {
        fprintf(stderr, "canvas_flood_fill(region) failed\n");
        canvas_free(&c);
        return 1;
    }

    if (!canvas_write_bmp32(&c, "test-artifacts/fill_regions.bmp")) {
        fprintf(stderr, "failed writing test-artifacts/fill_regions.bmp\n");
        canvas_free(&c);
        return 1;
    }

    ok = ok && assert_pixel(&c, 15, 15, red, "filled region");
    ok = ok && assert_pixel(&c, 3, 3, white, "outside remains white");

    uint64_t hash2 = fnv1a64(c.pixels, (size_t)c.width * (size_t)c.height);
    const uint64_t expected_hash2 = 0xDA08D84CDFB18101ULL;
    if (hash2 != expected_hash2) {
        fprintf(stderr, "fill hash mismatch expected=0x%016llX actual=0x%016llX\n",
                (unsigned long long)expected_hash2,
                (unsigned long long)hash2);
        ok = 0;
    }

    canvas_free(&c);
    if (!ok) {
        return 1;
    }

    printf("image selftest ok\n");
    return 0;
}
