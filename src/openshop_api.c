#include "openshop_api.h"

#include "app_shape.h"

#include <stddef.h>
#include <stdlib.h>

static int valid_layer(const OpenshopDocument *doc, int index) {
    return doc && index >= 0 && index < doc->layers.layer_count;
}

static Layer *active_editable_layer(OpenshopDocument *doc) {
    Layer *layer = NULL;
    if (!doc) {
        return NULL;
    }
    layer = layer_stack_active(&doc->layers);
    if (!layer || layer->locked) {
        return NULL;
    }
    return layer;
}

static Tool to_tool(OpenshopTool tool) {
    switch (tool) {
    case OPENSHOP_TOOL_ERASER:
        return TOOL_ERASER;
    case OPENSHOP_TOOL_LINE:
        return TOOL_LINE;
    case OPENSHOP_TOOL_RECT:
        return TOOL_RECT;
    case OPENSHOP_TOOL_FILLED_RECT:
        return TOOL_FILLED_RECT;
    case OPENSHOP_TOOL_ELLIPSE:
        return TOOL_ELLIPSE;
    case OPENSHOP_TOOL_FILLED_ELLIPSE:
        return TOOL_FILLED_ELLIPSE;
    case OPENSHOP_TOOL_BRUSH:
    default:
        return TOOL_BRUSH;
    }
}

static int brush_shape_contains(OpenshopBrushShape shape, int dx, int dy, int radius) {
    int ax = abs(dx);
    int ay = abs(dy);

    if (radius <= 0) {
        return 0;
    }
    switch (shape) {
    case OPENSHOP_BRUSH_SQUARE:
        return ax <= radius && ay <= radius;
    case OPENSHOP_BRUSH_DIAMOND:
        return ax + ay <= radius;
    case OPENSHOP_BRUSH_ROUND:
    default:
        return dx * dx + dy * dy <= radius * radius;
    }
}

static void stamp_brush(Canvas *canvas, int cx, int cy, int radius, uint32_t color, OpenshopBrushShape shape, int erase) {
    if (!canvas || !canvas->pixels || radius <= 0) {
        return;
    }
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (!brush_shape_contains(shape, x, y, radius)) {
                continue;
            }
            if (erase) {
                canvas_set_pixel_raw(canvas, cx + x, cy + y, 0x00000000);
            } else {
                canvas_set_pixel(canvas, cx + x, cy + y, color);
            }
        }
    }
}

static void draw_brush_line(Canvas *canvas, int x0, int y0, int x1, int y1, int radius, uint32_t color, OpenshopBrushShape shape, int erase) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    if (!canvas || !canvas->pixels) {
        return;
    }

    for (;;) {
        int e2 = 0;
        stamp_brush(canvas, x0, y0, radius, color, shape, erase);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void mark_dirty(OpenshopDocument *doc) {
    if (doc) {
        doc->dirty = 1;
    }
}

int openshop_document_init(OpenshopDocument *doc, int width, int height, uint32_t background_color) {
    if (!doc || width <= 0 || height <= 0) {
        return 0;
    }
    doc->background_color = background_color;
    doc->dirty = 0;
    doc->composite.width = 0;
    doc->composite.height = 0;
    doc->composite.pixels = 0;
    if (!layer_stack_init(&doc->layers, width, height, background_color)) {
        return 0;
    }
    if (!canvas_init(&doc->composite, width, height)) {
        layer_stack_free(&doc->layers);
        return 0;
    }
    layer_stack_composite(&doc->layers, &doc->composite, background_color);
    return 1;
}

void openshop_document_free(OpenshopDocument *doc) {
    if (!doc) {
        return;
    }
    layer_stack_free(&doc->layers);
    canvas_free(&doc->composite);
    doc->background_color = 0;
    doc->dirty = 0;
}

int openshop_document_width(const OpenshopDocument *doc) {
    return doc ? doc->layers.width : 0;
}

int openshop_document_height(const OpenshopDocument *doc) {
    return doc ? doc->layers.height : 0;
}

int openshop_document_layer_count(const OpenshopDocument *doc) {
    return doc ? doc->layers.layer_count : 0;
}

int openshop_document_active_layer(const OpenshopDocument *doc) {
    return doc ? doc->layers.active_layer : -1;
}

int openshop_document_is_dirty(const OpenshopDocument *doc) {
    return doc ? doc->dirty : 0;
}

void openshop_document_mark_clean(OpenshopDocument *doc) {
    if (doc) {
        doc->dirty = 0;
    }
}

int openshop_add_layer(OpenshopDocument *doc, const char *name) {
    int index = -1;
    if (!doc) {
        return -1;
    }
    index = layer_stack_add(&doc->layers, name, 0x00000000);
    if (index >= 0) {
        mark_dirty(doc);
    }
    return index;
}

int openshop_insert_layer(OpenshopDocument *doc, int index, const char *name) {
    int inserted = -1;
    if (!doc) {
        return -1;
    }
    inserted = layer_stack_insert(&doc->layers, index, name, 0x00000000);
    if (inserted >= 0) {
        mark_dirty(doc);
    }
    return inserted;
}

int openshop_select_layer(OpenshopDocument *doc, int index) {
    if (!valid_layer(doc, index)) {
        return 0;
    }
    doc->layers.active_layer = index;
    return 1;
}

int openshop_duplicate_layer(OpenshopDocument *doc, int index, const char *name) {
    if (!valid_layer(doc, index) || !layer_stack_duplicate(&doc->layers, index, name)) {
        return 0;
    }
    mark_dirty(doc);
    return 1;
}

int openshop_delete_layer(OpenshopDocument *doc, int index) {
    if (!valid_layer(doc, index) || !layer_stack_delete(&doc->layers, index)) {
        return 0;
    }
    mark_dirty(doc);
    return 1;
}

int openshop_move_layer(OpenshopDocument *doc, int index, int direction) {
    if (!valid_layer(doc, index) || !layer_stack_move(&doc->layers, index, direction)) {
        return 0;
    }
    mark_dirty(doc);
    return 1;
}

int openshop_set_layer_visible(OpenshopDocument *doc, int index, int visible) {
    const Layer *layer = NULL;
    if (!valid_layer(doc, index)) {
        return 0;
    }
    layer = layer_stack_get(&doc->layers, index);
    if (!layer || layer->visible == !!visible) {
        return 1;
    }
    if (!layer_stack_toggle_visibility(&doc->layers, index)) {
        return 0;
    }
    mark_dirty(doc);
    return 1;
}

int openshop_set_layer_locked(OpenshopDocument *doc, int index, int locked) {
    const Layer *layer = NULL;
    if (!valid_layer(doc, index)) {
        return 0;
    }
    layer = layer_stack_get(&doc->layers, index);
    if (!layer || layer->locked == !!locked) {
        return 1;
    }
    if (!layer_stack_toggle_lock(&doc->layers, index)) {
        return 0;
    }
    mark_dirty(doc);
    return 1;
}

int openshop_set_layer_opacity(OpenshopDocument *doc, int index, int opacity_percent) {
    if (!valid_layer(doc, index) || !layer_stack_set_opacity(&doc->layers, index, opacity_percent)) {
        return 0;
    }
    mark_dirty(doc);
    return 1;
}

int openshop_set_layer_blend_mode(OpenshopDocument *doc, int index, int blend_mode) {
    if (!valid_layer(doc, index) || !blend_mode_valid(blend_mode)) {
        return 0;
    }
    if (!layer_stack_set_blend_mode(&doc->layers, index, blend_mode)) {
        return 1;
    }
    mark_dirty(doc);
    return 1;
}

int openshop_get_layer_blend_mode(const OpenshopDocument *doc, int index) {
    if (!valid_layer(doc, index)) {
        return -1;
    }
    return doc->layers.layers[index].blend_mode;
}

int openshop_rename_layer(OpenshopDocument *doc, int index, const char *name) {
    if (!valid_layer(doc, index) || !layer_stack_rename(&doc->layers, index, name)) {
        return 0;
    }
    mark_dirty(doc);
    return 1;
}

int openshop_clear_layer(OpenshopDocument *doc, int index) {
    if (!valid_layer(doc, index) || !layer_stack_clear_layer(&doc->layers, index, 0x00000000)) {
        return 0;
    }
    mark_dirty(doc);
    return 1;
}

int openshop_merge_down(OpenshopDocument *doc, int index) {
    if (!valid_layer(doc, index) || !layer_stack_merge_down(&doc->layers, index)) {
        return 0;
    }
    mark_dirty(doc);
    return 1;
}

int openshop_flatten(OpenshopDocument *doc) {
    if (!doc || !layer_stack_flatten(&doc->layers, doc->background_color)) {
        return 0;
    }
    mark_dirty(doc);
    return 1;
}

int openshop_draw_stroke(
    OpenshopDocument *doc,
    int x0,
    int y0,
    int x1,
    int y1,
    int radius,
    uint32_t argb,
    OpenshopBrushShape shape
) {
    Layer *layer = active_editable_layer(doc);
    if (!layer || radius <= 0) {
        return 0;
    }
    draw_brush_line(&layer->canvas, x0, y0, x1, y1, radius, argb, shape, 0);
    mark_dirty(doc);
    return 1;
}

int openshop_erase_stroke(
    OpenshopDocument *doc,
    int x0,
    int y0,
    int x1,
    int y1,
    int radius,
    OpenshopBrushShape shape
) {
    Layer *layer = active_editable_layer(doc);
    if (!layer || radius <= 0) {
        return 0;
    }
    draw_brush_line(&layer->canvas, x0, y0, x1, y1, radius, 0x00000000, shape, 1);
    mark_dirty(doc);
    return 1;
}

int openshop_draw_shape(
    OpenshopDocument *doc,
    OpenshopTool tool,
    int x0,
    int y0,
    int x1,
    int y1,
    int radius,
    uint32_t argb
) {
    Layer *layer = active_editable_layer(doc);
    if (!layer) {
        return 0;
    }
    app_draw_shape(&layer->canvas, to_tool(tool), x0, y0, x1, y1, radius, argb);
    mark_dirty(doc);
    return 1;
}

int openshop_fill(OpenshopDocument *doc, int x, int y, uint32_t argb) {
    Layer *layer = active_editable_layer(doc);
    if (!layer || !canvas_flood_fill(&layer->canvas, x, y, argb)) {
        return 0;
    }
    mark_dirty(doc);
    return 1;
}

int openshop_flip_active_horizontal(OpenshopDocument *doc) {
    Layer *layer = active_editable_layer(doc);
    if (!layer) {
        return 0;
    }
    canvas_flip_horizontal(&layer->canvas);
    mark_dirty(doc);
    return 1;
}

int openshop_flip_active_vertical(OpenshopDocument *doc) {
    Layer *layer = active_editable_layer(doc);
    if (!layer) {
        return 0;
    }
    canvas_flip_vertical(&layer->canvas);
    mark_dirty(doc);
    return 1;
}

int openshop_rotate_active_180(OpenshopDocument *doc) {
    Layer *layer = active_editable_layer(doc);
    if (!layer) {
        return 0;
    }
    canvas_rotate_180(&layer->canvas);
    mark_dirty(doc);
    return 1;
}

int openshop_invert_active_rgb(OpenshopDocument *doc) {
    Layer *layer = active_editable_layer(doc);
    if (!layer) {
        return 0;
    }
    canvas_invert_rgb(&layer->canvas);
    mark_dirty(doc);
    return 1;
}

int openshop_translate_active(OpenshopDocument *doc, int dx, int dy) {
    Layer *layer = active_editable_layer(doc);
    if (!layer) {
        return 0;
    }
    canvas_translate(&layer->canvas, dx, dy, 0x00000000);
    mark_dirty(doc);
    return 1;
}

int openshop_draw_vfx_stroke(
    OpenshopDocument *doc,
    int x0,
    int y0,
    int x1,
    int y1,
    int radius,
    uint32_t argb,
    int preset,
    uint32_t seed
) {
    Layer *layer = active_editable_layer(doc);
    BrushDynamics dyn;

    if (!layer || radius <= 0 || !vfx_brush_valid(preset)) {
        return 0;
    }
    dyn = brush_dynamics_for_preset((VfxBrushPreset)preset, radius);
    brush_engine_stroke(&layer->canvas, x0, y0, x1, y1, &dyn, argb, seed);
    mark_dirty(doc);
    return 1;
}

int openshop_adjust_brightness_contrast(OpenshopDocument *doc, int brightness, int contrast) {
    Layer *layer = active_editable_layer(doc);
    if (!layer) {
        return 0;
    }
    canvas_adjust_brightness_contrast(&layer->canvas, brightness, contrast);
    mark_dirty(doc);
    return 1;
}

int openshop_adjust_hue_saturation(OpenshopDocument *doc, int hue_degrees, int saturation, int lightness) {
    Layer *layer = active_editable_layer(doc);
    if (!layer) {
        return 0;
    }
    canvas_adjust_hue_saturation(&layer->canvas, hue_degrees, saturation, lightness);
    mark_dirty(doc);
    return 1;
}

int openshop_adjust_levels(OpenshopDocument *doc, int in_black, int in_white, double gamma, int out_black, int out_white) {
    Layer *layer = active_editable_layer(doc);
    if (!layer) {
        return 0;
    }
    canvas_adjust_levels(&layer->canvas, in_black, in_white, gamma, out_black, out_white);
    mark_dirty(doc);
    return 1;
}

int openshop_desaturate_active(OpenshopDocument *doc) {
    Layer *layer = active_editable_layer(doc);
    if (!layer) {
        return 0;
    }
    canvas_desaturate(&layer->canvas);
    mark_dirty(doc);
    return 1;
}

int openshop_posterize_active(OpenshopDocument *doc, int levels) {
    Layer *layer = active_editable_layer(doc);
    if (!layer || levels < 2) {
        return 0;
    }
    canvas_posterize(&layer->canvas, levels);
    mark_dirty(doc);
    return 1;
}

int openshop_threshold_active(OpenshopDocument *doc, int level) {
    Layer *layer = active_editable_layer(doc);
    if (!layer) {
        return 0;
    }
    canvas_threshold(&layer->canvas, level);
    mark_dirty(doc);
    return 1;
}

int openshop_blur_active(OpenshopDocument *doc, int radius) {
    Layer *layer = active_editable_layer(doc);
    if (!layer || radius <= 0) {
        return 0;
    }
    canvas_gaussian_blur(&layer->canvas, radius);
    mark_dirty(doc);
    return 1;
}

int openshop_sharpen_active(OpenshopDocument *doc, int amount_percent) {
    Layer *layer = active_editable_layer(doc);
    if (!layer || amount_percent <= 0) {
        return 0;
    }
    canvas_sharpen(&layer->canvas, amount_percent);
    mark_dirty(doc);
    return 1;
}

const Canvas *openshop_composite(OpenshopDocument *doc) {
    if (!doc) {
        return 0;
    }
    layer_stack_composite(&doc->layers, &doc->composite, doc->background_color);
    return &doc->composite;
}
