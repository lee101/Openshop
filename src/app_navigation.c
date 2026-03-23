#include "app_navigation.h"

AppNavigationCommand app_navigation_command_for_key(int key, int ctrl, int shift, int alt) {
    if (ctrl && shift && alt && key >= '1' && key <= '8') {
        return (AppNavigationCommand){1, APP_NAV_SELECT_NTH_UNLOCKED, key - '1'};
    }
    if (ctrl && shift && key >= '1' && key <= '8') {
        return (AppNavigationCommand){1, APP_NAV_SELECT_NTH_EDITABLE_VISIBLE, key - '1'};
    }
    if (ctrl && alt && key >= '1' && key <= '8') {
        return (AppNavigationCommand){1, APP_NAV_SELECT_NTH_VISIBLE, key - '1'};
    }
    if (ctrl && key >= '1' && key <= '8') {
        return (AppNavigationCommand){1, APP_NAV_SELECT_NTH_DIRECT, key - '1'};
    }
    if (key == 1073741899) {
        if (ctrl && shift) {
            return (AppNavigationCommand){1, APP_NAV_CYCLE_EDITABLE_VISIBLE, 1};
        }
        if (ctrl) {
            return (AppNavigationCommand){1, APP_NAV_CYCLE_UNLOCKED, 1};
        }
        if (shift) {
            return (AppNavigationCommand){1, APP_NAV_CYCLE_VISIBLE, 1};
        }
        return (AppNavigationCommand){1, APP_NAV_CYCLE_ALL, 1};
    }
    if (key == 1073741902) {
        if (ctrl && shift) {
            return (AppNavigationCommand){1, APP_NAV_CYCLE_EDITABLE_VISIBLE, -1};
        }
        if (ctrl) {
            return (AppNavigationCommand){1, APP_NAV_CYCLE_UNLOCKED, -1};
        }
        if (shift) {
            return (AppNavigationCommand){1, APP_NAV_CYCLE_VISIBLE, -1};
        }
        return (AppNavigationCommand){1, APP_NAV_CYCLE_ALL, -1};
    }
    if (!ctrl && key == 1073741898) {
        return (AppNavigationCommand){1, shift ? APP_NAV_EDGE_VISIBLE : APP_NAV_EDGE_ALL, -1};
    }
    if (!ctrl && key == 1073741901) {
        return (AppNavigationCommand){1, shift ? APP_NAV_EDGE_VISIBLE : APP_NAV_EDGE_ALL, 1};
    }
    if (ctrl && alt && key == 1073741898) {
        return (AppNavigationCommand){1, APP_NAV_EDGE_UNLOCKED, -1};
    }
    if (ctrl && alt && key == 1073741901) {
        return (AppNavigationCommand){1, APP_NAV_EDGE_UNLOCKED, 1};
    }
    if (ctrl && shift && key == 1073741898) {
        return (AppNavigationCommand){1, APP_NAV_EDGE_EDITABLE_VISIBLE, -1};
    }
    if (ctrl && shift && key == 1073741901) {
        return (AppNavigationCommand){1, APP_NAV_EDGE_EDITABLE_VISIBLE, 1};
    }

    return (AppNavigationCommand){0, APP_NAV_NONE, 0};
}
