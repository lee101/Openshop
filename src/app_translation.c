#include "app_translation.h"

#include <stddef.h>

AppTranslationCommand app_translation_command_for_key(int key, int shift) {
    int step = shift ? 10 : 1;

    switch (key) {
    case 1073741906: /* SDLK_UP */
        return (AppTranslationCommand){1, 0, -step};
    case 1073741905: /* SDLK_DOWN */
        return (AppTranslationCommand){1, 0, step};
    case 1073741904: /* SDLK_LEFT */
        return (AppTranslationCommand){1, -step, 0};
    case 1073741903: /* SDLK_RIGHT */
        return (AppTranslationCommand){1, step, 0};
    default:
        return (AppTranslationCommand){0, 0, 0};
    }
}

int app_translation_apply(
    AppTranslationCommand command,
    LayerStack *layers,
    AppTranslationState *state,
    uint32_t clear_color,
    const AppTranslationCallbacks *callbacks
) {
    Layer *active = NULL;

    if (!layers || !state || !command.handled || (command.dx == 0 && command.dy == 0)) {
        return 0;
    }

    active = layer_stack_active(layers);
    if (!active || active->locked || !active->canvas.pixels || !callbacks || !callbacks->translate_canvas) {
        return 0;
    }

    if (callbacks->push_snapshot) {
        callbacks->push_snapshot(layers, callbacks->userdata);
    }
    callbacks->translate_canvas(&active->canvas, command.dx, command.dy, clear_color, callbacks->userdata);
    state->needs_composite = 1;
    return 1;
}
