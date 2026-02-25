CC = clang
CXX = clang++
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99 -Iinclude -Ivendor -Isrc $(shell pkg-config --cflags exiv2 2>/dev/null || echo "-I/opt/homebrew/include")
CXXFLAGS = -Wall -Wextra -Werror -pedantic -std=c++17 -Iinclude -Ivendor -Isrc $(shell pkg-config --cflags exiv2 2>/dev/null || echo "-I/opt/homebrew/include")
LDFLAGS = $(shell pkg-config --libs exiv2 2>/dev/null || echo "-L/opt/homebrew/lib -lexiv2")

SRC_DIRS = src/core src/systems src/utils
C_SRCS = $(wildcard $(addsuffix /*.c, $(SRC_DIRS))) src/main.c vendor/cJSON.c vendor/md5.c vendor/sha256.c src/systems/duplicate_finder.c
CXX_SRCS = $(wildcard $(addsuffix /*.cpp, $(SRC_DIRS)))
OBJS = $(C_SRCS:.c=.o) $(CXX_SRCS:.cpp=.o)

TEST_SRCS = $(wildcard tests/test_*.c) vendor/cJSON.c vendor/md5.c vendor/sha256.c $(wildcard $(addsuffix /*.c, $(SRC_DIRS))) src/systems/duplicate_finder.c
TEST_CXX_SRCS = $(wildcard $(addsuffix /*.cpp, $(SRC_DIRS)))
TEST_OBJS = $(TEST_SRCS:.c=.o) $(TEST_CXX_SRCS:.cpp=.o)
TEST_BIN = tests/bin/test_runner

TARGET = bin/gallery_organizer

.PHONY: all clean test help

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CXX) -o $@ $^ $(LDFLAGS)

test: $(TEST_BIN)
	@./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS) tests/test_runner.o
	@mkdir -p tests/bin
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(TEST_OBJS) tests/test_runner.o
	rm -rf bin
	rm -rf tests/bin
	rm -f gallery_cache.json

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

help:
	@echo "Available commands:"
	@echo "  make all    - Build the main executable (default)"
	@echo "  make test   - Build and run the test suite"
	@echo "  make clean  - Remove built objects and binaries"
	@echo "  make help   - Show this help message"
