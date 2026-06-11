#ifndef UI_SHELL_H
#define UI_SHELL_H

#include "ui_sdf.h"

#define UI_SHELL_MAX_TOOLS 10
#define UI_SHELL_MAX_LAYERS 8
#define UI_SHELL_NAME_MAX 32

typedef struct {
    int doc_width;
    int doc_height;
    int compact;
    double zoom;
    int pan_x;
    int pan_y;
    int tool_count;
    int active_tool;
    const char *tool_labels[UI_SHELL_MAX_TOOLS];
    int layer_count;
    int active_layer;
    char layer_names[UI_SHELL_MAX_LAYERS][UI_SHELL_NAME_MAX];
    int layer_visible[UI_SHELL_MAX_LAYERS];
    int layer_opacity[UI_SHELL_MAX_LAYERS];
    const char *blend_label;
    const char *status_text;
    uint32_t foreground_color;
    uint32_t background_color;
} UiShellState;

typedef struct {
    UiRectF well;
    UiRectF viewport;
    double scale;
    UiDrawList draw_list;
} UiShellFrame;

int ui_shell_init(void);
void ui_shell_shutdown(void);
void ui_shell_build(int window_width, int window_height, const UiShellState *state, UiShellFrame *out);
void ui_shell_screen_to_doc(const UiShellFrame *frame, int screen_x, int screen_y, int *doc_x, int *doc_y, int *inside);
void ui_shell_screen_to_doc_clamped(const UiShellFrame *frame, int screen_x, int screen_y, int doc_w, int doc_h, int *doc_x, int *doc_y);

#endif
