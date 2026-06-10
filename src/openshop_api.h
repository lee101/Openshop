#ifndef OPENSHOP_API_H
#define OPENSHOP_API_H

#include "adjust.h"
#include "blend.h"
#include "brush_engine.h"
#include "canvas.h"
#include "gradient.h"
#include "layers.h"
#include "psd.h"
#include "selection.h"

#include <stdint.h>

typedef struct {
    LayerStack layers;
    Canvas composite;
    Selection selection;
    uint32_t background_color;
    int dirty;
} OpenshopDocument;

typedef enum {
    OPENSHOP_TOOL_BRUSH = 0,
    OPENSHOP_TOOL_ERASER,
    OPENSHOP_TOOL_LINE,
    OPENSHOP_TOOL_RECT,
    OPENSHOP_TOOL_FILLED_RECT,
    OPENSHOP_TOOL_ELLIPSE,
    OPENSHOP_TOOL_FILLED_ELLIPSE
} OpenshopTool;

typedef enum {
    OPENSHOP_BRUSH_ROUND = 0,
    OPENSHOP_BRUSH_SQUARE,
    OPENSHOP_BRUSH_DIAMOND
} OpenshopBrushShape;

int openshop_document_init(OpenshopDocument *doc, int width, int height, uint32_t background_color);
void openshop_document_free(OpenshopDocument *doc);
int openshop_document_width(const OpenshopDocument *doc);
int openshop_document_height(const OpenshopDocument *doc);
int openshop_document_layer_count(const OpenshopDocument *doc);
int openshop_document_active_layer(const OpenshopDocument *doc);
int openshop_document_is_dirty(const OpenshopDocument *doc);
void openshop_document_mark_clean(OpenshopDocument *doc);

int openshop_add_layer(OpenshopDocument *doc, const char *name);
int openshop_insert_layer(OpenshopDocument *doc, int index, const char *name);
int openshop_select_layer(OpenshopDocument *doc, int index);
int openshop_duplicate_layer(OpenshopDocument *doc, int index, const char *name);
int openshop_delete_layer(OpenshopDocument *doc, int index);
int openshop_move_layer(OpenshopDocument *doc, int index, int direction);
int openshop_set_layer_visible(OpenshopDocument *doc, int index, int visible);
int openshop_set_layer_locked(OpenshopDocument *doc, int index, int locked);
int openshop_set_layer_opacity(OpenshopDocument *doc, int index, int opacity_percent);
int openshop_set_layer_blend_mode(OpenshopDocument *doc, int index, int blend_mode);
int openshop_get_layer_blend_mode(const OpenshopDocument *doc, int index);
int openshop_rename_layer(OpenshopDocument *doc, int index, const char *name);
int openshop_clear_layer(OpenshopDocument *doc, int index);
int openshop_merge_down(OpenshopDocument *doc, int index);
int openshop_flatten(OpenshopDocument *doc);

int openshop_draw_stroke(
    OpenshopDocument *doc,
    int x0,
    int y0,
    int x1,
    int y1,
    int radius,
    uint32_t argb,
    OpenshopBrushShape shape
);
int openshop_erase_stroke(
    OpenshopDocument *doc,
    int x0,
    int y0,
    int x1,
    int y1,
    int radius,
    OpenshopBrushShape shape
);
int openshop_draw_shape(
    OpenshopDocument *doc,
    OpenshopTool tool,
    int x0,
    int y0,
    int x1,
    int y1,
    int radius,
    uint32_t argb
);
int openshop_fill(OpenshopDocument *doc, int x, int y, uint32_t argb);
int openshop_flip_active_horizontal(OpenshopDocument *doc);
int openshop_flip_active_vertical(OpenshopDocument *doc);
int openshop_rotate_active_180(OpenshopDocument *doc);
int openshop_invert_active_rgb(OpenshopDocument *doc);
int openshop_translate_active(OpenshopDocument *doc, int dx, int dy);

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
);
int openshop_adjust_brightness_contrast(OpenshopDocument *doc, int brightness, int contrast);
int openshop_adjust_hue_saturation(OpenshopDocument *doc, int hue_degrees, int saturation, int lightness);
int openshop_adjust_levels(OpenshopDocument *doc, int in_black, int in_white, double gamma, int out_black, int out_white);
int openshop_desaturate_active(OpenshopDocument *doc);
int openshop_posterize_active(OpenshopDocument *doc, int levels);
int openshop_threshold_active(OpenshopDocument *doc, int level);
int openshop_blur_active(OpenshopDocument *doc, int radius);
int openshop_sharpen_active(OpenshopDocument *doc, int amount_percent);

int openshop_select_all(OpenshopDocument *doc);
int openshop_deselect(OpenshopDocument *doc);
int openshop_invert_selection(OpenshopDocument *doc);
int openshop_select_rect(OpenshopDocument *doc, int x0, int y0, int x1, int y1, int op);
int openshop_select_ellipse(OpenshopDocument *doc, int x0, int y0, int x1, int y1, int op);
int openshop_magic_wand(OpenshopDocument *doc, int x, int y, int tolerance, int op);
int openshop_feather_selection(OpenshopDocument *doc, int radius);
int openshop_has_selection(const OpenshopDocument *doc);
int openshop_selection_bounds(const OpenshopDocument *doc, int *x0, int *y0, int *x1, int *y1);

int openshop_gradient_fill(OpenshopDocument *doc, int x0, int y0, int x1, int y1, uint32_t start, uint32_t end, int type);

int openshop_resize_image(OpenshopDocument *doc, int new_width, int new_height);
int openshop_resize_canvas(OpenshopDocument *doc, int new_width, int new_height, int offset_x, int offset_y);
int openshop_crop(OpenshopDocument *doc, int x0, int y0, int x1, int y1);

int openshop_save_psd(OpenshopDocument *doc, const char *path);
int openshop_load_psd_into_active(OpenshopDocument *doc, const char *path);

const Canvas *openshop_composite(OpenshopDocument *doc);
int openshop_save_bmp(OpenshopDocument *doc, const char *path);
int openshop_load_bmp_into_active(OpenshopDocument *doc, const char *path);

#endif
