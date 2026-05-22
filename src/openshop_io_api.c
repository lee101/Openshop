#include "openshop_api.h"
#include "image_io.h"

int openshop_save_bmp(OpenshopDocument *doc, const char *path) {
    const Canvas *composite = openshop_composite(doc);
    if (!composite || !canvas_save_bmp(composite, path)) {
        return 0;
    }
    openshop_document_mark_clean(doc);
    return 1;
}

int openshop_load_bmp_into_active(OpenshopDocument *doc, const char *path) {
    Layer *layer = 0;
    if (!doc || !path) {
        return 0;
    }
    layer = layer_stack_active(&doc->layers);
    if (!layer || layer->locked || !canvas_load_bmp(&layer->canvas, path, 0x00000000)) {
        return 0;
    }
    doc->dirty = 1;
    return 1;
}
