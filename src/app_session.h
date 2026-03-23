#ifndef APP_SESSION_H
#define APP_SESSION_H

typedef struct {
    int handled;
    int cancel_shape;
    int stop_running;
} AppSessionCommand;

typedef struct {
    int shaping;
    int preview_active;
    int running;
} AppSessionState;

int app_session_should_cancel_shape(int key, int ctrl);
AppSessionCommand app_session_command_for_key(int key, int ctrl, int shaping);
int app_session_apply(AppSessionCommand command, AppSessionState *state);

#endif
