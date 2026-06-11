#include "../src/psd.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define portable_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define portable_mkdir(path) mkdir(path, 0777)
#endif

static void ensure_artifacts_dir(void) {
    if (portable_mkdir("test-artifacts") != 0) {
        assert(errno == EEXIST);
    }
}

static void test_roundtrip(void) {
    Canvas out = {0};
    Canvas in = {0};

    assert(canvas_init(&out, 33, 21));
    for (int y = 0; y < out.height; y++) {
        for (int x = 0; x < out.width; x++) {
            canvas_set_pixel_raw(&out, x, y, 0xFF000000u | ((uint32_t)(x * 7) << 16) | ((uint32_t)(y * 11) << 8) | (uint32_t)((x + y) * 3 & 0xFF));
        }
    }
    assert(psd_save_canvas(&out, "test-artifacts/roundtrip.psd"));
    assert(psd_load_canvas(&in, "test-artifacts/roundtrip.psd"));
    assert(in.width == out.width && in.height == out.height);
    for (int y = 0; y < out.height; y++) {
        for (int x = 0; x < out.width; x++) {
            assert(canvas_get_pixel(&in, x, y) == canvas_get_pixel(&out, x, y));
        }
    }
    canvas_free(&in);
    canvas_free(&out);
}

static void write_u16be(FILE *f, int value) {
    fputc((value >> 8) & 0xFF, f);
    fputc(value & 0xFF, f);
}

static void write_u32be(FILE *f, long value) {
    fputc((int)((value >> 24) & 0xFF), f);
    fputc((int)((value >> 16) & 0xFF), f);
    fputc((int)((value >> 8) & 0xFF), f);
    fputc((int)(value & 0xFF), f);
}

static void test_rle_import(void) {
    FILE *f = fopen("test-artifacts/rle.psd", "wb");
    Canvas in = {0};

    assert(f);
    fwrite("8BPS", 1, 4, f);
    write_u16be(f, 1);
    for (int i = 0; i < 6; i++) fputc(0, f);
    write_u16be(f, 3);
    write_u32be(f, 2);
    write_u32be(f, 4);
    write_u16be(f, 8);
    write_u16be(f, 3);
    write_u32be(f, 0);
    write_u32be(f, 0);
    write_u32be(f, 0);
    write_u16be(f, 1);
    for (int i = 0; i < 6; i++) {
        write_u16be(f, 2);
    }
    for (int i = 0; i < 6; i++) {
        fputc(0xFD, f);
        fputc(i < 2 ? 0x40 : (i < 4 ? 0x80 : 0xC0), f);
    }
    fclose(f);

    assert(psd_load_canvas(&in, "test-artifacts/rle.psd"));
    assert(in.width == 4 && in.height == 2);
    assert(canvas_get_pixel(&in, 0, 0) == 0xFF4080C0);
    assert(canvas_get_pixel(&in, 3, 1) == 0xFF4080C0);
    canvas_free(&in);
}

static void test_reject_bad(void) {
    Canvas in = {0};
    FILE *f = fopen("test-artifacts/bad.psd", "wb");

    assert(f);
    fwrite("NOPE", 1, 4, f);
    fclose(f);
    assert(!psd_load_canvas(&in, "test-artifacts/bad.psd"));
    assert(!psd_load_canvas(&in, "test-artifacts/does-not-exist.psd"));
}

int main(void) {
    ensure_artifacts_dir();
    test_roundtrip();
    test_rle_import();
    test_reject_bad();
    printf("psd selftest ok\n");
    return 0;
}
