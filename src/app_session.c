#include "app_session.h"

int app_session_should_cancel_shape(int key, int ctrl) {
    if (key == 27 || key == 1073742049 || key == 1073742053) {
        return 0;
    }
    if (ctrl && (key == 's' || key == 'o' || key == 'z' || key == 'y' || key == 'n' || key == 'u' || key == 'v' ||
                 key == 'm' || key == 'd' || key == 'e' || key == 'g' || key == 'h' || key == 'l' || key == 'k' ||
                 key == 'a' || key == 'r' || key == 'i' || key == '0' || key == ',' || key == '[' || key == ']' ||
                 key == '-' || key == '=' || key == '/' || key == 1073741898 || key == 1073741901 ||
                 key == '1' || key == '2' || key == '3' || key == '4' || key == '5' || key == '6' ||
                 key == '7' || key == '8' || key == 1073741910 || key == 1073741911)) {
        return 1;
    }

    switch (key) {
    case 'b':
    case 'e':
    case 'l':
    case 'r':
    case 't':
    case 'o':
    case 'p':
    case '[':
    case ']':
    case ',':
    case '.':
    case '-':
    case '=':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case 'c':
    case 'h':
    case 'v':
    case 'j':
    case 'x':
    case 'f':
    case 'i':
    case 1073741906:
    case 1073741905:
    case 1073741904:
    case 1073741903:
    case 1073741899:
    case 1073741902:
    case 1073741898:
    case 1073741901:
    case 127:
    case 8:
    case 1073741910:
    case 1073741911:
        return 1;
    default:
        return 0;
    }
}

AppSessionCommand app_session_command_for_key(int key, int ctrl, int shaping) {
    AppSessionCommand command = {0, 0, 0};

    if (shaping && app_session_should_cancel_shape(key, ctrl)) {
        command.cancel_shape = 1;
    }

    if (key != 27) {
        return command;
    }

    command.handled = 1;
    if (shaping) {
        command.cancel_shape = 1;
    } else {
        command.stop_running = 1;
    }
    return command;
}

int app_session_apply(AppSessionCommand command, AppSessionState *state) {
    if (!state || !command.handled) {
        return 0;
    }

    if (command.cancel_shape) {
        state->shaping = 0;
        state->preview_active = 0;
    }
    if (command.stop_running) {
        state->running = 0;
    }
    return 1;
}
