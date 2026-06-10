#include "../src/app_layout.h"

#include <stdio.h>

static int expect_int(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_rect(const char *label, AppRect got, AppRect want) {
    int ok = 1;
    ok = ok && expect_int(label, got.x, want.x);
    ok = ok && expect_int(label, got.y, want.y);
    ok = ok && expect_int(label, got.width, want.width);
    ok = ok && expect_int(label, got.height, want.height);
    return ok;
}

int main(void) {
    AppLayout layout = app_layout_default();
    int x = -1;
    int y = -1;
    int ok = 1;

    ok = ok && expect_int("window width", layout.window_width, 1024);
    ok = ok && expect_int("window height", layout.window_height, 768);
    ok = ok && expect_rect("canvas", layout.canvas, (AppRect){64, 52, 800, 600});
    ok = ok && expect_rect("right panel", layout.right_panel, (AppRect){882, 52, 126, 704});

    ok = ok && expect_int("canvas contains origin", app_rect_contains(layout.canvas, 64, 52), 1);
    ok = ok && expect_int("canvas excludes left edge", app_rect_contains(layout.canvas, 63, 52), 0);
    ok = ok && expect_int("canvas excludes right edge", app_rect_contains(layout.canvas, 864, 52), 0);

    ok = ok && expect_int("screen to canvas inside", app_layout_screen_to_canvas(&layout, 74, 72, &x, &y), 1);
    ok = ok && expect_int("canvas x", x, 10);
    ok = ok && expect_int("canvas y", y, 20);
    ok = ok && expect_int("screen to canvas outside", app_layout_screen_to_canvas(&layout, 10, 20, &x, &y), 0);

    app_layout_screen_to_canvas_clamped(&layout, 10, 20, &x, &y);
    ok = ok && expect_int("clamped low x", x, 0);
    ok = ok && expect_int("clamped low y", y, 0);
    app_layout_screen_to_canvas_clamped(&layout, 2000, 2000, &x, &y);
    ok = ok && expect_int("clamped high x", x, 799);
    ok = ok && expect_int("clamped high y", y, 599);

    ok = ok && expect_rect("toolbar button 0", app_layout_toolbar_button(&layout, 0), (AppRect){14, 64, 36, 34});
    ok = ok && expect_rect("toolbar button 7", app_layout_toolbar_button(&layout, 7), (AppRect){14, 372, 36, 34});

    {
        AppLayout compact = app_layout_compact();
        ok = ok && expect_rect("compact canvas", compact.canvas, (AppRect){64, 52, 800, 600});
        ok = ok && expect_rect("compact left toolbar", compact.left_toolbar, (AppRect){0, 0, 0, 0});
        ok = ok && expect_rect("compact right panel", compact.right_panel, (AppRect){0, 0, 0, 0});
        ok = ok && expect_rect("compact bottom panel", compact.bottom_panel, (AppRect){0, 0, 0, 0});
        ok = ok && expect_int("compact hides panel hits", app_rect_contains(compact.right_panel, 900, 100), 0);
        ok = ok && expect_int("compact canvas mapping", app_layout_screen_to_canvas(&compact, 74, 72, &x, &y), 1);
        ok = ok && expect_int("compact canvas x", x, 10);
        ok = ok && expect_int("compact canvas y", y, 20);
    }

    if (!ok) {
        return 1;
    }
    puts("app layout selftest ok");
    return 0;
}
