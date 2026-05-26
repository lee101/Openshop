#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

cc_bin="${CC:-cc}"
if ! command -v "$cc_bin" >/dev/null 2>&1; then
    echo "C compiler not found: set CC or install cc/gcc/clang." >&2
    exit 127
fi

cflags=(-std=c11 -O2 -Wall -Wextra)
ldflags=(-lm)

build_and_run() {
    local output="$1"
    shift
    "$cc_bin" "${cflags[@]}" "$@" -o "$output" "${ldflags[@]}"
    "./$output"
}

build_and_run canvas_smoke tests/canvas_smoke.c src/canvas.c src/layers.c
build_and_run image_selftest tests/image_selftest.c src/canvas.c
build_and_run shortcut_selftest \
    tests/shortcut_selftest.c \
    src/app_brush.c \
    src/app_brush_mask.c \
    src/app_canvas_click.c \
    src/app_canvas_ops.c \
    src/app_color.c \
    src/app_layer_state.c \
    src/app_preview.c \
    src/app_runtime_shortcuts.c \
    src/app_sampled_color.c \
    src/app_shape.c \
    src/app_shape_cancel.c \
    src/app_title.c \
    src/canvas.c \
    src/history_state.c \
    src/layers.c \
    src/layer_name_shortcuts.c \
    src/direct_layer_shortcuts.c \
    src/history_shortcuts.c \
    src/file_shortcuts.c \
    src/merge_shortcuts.c \
    src/paint_shortcuts.c \
    src/brush_shortcuts.c \
    src/view_shortcuts.c \
    src/canvas_shortcuts.c
build_and_run history_selftest \
    tests/history_selftest.c \
    src/app_canvas_ops.c \
    src/app_layer_state.c \
    src/canvas.c \
    src/layers.c \
    src/history_state.c
build_and_run api_selftest tests/api_selftest.c src/openshop_api.c src/app_shape.c src/canvas.c src/layers.c
build_and_run app_layout_selftest tests/app_layout_selftest.c src/app_layout.c

if command -v node >/dev/null 2>&1; then
    node tests/js_api_selftest.mjs
else
    echo "node not found; skipping JS API selftest"
fi
