# CGalleryOrganizer - Test Documentation

## Overview

This document describes the test infrastructure and coverage for the CGalleryOrganizer project.

## Running Tests

```bash
# Run all tests
make test

# Build runner only
make tests/bin/test_runner

# Clean build + test artifacts
make clean
```

## Test Infrastructure

### Framework

Custom lightweight test framework located in `tests/test_framework.h` (adapted from Project Carrera):

- **Simple registration**: Tests are registered via `register_test()` function
- **Assertions**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_FLOAT_EQ`, `ASSERT_STR_EQ`
- **Zero dependencies**: Works entirely in standard C.

### How It Works

Tests use the **actual source files** from `src/` with the `TESTING` preprocessor flag:

1. **TESTING flag** triggers conditional compilation in source files if necessary to mock OS interactions or slow IO.
2. **Makefile** compiles source files separately for tests:
   - `make test` compiles `src/**/*.c` and `tests/*.c` defining `-DTESTING`.

### Adding New Tests

1. Add test function to appropriate file:
   ```c
   void test_new_feature(void) {
       // Setup
       int result = 2 + 2;
       // Assert
       ASSERT_EQ(result, 4);
   }
   ```

2. Register the test:
   ```c
   void register_new_tests(void) {
       register_test("test_new_feature", test_new_feature, "category");
   }
   ```

3. Call registration in `tests/test_runner.c` main().

## Manual Smoke Testing Guide

While unit tests protect the core logic, you should manually verify the user experience against a dummy directory whenever modifying CLI arguments, prompts, or execution systems.

### 1. Preparation
Create a dummy directory with several test images (copies of real images to ensure EXIF data exists).
```bash
mkdir -p build/smoke_source build/smoke_env
cp tests/assets/jpg/sample_exif.jpg build/smoke_source/test1.jpg
cp tests/assets/jpg/sample_deep_exif.jpg build/smoke_source/test2.jpg
cp tests/assets/png/sample_no_exif.png build/smoke_source/test3.png
```

### 2. Standard Passive Scan
Verify the command runs without moving files, but writes a cache correctly.
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env
```
**Expected Outcome**: 
- `Files scanned: 3`, `Media files cached: 3`
- A file `.cache/gallery_cache.json` is generated inside `build/smoke_env`.
- No image files are moved.

### 3. Tree Preview Check
Verify the reorganization logic is generating correct targets before moving.
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --preview
```
**Expected Outcome**:
- Prints a visual tree to stdout.
- `_Unknown` group contains `test3.png` (since it lacks DateTaken).
- Another date directory (e.g., `_2004/_05`) contains `test1.jpg` and `test2.jpg`.
- Ends with `[*] Preview mode: No files were moved.`

### 4. Interactive Organization Execution
Verify the move operations and manifest creation.
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --organize
```
**Expected Outcome**:
- Prints the tree again.
- Prompts `Execute plan? [y/N]:`.
- Entering `n` cancels the operation.
- Entering `y` moves the files physically to `build/smoke_env/_...`.
- `build/smoke_source` is now empty.
- A `manifest.json` tracker file appears in `build/smoke_env`.

### 5. Rollback Recovery
Verify the application can restore the gallery safely.
```bash
./build/bin/gallery_organizer build/smoke_env build/smoke_env --rollback
```
**Expected Outcome**:
- Reads the manifest.
- Re-moves the files precisely back to `build/smoke_source`.
- Prints `[*] Rollback complete. Restored 3 files.`
- `build/smoke_env` is empty again (or only contains `.cache`).
