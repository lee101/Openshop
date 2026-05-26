CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra
LDFLAGS = -lm

SDL2_CONFIG := $(shell command -v sdl2-config 2>/dev/null)
HAVE_SDL2 := $(if $(SDL2_CONFIG),1,0)
ifeq ($(HAVE_SDL2),1)
CFLAGS += $(shell sdl2-config --cflags)
LDFLAGS += $(shell sdl2-config --libs)
endif

SRC = src/main.c src/app.c src/app_brush.c src/app_brush_mask.c src/app_canvas_click.c src/app_canvas_ops.c src/app_color.c src/app_layer_state.c src/app_layout.c src/app_preview.c src/app_runtime_shortcuts.c src/app_sampled_color.c src/app_shape.c src/app_shape_cancel.c src/app_title.c src/canvas.c src/image_io.c src/layers.c src/layer_name_shortcuts.c src/direct_layer_shortcuts.c src/history_shortcuts.c src/history_state.c src/file_shortcuts.c src/merge_shortcuts.c src/paint_shortcuts.c src/brush_shortcuts.c src/view_shortcuts.c src/canvas_shortcuts.c src/openshop_api.c src/openshop_io_api.c
OBJ = $(SRC:.c=.o)
BIN = openshop

TEST_BIN = canvas_smoke
TEST_SRC = tests/canvas_smoke.c src/canvas.c src/layers.c
IMAGE_TEST_BIN = image_selftest
IMAGE_TEST_SRC = tests/image_selftest.c src/canvas.c
SHORTCUT_TEST_BIN = shortcut_selftest
SHORTCUT_TEST_SRC = tests/shortcut_selftest.c src/app_brush.c src/app_brush_mask.c src/app_canvas_click.c src/app_canvas_ops.c src/app_color.c src/app_layer_state.c src/app_preview.c src/app_runtime_shortcuts.c src/app_sampled_color.c src/app_shape.c src/app_shape_cancel.c src/app_title.c src/canvas.c src/history_state.c src/layers.c src/layer_name_shortcuts.c src/direct_layer_shortcuts.c src/history_shortcuts.c src/file_shortcuts.c src/merge_shortcuts.c src/paint_shortcuts.c src/brush_shortcuts.c src/view_shortcuts.c src/canvas_shortcuts.c
HISTORY_TEST_BIN = history_selftest
HISTORY_TEST_SRC = tests/history_selftest.c src/app_canvas_ops.c src/app_layer_state.c src/canvas.c src/layers.c src/history_state.c
API_TEST_BIN = api_selftest
API_TEST_SRC = tests/api_selftest.c src/openshop_api.c src/app_shape.c src/canvas.c src/layers.c
LAYOUT_TEST_BIN = app_layout_selftest
LAYOUT_TEST_SRC = tests/app_layout_selftest.c src/app_layout.c
SDL_TEST_BIN = image_io_smoke
SDL_TEST_SRC = tests/image_io_smoke.c src/canvas.c src/image_io.c
VISUALBENCH_BIN = visualbench_runner
VISUALBENCH_SRC = tools/visualbench.c src/app_brush.c src/app_brush_mask.c src/app_layer_state.c src/app_shape.c src/canvas.c src/history_state.c src/image_io.c src/layers.c

all: $(BIN)

help:
	@echo "Targets:"
	@printf "  %-20s %s\n" "make or make $(BIN)" "Build the SDL app when SDL2 development tools are installed."
	@printf "  %-20s %s\n" "make test" "Run C selftests and JS API selftests when Node is available."
	@printf "  %-20s %s\n" "make check" "Alias for make test."
	@printf "  %-20s %s\n" "make check-sdl" "Verify that SDL2 development tools are available."
	@printf "  %-20s %s\n" "make test-sdl" "Run the SDL-backed image I/O smoke test when SDL2 is available."
	@printf "  %-20s %s\n" "make visualbench" "Generate deterministic screenshots in visualbench/."
	@printf "  %-20s %s\n" "make clean" "Remove built binaries and object files."
ifeq ($(HAVE_SDL2),0)
	@echo ""
	@echo "SDL2 status: unavailable (sdl2-config not found)."
	@echo "  openshop, check-sdl, and test-sdl will print install guidance in this environment."
endif
ifeq ($(HAVE_SDL2),1)
	@echo ""
	@echo "SDL2 status: available ($(shell sdl2-config --version))"
endif

check-sdl2:
ifeq ($(HAVE_SDL2),0)
	@echo "Missing SDL2 development tools: sdl2-config not found."
	@echo "Install libsdl2-dev (or equivalent) to build $(BIN)."
	@echo "Canvas tests remain available with: make test or make check"
	@false
endif

check-sdl: check-sdl2
ifeq ($(HAVE_SDL2),1)
	@echo "SDL2 status: available ($(shell sdl2-config --version))"
endif

$(BIN): check-sdl2 $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

src/app.o: check-sdl2

test: $(TEST_BIN) $(IMAGE_TEST_BIN) $(SHORTCUT_TEST_BIN) $(HISTORY_TEST_BIN) $(API_TEST_BIN) $(LAYOUT_TEST_BIN) test-js
	./$(TEST_BIN)
	./$(IMAGE_TEST_BIN)
	./$(SHORTCUT_TEST_BIN)
	./$(HISTORY_TEST_BIN)
	./$(API_TEST_BIN)
	./$(LAYOUT_TEST_BIN)

check: test

test-js:
	@if command -v node >/dev/null 2>&1; then node tests/js_api_selftest.mjs; else echo "node not found; skipping JS API selftest"; fi

test-sdl: check-sdl2 $(SDL_TEST_BIN)
	./$(SDL_TEST_BIN)

visualbench: check-sdl2 $(VISUALBENCH_BIN)
	mkdir -p visualbench
	./$(VISUALBENCH_BIN)

$(TEST_BIN): $(TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(TEST_SRC) -o $(TEST_BIN) -lm

$(IMAGE_TEST_BIN): $(IMAGE_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(IMAGE_TEST_SRC) -o $(IMAGE_TEST_BIN) -lm

$(SHORTCUT_TEST_BIN): $(SHORTCUT_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(SHORTCUT_TEST_SRC) -o $(SHORTCUT_TEST_BIN) -lm

$(HISTORY_TEST_BIN): $(HISTORY_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(HISTORY_TEST_SRC) -o $(HISTORY_TEST_BIN) -lm

$(API_TEST_BIN): $(API_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(API_TEST_SRC) -o $(API_TEST_BIN) -lm

$(LAYOUT_TEST_BIN): $(LAYOUT_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(LAYOUT_TEST_SRC) -o $(LAYOUT_TEST_BIN) -lm

$(SDL_TEST_BIN): check-sdl2 $(SDL_TEST_SRC)
	$(CC) $(CFLAGS) $(SDL_TEST_SRC) -o $(SDL_TEST_BIN) $(LDFLAGS) -lm

$(VISUALBENCH_BIN): check-sdl2 $(VISUALBENCH_SRC)
	$(CC) $(CFLAGS) $(VISUALBENCH_SRC) -o $(VISUALBENCH_BIN) $(LDFLAGS) -lm

clean:
	rm -f $(OBJ) $(BIN) $(TEST_BIN) $(IMAGE_TEST_BIN) $(SHORTCUT_TEST_BIN) $(HISTORY_TEST_BIN) $(API_TEST_BIN) $(LAYOUT_TEST_BIN) $(SDL_TEST_BIN) $(VISUALBENCH_BIN)

.PHONY: all check check-sdl check-sdl2 clean help test test-js test-sdl visualbench
