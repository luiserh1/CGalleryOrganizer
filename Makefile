CC = clang
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99 -Iinclude -Ivendor -Isrc

SRC_DIRS = src/core src/systems src/utils
SRCS = $(wildcard $(addsuffix /*.c, $(SRC_DIRS))) src/main.c vendor/cJSON.c vendor/md5.c vendor/sha256.c src/systems/duplicate_finder.c
OBJS = $(SRCS:.c=.o)

TEST_SRCS = $(wildcard tests/test_*.c) vendor/cJSON.c vendor/md5.c vendor/sha256.c $(wildcard $(addsuffix /*.c, $(SRC_DIRS))) src/systems/duplicate_finder.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_BIN = tests/bin/test_runner

TARGET = bin/gallery_organizer

.PHONY: all clean test help

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
	rm -f gallery_cache.json

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

help:
	@echo "Available commands:"
	@echo "  make all    - Build the main executable (default)"
	@echo "  make test   - Build and run the test suite"
	@echo "  make clean  - Remove built objects and binaries"
	@echo "  make help   - Show this help message"
