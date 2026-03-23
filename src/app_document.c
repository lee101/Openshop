#include "app_document.h"

int app_document_apply(
    AppDocumentAction action,
    LayerStack *layers,
    AppDocumentState *state,
    const Canvas *preview_canvas,
    const Canvas *composite,
    uint32_t active_clear_color,
    const AppDocumentCallbacks *callbacks
) {
    if (!layers || !state || !callbacks) {
        return 0;
    }

    switch (action) {
    case APP_DOCUMENT_ACTION_SAVE: {
        const Canvas *save_canvas =
            (state->preview_active && preview_canvas && preview_canvas->pixels) ? preview_canvas : composite;
        return callbacks->save_canvas && callbacks->save_canvas(save_canvas, "output.bmp", callbacks->userdata);
    }
    case APP_DOCUMENT_ACTION_LOAD: {
        Layer *active = layer_stack_active(layers);
        if (!active || active->locked || !callbacks->load_canvas || !callbacks->push_snapshot) {
            return 0;
        }
        callbacks->push_snapshot(layers, callbacks->userdata);
        if (!callbacks->load_canvas(&active->canvas, "input.bmp", active_clear_color, callbacks->userdata)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    }
    case APP_DOCUMENT_ACTION_UNDO:
        if (!callbacks->restore_history) {
            return 0;
        }
        if (!callbacks->restore_history(layers, 0, callbacks->userdata)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_DOCUMENT_ACTION_REDO:
        if (!callbacks->restore_history) {
            return 0;
        }
        if (!callbacks->restore_history(layers, 1, callbacks->userdata)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_DOCUMENT_ACTION_RESET_OPACITY: {
        Layer *active = layer_stack_active(layers);
        if (!active || active->opacity_percent == 100 || !callbacks->push_snapshot) {
            return 0;
        }
        callbacks->push_snapshot(layers, callbacks->userdata);
        layer_stack_set_opacity(layers, layers->active_layer, 100);
        state->needs_composite = 1;
        return 1;
    }
    case APP_DOCUMENT_ACTION_SHOW_ALL:
        if (!callbacks->push_snapshot) {
            return 0;
        }
        callbacks->push_snapshot(layers, callbacks->userdata);
        if (!layer_stack_show_all(layers)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    case APP_DOCUMENT_ACTION_SHOW_ACTIVE:
        if (!callbacks->push_snapshot) {
            return 0;
        }
        callbacks->push_snapshot(layers, callbacks->userdata);
        if (!layer_stack_show(layers, layers->active_layer)) {
            return 0;
        }
        state->needs_composite = 1;
        return 1;
    default:
        return 0;
    }
}
