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
