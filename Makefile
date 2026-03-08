CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs)

SRC = src/main.c src/app.c src/canvas.c
OBJ = $(SRC:.c=.o)
BIN = openshop

TEST_BIN = canvas_smoke
TEST_SRC = tests/canvas_smoke.c src/canvas.c

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC)
	$(CC) -std=c11 -O2 -Wall -Wextra $(TEST_SRC) -o $(TEST_BIN)

clean:
	rm -f $(OBJ) $(BIN) $(TEST_BIN)

.PHONY: all clean test
