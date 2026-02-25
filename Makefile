CC = clang
CXX = clang++
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99 -Iinclude -Ivendor -Isrc $(shell pkg-config --cflags exiv2 2>/dev/null || echo "-I/opt/homebrew/include")
CXXFLAGS = -Wall -Wextra -Werror -pedantic -std=c++17 -Iinclude -Ivendor -Isrc $(shell pkg-config --cflags exiv2 2>/dev/null || echo "-I/opt/homebrew/include")
LDFLAGS = $(shell pkg-config --libs exiv2 2>/dev/null || echo "-L/opt/homebrew/lib -lexiv2")

# Directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
TEST_OBJ_DIR = $(BUILD_DIR)/tests
BIN_DIR = $(BUILD_DIR)/bin
TEST_BIN_DIR = $(BUILD_DIR)/tests/bin

SRC_DIRS = src/core src/systems src/utils
C_SRCS = $(wildcard $(addsuffix /*.c, $(SRC_DIRS))) src/main.c vendor/cJSON.c vendor/md5.c vendor/sha256.c src/systems/duplicate_finder.c
CXX_SRCS = $(wildcard $(addsuffix /*.cpp, $(SRC_DIRS)))

# Generate object file paths in the OBJ_DIR
OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SRCS)) $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CXX_SRCS))

TEST_SRCS = $(wildcard tests/test_*.c) vendor/cJSON.c vendor/md5.c vendor/sha256.c $(wildcard $(addsuffix /*.c, $(SRC_DIRS))) src/systems/duplicate_finder.c
TEST_CXX_SRCS = $(wildcard $(addsuffix /*.cpp, $(SRC_DIRS)))

# Generate test object file paths in TEST_OBJ_DIR
TEST_ALL_OBJS = $(patsubst %.c,$(TEST_OBJ_DIR)/%.o,$(TEST_SRCS)) $(patsubst %.cpp,$(TEST_OBJ_DIR)/%.o,$(TEST_CXX_SRCS))
TEST_RUNNER_OBJ = $(TEST_OBJ_DIR)/tests/test_runner.o
TEST_OBJS = $(filter-out $(TEST_RUNNER_OBJ), $(TEST_ALL_OBJS))
TEST_BIN = $(TEST_BIN_DIR)/test_runner

PERF_RUNNER_OBJ = $(TEST_OBJ_DIR)/tests/perf_runner.o
PERF_BIN = $(TEST_BIN_DIR)/perf_runner

TARGET = $(BIN_DIR)/gallery_organizer

.PHONY: all clean clean-all test stress help

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

test: $(TARGET) $(TEST_BIN)
	@./$(TEST_BIN)

stress: $(PERF_BIN)
	@if [ ! -f "build/stress_data/.downloaded" ]; then \
		echo "[*] Dataset not found via lockfile. Attempting to download..."; \
		./scripts/download_stress_dataset.sh; \
	fi
	@./$(PERF_BIN)

$(TEST_BIN): $(TEST_OBJS) $(TEST_RUNNER_OBJ)
	@mkdir -p $(TEST_BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(PERF_BIN): $(TEST_OBJS) $(PERF_RUNNER_OBJ)
	@mkdir -p $(TEST_BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(TEST_OBJ_DIR)
	rm -rf $(BIN_DIR)
	rm -rf $(TEST_BIN_DIR)
	rm -rf .cache
	@find . -type f -name '*.o' -delete

clean-all: clean
	rm -rf $(BUILD_DIR)

# Compile C files
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C++ files
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile test C files
$(TEST_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test C++ files
$(TEST_OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

help:
	@echo "Available commands:"
	@echo "  make        - Build the main executable (alias for make all)"
	@echo "  make all    - Build the main executable (default)"
	@echo "  make test   - Build and run the test suite"
	@echo "  make clean      - Remove built objects and binaries (preserves datasets)"
	@echo "  make clean-all  - Recursively remove the entire build directory (including datasets)"
	@echo "  make help       - Show this help message"
	@echo "  make stress     - Run performance and resiliency tests against datasets"
