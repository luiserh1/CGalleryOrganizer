CC = clang
CXX = clang++
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99 -Iinclude -Ivendor -Isrc $(shell pkg-config --cflags exiv2 2>/dev/null || echo "-I/opt/homebrew/include") $(shell pkg-config --cflags onnxruntime 2>/dev/null || echo "") $(shell pkg-config --cflags libzstd 2>/dev/null || pkg-config --cflags zstd 2>/dev/null || echo "")
CXXFLAGS = -Wall -Wextra -Werror -pedantic -std=c++17 -Iinclude -Ivendor -Isrc $(shell pkg-config --cflags exiv2 2>/dev/null || echo "-I/opt/homebrew/include") $(shell pkg-config --cflags onnxruntime 2>/dev/null || echo "") $(shell pkg-config --cflags libzstd 2>/dev/null || pkg-config --cflags zstd 2>/dev/null || echo "")
LDFLAGS = $(shell pkg-config --libs exiv2 2>/dev/null || echo "-L/opt/homebrew/lib -lexiv2") $(shell pkg-config --libs onnxruntime 2>/dev/null || echo "") $(shell pkg-config --libs libzstd 2>/dev/null || pkg-config --libs zstd 2>/dev/null || echo "") -lm

# Directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
TEST_OBJ_DIR = $(BUILD_DIR)/tests
BIN_DIR = $(BUILD_DIR)/bin
TEST_BIN_DIR = $(BUILD_DIR)/tests/bin

SRC_DIRS = src/core src/systems src/utils src/ml src/ml/providers
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

BENCHMARK_RUNNER_OBJ = $(TEST_OBJ_DIR)/tests/benchmark_runner.o
BENCHMARK_BIN = $(TEST_BIN_DIR)/benchmark_runner

TARGET = $(BIN_DIR)/gallery_organizer

.PHONY: all clean clean-all test stress benchmark benchmark-compare models help

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

test: $(TARGET) $(TEST_BIN) $(BENCHMARK_BIN)
	@./$(TEST_BIN)

benchmark: $(BENCHMARK_BIN)
	@if [ -z "$$BENCHMARK_DATASET" ] && [ ! -f "build/stress_data/.downloaded" ]; then \
		echo "[*] Dataset not found via lockfile. Attempting to download..."; \
		./scripts/download_stress_dataset.sh; \
	fi
	@./$(BENCHMARK_BIN)

benchmark-compare: $(BENCHMARK_BIN)
	@if [ -z "$$BENCHMARK_DATASET" ] && [ ! -f "build/stress_data/.downloaded" ]; then \
		echo "[*] Dataset not found via lockfile. Attempting to download..."; \
		./scripts/download_stress_dataset.sh; \
	fi
	@./$(BENCHMARK_BIN) --profile uncompressed
	@./$(BENCHMARK_BIN) --profile zstd-l3

stress: benchmark

models:
	@./scripts/download_models.sh

$(TEST_BIN): $(TEST_OBJS) $(TEST_RUNNER_OBJ)
	@mkdir -p $(TEST_BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(BENCHMARK_BIN): $(TEST_OBJS) $(BENCHMARK_RUNNER_OBJ)
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
	@echo "  make benchmark  - Run benchmark workloads and append JSONL output"
	@echo "  make benchmark-compare - Run benchmark profiles (uncompressed + zstd)"
	@echo "  make stress     - Alias for make benchmark"
	@echo "  make models     - Download and verify ML model artifacts into build/models"
