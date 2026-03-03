CC = clang
CXX = clang++
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99 -pthread -Iinclude -Ivendor -Isrc $(shell pkg-config --cflags exiv2 2>/dev/null || echo "-I/opt/homebrew/include") $(shell pkg-config --cflags onnxruntime 2>/dev/null || echo "") $(shell pkg-config --cflags libzstd 2>/dev/null || pkg-config --cflags zstd 2>/dev/null || echo "") $(EXTRA_CFLAGS)
CXXFLAGS = -Wall -Wextra -Werror -pedantic -std=c++17 -pthread -Iinclude -Ivendor -Isrc $(shell pkg-config --cflags exiv2 2>/dev/null || echo "-I/opt/homebrew/include") $(shell pkg-config --cflags onnxruntime 2>/dev/null || echo "") $(shell pkg-config --cflags libzstd 2>/dev/null || pkg-config --cflags zstd 2>/dev/null || echo "") $(EXTRA_CXXFLAGS)
DEPFLAGS = -MMD -MP
LDFLAGS = $(shell pkg-config --libs exiv2 2>/dev/null || echo "-L/opt/homebrew/lib -lexiv2") $(shell pkg-config --libs onnxruntime 2>/dev/null || echo "") $(shell pkg-config --libs libzstd 2>/dev/null || pkg-config --libs zstd 2>/dev/null || echo "") -lm -pthread $(EXTRA_LDFLAGS)
RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib 2>/dev/null || echo "")
RAYLIB_LIBS = $(shell pkg-config --libs raylib 2>/dev/null || echo "")
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
APP_API_LIB_EXT = dylib
APP_API_LIB_LDFLAGS = -dynamiclib
else
APP_API_LIB_EXT = so
APP_API_LIB_LDFLAGS = -shared
CFLAGS += -fPIC
CXXFLAGS += -fPIC
endif

ifeq ($(UNAME_S),Linux)
CFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += -D_XOPEN_SOURCE=700
endif

# Directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
TEST_OBJ_DIR = $(BUILD_DIR)/tests
BIN_DIR = $(BUILD_DIR)/bin
TEST_BIN_DIR = $(BUILD_DIR)/tests/bin

SRC_DIRS = src/core src/systems src/utils src/ml src/ml/providers src/cli src/app
COMMON_C_SRCS = $(wildcard $(addsuffix /*.c, $(SRC_DIRS))) vendor/cJSON.c vendor/md5.c vendor/sha256.c
CLI_MAIN_SRCS = src/main.c
C_SRCS = $(COMMON_C_SRCS) $(CLI_MAIN_SRCS)
CXX_SRCS = $(wildcard $(addsuffix /*.cpp, $(SRC_DIRS)))

# Generate object file paths in the OBJ_DIR
OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SRCS)) $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CXX_SRCS))

TEST_GUI_SRCS = src/gui/gui_state.c src/gui/gui_worker.c src/gui/gui_worker_inspect.c src/gui/gui_worker_tasks.c src/gui/frontends/functional/gui_fixed_layout.c src/gui/core/gui_action_rules.c src/gui/core/gui_rename_map.c src/gui/core/gui_path_picker.c src/gui/core/gui_rename_preview_model.c
TEST_SRCS = $(wildcard tests/test_*.c) vendor/cJSON.c vendor/md5.c vendor/sha256.c $(wildcard $(addsuffix /*.c, $(SRC_DIRS))) $(TEST_GUI_SRCS)
TEST_CXX_SRCS = $(wildcard $(addsuffix /*.cpp, $(SRC_DIRS)))

# Generate test object file paths in TEST_OBJ_DIR
TEST_ALL_OBJS = $(patsubst %.c,$(TEST_OBJ_DIR)/%.o,$(TEST_SRCS)) $(patsubst %.cpp,$(TEST_OBJ_DIR)/%.o,$(TEST_CXX_SRCS))
TEST_RUNNER_OBJ = $(TEST_OBJ_DIR)/tests/test_runner.o
TEST_OBJS = $(filter-out $(TEST_RUNNER_OBJ), $(TEST_ALL_OBJS))
TEST_BIN = $(TEST_BIN_DIR)/test_runner

BENCHMARK_RUNNER_OBJ = $(TEST_OBJ_DIR)/tests/benchmark_runner.o
BENCH_SUPPORT_SRCS = $(wildcard tests/bench/*.c)
BENCH_SUPPORT_OBJS = $(patsubst %.c,$(TEST_OBJ_DIR)/%.o,$(BENCH_SUPPORT_SRCS))
BENCHMARK_BIN = $(TEST_BIN_DIR)/benchmark_runner

TARGET = $(BIN_DIR)/gallery_organizer
GUI_SRC_DIRS = src/gui src/gui/core src/gui/frontends/functional
GUI_SRCS = $(wildcard $(addsuffix /*.c, $(GUI_SRC_DIRS)))
GUI_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(COMMON_C_SRCS) $(GUI_SRCS)) $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CXX_SRCS))
GUI_BIN = $(BIN_DIR)/gallery_organizer_gui
$(GUI_OBJS): CFLAGS += $(RAYLIB_CFLAGS)
APP_API_LIB = $(BUILD_DIR)/lib/libcgallery_app_api.$(APP_API_LIB_EXT)
APP_API_SRC_DIRS = src/core src/systems src/utils src/ml src/ml/providers src/app
APP_API_LIB_SRCS = $(wildcard $(addsuffix /*.c, $(APP_API_SRC_DIRS))) vendor/cJSON.c vendor/md5.c vendor/sha256.c
APP_API_LIB_SRCS += src/cli/cli_scan_pipeline.c
APP_API_LIB_CXX_SRCS = $(wildcard $(addsuffix /*.cpp, $(APP_API_SRC_DIRS)))
APP_API_LIB_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(APP_API_LIB_SRCS)) $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(APP_API_LIB_CXX_SRCS))

.PHONY: all clean clean-all test coverage benchmark benchmark-compare benchmark-sim-memory-compare benchmark-stats models gui run-gui check-gui-deps app-api-lib help

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

test: $(TARGET) $(TEST_BIN) $(BENCHMARK_BIN)
	@./$(TEST_BIN)

coverage:
	@./scripts/coverage_ci.sh

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
	@./$(BENCHMARK_BIN) --profile uncompressed --compare-profile zstd-l3 --comparison-path build/benchmark_compare.json

benchmark-stats: $(BENCHMARK_BIN)
	@if [ -z "$$BENCHMARK_DATASET" ] && [ ! -f "build/stress_data/.downloaded" ]; then \
		echo "[*] Dataset not found via lockfile. Attempting to download..."; \
		./scripts/download_stress_dataset.sh; \
	fi
	@./$(BENCHMARK_BIN) --runs $${BENCHMARK_RUNS:-5} --warmup-runs $${BENCHMARK_WARMUP_RUNS:-1}

benchmark-sim-memory-compare: $(BENCHMARK_BIN)
	@if [ -z "$$BENCHMARK_DATASET" ] && [ ! -f "build/stress_data/.downloaded" ]; then \
		echo "[*] Dataset not found via lockfile. Attempting to download..."; \
		./scripts/download_stress_dataset.sh; \
	fi
	@./$(BENCHMARK_BIN) --profile uncompressed --workload similarity_search --sim-memory-mode eager
	@./$(BENCHMARK_BIN) --profile uncompressed --workload similarity_search --sim-memory-mode chunked

models:
	@./scripts/download_models.sh

gui: check-gui-deps $(GUI_BIN)

run-gui: check-gui-deps $(GUI_BIN)
	@./$(GUI_BIN)

app-api-lib: $(APP_API_LIB)

check-gui-deps:
	@if ! pkg-config --exists raylib; then \
		echo "Error: raylib not found. Install raylib and ensure pkg-config can resolve it."; \
		exit 1; \
	fi

$(TEST_BIN): $(TEST_OBJS) $(TEST_RUNNER_OBJ)
	@mkdir -p $(TEST_BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(BENCHMARK_BIN): $(TEST_OBJS) $(BENCHMARK_RUNNER_OBJ) $(BENCH_SUPPORT_OBJS)
	@mkdir -p $(TEST_BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(GUI_BIN): $(GUI_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS) $(RAYLIB_LIBS)

$(APP_API_LIB): $(APP_API_LIB_OBJS)
	@mkdir -p $(BUILD_DIR)/lib
	$(CXX) $(APP_API_LIB_LDFLAGS) -o $@ $^ $(LDFLAGS)

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
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# Compile C++ files
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# Compile test C files
$(TEST_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# Compile test C++ files
$(TEST_OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(OBJS:.o=.d) $(TEST_ALL_OBJS:.o=.d) $(BENCH_SUPPORT_OBJS:.o=.d) $(GUI_OBJS:.o=.d)

help:
	@echo "Available commands:"
	@echo "  make        - Build the main executable (alias for make all)"
	@echo "  make all    - Build the main executable (default)"
	@echo "  make test   - Build and run the test suite"
	@echo "  make coverage - Run tests with coverage instrumentation and emit reports"
	@echo "  make clean      - Remove built objects and binaries (preserves datasets)"
	@echo "  make clean-all  - Recursively remove the entire build directory (including datasets)"
	@echo "  make help       - Show this help message"
	@echo "  make benchmark  - Run benchmark workloads and append JSONL output"
	@echo "  make benchmark-compare - Run baseline vs candidate profiles with JSON comparison export"
	@echo "  make benchmark-stats - Run repeated benchmark samples with warmups and summary stats"
	@echo "  make benchmark-sim-memory-compare - Compare similarity_search RSS in eager vs chunked modes"
	@echo "  make models     - Download and verify ML model artifacts into build/models"
	@echo "  make gui        - Build GUI frontend (requires raylib)"
	@echo "  make run-gui    - Build and run GUI frontend"
	@echo "  make app-api-lib - Build shared app API library for native frontends"
