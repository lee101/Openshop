#include "layers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int layers_init(LayerStack *ls, int width, int height) {
    if (!ls || width <= 0 || height <= 0) {
        return 0;
    }
    memset(ls, 0, sizeof(*ls));
    if (!canvas_init(&ls->layers[0].canvas, width, height)) {
        return 0;
    }
    canvas_clear(&ls->layers[0].canvas, 0xFFFFFFFF); /* white background */
    snprintf(ls->layers[0].name, sizeof(ls->layers[0].name), "Background");
    ls->layers[0].opacity = 100;
    ls->layers[0].visible = 1;
    ls->count  = 1;
    ls->active = 0;
    return 1;
}

void layers_free(LayerStack *ls) {
    if (!ls) {
        return;
    }
    for (int i = 0; i < ls->count; i++) {
        canvas_free(&ls->layers[i].canvas);
    }
    ls->count  = 0;
    ls->active = 0;
}

int layers_add(LayerStack *ls) {
    if (!ls || ls->count >= MAX_LAYERS) {
        return 0;
    }
    int idx = ls->count;
    int w   = ls->layers[0].canvas.width;
    int h   = ls->layers[0].canvas.height;
    if (!canvas_init(&ls->layers[idx].canvas, w, h)) {
        return 0;
    }
    canvas_clear(&ls->layers[idx].canvas, 0x00000000); /* transparent */
    snprintf(ls->layers[idx].name, sizeof(ls->layers[idx].name), "Layer %d", idx + 1);
    ls->layers[idx].opacity = 100;
    ls->layers[idx].visible = 1;
    ls->count++;
    ls->active = idx;
    return 1;
}

void layers_delete(LayerStack *ls, int idx) {
    if (!ls || idx < 0 || idx >= ls->count || ls->count <= 1) {
        return;
    }
    canvas_free(&ls->layers[idx].canvas);
    for (int i = idx; i < ls->count - 1; i++) {
        ls->layers[i] = ls->layers[i + 1];
    }
    memset(&ls->layers[ls->count - 1], 0, sizeof(Layer));
    ls->count--;
    if (ls->active >= ls->count) {
        ls->active = ls->count - 1;
    }
}

static uint8_t blend_ch(uint8_t src, uint8_t dst, uint8_t alpha) {
    int inv = 255 - (int)alpha;
    return (uint8_t)(((int)src * (int)alpha + (int)dst * inv + 127) / 255);
}

void layers_composite(const LayerStack *ls, Canvas *dst) {
    if (!ls || !dst || !dst->pixels) {
        return;
    }
    canvas_clear(dst, 0x00000000);

    for (int li = 0; li < ls->count; li++) {
        const Layer *layer = &ls->layers[li];
        if (!layer->visible || !layer->canvas.pixels) {
            continue;
        }
        int layer_alpha = (layer->opacity * 255 + 50) / 100;

        int w = dst->width  < layer->canvas.width  ? dst->width  : layer->canvas.width;
        int h = dst->height < layer->canvas.height ? dst->height : layer->canvas.height;

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                uint32_t src_pix = layer->canvas.pixels[y * layer->canvas.width + x];
                uint8_t  sa = (uint8_t)((src_pix >> 24) & 0xFF);
                /* Apply layer-level opacity on top of per-pixel alpha */
                sa = (uint8_t)(((int)sa * layer_alpha + 127) / 255);
                if (sa == 0) {
                    continue;
                }

                uint32_t dst_pix = dst->pixels[y * dst->width + x];
                uint8_t sr = (uint8_t)((src_pix >> 16) & 0xFF);
                uint8_t sg = (uint8_t)((src_pix >>  8) & 0xFF);
                uint8_t sb = (uint8_t)( src_pix        & 0xFF);
                uint8_t dr = (uint8_t)((dst_pix >> 16) & 0xFF);
                uint8_t dg = (uint8_t)((dst_pix >>  8) & 0xFF);
                uint8_t db = (uint8_t)( dst_pix        & 0xFF);
                uint8_t da = (uint8_t)((dst_pix >> 24) & 0xFF);

                uint8_t out_r = blend_ch(sr, dr, sa);
                uint8_t out_g = blend_ch(sg, dg, sa);
                uint8_t out_b = blend_ch(sb, db, sa);
                uint8_t out_a = (uint8_t)(sa + ((int)da * (255 - (int)sa) + 127) / 255);

                dst->pixels[y * dst->width + x] =
                    ((uint32_t)out_a << 24) | ((uint32_t)out_r << 16) |
                    ((uint32_t)out_g <<  8) |  (uint32_t)out_b;
            }
        }
    }
}

void layers_move_up(LayerStack *ls, int idx) {
    if (!ls || idx < 0 || idx >= ls->count - 1) {
        return;
    }
    Layer tmp           = ls->layers[idx];
    ls->layers[idx]     = ls->layers[idx + 1];
    ls->layers[idx + 1] = tmp;
    if (ls->active == idx) {
        ls->active = idx + 1;
    } else if (ls->active == idx + 1) {
        ls->active = idx;
    }
}

void layers_move_down(LayerStack *ls, int idx) {
    if (!ls || idx <= 0 || idx >= ls->count) {
        return;
    }
    Layer tmp           = ls->layers[idx];
    ls->layers[idx]     = ls->layers[idx - 1];
    ls->layers[idx - 1] = tmp;
    if (ls->active == idx) {
        ls->active = idx - 1;
    } else if (ls->active == idx - 1) {
        ls->active = idx;
    }
}

Canvas *layers_active_canvas(LayerStack *ls) {
    if (!ls || ls->active < 0 || ls->active >= ls->count) {
        return NULL;
    }
    return &ls->layers[ls->active].canvas;
}
