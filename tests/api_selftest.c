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

    if (!expect_true("set blend mode", openshop_set_layer_blend_mode(&doc, paint, BLEND_MULTIPLY))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("get blend mode", openshop_get_layer_blend_mode(&doc, paint) == BLEND_MULTIPLY)) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("reject invalid blend mode", !openshop_set_layer_blend_mode(&doc, paint, 999))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("dirty after blend mode", openshop_document_is_dirty(&doc))) {
        openshop_document_free(&doc);
        return 1;
    }

    if (!expect_true("vfx stroke", openshop_draw_vfx_stroke(&doc, 8, 30, 50, 30, 5, 0xFF00FFFF, VFX_BRUSH_AIRBRUSH, 77))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("reject invalid vfx preset", !openshop_draw_vfx_stroke(&doc, 8, 30, 50, 30, 5, 0xFF00FFFF, 999, 77))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("brightness contrast", openshop_adjust_brightness_contrast(&doc, 20, 10))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("hue saturation", openshop_adjust_hue_saturation(&doc, 45, 20, 0))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("levels", openshop_adjust_levels(&doc, 10, 240, 1.1, 0, 255))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("desaturate", openshop_desaturate_active(&doc))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("posterize", openshop_posterize_active(&doc, 4))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("threshold", openshop_threshold_active(&doc, 128))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("blur", openshop_blur_active(&doc, 2))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("sharpen", openshop_sharpen_active(&doc, 100))) {
        openshop_document_free(&doc);
        return 1;
    }

    if (!expect_true("select rect", openshop_select_rect(&doc, 0, 0, 31, 47, 0))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("has selection", openshop_has_selection(&doc))) {
        openshop_document_free(&doc);
        return 1;
    }
    {
        int bx0 = -1, by0 = -1, bx1 = -1, by1 = -1;
        if (!expect_true("selection bounds", openshop_selection_bounds(&doc, &bx0, &by0, &bx1, &by1))) {
            openshop_document_free(&doc);
            return 1;
        }
        if (!expect_true("bounds values", bx0 == 0 && by0 == 0 && bx1 == 31 && by1 == 47)) {
            openshop_document_free(&doc);
            return 1;
        }
    }

    if (!expect_true("reset active opacity", openshop_set_layer_opacity(&doc, openshop_document_active_layer(&doc), 100))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("masked shape", openshop_draw_shape(&doc, OPENSHOP_TOOL_FILLED_RECT, 0, 0, 63, 47, 1, 0xFF00FF00))) {
        openshop_document_free(&doc);
        return 1;
    }
    composite = openshop_composite(&doc);
    if (!expect_true("paint inside selection", canvas_get_pixel(composite, 10, 10) == 0xFF00FF00)) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("paint clipped outside selection", canvas_get_pixel(composite, 50, 10) != 0xFF00FF00)) {
        openshop_document_free(&doc);
        return 1;
    }

    if (!expect_true("invert selection", openshop_invert_selection(&doc))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("deselect", openshop_deselect(&doc))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("no selection after deselect", !openshop_has_selection(&doc))) {
        openshop_document_free(&doc);
        return 1;
    }

    if (!expect_true("select all", openshop_select_all(&doc))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("magic wand", openshop_magic_wand(&doc, 10, 10, 32, 0))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("feather", openshop_feather_selection(&doc, 2))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("reject bad selection op", !openshop_select_rect(&doc, 0, 0, 5, 5, 99))) {
        openshop_document_free(&doc);
        return 1;
    }
    openshop_deselect(&doc);

    if (!expect_true("gradient", openshop_gradient_fill(&doc, 0, 0, 63, 0, 0xFF000000, 0xFFFFFFFF, 0))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("reject bad gradient type", !openshop_gradient_fill(&doc, 0, 0, 63, 0, 0, 0, 99))) {
        openshop_document_free(&doc);
        return 1;
    }

    if (!expect_true("save psd", openshop_save_psd(&doc, "test-artifacts/api.psd"))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("load psd", openshop_load_psd_into_active(&doc, "test-artifacts/api.psd"))) {
        openshop_document_free(&doc);
        return 1;
    }

    if (!expect_true("resize image", openshop_resize_image(&doc, 32, 24))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("resized width", openshop_document_width(&doc) == 32)) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("resize canvas", openshop_resize_canvas(&doc, 48, 36, 8, 6))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("crop", openshop_crop(&doc, 8, 6, 39, 29))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("cropped width", openshop_document_width(&doc) == 32)) {
        openshop_document_free(&doc);
        return 1;
    }
    composite = openshop_composite(&doc);
    if (!expect_true("composite resized", composite->width == 32 && composite->height == 24)) {
        openshop_document_free(&doc);
        return 1;
    }

    {
        int xs[] = {2, 30, 2};
        int ys[] = {2, 2, 22};
        if (!expect_true("select polygon", openshop_select_polygon(&doc, xs, ys, 3, 0))) {
            openshop_document_free(&doc);
            return 1;
        }
        if (!expect_true("reject degenerate polygon", !openshop_select_polygon(&doc, xs, ys, 2, 0))) {
            openshop_document_free(&doc);
            return 1;
        }
    }

    if (!expect_true("add mask from selection", openshop_add_layer_mask(&doc, openshop_document_active_layer(&doc)))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("layer has mask", openshop_layer_has_mask(&doc, openshop_document_active_layer(&doc)))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("disable mask", openshop_set_layer_mask_enabled(&doc, openshop_document_active_layer(&doc), 0))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("remove mask", openshop_remove_layer_mask(&doc, openshop_document_active_layer(&doc), 0))) {
        openshop_document_free(&doc);
        return 1;
    }
    openshop_deselect(&doc);

    if (!expect_true("set clipping", openshop_set_layer_clipping(&doc, openshop_document_active_layer(&doc), 1))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("layer is clipping", openshop_layer_is_clipping(&doc, openshop_document_active_layer(&doc)))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("clear clipping", openshop_set_layer_clipping(&doc, openshop_document_active_layer(&doc), 0))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("reject clipping background", !openshop_set_layer_clipping(&doc, 0, 1))) {
        openshop_document_free(&doc);
        return 1;
    }

    if (!expect_true("clone stroke", openshop_clone_stroke(&doc, -8, 0, 16, 8, 24, 8, 3, 80))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("reject zero clone offset", !openshop_clone_stroke(&doc, 0, 0, 16, 8, 24, 8, 3, 80))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("dodge stroke", openshop_dodge_burn_stroke(&doc, 4, 4, 20, 4, 4, 50, 0))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("burn stroke", openshop_dodge_burn_stroke(&doc, 4, 12, 20, 12, 4, 50, 1))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("sponge stroke", openshop_sponge_stroke(&doc, 4, 18, 20, 18, 4, 50, 1))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("smudge stroke", openshop_smudge_stroke(&doc, 4, 20, 20, 20, 4, 70))) {
        openshop_document_free(&doc);
        return 1;
    }

    if (!expect_true("draw text", openshop_draw_text(&doc, 2, 2, "Hi", 1, 0xFF000000))) {
        openshop_document_free(&doc);
        return 1;
    }
    if (!expect_true("reject empty text", !openshop_draw_text(&doc, 2, 2, "", 1, 0xFF000000))) {
        openshop_document_free(&doc);
        return 1;
    }

    openshop_document_free(&doc);
    puts("openshop api selftest ok");
    return 0;
}
