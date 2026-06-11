#include "ui_sdf.h"
#include "font.h"

#include <math.h>
#include <string.h>

void ui_draw_list_reset(UiDrawList *list) {
    if (list) {
        list->count = 0;
    }
}

static UiCommand *push_command(UiDrawList *list, UiCommandType type) {
    UiCommand *cmd;

    if (!list || list->count >= UI_MAX_COMMANDS) {
        return 0;
    }
    cmd = &list->commands[list->count++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = type;
    return cmd;
}

int ui_draw_rect(UiDrawList *list, UiRectF box, uint32_t argb, float corner_radius) {
    UiCommand *cmd = push_command(list, UI_CMD_RECT);
    if (!cmd) {
        return 0;
    }
    cmd->box = box;
    cmd->color = argb;
    cmd->corner_radius = corner_radius;
    return 1;
}

int ui_draw_border(UiDrawList *list, UiRectF box, uint32_t argb, float corner_radius, float thickness) {
    UiCommand *cmd = push_command(list, UI_CMD_BORDER);
    if (!cmd) {
        return 0;
    }
    cmd->box = box;
    cmd->color = argb;
    cmd->corner_radius = corner_radius;
    cmd->thickness = thickness < 1.0f ? 1.0f : thickness;
    return 1;
}

int ui_draw_circle(UiDrawList *list, float cx, float cy, float radius, uint32_t argb) {
    UiCommand *cmd = push_command(list, UI_CMD_CIRCLE);
    if (!cmd) {
        return 0;
    }
    cmd->box = (UiRectF){cx - radius, cy - radius, radius * 2.0f, radius * 2.0f};
    cmd->color = argb;
    cmd->corner_radius = radius;
    return 1;
}

int ui_draw_line(UiDrawList *list, float x0, float y0, float x1, float y1, float thickness, uint32_t argb) {
    UiCommand *cmd = push_command(list, UI_CMD_LINE);
    if (!cmd) {
        return 0;
    }
    cmd->box = (UiRectF){x0, y0, 0.0f, 0.0f};
    cmd->x1 = x1;
    cmd->y1 = y1;
    cmd->thickness = thickness < 1.0f ? 1.0f : thickness;
    cmd->color = argb;
    return 1;
}

int ui_draw_text(UiDrawList *list, float x, float y, const char *text, int scale, uint32_t argb) {
    UiCommand *cmd;

    if (!text || !text[0]) {
        return 0;
    }
    cmd = push_command(list, UI_CMD_TEXT);
    if (!cmd) {
        return 0;
    }
    cmd->box = (UiRectF){x, y, 0.0f, 0.0f};
    cmd->color = argb;
    cmd->text_scale = scale < 1 ? 1 : scale;
    strncpy(cmd->text, text, UI_TEXT_MAX - 1);
    return 1;
}

int ui_draw_checkerboard(UiDrawList *list, UiRectF box, int checker_size, uint32_t color_a, uint32_t color_b) {
    UiCommand *cmd = push_command(list, UI_CMD_CHECKERBOARD);
    if (!cmd) {
        return 0;
    }
    cmd->box = box;
    cmd->color = color_a;
    cmd->color2 = color_b;
    cmd->checker_size = checker_size < 1 ? 8 : checker_size;
    return 1;
}

int ui_scissor_start(UiDrawList *list, UiRectF box) {
    UiCommand *cmd = push_command(list, UI_CMD_SCISSOR_START);
    if (!cmd) {
        return 0;
    }
    cmd->box = box;
    return 1;
}

int ui_scissor_end(UiDrawList *list) {
    return push_command(list, UI_CMD_SCISSOR_END) != 0;
}

float ui_sdf_rounded_rect(float px, float py, UiRectF box, float corner_radius) {
    float half_w = box.width * 0.5f;
    float half_h = box.height * 0.5f;
    float max_r = half_w < half_h ? half_w : half_h;
    float r = corner_radius;
    float dx;
    float dy;
    float ox;
    float oy;
    float inside;

    if (r > max_r) {
        r = max_r;
    }
    if (r < 0.0f) {
        r = 0.0f;
    }
    dx = fabsf(px - (box.x + half_w)) - (half_w - r);
    dy = fabsf(py - (box.y + half_h)) - (half_h - r);
    ox = dx > 0.0f ? dx : 0.0f;
    oy = dy > 0.0f ? dy : 0.0f;
    inside = (dx > dy ? dx : dy);
    if (inside > 0.0f) {
        inside = 0.0f;
    }
    return sqrtf(ox * ox + oy * oy) + inside - r;
}

float ui_sdf_segment(float px, float py, float x0, float y0, float x1, float y1) {
    float bx = x1 - x0;
    float by = y1 - y0;
    float ax = px - x0;
    float ay = py - y0;
    float len2 = bx * bx + by * by;
    float t = len2 > 0.0f ? (ax * bx + ay * by) / len2 : 0.0f;
    float cx;
    float cy;

    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    cx = ax - bx * t;
    cy = ay - by * t;
    return sqrtf(cx * cx + cy * cy);
}

static float coverage_from_distance(float d) {
    float cov = 0.5f - d;
    if (cov < 0.0f) {
        return 0.0f;
    }
    if (cov > 1.0f) {
        return 1.0f;
    }
    return cov;
}

static void blend_at(Canvas *target, int x, int y, uint32_t argb, float coverage) {
    uint32_t a;

    if (coverage <= 0.0f) {
        return;
    }
    a = (uint32_t)(((argb >> 24) & 0xFF) * coverage + 0.5f);
    if (a == 0) {
        return;
    }
    canvas_set_pixel(target, x, y, (a << 24) | (argb & 0x00FFFFFF));
}

typedef struct {
    int active;
    int x0;
    int y0;
    int x1;
    int y1;
} UiClip;

static void clip_bounds(const UiClip *clip, const Canvas *target, float fx0, float fy0, float fx1, float fy1, int *x0, int *y0, int *x1, int *y1) {
    *x0 = (int)floorf(fx0);
    *y0 = (int)floorf(fy0);
    *x1 = (int)ceilf(fx1);
    *y1 = (int)ceilf(fy1);
    if (clip->active) {
        if (*x0 < clip->x0) *x0 = clip->x0;
        if (*y0 < clip->y0) *y0 = clip->y0;
        if (*x1 > clip->x1) *x1 = clip->x1;
        if (*y1 > clip->y1) *y1 = clip->y1;
    }
    if (*x0 < 0) *x0 = 0;
    if (*y0 < 0) *y0 = 0;
    if (*x1 > target->width) *x1 = target->width;
    if (*y1 > target->height) *y1 = target->height;
}

void ui_draw_list_rasterize(const UiDrawList *list, Canvas *target) {
    UiClip clip = {0, 0, 0, 0, 0};

    if (!list || !target || !target->pixels) {
        return;
    }

    for (int i = 0; i < list->count; i++) {
        const UiCommand *cmd = &list->commands[i];
        int x0;
        int y0;
        int x1;
        int y1;

        switch (cmd->type) {
        case UI_CMD_SCISSOR_START:
            clip.active = 1;
            clip.x0 = (int)cmd->box.x;
            clip.y0 = (int)cmd->box.y;
            clip.x1 = (int)(cmd->box.x + cmd->box.width);
            clip.y1 = (int)(cmd->box.y + cmd->box.height);
            break;
        case UI_CMD_SCISSOR_END:
            clip.active = 0;
            break;
        case UI_CMD_RECT:
        case UI_CMD_CIRCLE: {
            clip_bounds(&clip, target, cmd->box.x - 1.0f, cmd->box.y - 1.0f, cmd->box.x + cmd->box.width + 1.0f, cmd->box.y + cmd->box.height + 1.0f, &x0, &y0, &x1, &y1);
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    float d = ui_sdf_rounded_rect((float)x + 0.5f, (float)y + 0.5f, cmd->box, cmd->corner_radius);
                    blend_at(target, x, y, cmd->color, coverage_from_distance(d));
                }
            }
            break;
        }
        case UI_CMD_BORDER: {
            clip_bounds(&clip, target, cmd->box.x - 1.0f, cmd->box.y - 1.0f, cmd->box.x + cmd->box.width + 1.0f, cmd->box.y + cmd->box.height + 1.0f, &x0, &y0, &x1, &y1);
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    float d = ui_sdf_rounded_rect((float)x + 0.5f, (float)y + 0.5f, cmd->box, cmd->corner_radius);
                    float ring = fabsf(d + cmd->thickness * 0.5f) - cmd->thickness * 0.5f;
                    blend_at(target, x, y, cmd->color, coverage_from_distance(ring));
                }
            }
            break;
        }
        case UI_CMD_LINE: {
            float pad = cmd->thickness + 1.0f;
            float min_x = cmd->box.x < cmd->x1 ? cmd->box.x : cmd->x1;
            float max_x = cmd->box.x > cmd->x1 ? cmd->box.x : cmd->x1;
            float min_y = cmd->box.y < cmd->y1 ? cmd->box.y : cmd->y1;
            float max_y = cmd->box.y > cmd->y1 ? cmd->box.y : cmd->y1;
            clip_bounds(&clip, target, min_x - pad, min_y - pad, max_x + pad, max_y + pad, &x0, &y0, &x1, &y1);
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    float d = ui_sdf_segment((float)x + 0.5f, (float)y + 0.5f, cmd->box.x, cmd->box.y, cmd->x1, cmd->y1) - cmd->thickness * 0.5f;
                    blend_at(target, x, y, cmd->color, coverage_from_distance(d));
                }
            }
            break;
        }
        case UI_CMD_TEXT:
            canvas_draw_text(target, (int)cmd->box.x, (int)cmd->box.y, cmd->text, cmd->text_scale, cmd->color);
            break;
        case UI_CMD_CHECKERBOARD: {
            clip_bounds(&clip, target, cmd->box.x, cmd->box.y, cmd->box.x + cmd->box.width, cmd->box.y + cmd->box.height, &x0, &y0, &x1, &y1);
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    int cx = (x - (int)cmd->box.x) / cmd->checker_size;
                    int cy = (y - (int)cmd->box.y) / cmd->checker_size;
                    canvas_set_pixel_raw(target, x, y, ((cx + cy) & 1) ? cmd->color2 : cmd->color);
                }
            }
            break;
        }
        default:
            break;
        }
    }
}
