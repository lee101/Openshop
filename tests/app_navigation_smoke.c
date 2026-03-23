#include "../src/app_navigation.h"

#include <stdio.h>

static int expect_int_eq(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int test_numeric_selection_variants(void) {
    AppNavigationCommand unlocked = app_navigation_command_for_key('3', 1, 1, 1);
    AppNavigationCommand editable = app_navigation_command_for_key('4', 1, 1, 0);
    AppNavigationCommand visible = app_navigation_command_for_key('5', 1, 0, 1);
    AppNavigationCommand direct = app_navigation_command_for_key('6', 1, 0, 0);

    return expect_int_eq("unlocked_action", unlocked.action, APP_NAV_SELECT_NTH_UNLOCKED) &&
           expect_int_eq("unlocked_arg", unlocked.argument, 2) &&
           expect_int_eq("editable_action", editable.action, APP_NAV_SELECT_NTH_EDITABLE_VISIBLE) &&
           expect_int_eq("editable_arg", editable.argument, 3) &&
           expect_int_eq("visible_action", visible.action, APP_NAV_SELECT_NTH_VISIBLE) &&
           expect_int_eq("visible_arg", visible.argument, 4) &&
           expect_int_eq("direct_action", direct.action, APP_NAV_SELECT_NTH_DIRECT) &&
           expect_int_eq("direct_arg", direct.argument, 5);
}

static int test_cycle_and_edge_variants(void) {
    AppNavigationCommand cycle_all = app_navigation_command_for_key(1073741899, 0, 0, 0);
    AppNavigationCommand cycle_visible = app_navigation_command_for_key(1073741902, 0, 1, 0);
    AppNavigationCommand cycle_unlocked = app_navigation_command_for_key(1073741899, 1, 0, 0);
    AppNavigationCommand cycle_editable = app_navigation_command_for_key(1073741902, 1, 1, 0);
    AppNavigationCommand edge_all = app_navigation_command_for_key(1073741898, 0, 0, 0);
    AppNavigationCommand edge_visible = app_navigation_command_for_key(1073741901, 0, 1, 0);
    AppNavigationCommand edge_unlocked = app_navigation_command_for_key(1073741898, 1, 0, 1);
    AppNavigationCommand edge_editable = app_navigation_command_for_key(1073741901, 1, 1, 0);

    return expect_int_eq("cycle_all_action", cycle_all.action, APP_NAV_CYCLE_ALL) &&
           expect_int_eq("cycle_all_arg", cycle_all.argument, 1) &&
           expect_int_eq("cycle_visible_action", cycle_visible.action, APP_NAV_CYCLE_VISIBLE) &&
           expect_int_eq("cycle_visible_arg", cycle_visible.argument, -1) &&
           expect_int_eq("cycle_unlocked_action", cycle_unlocked.action, APP_NAV_CYCLE_UNLOCKED) &&
           expect_int_eq("cycle_editable_action", cycle_editable.action, APP_NAV_CYCLE_EDITABLE_VISIBLE) &&
           expect_int_eq("edge_all_action", edge_all.action, APP_NAV_EDGE_ALL) &&
           expect_int_eq("edge_all_arg", edge_all.argument, -1) &&
           expect_int_eq("edge_visible_action", edge_visible.action, APP_NAV_EDGE_VISIBLE) &&
           expect_int_eq("edge_visible_arg", edge_visible.argument, 1) &&
           expect_int_eq("edge_unlocked_action", edge_unlocked.action, APP_NAV_EDGE_UNLOCKED) &&
           expect_int_eq("edge_editable_action", edge_editable.action, APP_NAV_EDGE_EDITABLE_VISIBLE);
}

static int test_unhandled_key(void) {
    AppNavigationCommand none = app_navigation_command_for_key('q', 0, 0, 0);
    return expect_int_eq("none_handled", none.handled, 0) &&
           expect_int_eq("none_action", none.action, APP_NAV_NONE) &&
           expect_int_eq("none_arg", none.argument, 0);
}

int main(void) {
    if (!test_numeric_selection_variants()) {
        return 1;
    }
    if (!test_cycle_and_edge_variants()) {
        return 1;
    }
    if (!test_unhandled_key()) {
        return 1;
    }
    return 0;
}
