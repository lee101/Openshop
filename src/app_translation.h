#ifndef APP_TRANSLATION_H
#define APP_TRANSLATION_H

#include "layers.h"
#include <stdint.h>

typedef struct {
    int handled;
    int dx;
    int dy;
} AppTranslationCommand;

typedef struct {
    int needs_composite;
} AppTranslationState;

typedef struct {
    void (*push_snapshot)(const LayerStack *layers, void *userdata);
    void (*translate_canvas)(Canvas *canvas, int dx, int dy, uint32_t clear_color, void *userdata);
    void *userdata;
} AppTranslationCallbacks;

AppTranslationCommand app_translation_command_for_key(int key, int shift);
int app_translation_apply(
    AppTranslationCommand command,
    LayerStack *layers,
    AppTranslationState *state,
    uint32_t clear_color,
    const AppTranslationCallbacks *callbacks
);

#endif
