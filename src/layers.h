#ifndef LAYERS_H
#define LAYERS_H

#include "canvas.h"

#define MAX_LAYERS 8

typedef struct {
    Canvas canvas;
    char   name[32];
    int    opacity; /* 0-100 */
    int    visible; /* 0 or 1 */
} Layer;

typedef struct {
    Layer layers[MAX_LAYERS];
    int   count;
    int   active; /* index of active layer */
} LayerStack;

/* Initialise with one white background layer. */
int     layers_init(LayerStack *ls, int width, int height);
void    layers_free(LayerStack *ls);

/* Add a new transparent layer on top; returns 0 if at max. */
int     layers_add(LayerStack *ls);

/* Delete layer at idx (must have at least 2 layers). */
void    layers_delete(LayerStack *ls, int idx);

/* Flatten all visible layers into dst using Porter-Duff over. */
void    layers_composite(const LayerStack *ls, Canvas *dst);

/* Reorder helpers; swap active index too. */
void    layers_move_up(LayerStack *ls, int idx);
void    layers_move_down(LayerStack *ls, int idx);

/* Returns pointer to the active layer's canvas, or NULL. */
Canvas *layers_active_canvas(LayerStack *ls);

#endif /* LAYERS_H */
