#include "../src/adjust.h"
#include "../src/app_brush.h"
#include "../src/gradient.h"
#include "../src/selection.h"
#include "../src/brush_engine.h"
#include "../src/app_shape.h"
#include "../src/canvas.h"
#include "../src/image_io.h"
#include "../src/layers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHOT_WIDTH 1024
#define SHOT_HEIGHT 768
#define DOC_X 64
#define DOC_Y 52
#define DOC_WIDTH 800
#define DOC_HEIGHT 600
#define RIGHT_PANEL_X 882
#define BOTTOM_PANEL_Y 666
#define DOC_BG 0xFFFFFFFF

static void rect(Canvas *canvas, int x0, int y0, int x1, int y1, uint32_t color) {
    canvas_draw_rect_filled(canvas, x0, y0, x1, y1, color);
}

static void stroke(Canvas *canvas, int x0, int y0, int x1, int y1, uint32_t color) {
    canvas_draw_rect_outline(canvas, x0, y0, x1, y1, 1, color);
}

static void blit_canvas(Canvas *dest, const Canvas *src, int dx, int dy) {
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            canvas_set_pixel_raw(dest, dx + x, dy + y, canvas_get_pixel(src, x, y));
        }
    }
}

static void draw_toolbar_button(Canvas *shot, int index, int active) {
    int x = 14;
    int y = DOC_Y + 12 + index * 44;
    uint32_t border = active ? 0xFF5DADE2 : 0xFF4A4A50;
    uint32_t glyph = active ? 0xFFE7F3FF : 0xFFC9CDD3;

    rect(shot, x, y, x + 36, y + 34, active ? 0xFF2D4D68 : 0xFF242428);
    stroke(shot, x, y, x + 36, y + 34, border);

    if (index == 0) {
        app_draw_shape(shot, TOOL_LINE, x + 12, y + 9, x + 25, y + 17, 1, glyph);
        app_draw_shape(shot, TOOL_LINE, x + 25, y + 17, x + 13, y + 25, 1, glyph);
    } else if (index == 1) {
        rect(shot, x + 11, y + 9, x + 25, y + 23, glyph);
        rect(shot, x + 21, y + 21, x + 26, y + 26, glyph);
    } else if (index == 2) {
        rect(shot, x + 10, y + 22, x + 28, y + 26, glyph);
        rect(shot, x + 18, y + 8, x + 24, y + 24, glyph);
    } else if (index == 3) {
        stroke(shot, x + 9, y + 9, x + 28, y + 25, glyph);
    } else if (index == 4) {
        app_draw_shape(shot, TOOL_LINE, x + 9, y + 25, x + 28, y + 9, 1, glyph);
    } else if (index == 5) {
        stroke(shot, x + 9, y + 10, x + 28, y + 24, glyph);
        app_draw_shape(shot, TOOL_LINE, x + 9, y + 10, x + 28, y + 24, 1, glyph);
    } else {
        rect(shot, x + 9, y + 9, x + 18, y + 18, 0xFFE53935);
        rect(shot, x + 18, y + 9, x + 27, y + 18, 0xFF1E88E5);
        rect(shot, x + 9, y + 18, x + 18, y + 27, 0xFFFDD835);
        rect(shot, x + 18, y + 18, x + 27, y + 27, 0xFF43A047);
    }
}

static void draw_shell(Canvas *shot, int active_tool_index) {
    rect(shot, 0, 0, SHOT_WIDTH - 1, SHOT_HEIGHT - 1, 0xFF1F1F23);

    rect(shot, 0, 0, SHOT_WIDTH - 1, 23, 0xFF2B2B2F);
    for (int i = 0; i < 7; i++) {
        rect(shot, 18 + i * 58, 8, 52 + i * 58 + (i % 3) * 8, 13, 0xFFBFC3C9);
    }

    rect(shot, 0, 24, SHOT_WIDTH - 1, 51, 0xFF333338);
    rect(shot, 78, 34, 186, 42, 0xFF56565D);
    rect(shot, 206, 34, 270, 42, 0xFF56565D);
    rect(shot, 292, 34, 380, 42, 0xFF56565D);
    rect(shot, 404, 34, 456, 42, 0xFF56565D);

    rect(shot, 0, 52, 63, SHOT_HEIGHT - 1, 0xFF2A2A2F);
    for (int i = 0; i < 8; i++) {
        draw_toolbar_button(shot, i, i == active_tool_index);
    }
    rect(shot, 15, 432, 31, 448, 0xFF1B1F24);
    rect(shot, 31, 448, 47, 464, 0xFFFDD835);
    stroke(shot, 14, 431, 32, 449, 0xFF0C0C0D);
    stroke(shot, 30, 447, 48, 465, 0xFF0C0C0D);

    rect(shot, RIGHT_PANEL_X, 52, 1008, SHOT_HEIGHT - 13, 0xFF2B2B30);
    rect(shot, RIGHT_PANEL_X + 10, 68, 988, 86, 0xFF3A3A40);
    rect(shot, RIGHT_PANEL_X + 10, 96, RIGHT_PANEL_X + 38, 124, 0xFFE53935);
    rect(shot, RIGHT_PANEL_X + 41, 96, RIGHT_PANEL_X + 69, 124, 0xFF43A047);
    rect(shot, RIGHT_PANEL_X + 72, 96, RIGHT_PANEL_X + 100, 124, 0xFF1E88E5);

    rect(shot, RIGHT_PANEL_X + 10, 150, 988, 168, 0xFF3A3A40);
    for (int i = 0; i < 4; i++) {
        int y = 182 + i * 42;
        stroke(shot, RIGHT_PANEL_X + 10, y - 5, 988, y + 29, i == 1 ? 0xFF5DADE2 : 0xFF484850);
        rect(shot, RIGHT_PANEL_X + 14, y, RIGHT_PANEL_X + 46, y + 24, i == 1 ? 0xFF5DADE2 : 0xFF5B6573);
        rect(shot, RIGHT_PANEL_X + 54, y + 5, RIGHT_PANEL_X + 102, y + 11, i == 1 ? 0xFFE8EDF5 : 0xFF8B929C);
        rect(shot, RIGHT_PANEL_X + 54, y + 17, RIGHT_PANEL_X + 90, y + 22, 0xFF626A73);
    }

    rect(shot, RIGHT_PANEL_X + 10, 392, 988, 410, 0xFF3A3A40);
    rect(shot, RIGHT_PANEL_X + 16, 428, RIGHT_PANEL_X + 108, 434, 0xFF5DADE2);
    rect(shot, RIGHT_PANEL_X + 16, 460, RIGHT_PANEL_X + 84, 466, 0xFFFDD835);
    rect(shot, RIGHT_PANEL_X + 16, 492, RIGHT_PANEL_X + 98, 498, 0xFF8E24AA);

    rect(shot, 64, BOTTOM_PANEL_Y, 864, 738, 0xFF252529);
    rect(shot, 82, BOTTOM_PANEL_Y + 18, 252, BOTTOM_PANEL_Y + 28, 0xFF7B7F87);
    rect(shot, 292, BOTTOM_PANEL_Y + 18, 388, BOTTOM_PANEL_Y + 28, 0xFF7B7F87);
    rect(shot, 430, BOTTOM_PANEL_Y + 18, 578, BOTTOM_PANEL_Y + 28, 0xFF7B7F87);
    rect(shot, 82, BOTTOM_PANEL_Y + 45, 392, BOTTOM_PANEL_Y + 53, 0xFF3F4147);

    rect(shot, DOC_X - 12, DOC_Y - 12, DOC_X + DOC_WIDTH + 11, DOC_Y + DOC_HEIGHT + 11, 0xFF111113);
    stroke(shot, DOC_X - 12, DOC_Y - 12, DOC_X + DOC_WIDTH + 11, DOC_Y + DOC_HEIGHT + 11, 0xFF333338);
}

static int save_shot_with_doc(const LayerStack *doc, const char *path, int active_tool_index) {
    Canvas shot;
    Canvas composite;
    int ok = 0;

    if (!canvas_init(&shot, SHOT_WIDTH, SHOT_HEIGHT) || !canvas_init(&composite, DOC_WIDTH, DOC_HEIGHT)) {
        canvas_free(&shot);
        canvas_free(&composite);
        return 0;
    }

    draw_shell(&shot, active_tool_index);
    layer_stack_composite(doc, &composite, DOC_BG);
    blit_canvas(&shot, &composite, DOC_X, DOC_Y);
    stroke(&shot, DOC_X - 1, DOC_Y - 1, DOC_X + DOC_WIDTH, DOC_Y + DOC_HEIGHT, 0xFF0B0B0D);
    ok = canvas_save_bmp(&shot, path);

    canvas_free(&composite);
    canvas_free(&shot);
    if (!ok) {
        fprintf(stderr, "visualbench: failed to save %s\n", path);
    }
    return ok;
}

static void draw_gradient(Canvas *canvas) {
    for (int y = 0; y < canvas->height; y++) {
        for (int x = 0; x < canvas->width; x++) {
            int r = 28 + (x * 165) / canvas->width;
            int g = 36 + (y * 118) / canvas->height;
            int b = 62 + ((canvas->width - x) * 128) / canvas->width;
            canvas_set_pixel_raw(canvas, x, y, 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
        }
    }
}

static void draw_sample_photo(Canvas *canvas) {
    draw_gradient(canvas);
    canvas_draw_ellipse_filled(canvas, 628, 132, 76, 76, 0xFFFFD36A);
    canvas_draw_ellipse_filled(canvas, 636, 124, 52, 52, 0xFFFFEAB0);
    rect(canvas, 0, 398, DOC_WIDTH - 1, DOC_HEIGHT - 1, 0xFF174F49);
    canvas_draw_ellipse_filled(canvas, 260, 424, 260, 94, 0xFF256F61);
    canvas_draw_ellipse_filled(canvas, 596, 430, 316, 120, 0xFF143D45);
    rect(canvas, 132, 268, 312, 452, 0xFFF5E7C8);
    rect(canvas, 196, 356, 244, 452, 0xFF3A2D32);
    app_draw_shape(canvas, TOOL_LINE, 102, 268, 222, 174, 5, 0xFFB34B3C);
    app_draw_shape(canvas, TOOL_LINE, 222, 174, 344, 268, 5, 0xFFB34B3C);
    app_draw_shape(canvas, TOOL_LINE, 102, 268, 344, 268, 5, 0xFFB34B3C);
    rect(canvas, 150, 310, 188, 350, 0xFF94BBD0);
    rect(canvas, 260, 310, 294, 350, 0xFF94BBD0);
}

static int make_editor_overview(void) {
    LayerStack doc;
    int ok = 0;

    if (!layer_stack_init(&doc, DOC_WIDTH, DOC_HEIGHT, DOC_BG)) {
        return 0;
    }

    draw_sample_photo(&doc.layers[0].canvas);
    int retouch = layer_stack_add(&doc, "Retouch", 0x00000000);
    int shape = layer_stack_add(&doc, "Vector shapes", 0x00000000);
    if (retouch < 0 || shape < 0) {
        goto done;
    }

    app_draw_brush_line(&doc.layers[retouch].canvas, 424, 228, 548, 280, 17, 0xCCFFD36A, BRUSH_SHAPE_ROUND);
    app_draw_brush_line(&doc.layers[retouch].canvas, 440, 304, 592, 360, 12, 0xBB38BDF8, BRUSH_SHAPE_DIAMOND);
    app_draw_shape(&doc.layers[shape].canvas, TOOL_RECT, 386, 118, 700, 392, 4, 0xCCFFFFFF);
    app_draw_shape(&doc.layers[shape].canvas, TOOL_ELLIPSE, 468, 172, 624, 326, 4, 0xCCF472B6);
    ok = save_shot_with_doc(&doc, "visualbench/editor-overview.bmp", 1);

done:
    layer_stack_free(&doc);
    return ok;
}

static int make_layer_states(void) {
    LayerStack doc;
    int ok = 0;

    if (!layer_stack_init(&doc, DOC_WIDTH, DOC_HEIGHT, 0xFF101820)) {
        return 0;
    }
    rect(&doc.layers[0].canvas, 80, 82, 720, 516, 0xFF243B55);

    int red = layer_stack_add(&doc, "Red paint", 0x00000000);
    int blue = layer_stack_add(&doc, "Blue paint", 0x00000000);
    int locked = layer_stack_add(&doc, "Locked guide", 0x00000000);
    if (red < 0 || blue < 0 || locked < 0) {
        goto done;
    }

    canvas_draw_ellipse_filled(&doc.layers[red].canvas, 316, 300, 164, 164, 0xDDEF4444);
    canvas_draw_ellipse_filled(&doc.layers[blue].canvas, 480, 300, 164, 164, 0xDD38BDF8);
    app_draw_shape(&doc.layers[locked].canvas, TOOL_LINE, 120, 110, 680, 490, 6, 0xEEFFFFFF);
    app_draw_shape(&doc.layers[locked].canvas, TOOL_LINE, 680, 110, 120, 490, 6, 0xEEFFFFFF);
    layer_stack_set_opacity(&doc, red, 78);
    layer_stack_set_opacity(&doc, blue, 62);
    layer_stack_toggle_lock(&doc, locked);
    doc.active_layer = blue;
    ok = save_shot_with_doc(&doc, "visualbench/layer-states.bmp", 1);

done:
    layer_stack_free(&doc);
    return ok;
}

static int make_tools_gallery(void) {
    LayerStack doc;
    int ok = 0;

    if (!layer_stack_init(&doc, DOC_WIDTH, DOC_HEIGHT, 0xFFF8FAFC)) {
        return 0;
    }

    int layer = layer_stack_add(&doc, "Tools", 0x00000000);
    if (layer < 0) {
        goto done;
    }

    Canvas *canvas = &doc.layers[layer].canvas;
    app_draw_brush_line(canvas, 70, 112, 330, 180, 18, 0xFF111827, BRUSH_SHAPE_ROUND);
    app_draw_brush_line(canvas, 90, 244, 338, 244, 14, 0xFFE11D48, BRUSH_SHAPE_SQUARE);
    app_draw_brush_line(canvas, 94, 330, 336, 430, 16, 0xFF7C3AED, BRUSH_SHAPE_DIAMOND);
    app_draw_shape(canvas, TOOL_LINE, 430, 96, 726, 174, 8, 0xFF2563EB);
    app_draw_shape(canvas, TOOL_RECT, 430, 220, 724, 330, 5, 0xFF059669);
    app_draw_shape(canvas, TOOL_FILLED_RECT, 448, 386, 596, 512, 1, 0xBBF59E0B);
    app_draw_shape(canvas, TOOL_ELLIPSE, 596, 366, 746, 526, 5, 0xFFDB2777);
    ok = save_shot_with_doc(&doc, "visualbench/tools-gallery.bmp", 1);

done:
    layer_stack_free(&doc);
    return ok;
}

static void draw_tool_base(Canvas *canvas) {
    draw_sample_photo(canvas);
    rect(canvas, 32, 34, 250, 84, 0xAA0F172A);
    rect(canvas, 52, 108, 180, 116, 0xCCF8FAFC);
    rect(canvas, 52, 130, 226, 138, 0xCCCBD5E1);
    rect(canvas, 52, 152, 146, 160, 0xCC94A3B8);
}

static int save_tool_screen(const char *path, int active_tool_index, void (*draw_tool)(Canvas *)) {
    LayerStack doc;
    int ok = 0;

    if (!layer_stack_init(&doc, DOC_WIDTH, DOC_HEIGHT, DOC_BG)) {
        return 0;
    }
    draw_tool_base(&doc.layers[0].canvas);

    int layer = layer_stack_add(&doc, "Tool preview", 0x00000000);
    if (layer < 0) {
        goto done;
    }
    draw_tool(&doc.layers[layer].canvas);
    ok = save_shot_with_doc(&doc, path, active_tool_index);

done:
    layer_stack_free(&doc);
    return ok;
}

static void draw_brush_tool(Canvas *canvas) {
    app_draw_brush_line(canvas, 312, 170, 620, 246, 24, 0xDDFDD835, BRUSH_SHAPE_ROUND);
    app_draw_brush_line(canvas, 332, 286, 664, 338, 14, 0xDD38BDF8, BRUSH_SHAPE_DIAMOND);
    canvas_draw_ellipse_filled(canvas, 686, 188, 18, 18, 0x99FDD835);
}

static void draw_eraser_tool(Canvas *canvas) {
    app_draw_brush_line(canvas, 290, 164, 628, 238, 30, 0xDDDB2777, BRUSH_SHAPE_ROUND);
    app_draw_brush_line(canvas, 364, 204, 532, 224, 34, DOC_BG, BRUSH_SHAPE_SQUARE);
    app_draw_brush_line(canvas, 388, 326, 646, 382, 18, 0xDD1D4ED8, BRUSH_SHAPE_ROUND);
    app_draw_brush_line(canvas, 452, 340, 552, 362, 22, DOC_BG, BRUSH_SHAPE_ROUND);
}

static void draw_line_tool(Canvas *canvas) {
    app_draw_shape(canvas, TOOL_LINE, 286, 180, 664, 388, 7, 0xFFE11D48);
    app_draw_shape(canvas, TOOL_LINE, 316, 396, 704, 172, 4, 0xFFF8FAFC);
    app_draw_shape(canvas, TOOL_LINE, 352, 112, 352, 496, 2, 0x8838BDF8);
}

static void draw_rectangle_tool(Canvas *canvas) {
    app_draw_shape(canvas, TOOL_RECT, 282, 150, 670, 414, 5, 0xFFF8FAFC);
    app_draw_shape(canvas, TOOL_FILLED_RECT, 338, 214, 538, 360, 1, 0xBB2563EB);
    app_draw_shape(canvas, TOOL_RECT, 396, 92, 740, 504, 2, 0x99FDD835);
}

static void draw_ellipse_tool(Canvas *canvas) {
    app_draw_shape(canvas, TOOL_ELLIPSE, 296, 134, 658, 430, 6, 0xFFF8FAFC);
    canvas_draw_ellipse_filled(canvas, 510, 292, 104, 72, 0xAA059669);
    app_draw_shape(canvas, TOOL_ELLIPSE, 430, 94, 734, 378, 3, 0xBBFDD835);
}

static void draw_fill_tool(Canvas *canvas) {
    rect(canvas, 300, 156, 656, 432, 0xCC1D4ED8);
    rect(canvas, 344, 202, 614, 386, 0xCCE11D48);
    canvas_draw_ellipse_filled(canvas, 490, 290, 98, 98, 0xCCFDD835);
    stroke(canvas, 300, 156, 656, 432, 0xFFF8FAFC);
}

static void draw_vfx_brushes(Canvas *canvas) {
    static const VfxBrushPreset presets[] = {
        VFX_BRUSH_SOFT_ROUND,
        VFX_BRUSH_AIRBRUSH,
        VFX_BRUSH_SPLATTER,
        VFX_BRUSH_GLOW,
        VFX_BRUSH_SPARKLE,
        VFX_BRUSH_SMOKE,
    };
    static const uint32_t colors[] = {
        0xFFE11D48,
        0xFF38BDF8,
        0xFFFDD835,
        0xFF8B5CF6,
        0xFFF8FAFC,
        0xFF94A3B8,
    };

    for (int i = 0; i < 6; i++) {
        BrushDynamics dyn = brush_dynamics_for_preset(presets[i], 14);
        int y = 110 + i * 78;
        brush_engine_stroke(canvas, 120, y, 700, y, &dyn, colors[i], 1000u + (uint32_t)i);
    }
}

static int make_vfx_brush_screen(void) {
    LayerStack doc;
    int ok = 0;

    if (!layer_stack_init(&doc, DOC_WIDTH, DOC_HEIGHT, 0xFF10141C)) {
        return 0;
    }

    int layer = layer_stack_add(&doc, "VFX", 0x00000000);
    if (layer < 0) {
        goto done;
    }
    draw_vfx_brushes(&doc.layers[layer].canvas);
    ok = save_shot_with_doc(&doc, "visualbench/vfx-brushes.bmp", 1);

done:
    layer_stack_free(&doc);
    return ok;
}

static int make_blend_modes_screen(void) {
    LayerStack doc;
    int ok = 0;

    if (!layer_stack_init(&doc, DOC_WIDTH, DOC_HEIGHT, DOC_BG)) {
        return 0;
    }
    draw_sample_photo(&doc.layers[0].canvas);

    int layer = layer_stack_add(&doc, "Blend", 0x00000000);
    if (layer < 0) {
        goto done;
    }

    for (int mode = 0; mode < BLEND_MODE_COUNT; mode++) {
        int col = mode % 7;
        int row = mode / 7;
        int x0 = 24 + col * 110;
        int y0 = 60 + row * 250;
        canvas_draw_rect_filled(&doc.layers[layer].canvas, x0, y0, x0 + 96, y0 + 220, 0xFF6D9EEB);
    }

    {
        Canvas shot = {0};
        Canvas composite = {0};
        if (!canvas_init(&shot, SHOT_WIDTH, SHOT_HEIGHT) || !canvas_init(&composite, DOC_WIDTH, DOC_HEIGHT)) {
            canvas_free(&shot);
            canvas_free(&composite);
            goto done;
        }
        draw_shell(&shot, 1);

        doc.layers[layer].visible = 0;
        layer_stack_composite(&doc, &composite, DOC_BG);
        blit_canvas(&shot, &composite, DOC_X, DOC_Y);
        doc.layers[layer].visible = 1;

        for (int mode = 0; mode < BLEND_MODE_COUNT; mode++) {
            doc.layers[layer].blend_mode = mode;
            layer_stack_composite(&doc, &composite, DOC_BG);
            int col = mode % 7;
            int row = mode / 7;
            int x0 = 24 + col * 110;
            int y0 = 60 + row * 250;
            for (int y = y0; y <= y0 + 220 && y < DOC_HEIGHT; y++) {
                for (int x = x0; x <= x0 + 96 && x < DOC_WIDTH; x++) {
                    canvas_set_pixel_raw(&shot, DOC_X + x, DOC_Y + y, canvas_get_pixel(&composite, x, y));
                }
            }
        }

        stroke(&shot, DOC_X - 1, DOC_Y - 1, DOC_X + DOC_WIDTH, DOC_Y + DOC_HEIGHT, 0xFF0B0B0D);
        ok = canvas_save_bmp(&shot, "visualbench/blend-modes.bmp");
        canvas_free(&shot);
        canvas_free(&composite);
        if (!ok) {
            fprintf(stderr, "visualbench: failed to save visualbench/blend-modes.bmp\n");
        }
    }

done:
    layer_stack_free(&doc);
    return ok;
}

static int make_adjustments_screen(void) {
    LayerStack doc;
    int ok = 0;

    if (!layer_stack_init(&doc, DOC_WIDTH, DOC_HEIGHT, DOC_BG)) {
        return 0;
    }
    draw_sample_photo(&doc.layers[0].canvas);
    {
        Canvas *canvas = &doc.layers[0].canvas;
        Canvas strip = {0};
        if (canvas_init(&strip, DOC_WIDTH, 100)) {
            for (int pass = 0; pass < 6; pass++) {
                for (int y = 0; y < 100; y++) {
                    for (int x = 0; x < DOC_WIDTH; x++) {
                        canvas_set_pixel_raw(&strip, x, y, canvas_get_pixel(canvas, x, pass * 100 + y));
                    }
                }
                switch (pass) {
                case 1: canvas_adjust_brightness_contrast(&strip, 35, 30); break;
                case 2: canvas_adjust_hue_saturation(&strip, 120, 40, 0); break;
                case 3: canvas_desaturate(&strip); break;
                case 4: canvas_posterize(&strip, 4); break;
                case 5: canvas_gaussian_blur(&strip, 4); break;
                default: break;
                }
                for (int y = 0; y < 100; y++) {
                    for (int x = 0; x < DOC_WIDTH; x++) {
                        canvas_set_pixel_raw(canvas, x, pass * 100 + y, canvas_get_pixel(&strip, x, y));
                    }
                }
            }
            canvas_free(&strip);
        }
    }
    ok = save_shot_with_doc(&doc, "visualbench/adjustments.bmp", 1);
    layer_stack_free(&doc);
    return ok;
}

static void draw_mask_ants(Canvas *shot, const Selection *sel) {
    for (int y = 0; y < sel->height; y++) {
        for (int x = 0; x < sel->width; x++) {
            int edge;
            if (selection_coverage(sel, x, y) <= 127) {
                continue;
            }
            edge = x == 0 || y == 0 || x == sel->width - 1 || y == sel->height - 1 ||
                   selection_coverage(sel, x - 1, y) <= 127 || selection_coverage(sel, x + 1, y) <= 127 ||
                   selection_coverage(sel, x, y - 1) <= 127 || selection_coverage(sel, x, y + 1) <= 127;
            if (edge) {
                canvas_set_pixel_raw(shot, DOC_X + x, DOC_Y + y, ((x + y) >> 2 & 1) ? 0xFFFFFFFF : 0xFF000000);
            }
        }
    }
}

static int make_selection_screen(void) {
    LayerStack doc;
    Selection sel = {0};
    Canvas shot = {0};
    Canvas composite = {0};
    uint32_t *backup = NULL;
    int ok = 0;

    if (!layer_stack_init(&doc, DOC_WIDTH, DOC_HEIGHT, DOC_BG)) {
        return 0;
    }
    draw_sample_photo(&doc.layers[0].canvas);

    if (!selection_init(&sel, DOC_WIDTH, DOC_HEIGHT)) {
        goto done;
    }
    selection_select_ellipse(&sel, 180, 110, 620, 490, SELECTION_REPLACE);
    selection_select_rect(&sel, 60, 60, 220, 180, SELECTION_ADD);
    selection_feather(&sel, 6);

    backup = (uint32_t *)malloc((size_t)DOC_WIDTH * (size_t)DOC_HEIGHT * sizeof(uint32_t));
    if (!backup) {
        goto done;
    }
    memcpy(backup, doc.layers[0].canvas.pixels, (size_t)DOC_WIDTH * (size_t)DOC_HEIGHT * sizeof(uint32_t));
    canvas_adjust_hue_saturation(&doc.layers[0].canvas, 140, 50, 0);
    selection_clamp_edit(&sel, &doc.layers[0].canvas, backup);

    if (!canvas_init(&shot, SHOT_WIDTH, SHOT_HEIGHT) || !canvas_init(&composite, DOC_WIDTH, DOC_HEIGHT)) {
        goto done;
    }
    draw_shell(&shot, 1);
    layer_stack_composite(&doc, &composite, DOC_BG);
    blit_canvas(&shot, &composite, DOC_X, DOC_Y);
    draw_mask_ants(&shot, &sel);
    stroke(&shot, DOC_X - 1, DOC_Y - 1, DOC_X + DOC_WIDTH, DOC_Y + DOC_HEIGHT, 0xFF0B0B0D);
    ok = canvas_save_bmp(&shot, "visualbench/selection-masked-edit.bmp");
    if (!ok) {
        fprintf(stderr, "visualbench: failed to save visualbench/selection-masked-edit.bmp\n");
    }

done:
    free(backup);
    canvas_free(&shot);
    canvas_free(&composite);
    selection_free(&sel);
    layer_stack_free(&doc);
    return ok;
}

static int make_gradient_screen(void) {
    LayerStack doc;
    int ok = 0;

    if (!layer_stack_init(&doc, DOC_WIDTH, DOC_HEIGHT, DOC_BG)) {
        return 0;
    }

    {
        Canvas *canvas = &doc.layers[0].canvas;
        Canvas strip = {0};
        if (!canvas_init(&strip, DOC_WIDTH, 190)) {
            goto done;
        }

        canvas_gradient_fill(&strip, 0, 0, DOC_WIDTH - 1, 0, 0xFF0F172A, 0xFF38BDF8, GRADIENT_LINEAR);
        for (int y = 0; y < 190; y++) {
            for (int x = 0; x < DOC_WIDTH; x++) {
                canvas_set_pixel_raw(canvas, x, 10 + y, canvas_get_pixel(&strip, x, y));
            }
        }

        canvas_gradient_fill(&strip, 0, 95, DOC_WIDTH / 2, 95, 0xFFFDD835, 0xFFE11D48, GRADIENT_LINEAR);
        for (int y = 0; y < 190; y++) {
            for (int x = 0; x < DOC_WIDTH; x++) {
                canvas_set_pixel_raw(canvas, x, 205 + y, canvas_get_pixel(&strip, x, y));
            }
        }

        canvas_gradient_fill(&strip, DOC_WIDTH / 2, 95, DOC_WIDTH / 2 + 170, 95, 0xFFF8FAFC, 0xFF1D4ED8, GRADIENT_RADIAL);
        for (int y = 0; y < 190; y++) {
            for (int x = 0; x < DOC_WIDTH; x++) {
                canvas_set_pixel_raw(canvas, x, 400 + y, canvas_get_pixel(&strip, x, y));
            }
        }
        canvas_free(&strip);
    }
    ok = save_shot_with_doc(&doc, "visualbench/gradients.bmp", 1);

done:
    layer_stack_free(&doc);
    return ok;
}

static int make_tool_screens(void) {
    int ok = 1;

    ok = ok && save_tool_screen("visualbench/tool-brush.bmp", 1, draw_brush_tool);
    ok = ok && save_tool_screen("visualbench/tool-eraser.bmp", 2, draw_eraser_tool);
    ok = ok && save_tool_screen("visualbench/tool-line.bmp", 4, draw_line_tool);
    ok = ok && save_tool_screen("visualbench/tool-rectangle.bmp", 3, draw_rectangle_tool);
    ok = ok && save_tool_screen("visualbench/tool-ellipse.bmp", 5, draw_ellipse_tool);
    ok = ok && save_tool_screen("visualbench/tool-fill.bmp", 6, draw_fill_tool);
    return ok;
}

int main(void) {
    if (!make_editor_overview()) {
        return 1;
    }
    if (!make_layer_states()) {
        return 1;
    }
    if (!make_tools_gallery()) {
        return 1;
    }
    if (!make_tool_screens()) {
        return 1;
    }
    if (!make_vfx_brush_screen()) {
        return 1;
    }
    if (!make_blend_modes_screen()) {
        return 1;
    }
    if (!make_adjustments_screen()) {
        return 1;
    }
    if (!make_selection_screen()) {
        return 1;
    }
    if (!make_gradient_screen()) {
        return 1;
    }

    puts("visualbench wrote visualbench/editor-overview.bmp");
    puts("visualbench wrote visualbench/layer-states.bmp");
    puts("visualbench wrote visualbench/tools-gallery.bmp");
    puts("visualbench wrote visualbench/tool-*.bmp");
    puts("visualbench wrote visualbench/vfx-brushes.bmp");
    puts("visualbench wrote visualbench/blend-modes.bmp");
    puts("visualbench wrote visualbench/adjustments.bmp");
    puts("visualbench wrote visualbench/selection-masked-edit.bmp");
    puts("visualbench wrote visualbench/gradients.bmp");
    return 0;
}
