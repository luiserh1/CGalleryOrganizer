# CGalleryOrganizer - Test Documentation

## Overview

The repository uses a custom lightweight test framework in `tests/test_framework.h` and a single runner in `tests/test_runner.c`.

## Running Tests

```bash
# Build main binary + test runner and execute all tests
make test

# Build binaries only
make

# Clean binaries and generated build artifacts
make clean
```

## Framework

Available assertions currently used by the suite:
- `ASSERT_TRUE`
- `ASSERT_FALSE`
- `ASSERT_EQ`
- `ASSERT_STR_EQ`

Tests are registered with `register_test(name, fn, category)` and executed by the runner.

## Current Coverage Focus

- Filesystem utilities (`src/utils/fs_utils.c`)
- Cache lifecycle and key enumeration (`src/core/gallery_cache.c`)
- Hashing helpers (`src/utils/hash_utils.c`)
- Duplicate grouping (`src/systems/duplicate_finder.c`)
- Organizer plan, execution primitives, and rollback (`src/systems/organizer.c`)
- Metadata extraction integration through real sample assets (`tests/assets/**`)
- CLI flow checks through the built binary (`build/bin/gallery_organizer`):
  - `--exhaustive` behavior
  - rollback single-positional and legacy forms
  - `--group-by` invalid/empty input rejection

## Manual Smoke Checklist

### 1. Prepare sample dirs
```bash
mkdir -p build/smoke_source build/smoke_env
cp tests/assets/jpg/sample_exif.jpg build/smoke_source/test1.jpg
cp tests/assets/jpg/sample_deep_exif.jpg build/smoke_source/test2.jpg
cp tests/assets/png/sample_no_exif.png build/smoke_source/test3.png
```

### 2. Scan
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env
```
Expected: scan succeeds, cache stored in `build/smoke_env/.cache/gallery_cache.json`.

### 3. Preview
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --preview --group-by date
```
Expected: plan printed once per directory group, no file moves.

### 4. Organize
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --organize
```
Expected: confirmation prompt, files moved after `y`, `manifest.json` created.

### 5. Rollback (ergonomic)
```bash
./build/bin/gallery_organizer build/smoke_env --rollback
```
Expected: files restored according to manifest.

## Notes on Test Fragility

- Some tests invoke shell commands (`system`/`popen`) and use temporary build directories.
- Keep temp paths scoped under `build/` and clean them in each test to avoid cross-test interference.

## Asset Attribution Requirement

When adding or updating any fixture under `tests/assets/**`, update
`docs/test_assets.md` in the same commit. That file is the canonical attribution
record (author, source URL, license, and derivative notes).
