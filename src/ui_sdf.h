#ifndef UI_SDF_H
#define UI_SDF_H

#include "canvas.h"
#include <stdint.h>

#define UI_MAX_COMMANDS 1024
#define UI_TEXT_MAX 96

typedef enum {
    UI_CMD_RECT = 0,
    UI_CMD_BORDER,
    UI_CMD_CIRCLE,
    UI_CMD_LINE,
    UI_CMD_TEXT,
    UI_CMD_SCISSOR_START,
    UI_CMD_SCISSOR_END,
    UI_CMD_CHECKERBOARD
} UiCommandType;

typedef struct {
    float x;
    float y;
    float width;
    float height;
} UiRectF;

typedef struct {
    UiCommandType type;
    UiRectF box;
    uint32_t color;
    float corner_radius;
    float thickness;
    float x1;
    float y1;
    int text_scale;
    int checker_size;
    uint32_t color2;
    char text[UI_TEXT_MAX];
} UiCommand;

typedef struct {
    int count;
    UiCommand commands[UI_MAX_COMMANDS];
} UiDrawList;

void ui_draw_list_reset(UiDrawList *list);
int ui_draw_rect(UiDrawList *list, UiRectF box, uint32_t argb, float corner_radius);
int ui_draw_border(UiDrawList *list, UiRectF box, uint32_t argb, float corner_radius, float thickness);
int ui_draw_circle(UiDrawList *list, float cx, float cy, float radius, uint32_t argb);
int ui_draw_line(UiDrawList *list, float x0, float y0, float x1, float y1, float thickness, uint32_t argb);
int ui_draw_text(UiDrawList *list, float x, float y, const char *text, int scale, uint32_t argb);
int ui_draw_checkerboard(UiDrawList *list, UiRectF box, int checker_size, uint32_t color_a, uint32_t color_b);
int ui_scissor_start(UiDrawList *list, UiRectF box);
int ui_scissor_end(UiDrawList *list);

float ui_sdf_rounded_rect(float px, float py, UiRectF box, float corner_radius);
float ui_sdf_segment(float px, float py, float x0, float y0, float x1, float y1);
void ui_draw_list_rasterize(const UiDrawList *list, Canvas *target);

#endif
