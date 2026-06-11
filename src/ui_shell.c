#include "ui_shell.h"
#include "font.h"
#include "../third_party/clay/clay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *shell_arena_memory = NULL;
static int shell_ready = 0;

static Clay_Dimensions measure_text(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    int scale = config->fontSize < 1 ? 1 : config->fontSize;
    float width = 0.0f;

    (void)userData;
    for (int i = 0; i < text.length; i++) {
        char buf[2] = {text.chars[i], '\0'};
        width += (float)font_text_width(buf, scale);
    }
    return (Clay_Dimensions){width, (float)font_text_height(scale)};
}

static void shell_error(Clay_ErrorData error) {
    fprintf(stderr, "clay: %.*s\n", (int)error.errorText.length, error.errorText.chars);
}

int ui_shell_init(void) {
    uint32_t size;
    Clay_Arena arena;

    if (shell_ready) {
        return 1;
    }
    size = Clay_MinMemorySize();
    shell_arena_memory = malloc(size);
    if (!shell_arena_memory) {
        return 0;
    }
    arena = Clay_CreateArenaWithCapacityAndMemory(size, shell_arena_memory);
    Clay_Initialize(arena, (Clay_Dimensions){1280, 800}, (Clay_ErrorHandler){shell_error, NULL});
    Clay_SetMeasureTextFunction(measure_text, NULL);
    shell_ready = 1;
    return 1;
}

void ui_shell_shutdown(void) {
    free(shell_arena_memory);
    shell_arena_memory = NULL;
    shell_ready = 0;
}

static Clay_Color color_from_argb(uint32_t argb) {
    return (Clay_Color){
        (float)((argb >> 16) & 0xFF),
        (float)((argb >> 8) & 0xFF),
        (float)(argb & 0xFF),
        (float)((argb >> 24) & 0xFF),
    };
}

static uint32_t argb_from_color(Clay_Color color) {
    uint32_t a = (uint32_t)(color.a + 0.5f);
    uint32_t r = (uint32_t)(color.r + 0.5f);
    uint32_t g = (uint32_t)(color.g + 0.5f);
    uint32_t b = (uint32_t)(color.b + 0.5f);
    if (a > 255) a = 255;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static Clay_String dynamic_string(const char *text) {
    Clay_String value = {0};
    value.isStaticallyAllocated = false;
    value.length = (int32_t)strlen(text);
    value.chars = text;
    return value;
}

#define SHELL_COLOR_ROOT 0xFF1F1F23u
#define SHELL_COLOR_BAR 0xFF2B2B2Fu
#define SHELL_COLOR_OPTIONS 0xFF333338u
#define SHELL_COLOR_RAIL 0xFF2A2A2Fu
#define SHELL_COLOR_WELL 0xFF17181Cu
#define SHELL_COLOR_PANEL 0xFF2B2B30u
#define SHELL_COLOR_CARD 0xFF3A3A40u
#define SHELL_COLOR_ACCENT 0xFF5DADE2u
#define SHELL_COLOR_TEXT 0xFFE8EDF5u
#define SHELL_COLOR_TEXT_DIM 0xFF8B929Cu

static char status_buffer[96];
static char opacity_buffers[UI_SHELL_MAX_LAYERS][8];

static void build_tree(int window_width, int window_height, const UiShellState *state) {
    (void)window_width;
    (void)window_height;

    CLAY(CLAY_ID("Root"), {
        .layout = {
            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = color_from_argb(SHELL_COLOR_ROOT),
    }) {
        if (!state->compact) {
            CLAY(CLAY_ID("MenuBar"), {
                .layout = {
                    .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(26)},
                    .padding = {10, 10, 6, 4},
                    .childGap = 18,
                },
                .backgroundColor = color_from_argb(SHELL_COLOR_BAR),
            }) {
                CLAY_TEXT(CLAY_STRING("File"), CLAY_TEXT_CONFIG({.textColor = color_from_argb(SHELL_COLOR_TEXT), .fontSize = 1}));
                CLAY_TEXT(CLAY_STRING("Edit"), CLAY_TEXT_CONFIG({.textColor = color_from_argb(SHELL_COLOR_TEXT), .fontSize = 1}));
                CLAY_TEXT(CLAY_STRING("Image"), CLAY_TEXT_CONFIG({.textColor = color_from_argb(SHELL_COLOR_TEXT), .fontSize = 1}));
                CLAY_TEXT(CLAY_STRING("Layer"), CLAY_TEXT_CONFIG({.textColor = color_from_argb(SHELL_COLOR_TEXT), .fontSize = 1}));
                CLAY_TEXT(CLAY_STRING("Select"), CLAY_TEXT_CONFIG({.textColor = color_from_argb(SHELL_COLOR_TEXT), .fontSize = 1}));
                CLAY_TEXT(CLAY_STRING("Filter"), CLAY_TEXT_CONFIG({.textColor = color_from_argb(SHELL_COLOR_TEXT), .fontSize = 1}));
                CLAY_TEXT(CLAY_STRING("View"), CLAY_TEXT_CONFIG({.textColor = color_from_argb(SHELL_COLOR_TEXT), .fontSize = 1}));
            }
            CLAY(CLAY_ID("OptionsBar"), {
                .layout = {
                    .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(26)},
                    .padding = {10, 10, 6, 4},
                    .childGap = 16,
                },
                .backgroundColor = color_from_argb(SHELL_COLOR_OPTIONS),
            }) {
                if (state->blend_label && state->blend_label[0]) {
                    CLAY_TEXT(dynamic_string(state->blend_label), CLAY_TEXT_CONFIG({.textColor = color_from_argb(SHELL_COLOR_TEXT_DIM), .fontSize = 1}));
                }
            }
        }

        CLAY(CLAY_ID("Middle"), {
            .layout = {
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
        }) {
            if (!state->compact) {
                CLAY(CLAY_ID("ToolRail"), {
                    .layout = {
                        .sizing = {CLAY_SIZING_FIXED(46), CLAY_SIZING_GROW(0)},
                        .padding = CLAY_PADDING_ALL(6),
                        .childGap = 6,
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    },
                    .backgroundColor = color_from_argb(SHELL_COLOR_RAIL),
                }) {
                    for (int i = 0; i < state->tool_count && i < UI_SHELL_MAX_TOOLS; i++) {
                        int active = i == state->active_tool;
                        CLAY(CLAY_IDI("ToolButton", (uint32_t)i), {
                            .layout = {
                                .sizing = {CLAY_SIZING_FIXED(34), CLAY_SIZING_FIXED(30)},
                                .padding = {10, 0, 8, 0},
                            },
                            .backgroundColor = color_from_argb(active ? SHELL_COLOR_ACCENT : SHELL_COLOR_CARD),
                            .cornerRadius = CLAY_CORNER_RADIUS(6),
                        }) {
                            if (state->tool_labels[i]) {
                                CLAY_TEXT(dynamic_string(state->tool_labels[i]), CLAY_TEXT_CONFIG({.textColor = color_from_argb(active ? 0xFF14202B : SHELL_COLOR_TEXT), .fontSize = 1}));
                            }
                        }
                    }
                }
            }

            CLAY(CLAY_ID("CanvasWell"), {
                .layout = {
                    .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                },
                .backgroundColor = color_from_argb(SHELL_COLOR_WELL),
            }) {}

            if (!state->compact) {
                CLAY(CLAY_ID("RightPanel"), {
                    .layout = {
                        .sizing = {CLAY_SIZING_FIXED(200), CLAY_SIZING_GROW(0)},
                        .padding = CLAY_PADDING_ALL(10),
                        .childGap = 8,
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    },
                    .backgroundColor = color_from_argb(SHELL_COLOR_PANEL),
                }) {
                    CLAY(CLAY_ID("ColorHeader"), {
                        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(20)}, .padding = {6, 6, 4, 0}},
                        .backgroundColor = color_from_argb(SHELL_COLOR_CARD),
                        .cornerRadius = CLAY_CORNER_RADIUS(4),
                    }) {
                        CLAY_TEXT(CLAY_STRING("Color"), CLAY_TEXT_CONFIG({.textColor = color_from_argb(SHELL_COLOR_TEXT), .fontSize = 1}));
                    }
                    CLAY(CLAY_ID("Swatches"), {
                        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(28)}, .childGap = 8},
                    }) {
                        CLAY(CLAY_ID("Foreground"), {
                            .layout = {.sizing = {CLAY_SIZING_FIXED(28), CLAY_SIZING_FIXED(28)}},
                            .backgroundColor = color_from_argb(state->foreground_color),
                            .cornerRadius = CLAY_CORNER_RADIUS(4),
                            .border = {.color = color_from_argb(0xFF0C0C0D), .width = {1, 1, 1, 1, 0}},
                        }) {}
                        CLAY(CLAY_ID("BackgroundSwatch"), {
                            .layout = {.sizing = {CLAY_SIZING_FIXED(28), CLAY_SIZING_FIXED(28)}},
                            .backgroundColor = color_from_argb(state->background_color),
                            .cornerRadius = CLAY_CORNER_RADIUS(4),
                            .border = {.color = color_from_argb(0xFF0C0C0D), .width = {1, 1, 1, 1, 0}},
                        }) {}
                    }
                    CLAY(CLAY_ID("LayersHeader"), {
                        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(20)}, .padding = {6, 6, 4, 0}},
                        .backgroundColor = color_from_argb(SHELL_COLOR_CARD),
                        .cornerRadius = CLAY_CORNER_RADIUS(4),
                    }) {
                        CLAY_TEXT(CLAY_STRING("Layers"), CLAY_TEXT_CONFIG({.textColor = color_from_argb(SHELL_COLOR_TEXT), .fontSize = 1}));
                    }
                    for (int i = state->layer_count - 1; i >= 0; i--) {
                        int active = i == state->active_layer;
                        snprintf(opacity_buffers[i], sizeof(opacity_buffers[i]), "%d%%", state->layer_opacity[i]);
                        CLAY(CLAY_IDI("LayerRow", (uint32_t)i), {
                            .layout = {
                                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(26)},
                                .padding = {8, 8, 6, 0},
                                .childGap = 8,
                            },
                            .backgroundColor = color_from_argb(active ? 0xFF3D5A73 : SHELL_COLOR_CARD),
                            .cornerRadius = CLAY_CORNER_RADIUS(4),
                            .border = active ? (Clay_BorderElementConfig){.color = color_from_argb(SHELL_COLOR_ACCENT), .width = {1, 1, 1, 1, 0}} : (Clay_BorderElementConfig){0},
                        }) {
                            CLAY(CLAY_IDI("LayerEye", (uint32_t)i), {
                                .layout = {.sizing = {CLAY_SIZING_FIXED(10), CLAY_SIZING_FIXED(10)}},
                                .backgroundColor = color_from_argb(state->layer_visible[i] ? SHELL_COLOR_ACCENT : 0xFF54565E),
                                .cornerRadius = CLAY_CORNER_RADIUS(5),
                            }) {}
                            CLAY_TEXT(dynamic_string(state->layer_names[i]), CLAY_TEXT_CONFIG({.textColor = color_from_argb(active ? SHELL_COLOR_TEXT : SHELL_COLOR_TEXT_DIM), .fontSize = 1}));
                            CLAY_TEXT(dynamic_string(opacity_buffers[i]), CLAY_TEXT_CONFIG({.textColor = color_from_argb(SHELL_COLOR_TEXT_DIM), .fontSize = 1}));
                        }
                    }
                }
            }
        }

        if (!state->compact) {
            CLAY(CLAY_ID("StatusBar"), {
                .layout = {
                    .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(22)},
                    .padding = {10, 10, 5, 0},
                    .childGap = 16,
                },
                .backgroundColor = color_from_argb(0xFF252529),
            }) {
                CLAY_TEXT(dynamic_string(status_buffer), CLAY_TEXT_CONFIG({.textColor = color_from_argb(SHELL_COLOR_TEXT_DIM), .fontSize = 1}));
            }
        }
    }
}

static void convert_commands(Clay_RenderCommandArray commands, UiDrawList *list) {
    for (int32_t i = 0; i < commands.length; i++) {
        Clay_RenderCommand *cmd = &commands.internalArray[i];
        UiRectF box = {cmd->boundingBox.x, cmd->boundingBox.y, cmd->boundingBox.width, cmd->boundingBox.height};

        switch (cmd->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            ui_draw_rect(list, box, argb_from_color(cmd->renderData.rectangle.backgroundColor), cmd->renderData.rectangle.cornerRadius.topLeft);
            break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            float width = (float)cmd->renderData.border.width.top;
            if ((float)cmd->renderData.border.width.left > width) width = (float)cmd->renderData.border.width.left;
            if (width < 1.0f) width = 1.0f;
            ui_draw_border(list, box, argb_from_color(cmd->renderData.border.color), cmd->renderData.border.cornerRadius.topLeft, width);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            char text[UI_TEXT_MAX];
            int length = cmd->renderData.text.stringContents.length;
            if (length > UI_TEXT_MAX - 1) {
                length = UI_TEXT_MAX - 1;
            }
            memcpy(text, cmd->renderData.text.stringContents.chars, (size_t)length);
            text[length] = '\0';
            ui_draw_text(list, box.x, box.y, text, cmd->renderData.text.fontSize < 1 ? 1 : cmd->renderData.text.fontSize, argb_from_color(cmd->renderData.text.textColor));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            ui_scissor_start(list, box);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            ui_scissor_end(list);
            break;
        default:
            break;
        }
    }
}

void ui_shell_build(int window_width, int window_height, const UiShellState *state, UiShellFrame *out) {
    Clay_RenderCommandArray commands;
    Clay_ElementData well;

    if (!out) {
        return;
    }
    ui_draw_list_reset(&out->draw_list);
    out->well = (UiRectF){0, 0, (float)window_width, (float)window_height};
    out->viewport = out->well;
    out->scale = 1.0;
    if (!state || !ui_shell_init()) {
        return;
    }

    snprintf(status_buffer, sizeof(status_buffer), "%dx%d  %s  layer %d/%d",
             state->doc_width, state->doc_height,
             state->status_text ? state->status_text : "",
             state->active_layer + 1, state->layer_count);

    Clay_SetLayoutDimensions((Clay_Dimensions){(float)window_width, (float)window_height});
    Clay_BeginLayout();
    build_tree(window_width, window_height, state);
    commands = Clay_EndLayout(0.016f);

    well = Clay_GetElementData(Clay_GetElementId(CLAY_STRING("CanvasWell")));
    if (well.found && well.boundingBox.width > 0.0f && well.boundingBox.height > 0.0f) {
        out->well = (UiRectF){well.boundingBox.x, well.boundingBox.y, well.boundingBox.width, well.boundingBox.height};
    }

    {
        double pad = 16.0;
        double avail_w = out->well.width - pad * 2.0;
        double avail_h = out->well.height - pad * 2.0;
        double scale;
        double view_w;
        double view_h;
        double vx;
        double vy;

        if (avail_w < 8.0) avail_w = 8.0;
        if (avail_h < 8.0) avail_h = 8.0;
        if (state->zoom > 0.0) {
            scale = state->zoom;
        } else {
            double sx = avail_w / (double)state->doc_width;
            double sy = avail_h / (double)state->doc_height;
            scale = sx < sy ? sx : sy;
            if (scale > 1.0) {
                scale = 1.0;
            }
        }
        if (scale < 0.05) scale = 0.05;
        if (scale > 16.0) scale = 16.0;

        view_w = state->doc_width * scale;
        view_h = state->doc_height * scale;
        vx = out->well.x + (out->well.width - view_w) / 2.0 + state->pan_x;
        vy = out->well.y + (out->well.height - view_h) / 2.0 + state->pan_y;

        out->viewport = (UiRectF){(float)vx, (float)vy, (float)view_w, (float)view_h};
        out->scale = scale;
    }

    convert_commands(commands, &out->draw_list);

    ui_scissor_start(&out->draw_list, out->well);
    ui_draw_checkerboard(&out->draw_list, out->viewport, 12, 0xFFB9BCC2, 0xFF7E838B);
    ui_draw_border(&out->draw_list, (UiRectF){out->viewport.x - 2, out->viewport.y - 2, out->viewport.width + 4, out->viewport.height + 4}, 0xFF0B0B0D, 0, 2);
    ui_scissor_end(&out->draw_list);
}

void ui_shell_screen_to_doc(const UiShellFrame *frame, int screen_x, int screen_y, int *doc_x, int *doc_y, int *inside) {
    double dx;
    double dy;

    if (!frame || !doc_x || !doc_y) {
        if (inside) {
            *inside = 0;
        }
        return;
    }
    dx = ((double)screen_x - frame->viewport.x) / frame->scale;
    dy = ((double)screen_y - frame->viewport.y) / frame->scale;
    *doc_x = (int)dx;
    *doc_y = (int)dy;
    if (inside) {
        *inside = screen_x >= (int)frame->viewport.x && screen_y >= (int)frame->viewport.y &&
                  screen_x < (int)(frame->viewport.x + frame->viewport.width) &&
                  screen_y < (int)(frame->viewport.y + frame->viewport.height);
    }
}

void ui_shell_screen_to_doc_clamped(const UiShellFrame *frame, int screen_x, int screen_y, int doc_w, int doc_h, int *doc_x, int *doc_y) {
    int inside = 0;

    ui_shell_screen_to_doc(frame, screen_x, screen_y, doc_x, doc_y, &inside);
    if (!doc_x || !doc_y) {
        return;
    }
    if (*doc_x < 0) *doc_x = 0;
    if (*doc_y < 0) *doc_y = 0;
    if (*doc_x > doc_w - 1) *doc_x = doc_w - 1;
    if (*doc_y > doc_h - 1) *doc_y = doc_h - 1;
}
