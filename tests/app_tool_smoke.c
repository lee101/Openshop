#include "../src/app_tool.h"

#include <stdio.h>

static int expect_int_eq(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static int expect_uint_eq(const char *label, unsigned int got, unsigned int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got 0x%08X want 0x%08X\n", label, got, want);
        return 0;
    }
    return 1;
}

static int test_tool_and_palette_selection(void) {
    AppToolCommand brush = app_tool_command_for_key('b', 3, 0, 6, 100, 0x00000000u);
    AppToolCommand rect = app_tool_command_for_key('r', 0, 0, 6, 100, 0x001B1F24u);
    AppToolCommand palette = app_tool_command_for_key('4', 2, 0, 6, 40, 0x001B1F24u);

    return expect_int_eq("brush_handled", brush.handled, 1) &&
           expect_int_eq("brush_tool", brush.tool, 0) &&
           expect_uint_eq("brush_rgb", brush.brush_color_rgb, 0x001B1F24u) &&
           expect_int_eq("rect_tool", rect.tool, 3) &&
           expect_int_eq("palette_tool", palette.tool, 0) &&
           expect_uint_eq("palette_rgb", palette.brush_color_rgb, 0x001E88E5u) &&
           expect_uint_eq("palette_color", palette.brush_color, 0x661E88E5u);
}

static int test_radius_shape_and_opacity_adjustments(void) {
    AppToolCommand shrink = app_tool_command_for_key('[', 0, 0, 1, 100, 0x001B1F24u);
    AppToolCommand grow = app_tool_command_for_key(']', 0, 0, 64, 100, 0x001B1F24u);
    AppToolCommand cycle_back = app_tool_command_for_key(',', 0, 0, 6, 100, 0x001B1F24u);
    AppToolCommand cycle_forward = app_tool_command_for_key('.', 0, 2, 6, 100, 0x001B1F24u);
    AppToolCommand fade = app_tool_command_for_key('-', 0, 0, 6, 3, 0x001B1F24u);
    AppToolCommand brighten = app_tool_command_for_key('=', 0, 0, 6, 98, 0x001B1F24u);

    return expect_int_eq("shrink_radius", shrink.brush_radius, 1) &&
           expect_int_eq("grow_radius", grow.brush_radius, 64) &&
           expect_int_eq("cycle_back_shape", cycle_back.brush_shape, 2) &&
           expect_int_eq("cycle_forward_shape", cycle_forward.brush_shape, 0) &&
           expect_int_eq("fade_opacity", fade.brush_opacity, 1) &&
           expect_uint_eq("fade_color", fade.brush_color, 0x031B1F24u) &&
           expect_int_eq("brighten_opacity", brighten.brush_opacity, 100) &&
           expect_uint_eq("brighten_color", brighten.brush_color, 0xFF1B1F24u);
}

static int test_unhandled_key_passthrough(void) {
    AppToolCommand command = app_tool_command_for_key('q', 5, 1, 9, 77, 0x00ABCDEFu);
    return expect_int_eq("unhandled", command.handled, 0) &&
           expect_int_eq("unhandled_tool", command.tool, 5) &&
           expect_int_eq("unhandled_shape", command.brush_shape, 1) &&
           expect_int_eq("unhandled_radius", command.brush_radius, 9) &&
           expect_int_eq("unhandled_opacity", command.brush_opacity, 77) &&
           expect_uint_eq("unhandled_rgb", command.brush_color_rgb, 0x00ABCDEFu);
}

static int test_effect_shortcut_mapping(void) {
    AppToolEffectCommand clear = app_tool_effect_command_for_key('c');
    AppToolEffectCommand flip_h = app_tool_effect_command_for_key('h');
    AppToolEffectCommand flip_v = app_tool_effect_command_for_key('v');
    AppToolEffectCommand rotate = app_tool_effect_command_for_key('j');
    AppToolEffectCommand invert = app_tool_effect_command_for_key('x');
    AppToolEffectCommand none = app_tool_effect_command_for_key('f');

    return expect_int_eq("clear_handled", clear.handled, 1) &&
           expect_int_eq("clear_action", clear.action, APP_TOOL_EFFECT_CLEAR_LAYER) &&
           expect_int_eq("flip_h_action", flip_h.action, APP_TOOL_EFFECT_FLIP_HORIZONTAL) &&
           expect_int_eq("flip_v_action", flip_v.action, APP_TOOL_EFFECT_FLIP_VERTICAL) &&
           expect_int_eq("rotate_action", rotate.action, APP_TOOL_EFFECT_ROTATE_180) &&
           expect_int_eq("invert_action", invert.action, APP_TOOL_EFFECT_INVERT_RGB) &&
           expect_int_eq("none_handled", none.handled, 0) &&
           expect_int_eq("none_action", none.action, APP_TOOL_EFFECT_NONE);
}

int main(void) {
    if (!test_tool_and_palette_selection()) {
        return 1;
    }
    if (!test_radius_shape_and_opacity_adjustments()) {
        return 1;
    }
    if (!test_unhandled_key_passthrough()) {
        return 1;
    }
    if (!test_effect_shortcut_mapping()) {
        return 1;
    }
    return 0;
}
