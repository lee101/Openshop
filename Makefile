CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra
LDFLAGS =

SDL2_CONFIG := $(shell command -v sdl2-config 2>/dev/null)
HAVE_SDL2 := $(if $(SDL2_CONFIG),1,0)
ifeq ($(HAVE_SDL2),1)
CFLAGS += $(shell sdl2-config --cflags)
LDFLAGS += $(shell sdl2-config --libs)
endif

SRC = src/main.c src/app.c src/canvas.c src/image_io.c src/png_io.c src/path_routing.c src/layers.c src/cli.c
OBJ = $(SRC:.c=.o)
BIN = openshop

TEST_BIN = canvas_smoke
TEST_SRC = tests/canvas_smoke.c src/canvas.c src/layers.c
IMAGE_TEST_BIN = image_selftest
IMAGE_TEST_SRC = tests/image_selftest.c src/canvas.c
PNG_TEST_BIN = png_selftest
PNG_TEST_SRC = tests/png_selftest.c src/canvas.c src/png_io.c
PATH_TEST_BIN = path_routing_selftest
PATH_TEST_SRC = tests/path_routing_selftest.c src/path_routing.c
CLI_TEST_BIN = cli_selftest
CLI_TEST_SRC = tests/cli_selftest.c src/cli.c
SDL_TEST_BIN = image_io_smoke
SDL_TEST_SRC = tests/image_io_smoke.c src/canvas.c src/image_io.c src/png_io.c src/path_routing.c

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

test: $(TEST_BIN) $(IMAGE_TEST_BIN) $(PNG_TEST_BIN) $(PATH_TEST_BIN) $(CLI_TEST_BIN)
	./$(TEST_BIN)
	./$(IMAGE_TEST_BIN)
	./$(PNG_TEST_BIN)
	./$(PATH_TEST_BIN)
	./$(CLI_TEST_BIN)

test-sdl: check-sdl2 $(SDL_TEST_BIN)
	./$(SDL_TEST_BIN)

$(TEST_BIN): $(TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(TEST_SRC) -o $(TEST_BIN) -lm

$(IMAGE_TEST_BIN): $(IMAGE_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(IMAGE_TEST_SRC) -o $(IMAGE_TEST_BIN) -lm

$(PNG_TEST_BIN): $(PNG_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(PNG_TEST_SRC) -o $(PNG_TEST_BIN) -lm

$(PATH_TEST_BIN): $(PATH_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(PATH_TEST_SRC) -o $(PATH_TEST_BIN)

$(CLI_TEST_BIN): $(CLI_TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(CLI_TEST_SRC) -o $(CLI_TEST_BIN)

$(SDL_TEST_BIN): check-sdl2 $(SDL_TEST_SRC)
	$(CC) $(CFLAGS) $(SDL_TEST_SRC) -o $(SDL_TEST_BIN) $(LDFLAGS) -lm

clean:
	rm -f $(OBJ) $(BIN) $(TEST_BIN) $(IMAGE_TEST_BIN) $(PNG_TEST_BIN) $(PATH_TEST_BIN) $(CLI_TEST_BIN) $(SDL_TEST_BIN)

.PHONY: all clean test test-sdl
