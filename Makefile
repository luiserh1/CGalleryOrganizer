CC = clang
CFLAGS = -Wall -Wextra -std=c99 -Iinclude -Ivendor -Isrc

SRC_DIRS = src/core src/systems src/utils
SRCS = $(wildcard $(addsuffix /*.c, $(SRC_DIRS))) src/main.c vendor/cJSON.c
OBJS = $(SRCS:.c=.o)

TEST_SRCS = $(wildcard tests/test_*.c) vendor/cJSON.c $(wildcard $(addsuffix /*.c, $(SRC_DIRS)))
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_BIN = tests/bin/test_runner

TARGET = bin/gallery_organizer

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

test: $(TEST_BIN)
	@./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS) tests/test_runner.o
	@mkdir -p tests/bin
	$(CC) $(CFLAGS) -DTESTING -o $@ $^

clean:
	rm -f $(OBJS) $(TEST_OBJS) tests/test_runner.o
	rm -rf bin
	rm -rf tests/bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
