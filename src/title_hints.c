#include "title_hints.h"

#include <stdio.h>

void format_hidden_layer_hint(const LayerStack *layers, char *buffer, size_t buffer_size) {
    int hidden_locked = 0;
    int hidden_unlocked = 0;

    if (!buffer || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';
    if (!layers) {
        return;
    }

    for (int i = 0; i < layers->layer_count; i++) {
        if (!layers->layers[i].visible) {
            if (layers->layers[i].locked) {
                hidden_locked++;
            } else {
                hidden_unlocked++;
            }
        }
    }

    if (hidden_locked > 0 && hidden_unlocked > 0) {
        snprintf(buffer, buffer_size, " | hints hu C-A-;/' hl C-S-,/.");
    } else if (hidden_unlocked > 0) {
        snprintf(buffer, buffer_size, " | hint hu C-A-;/'");
    } else if (hidden_locked > 0) {
        snprintf(buffer, buffer_size, " | hint hl C-S-,/.");
    }
}
