#ifndef OPENSHOP_API_H
#define OPENSHOP_API_H

#include "canvas.h"
#include "layers.h"

#include <stdint.h>

typedef struct {
    LayerStack layers;
    Canvas composite;
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
int openshop_grayscale_active(OpenshopDocument *doc);
int openshop_adjust_active_brightness(OpenshopDocument *doc, int delta);
int openshop_posterize_active(OpenshopDocument *doc, int levels);

const Canvas *openshop_composite(OpenshopDocument *doc);
int openshop_save_bmp(OpenshopDocument *doc, const char *path);
int openshop_load_bmp_into_active(OpenshopDocument *doc, const char *path);

#endif
