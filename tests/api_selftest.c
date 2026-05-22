#include "../src/openshop_api.h"

#include <stdio.h>

static int expect_true(const char *label, int value) {
    if (!value) {
        fprintf(stderr, "%s failed\n", label);
        return 0;
    }
    return 1;
}

static int expect_eq_u32(const char *label, uint32_t got, uint32_t want) {
    if (got != want) {
        fprintf(stderr, "%s got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

int main(void) {
    OpenshopDocument doc;
    const Canvas *composite = 0;
    int paint = -1;

    if (!expect_true("document init", openshop_document_init(&doc, 64, 48, 0xFFFFFFFF))) {
        return 1;
    }
    if (!expect_true("initial layer count", openshop_document_layer_count(&doc) == 1)) {
        openshop_document_free(&doc);
        return 1;
    }

    paint = openshop_add_layer(&doc, "Paint");
    if (!expect_true("add layer", paint == 1)) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("draw stroke", openshop_draw_stroke(&doc, 8, 8, 40, 8, 4, 0xFFFF0000, OPENSHOP_BRUSH_ROUND))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("draw shape", openshop_draw_shape(&doc, OPENSHOP_TOOL_FILLED_RECT, 12, 18, 30, 30, 1, 0x800000FF))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("set opacity", openshop_set_layer_opacity(&doc, paint, 80))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("duplicate", openshop_duplicate_layer(&doc, paint, "Paint Copy"))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("translate", openshop_translate_active(&doc, 4, 3))) {
        openshop_document_free(&doc);
        return 1;
    }

    composite = openshop_composite(&doc);
    if (!expect_true("composite exists", composite != 0)) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_eq_u32("background pixel", canvas_get_pixel(composite, 0, 0), 0xFFFFFFFF)) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("painted pixel changed", canvas_get_pixel(composite, 14, 11) != 0xFFFFFFFF)) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("dirty after edits", openshop_document_is_dirty(&doc))) {
        openshop_document_free(&doc);
        return 1;
    }

    openshop_document_mark_clean(&doc);
    if (!expect_true("clean marker", !openshop_document_is_dirty(&doc))) {
        openshop_document_free(&doc);
        return 1;
    }

    openshop_document_free(&doc);
    puts("openshop api selftest ok");
    return 0;
}
