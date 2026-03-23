CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra
LDFLAGS =

SDL2_CONFIG := $(shell command -v sdl2-config 2>/dev/null)
HAVE_SDL2 := $(if $(SDL2_CONFIG),1,0)
ifeq ($(HAVE_SDL2),1)
CFLAGS += $(shell sdl2-config --cflags)
LDFLAGS += $(shell sdl2-config --libs)
endif

SRC = src/main.c src/app.c src/app_document.c src/app_history.c src/app_navigation.c src/app_session.c src/app_tool.c src/app_translation.c src/canvas.c src/image_io.c src/layers.c
OBJ = $(SRC:.c=.o)
BIN = openshop

TEST_BIN = canvas_smoke
TEST_SRC = tests/canvas_smoke.c src/canvas.c src/layers.c
DOCUMENT_TEST_BIN = app_document_smoke
DOCUMENT_TEST_SRC = tests/app_document_smoke.c src/app_document.c src/canvas.c src/layers.c
HISTORY_TEST_BIN = app_history_smoke
HISTORY_TEST_SRC = tests/app_history_smoke.c src/app_history.c src/canvas.c src/layers.c
NAVIGATION_TEST_BIN = app_navigation_smoke
NAVIGATION_TEST_SRC = tests/app_navigation_smoke.c src/app_navigation.c
SESSION_TEST_BIN = app_session_smoke
SESSION_TEST_SRC = tests/app_session_smoke.c src/app_session.c
TRANSLATION_TEST_BIN = app_translation_smoke
TRANSLATION_TEST_SRC = tests/app_translation_smoke.c src/app_translation.c
TOOL_TEST_BIN = app_tool_smoke
TOOL_TEST_SRC = tests/app_tool_smoke.c src/app_tool.c
IMAGE_TEST_BIN = image_selftest
IMAGE_TEST_SRC = tests/image_selftest.c src/canvas.c
SDL_TEST_BIN = image_io_smoke
SDL_TEST_SRC = tests/image_io_smoke.c src/canvas.c src/image_io.c

all: $(BIN)

check-sdl2:
ifeq ($(HAVE_SDL2),0)
	@echo "Missing SDL2 development tools: sdl2-config not found."
	@echo "Install libsdl2-dev (or equivalent) to build $(BIN)."
	@echo "Canvas tests remain available with: make test"
	@false
endif

$(BIN): check-sdl2 $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

src/app.o: check-sdl2

test: $(TEST_BIN) $(DOCUMENT_TEST_BIN) $(HISTORY_TEST_BIN) $(NAVIGATION_TEST_BIN) $(SESSION_TEST_BIN) $(TRANSLATION_TEST_BIN) $(TOOL_TEST_BIN) $(IMAGE_TEST_BIN)
	./$(TEST_BIN)
	./$(DOCUMENT_TEST_BIN)
	./$(HISTORY_TEST_BIN)
	./$(NAVIGATION_TEST_BIN)
	./$(SESSION_TEST_BIN)
	./$(TRANSLATION_TEST_BIN)
	./$(TOOL_TEST_BIN)
	./$(IMAGE_TEST_BIN)

test-sdl: check-sdl2 $(SDL_TEST_BIN)
	./$(SDL_TEST_BIN)

$(TEST_BIN): $(TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(TEST_SRC) -o $(TEST_BIN) -lm

$(DOCUMENT_TEST_BIN): $(DOCUMENT_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(DOCUMENT_TEST_SRC) -o $(DOCUMENT_TEST_BIN) -lm

$(HISTORY_TEST_BIN): $(HISTORY_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(HISTORY_TEST_SRC) -o $(HISTORY_TEST_BIN) -lm

$(NAVIGATION_TEST_BIN): $(NAVIGATION_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(NAVIGATION_TEST_SRC) -o $(NAVIGATION_TEST_BIN) -lm

$(SESSION_TEST_BIN): $(SESSION_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(SESSION_TEST_SRC) -o $(SESSION_TEST_BIN) -lm

$(TRANSLATION_TEST_BIN): $(TRANSLATION_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(TRANSLATION_TEST_SRC) -o $(TRANSLATION_TEST_BIN) -lm

$(TOOL_TEST_BIN): $(TOOL_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(TOOL_TEST_SRC) -o $(TOOL_TEST_BIN) -lm

$(IMAGE_TEST_BIN): $(IMAGE_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(IMAGE_TEST_SRC) -o $(IMAGE_TEST_BIN) -lm

$(SDL_TEST_BIN): check-sdl2 $(SDL_TEST_SRC)
	$(CC) $(CFLAGS) $(SDL_TEST_SRC) -o $(SDL_TEST_BIN) $(LDFLAGS) -lm

clean:
	rm -f $(OBJ) $(BIN) $(TEST_BIN) $(DOCUMENT_TEST_BIN) $(HISTORY_TEST_BIN) $(NAVIGATION_TEST_BIN) $(SESSION_TEST_BIN) $(TRANSLATION_TEST_BIN) $(TOOL_TEST_BIN) $(IMAGE_TEST_BIN) $(SDL_TEST_BIN)

.PHONY: all clean test test-sdl
