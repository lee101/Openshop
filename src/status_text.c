#include "status_text.h"

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
    default:
        return "Action failed";
    }
}
