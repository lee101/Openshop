#include "../src/app_session.h"

#include <stdio.h>

static int expect_int_eq(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int test_shape_cancel_shortcuts(void) {
    AppSessionCommand ctrl_save = app_session_command_for_key('s', 1, 1);
    AppSessionCommand brush = app_session_command_for_key('b', 0, 1);
    AppSessionCommand shift = app_session_command_for_key(1073742049, 0, 1);

    return expect_int_eq("ctrl_save_handled", ctrl_save.handled, 0) &&
           expect_int_eq("ctrl_save_cancel", ctrl_save.cancel_shape, 1) &&
           expect_int_eq("brush_cancel", brush.cancel_shape, 1) &&
           expect_int_eq("shift_cancel", shift.cancel_shape, 0);
}

static int test_escape_cancels_shape_or_stops_running(void) {
    AppSessionCommand shaping = app_session_command_for_key(27, 0, 1);
    AppSessionCommand idle = app_session_command_for_key(27, 0, 0);
    AppSessionCommand unrelated = app_session_command_for_key('q', 0, 0);

    return expect_int_eq("escape_shape_handled", shaping.handled, 1) &&
           expect_int_eq("escape_shape_cancel", shaping.cancel_shape, 1) &&
           expect_int_eq("escape_shape_stop", shaping.stop_running, 0) &&
           expect_int_eq("escape_idle_handled", idle.handled, 1) &&
           expect_int_eq("escape_idle_cancel", idle.cancel_shape, 0) &&
           expect_int_eq("escape_idle_stop", idle.stop_running, 1) &&
           expect_int_eq("unrelated_handled", unrelated.handled, 0) &&
           expect_int_eq("unrelated_cancel", unrelated.cancel_shape, 0) &&
           expect_int_eq("unrelated_stop", unrelated.stop_running, 0);
}

static int test_apply_updates_session_state(void) {
    AppSessionState shaping = {.shaping = 1, .preview_active = 1, .running = 1};
    AppSessionState idle = {.shaping = 0, .preview_active = 0, .running = 1};
    AppSessionState both = {.shaping = 1, .preview_active = 1, .running = 1};
    AppSessionState untouched = {.shaping = 1, .preview_active = 1, .running = 1};

    if (!app_session_apply((AppSessionCommand){.handled = 1, .cancel_shape = 1, .stop_running = 0}, &shaping) ||
        !expect_int_eq("apply_shape_shaping", shaping.shaping, 0) ||
        !expect_int_eq("apply_shape_preview", shaping.preview_active, 0) ||
        !expect_int_eq("apply_shape_running", shaping.running, 1)) {
        return 0;
    }

    if (!app_session_apply((AppSessionCommand){.handled = 1, .cancel_shape = 0, .stop_running = 1}, &idle) ||
        !expect_int_eq("apply_idle_shaping", idle.shaping, 0) ||
        !expect_int_eq("apply_idle_preview", idle.preview_active, 0) ||
        !expect_int_eq("apply_idle_running", idle.running, 0)) {
        return 0;
    }

    if (!app_session_apply((AppSessionCommand){.handled = 1, .cancel_shape = 1, .stop_running = 1}, &both) ||
        !expect_int_eq("apply_both_shaping", both.shaping, 0) ||
        !expect_int_eq("apply_both_preview", both.preview_active, 0) ||
        !expect_int_eq("apply_both_running", both.running, 0)) {
        return 0;
    }

    if (app_session_apply((AppSessionCommand){.handled = 0, .cancel_shape = 1, .stop_running = 1}, &untouched) ||
        !expect_int_eq("apply_unhandled_shaping", untouched.shaping, 1) ||
        !expect_int_eq("apply_unhandled_preview", untouched.preview_active, 1) ||
        !expect_int_eq("apply_unhandled_running", untouched.running, 1)) {
        return 0;
    }

    return 1;
}

int main(void) {
    if (!test_shape_cancel_shortcuts()) {
        return 1;
    }
    if (!test_escape_cancels_shape_or_stops_running()) {
        return 1;
    }
    if (!test_apply_updates_session_state()) {
        return 1;
    }
    return 0;
}
