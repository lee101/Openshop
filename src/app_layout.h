#ifndef APP_LAYOUT_H
#define APP_LAYOUT_H

typedef struct {
    int x;
    int y;
    int width;
    int height;
} AppRect;

typedef struct {
    int window_width;
    int window_height;
    AppRect canvas;
    AppRect left_toolbar;
    AppRect right_panel;
    AppRect bottom_panel;
} AppLayout;

enum {
    APP_LAYOUT_WINDOW_WIDTH = 1024,
    APP_LAYOUT_WINDOW_HEIGHT = 768,
    APP_LAYOUT_CANVAS_WIDTH = 800,
    APP_LAYOUT_CANVAS_HEIGHT = 600,
    APP_LAYOUT_CANVAS_X = 64,
    APP_LAYOUT_CANVAS_Y = 52,
    APP_LAYOUT_RIGHT_PANEL_X = 882,
    APP_LAYOUT_RIGHT_PANEL_WIDTH = 126,
    APP_LAYOUT_BOTTOM_PANEL_Y = 666,
    APP_LAYOUT_TOOL_COUNT = 8
};

AppLayout app_layout_default(void);
int app_rect_contains(AppRect rect, int x, int y);
int app_layout_screen_to_canvas(const AppLayout *layout, int screen_x, int screen_y, int *canvas_x, int *canvas_y);
void app_layout_screen_to_canvas_clamped(const AppLayout *layout, int screen_x, int screen_y, int *canvas_x, int *canvas_y);
AppRect app_layout_toolbar_button(const AppLayout *layout, int index);

#endif
