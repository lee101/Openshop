#ifndef APP_DOCUMENT_H
#define APP_DOCUMENT_H

#include "canvas.h"
#include "layers.h"

typedef enum {
    APP_DOCUMENT_ACTION_SAVE = 0,
    APP_DOCUMENT_ACTION_LOAD,
    APP_DOCUMENT_ACTION_UNDO,
    APP_DOCUMENT_ACTION_REDO,
    APP_DOCUMENT_ACTION_RESET_OPACITY,
    APP_DOCUMENT_ACTION_SHOW_ALL,
    APP_DOCUMENT_ACTION_SHOW_ACTIVE
} AppDocumentAction;

typedef struct {
    int preview_active;
    int needs_composite;
} AppDocumentState;

typedef struct {
    int (*save_canvas)(const Canvas *canvas, const char *path, void *userdata);
    int (*load_canvas)(Canvas *canvas, const char *path, uint32_t clear_color, void *userdata);
    int (*restore_history)(LayerStack *layers, int redo_to_undo, void *userdata);
    void (*push_snapshot)(const LayerStack *layers, void *userdata);
    void *userdata;
} AppDocumentCallbacks;

int app_document_apply(
    AppDocumentAction action,
    LayerStack *layers,
    AppDocumentState *state,
    const Canvas *preview_canvas,
    const Canvas *composite,
    uint32_t active_clear_color,
    const AppDocumentCallbacks *callbacks
);

#endif
