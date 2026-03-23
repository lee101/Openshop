#include "app_translation.h"

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
