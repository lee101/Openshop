#ifndef APP_NAVIGATION_H
#define APP_NAVIGATION_H

typedef enum {
    APP_NAV_NONE = 0,
    APP_NAV_SELECT_NTH_UNLOCKED,
    APP_NAV_SELECT_NTH_EDITABLE_VISIBLE,
    APP_NAV_SELECT_NTH_VISIBLE,
    APP_NAV_SELECT_NTH_DIRECT,
    APP_NAV_CYCLE_EDITABLE_VISIBLE,
    APP_NAV_CYCLE_UNLOCKED,
    APP_NAV_CYCLE_VISIBLE,
    APP_NAV_CYCLE_ALL,
    APP_NAV_EDGE_VISIBLE,
    APP_NAV_EDGE_ALL,
    APP_NAV_EDGE_UNLOCKED,
    APP_NAV_EDGE_EDITABLE_VISIBLE
} AppNavigationAction;

typedef struct {
    int handled;
    AppNavigationAction action;
    int argument;
} AppNavigationCommand;

AppNavigationCommand app_navigation_command_for_key(int key, int ctrl, int shift, int alt);

#endif
