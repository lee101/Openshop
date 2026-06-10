#ifndef SELECTION_H
#define SELECTION_H

#include "canvas.h"
#include <stdint.h>

typedef enum {
    SELECTION_REPLACE = 0,
    SELECTION_ADD,
    SELECTION_SUBTRACT
} SelectionOp;

typedef struct {
    int width;
    int height;
    int active;
    uint8_t *mask;
} Selection;

int selection_init(Selection *sel, int width, int height);
void selection_free(Selection *sel);
int selection_resize(Selection *sel, int width, int height);
void selection_select_all(Selection *sel);
void selection_deselect(Selection *sel);
void selection_invert(Selection *sel);
void selection_select_rect(Selection *sel, int x0, int y0, int x1, int y1, SelectionOp op);
void selection_select_ellipse(Selection *sel, int x0, int y0, int x1, int y1, SelectionOp op);
int selection_magic_wand(Selection *sel, const Canvas *source, int x, int y, int tolerance, SelectionOp op);
void selection_feather(Selection *sel, int radius);
uint8_t selection_coverage(const Selection *sel, int x, int y);
int selection_is_empty(const Selection *sel);
int selection_bounds(const Selection *sel, int *x0, int *y0, int *x1, int *y1);
void selection_clamp_edit(const Selection *sel, Canvas *canvas, const uint32_t *original);

#endif
