#include "../src/app_tool.h"

#include <stdio.h>

typedef struct {
    int push_count;
    int transform_result;
    int flood_fill_result;
    unsigned int sampled_color;
    int last_transform_action;
    int last_transform_active_layer;
    unsigned int last_fill_color;
} ToolEffectStubState;

static int expect_int_eq(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s mismatch: got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

static void stub_push_snapshot(const LayerStack *layers, void *userdata) {
    ToolEffectStubState *state = (ToolEffectStubState *)userdata;
    (void)layers;
    if (state) {
        state->push_count++;
    }
}

static int stub_transform_layer(LayerStack *layers, int active_layer, AppToolEffectAction action, void *userdata) {
    ToolEffectStubState *state = (ToolEffectStubState *)userdata;
    (void)layers;
    if (state) {
        state->last_transform_action = (int)action;
        state->last_transform_active_layer = active_layer;
        return state->transform_result;
    }
    return 0;
}

static int stub_flood_fill(Canvas *canvas, int x, int y, uint32_t color, void *userdata) {
    ToolEffectStubState *state = (ToolEffectStubState *)userdata;
    (void)canvas;
    (void)x;
    (void)y;
    if (state) {
        state->last_fill_color = color;
        return state->flood_fill_result;
    }
    return 0;
}

static uint32_t stub_sample_canvas(const Canvas *canvas, int x, int y, void *userdata) {
    ToolEffectStubState *state = (ToolEffectStubState *)userdata;
    (void)canvas;
    (void)x;
    (void)y;
    return state ? state->sampled_color : 0;
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
    AppToolEffectCommand fill = app_tool_effect_command_for_key('f');
    AppToolEffectCommand pick = app_tool_effect_command_for_key('i');
    AppToolEffectCommand none = app_tool_effect_command_for_key('q');

    return expect_int_eq("clear_handled", clear.handled, 1) &&
           expect_int_eq("clear_action", clear.action, APP_TOOL_EFFECT_CLEAR_LAYER) &&
           expect_int_eq("flip_h_action", flip_h.action, APP_TOOL_EFFECT_FLIP_HORIZONTAL) &&
           expect_int_eq("flip_v_action", flip_v.action, APP_TOOL_EFFECT_FLIP_VERTICAL) &&
           expect_int_eq("rotate_action", rotate.action, APP_TOOL_EFFECT_ROTATE_180) &&
           expect_int_eq("invert_action", invert.action, APP_TOOL_EFFECT_INVERT_RGB) &&
           expect_int_eq("fill_action", fill.action, APP_TOOL_EFFECT_FLOOD_FILL) &&
           expect_int_eq("pick_action", pick.action, APP_TOOL_EFFECT_PICK_COLOR) &&
           expect_int_eq("none_handled", none.handled, 0) &&
           expect_int_eq("none_action", none.action, APP_TOOL_EFFECT_NONE);
}

static int test_tool_and_effect_decoders_stay_partitioned(void) {
    AppToolCommand eraser_tool = app_tool_command_for_key('e', 0, 0, 6, 100, 0x001B1F24u);
    AppToolCommand fill_tool = app_tool_command_for_key('f', 0, 0, 6, 100, 0x001B1F24u);
    AppToolCommand pick_tool = app_tool_command_for_key('i', 0, 0, 6, 100, 0x001B1F24u);
    AppToolEffectCommand eraser_effect = app_tool_effect_command_for_key('e');
    AppToolEffectCommand fill_effect = app_tool_effect_command_for_key('f');
    AppToolEffectCommand pick_effect = app_tool_effect_command_for_key('i');

    return expect_int_eq("eraser_tool_handled", eraser_tool.handled, 1) &&
           expect_int_eq("eraser_tool_value", eraser_tool.tool, 1) &&
           expect_int_eq("fill_tool_handled", fill_tool.handled, 0) &&
           expect_int_eq("pick_tool_handled", pick_tool.handled, 0) &&
           expect_int_eq("eraser_effect_handled", eraser_effect.handled, 0) &&
           expect_int_eq("fill_effect_action", fill_effect.action, APP_TOOL_EFFECT_FLOOD_FILL) &&
           expect_int_eq("pick_effect_action", pick_effect.action, APP_TOOL_EFFECT_PICK_COLOR);
}

static int test_effect_apply_success_and_failure_paths(void) {
    LayerStack stack;
    Canvas preview = {0};
    Canvas composite = {0};
    ToolEffectStubState stub = {
        .transform_result = 1,
        .flood_fill_result = 1,
        .sampled_color = 0x8044AA11u,
        .last_transform_action = -1,
        .last_transform_active_layer = -1,
    };
    AppToolEffectCallbacks callbacks = {
        .push_snapshot = stub_push_snapshot,
        .transform_layer = stub_transform_layer,
        .flood_fill = stub_flood_fill,
        .sample_canvas = stub_sample_canvas,
        .userdata = &stub,
    };
    AppToolEffectState state = {
        .tool = 3,
        .brush_opacity = 40,
        .brush_color_rgb = 0x001B1F24u,
        .brush_color = 0x661B1F24u,
        .preview_active = 1,
        .needs_composite = 0,
    };
    AppToolEffectCommand clear = {.handled = 1, .action = APP_TOOL_EFFECT_CLEAR_LAYER};
    AppToolEffectCommand flip = {.handled = 1, .action = APP_TOOL_EFFECT_FLIP_HORIZONTAL};
    AppToolEffectCommand fill = {.handled = 1, .action = APP_TOOL_EFFECT_FLOOD_FILL};
    AppToolEffectCommand pick = {.handled = 1, .action = APP_TOOL_EFFECT_PICK_COLOR};

    if (!layer_stack_init(&stack, 4, 4, 0xFFFFFFFF) ||
        !canvas_init(&preview, 4, 4) ||
        !canvas_init(&composite, 4, 4)) {
        fprintf(stderr, "initialization failed\n");
        layer_stack_free(&stack);
        canvas_free(&preview);
        canvas_free(&composite);
        return 0;
    }

    if (!app_tool_effect_apply(clear, &stack, &state, &preview, &composite, 1, 1, 0x00000000u, &callbacks) ||
        !expect_int_eq("effect_clear_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("effect_clear_push_count", stub.push_count, 1)) {
        layer_stack_free(&stack);
        canvas_free(&preview);
        canvas_free(&composite);
        return 0;
    }

    state.needs_composite = 0;
    if (!app_tool_effect_apply(flip, &stack, &state, &preview, &composite, 1, 1, 0x00000000u, &callbacks) ||
        !expect_int_eq("effect_flip_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("effect_flip_action", stub.last_transform_action, APP_TOOL_EFFECT_FLIP_HORIZONTAL) ||
        !expect_int_eq("effect_flip_active_layer", stub.last_transform_active_layer, 0)) {
        layer_stack_free(&stack);
        canvas_free(&preview);
        canvas_free(&composite);
        return 0;
    }

    state.needs_composite = 0;
    if (!app_tool_effect_apply(fill, &stack, &state, &preview, &composite, 2, 2, 0x00000000u, &callbacks) ||
        !expect_int_eq("effect_fill_needs_composite", state.needs_composite, 1) ||
        !expect_int_eq("effect_fill_push_count", stub.push_count, 2) ||
        !expect_uint_eq("effect_fill_color", stub.last_fill_color, 0x661B1F24u)) {
        layer_stack_free(&stack);
        canvas_free(&preview);
        canvas_free(&composite);
        return 0;
    }

    state.needs_composite = 0;
    if (!app_tool_effect_apply(pick, &stack, &state, &preview, &composite, 1, 1, 0x00000000u, &callbacks) ||
        !expect_int_eq("effect_pick_tool", state.tool, 0) ||
        !expect_int_eq("effect_pick_opacity", state.brush_opacity, 50) ||
        !expect_uint_eq("effect_pick_rgb", state.brush_color_rgb, 0x0044AA11u) ||
        !expect_uint_eq("effect_pick_color", state.brush_color, 0x8044AA11u)) {
        layer_stack_free(&stack);
        canvas_free(&preview);
        canvas_free(&composite);
        return 0;
    }

    stub.transform_result = 0;
    state.needs_composite = 1;
    if (app_tool_effect_apply(flip, &stack, &state, &preview, &composite, 1, 1, 0x00000000u, &callbacks) ||
        !expect_int_eq("effect_fail_preserve_needs_composite", state.needs_composite, 1)) {
        layer_stack_free(&stack);
        canvas_free(&preview);
        canvas_free(&composite);
        return 0;
    }

    stub.flood_fill_result = 0;
    state.needs_composite = 0;
    if (app_tool_effect_apply(fill, &stack, &state, &preview, &composite, 2, 2, 0x00000000u, &callbacks) ||
        !expect_int_eq("effect_fill_fail_needs_composite", state.needs_composite, 0) ||
        !expect_int_eq("effect_fill_fail_push_count", stub.push_count, 3)) {
        layer_stack_free(&stack);
        canvas_free(&preview);
        canvas_free(&composite);
        return 0;
    }

    layer_stack_free(&stack);
    canvas_free(&preview);
    canvas_free(&composite);
    return 1;
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
    if (!test_tool_and_effect_decoders_stay_partitioned()) {
        return 1;
    }
    if (!test_effect_apply_success_and_failure_paths()) {
        return 1;
    }
    return 0;
}
