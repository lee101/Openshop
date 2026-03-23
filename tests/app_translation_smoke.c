#include "../src/app_translation.h"

#include <stdio.h>

static int expect_int_eq(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int test_arrow_key_mappings(void) {
    AppTranslationCommand up = app_translation_command_for_key(1073741906, 0);
    AppTranslationCommand down = app_translation_command_for_key(1073741905, 0);
    AppTranslationCommand left = app_translation_command_for_key(1073741904, 0);
    AppTranslationCommand right = app_translation_command_for_key(1073741903, 0);

    return up.handled && down.handled && left.handled && right.handled &&
           expect_int_eq("up_dx", up.dx, 0) &&
           expect_int_eq("up_dy", up.dy, -1) &&
           expect_int_eq("down_dx", down.dx, 0) &&
           expect_int_eq("down_dy", down.dy, 1) &&
           expect_int_eq("left_dx", left.dx, -1) &&
           expect_int_eq("left_dy", left.dy, 0) &&
           expect_int_eq("right_dx", right.dx, 1) &&
           expect_int_eq("right_dy", right.dy, 0);
}

static int test_shift_uses_large_step_and_unknown_key_is_ignored(void) {
    AppTranslationCommand shifted = app_translation_command_for_key(1073741903, 1);
    AppTranslationCommand unknown = app_translation_command_for_key('x', 1);

    return expect_int_eq("shifted_handled", shifted.handled, 1) &&
           expect_int_eq("shifted_dx", shifted.dx, 10) &&
           expect_int_eq("shifted_dy", shifted.dy, 0) &&
           expect_int_eq("unknown_handled", unknown.handled, 0) &&
           expect_int_eq("unknown_dx", unknown.dx, 0) &&
           expect_int_eq("unknown_dy", unknown.dy, 0);
}

int main(void) {
    if (!test_arrow_key_mappings()) {
        return 1;
    }
    if (!test_shift_uses_large_step_and_unknown_key_is_ignored()) {
        return 1;
    }
    return 0;
}
