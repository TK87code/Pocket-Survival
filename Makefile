CC = gcc
CFLAGS = -Wall -Wextra -O2 -I vendor/pocket/include -DPKT_DEBUG

SRC = $(wildcard src/*.c)

ENGINE_SRC = $(wildcard vendor/pocket/src/engine/*.c)

EXT_SRC = $(wildcard vendor/pocket/src/ext/*.c)

TARGET = bin/pocket_island

all: $(TARGET)

$(TARGET): $(SRC) $(ENGINE_SRC) $(EXT_SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(TARGET)
