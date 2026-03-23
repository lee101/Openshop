#ifndef APP_TRANSLATION_H
#define APP_TRANSLATION_H

typedef struct {
    int handled;
    int dx;
    int dy;
} AppTranslationCommand;

AppTranslationCommand app_translation_command_for_key(int key, int shift);

#endif
