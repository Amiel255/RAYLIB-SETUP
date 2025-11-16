CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99
LIBS ?= -lraylib -lm -lpthread -ldl -lrt -lX11

SRC = main.c
BIN = neon_drift

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LIBS)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)

.PHONY: all run clean
