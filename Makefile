CC = gcc
CFLAGS = -Wall -Wextra -O2 -I vendor/pocket/include

# ゲームのソースファイル
SRC = main.c

# エンジンのコアファイル
ENGINE_SRC = $(wildcard vendor/pocket/src/engine/*.c)

# 出力される実行ファイル名
TARGET = bin/pocket_island

all: $(TARGET)

$(TARGET): $(SRC) $(ENGINE_SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(TARGET)
