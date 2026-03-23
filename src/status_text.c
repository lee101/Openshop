#include "status_text.h"

#include <stdio.h>

const char *status_text_action_error(StatusTextAction action) {
    switch (action) {
    case STATUS_LOCK_TOGGLE:
        return "Could not toggle layer lock";
    case STATUS_LOCK_AND_ADVANCE:
        return "Could not lock layer and advance";
    case STATUS_LOCK_AND_RETREAT:
        return "Could not lock layer and retreat";
    case STATUS_UNLOCK_ALL:
        return "Could not unlock all layers";
    case STATUS_SHOW_UNLOCKED_ONLY:
        return "Could not show unlocked layers only";
    case STATUS_SHOW_LOCKED_ONLY:
        return "Could not show locked layers only";
    case STATUS_SHOW_HIDDEN_LOCKED_ONLY:
        return "Could not show hidden locked layers only";
    case STATUS_SHOW_HIDDEN_UNLOCKED_ONLY:
        return "Could not show hidden unlocked layers only";
    case STATUS_INSERT_LAYER_ABOVE:
        return "Could not insert a layer above the active layer";
    case STATUS_INSERT_LAYER_BELOW:
        return "Could not insert a layer below the active layer";
    case STATUS_FLATTEN_LOCKED:
        return "Flatten failed (check for locked layers)";
    case STATUS_STAMP_VISIBLE_INTO_LOCKED:
        return "Stamp visible failed (active layer may be locked)";
    case STATUS_STAMP_VISIBLE_NEW:
        return "Could not stamp visible image into a new layer";
    case STATUS_DUPLICATE_LAYER:
        return "Could not duplicate layer";
    case STATUS_MOVE_LAYER_BOTTOM:
        return "Layer is already at the bottom";
    case STATUS_MOVE_LAYER_TOP:
        return "Layer is already at the top";
    case STATUS_HIDE_FINAL_VISIBLE:
        return "Cannot hide the final visible layer";
    case STATUS_TOGGLE_SOLO:
        return "Could not toggle solo mode";
    case STATUS_DELETE_FINAL_OR_LOCKED:
        return "Cannot delete the final or a locked layer";
    case STATUS_MERGE_DOWN_BLOCKED:
        return "No lower layer to merge into, or one of the layers is locked";
    case STATUS_MERGE_UP_BLOCKED:
        return "No upper layer to merge into, or one of the layers is locked";
    case STATUS_SAVE_OUTPUT_BMP:
        return "Failed to save output.bmp";
    case STATUS_ACTIVE_LAYER_LOCKED:
        return "Active layer is locked";
    case STATUS_LOAD_INPUT_BMP:
        return "Failed to load input.bmp";
    case STATUS_FILL_FAILED:
        return "Fill failed";
    default:
        return "Action failed";
    }
}

void format_status_text_max_layers(int max_layers, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return;
    }
    snprintf(buffer, buffer_size, "Max layers reached (%d)", max_layers);
}
