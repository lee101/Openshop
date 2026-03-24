#include "display_canvas.h"

#include <stddef.h>

const Canvas *current_display_canvas(int preview_active,
                                     const Canvas *preview_canvas,
                                     const Canvas *composite) {
    if (preview_active && preview_canvas && preview_canvas->pixels) {
        return preview_canvas;
    }
    return composite;
}

const uint32_t *current_display_pixels(int preview_active,
                                       const Canvas *preview_canvas,
                                       const Canvas *composite) {
    const Canvas *canvas = current_display_canvas(preview_active, preview_canvas, composite);
    return canvas ? canvas->pixels : NULL;
}
