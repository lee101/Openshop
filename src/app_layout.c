#include "app_layout.h"

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

AppLayout app_layout_default(void) {
    AppLayout layout;

    layout.window_width = APP_LAYOUT_WINDOW_WIDTH;
    layout.window_height = APP_LAYOUT_WINDOW_HEIGHT;
    layout.canvas = (AppRect){APP_LAYOUT_CANVAS_X, APP_LAYOUT_CANVAS_Y, APP_LAYOUT_CANVAS_WIDTH, APP_LAYOUT_CANVAS_HEIGHT};
    layout.left_toolbar = (AppRect){0, APP_LAYOUT_CANVAS_Y, APP_LAYOUT_CANVAS_X, APP_LAYOUT_WINDOW_HEIGHT - APP_LAYOUT_CANVAS_Y};
    layout.right_panel = (AppRect){APP_LAYOUT_RIGHT_PANEL_X, APP_LAYOUT_CANVAS_Y, APP_LAYOUT_RIGHT_PANEL_WIDTH, APP_LAYOUT_WINDOW_HEIGHT - 64};
    layout.bottom_panel = (AppRect){APP_LAYOUT_CANVAS_X, APP_LAYOUT_BOTTOM_PANEL_Y, APP_LAYOUT_CANVAS_WIDTH, 72};
    return layout;
}

int app_rect_contains(AppRect rect, int x, int y) {
    return x >= rect.x &&
           y >= rect.y &&
           x < rect.x + rect.width &&
           y < rect.y + rect.height;
}

int app_layout_screen_to_canvas(const AppLayout *layout, int screen_x, int screen_y, int *canvas_x, int *canvas_y) {
    AppRect canvas;

    if (!layout || !canvas_x || !canvas_y) {
        return 0;
    }

    canvas = layout->canvas;
    if (!app_rect_contains(canvas, screen_x, screen_y)) {
        return 0;
    }

    *canvas_x = screen_x - canvas.x;
    *canvas_y = screen_y - canvas.y;
    return 1;
}

void app_layout_screen_to_canvas_clamped(const AppLayout *layout, int screen_x, int screen_y, int *canvas_x, int *canvas_y) {
    AppRect canvas;

    if (!layout || !canvas_x || !canvas_y) {
        return;
    }

    canvas = layout->canvas;
    *canvas_x = clamp_int(screen_x - canvas.x, 0, canvas.width - 1);
    *canvas_y = clamp_int(screen_y - canvas.y, 0, canvas.height - 1);
}

AppRect app_layout_toolbar_button(const AppLayout *layout, int index) {
    AppLayout fallback;
    const AppLayout *source = layout;

    if (!source) {
        fallback = app_layout_default();
        source = &fallback;
    }

    return (AppRect){14, source->canvas.y + 12 + index * 44, 36, 34};
}
