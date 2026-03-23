#ifndef STATUS_TEXT_H
#define STATUS_TEXT_H

#include <stddef.h>

typedef enum {
    STATUS_LOCK_TOGGLE = 0,
    STATUS_LOCK_AND_ADVANCE,
    STATUS_LOCK_AND_RETREAT,
    STATUS_UNLOCK_ALL,
    STATUS_SHOW_UNLOCKED_ONLY,
    STATUS_SHOW_LOCKED_ONLY,
    STATUS_SHOW_HIDDEN_LOCKED_ONLY,
    STATUS_SHOW_HIDDEN_UNLOCKED_ONLY,
    STATUS_INSERT_LAYER_ABOVE,
    STATUS_INSERT_LAYER_BELOW,
    STATUS_FLATTEN_LOCKED,
    STATUS_STAMP_VISIBLE_INTO_LOCKED,
    STATUS_STAMP_VISIBLE_NEW,
    STATUS_DUPLICATE_LAYER,
    STATUS_MOVE_LAYER_BOTTOM,
    STATUS_MOVE_LAYER_TOP,
    STATUS_HIDE_FINAL_VISIBLE,
    STATUS_TOGGLE_SOLO,
    STATUS_DELETE_FINAL_OR_LOCKED,
    STATUS_MERGE_DOWN_BLOCKED,
    STATUS_MERGE_UP_BLOCKED,
    STATUS_SAVE_OUTPUT_BMP,
    STATUS_ACTIVE_LAYER_LOCKED,
    STATUS_LOAD_INPUT_BMP,
    STATUS_FILL_FAILED
} StatusTextAction;

const char *status_text_action_error(StatusTextAction action);
void format_status_text_max_layers(int max_layers, char *buffer, size_t buffer_size);
void format_status_text_startup(const char *label, char *buffer, size_t buffer_size);
void format_status_text_sdl(const char *label, const char *detail, char *buffer, size_t buffer_size);
void format_status_text_file_load(const char *path, char *buffer, size_t buffer_size);
void format_status_text_file_save(const char *path, char *buffer, size_t buffer_size);

#endif
