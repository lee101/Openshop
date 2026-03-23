#ifndef STATUS_TEXT_H
#define STATUS_TEXT_H

typedef enum {
    STATUS_LOCK_TOGGLE = 0,
    STATUS_LOCK_AND_ADVANCE,
    STATUS_LOCK_AND_RETREAT,
    STATUS_UNLOCK_ALL,
    STATUS_SHOW_UNLOCKED_ONLY,
    STATUS_SHOW_LOCKED_ONLY,
    STATUS_SHOW_HIDDEN_LOCKED_ONLY,
    STATUS_SHOW_HIDDEN_UNLOCKED_ONLY
} StatusTextAction;

const char *status_text_action_error(StatusTextAction action);

#endif
