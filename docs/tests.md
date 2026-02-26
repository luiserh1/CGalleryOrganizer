# CGalleryOrganizer - Test Documentation

## Overview

The repository uses a custom lightweight test framework in `tests/test_framework.h` and a single runner in `tests/test_runner.c`.

## Running Tests

```bash
# Build main binary + test runner and execute all tests
make test

# Build binaries only
make

# Run benchmark workloads (appends JSONL entries)
make benchmark

# Run baseline + compressed profile
make benchmark-compare

# Run repeated stats mode (defaults: 5 measured + 1 warmup)
make benchmark-stats

# Compare similarity memory modes
make benchmark-sim-memory-compare

# Build GUI frontend (requires raylib)
make gui

# Build shared app API library (native frontend integration target)
make app-api-lib

# Clean binaries and generated build artifacts
make clean
```

## Framework

Assertions currently used by the suite:
- `ASSERT_TRUE`
- `ASSERT_FALSE`
- `ASSERT_EQ`
- `ASSERT_STR_EQ`

Tests are registered with `register_test(name, fn, category)` and executed by the runner.

## Coverage Focus

- Filesystem utilities (`src/utils/fs_utils.c`)
- Cache lifecycle and schema evolution (`src/core/gallery_cache_io.c`, `src/core/gallery_cache_entry.c`)
- Hashing helpers (`src/utils/hash_utils.c`)
- Duplicate grouping (`src/systems/duplicate_finder.c`)
- Organizer plan/execute/rollback behavior (`src/systems/organizer_planner.c`, `src/systems/organizer_executor.c`, `src/systems/organizer_manifest.c`)
- Metadata extraction integration using real assets (`tests/assets/**`)
- ML subsystem API/provider behavior (`src/ml/**`)
- Similarity engine behavior (`src/systems/similarity_codec.c`, `src/systems/similarity_math.c`, `src/systems/similarity_report.c`)
- CLI flow checks through the built binary (`build/bin/gallery_organizer`), including:
  - `--exhaustive`
  - rollback compatibility forms
  - `--group-by` validation
  - `--ml-enrich` success/failure paths
  - `--similarity-report` generation path
  - `--sim-incremental` reuse/force behavior
  - `--sim-memory-mode` validation + parity path coverage
  - `--jobs` and `CGO_JOBS` validation/override behavior
  - `--cache-compress auto` threshold selection
- App API layer validation (`src/app/*`), including request validation and cancellation handling
- Cache profile persistence + strict match/rebuild logic (`src/app/app_cache_profile.c`)
- Runtime-state/model-management app API checks:
  - runtime prerequisite inspection
  - native model install manifest + checksum paths
- GUI state persistence and reset behavior (`src/gui/gui_state.c`)
- Functional GUI fixed-layout invariants (`src/gui/frontends/functional/gui_fixed_layout.c`)
- GUI action dependency rules (`src/gui/core/gui_action_rules.c`)

## Manual Smoke Checklist

### 1. Prepare sample directories
```bash
mkdir -p build/smoke_source build/smoke_env
cp tests/assets/jpg/sample_exif.jpg build/smoke_source/test1.jpg
cp tests/assets/jpg/sample_deep_exif.jpg build/smoke_source/test2.jpg
cp tests/assets/png/sample_no_exif.png build/smoke_source/test3.png
```

### 2. Install models
```bash
make models
```

### 3. Scan
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env
```
Expected:
- scan succeeds, cache stored in `build/smoke_env/.cache/gallery_cache.json`.
- first run reports profile rebuild (`Cache profile: rebuilt (cache profile missing)`).

Profile reuse check:
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env
```
Expected:
- second run reports `Cache profile: matched`.
- sidecar exists at `build/smoke_env/.cache/gallery_cache.profile.json`.

Profile mismatch check:
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --exhaustive
```
Expected:
- run reports cache profile rebuild with reason `exhaustive setting changed`.

### 4. ML enrichment
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --ml-enrich
```
Expected: ML summary counters are printed and cache entries include ML fields.

### 5. Preview
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --preview --group-by date
```
Expected: plan printed once per directory group, no file moves.

### 6. Organize
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --organize
```
Expected: confirmation prompt, files moved after `y`, `manifest.json` created.

### 7. Rollback (ergonomic)
```bash
./build/bin/gallery_organizer build/smoke_env --rollback
```
Expected: files restored according to manifest.

### 8. Similarity report
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report --sim-threshold 0.92 --sim-topk 5
```
Expected: `build/smoke_env/similarity_report.json` exists and includes `groupCount` + `groups`.

### 9. Incremental similarity smoke
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report --sim-incremental off
```
Expected:
- second run reuses embeddings (`ML evaluated: 0`).
- third run forces fresh embedding inference (`ML evaluated` increases).

### 10. Parallel jobs parity smoke
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report --jobs 1
cp build/smoke_env/similarity_report.json build/smoke_env/sim_jobs1_report.json
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report --jobs 4
```
Expected: `similarity_report.json` remains semantically equivalent to the `--jobs 1` output (ignoring `generatedAt` timestamp).

### 11. Similarity viewer smoke
```bash
python3 -m http.server 8000
```
Then open `/tools/similarity_viewer/`, click **Load Default** (after copying report to `build/similarity_report.json`) or upload `build/smoke_env/similarity_report.json`.

### 12. Benchmark smoke
```bash
BENCHMARK_DATASET=tests/assets ./build/tests/bin/benchmark_runner --profile uncompressed --workload cache_metadata_only
```
Expected:
- `build/benchmark_history.jsonl` appended with one JSON object.
- `build/benchmark_last.json` refreshed.
- each record includes `simMemoryMode` (`chunked` or `eager`).

Optional reproducibility command:
```bash
BENCHMARK_DATASET=tests/assets ./build/tests/bin/benchmark_runner --profile uncompressed --workload cache_metadata_only --runs 5 --warmup-runs 1 --compare-profile zstd-l3 --comparison-path build/benchmark_compare.json
```
Expected:
- `build/benchmark_last.json` contains median/p95/min/max/stddev stats per workload.
- `build/benchmark_compare.json` contains per-workload `deltaPct` fields for median metrics.

### 13. GUI smoke
```bash
make gui
./build/bin/gallery_organizer_gui
```
Expected:
- manual gallery/env path inputs are editable.
- scan, ML enrich, similarity, organize, rollback, duplicate actions are invokable.
- background tasks show progress and can be cancelled.
- active tab and selected mode controls are visually highlighted.
- while a task is running, task-start action buttons are disabled until completion/cancel.
- actions with unmet prerequisites are disabled and report an explicit reason when clicked.
- each panel exposes hover tooltip help for inputs/actions.
- Scan panel includes **Download Models** and completes model install workflow.
- jobs/compression level/threshold/topK fields enforce documented numeric ranges.
- GUI window starts fixed at `1280x820` (non-resizable baseline in 0.5.3).
- GUI window starts fixed at `1280x820` (non-resizable functional baseline).
- panel controls remain aligned with no overlaps in the fixed shell.
- saved gallery/env paths persist between runs.
- `--reset-state` clears saved GUI state.

## Notes on Test Fragility

- Some tests invoke shell commands (`system`/`popen`) and use temporary build directories.
- Keep temp paths scoped under `build/` and clean them in each test to avoid cross-test interference.
- Integration tests are split by domain (`tests/test_integration_metadata.c`, `tests/test_cli_core.c`, `tests/test_cli_ml_similarity.c`) and composed by `tests/test_integration.c`.

## Roadmap Test Gates (0.4.3+)

- `v0.4.3`:
  - verify `chunked` and `eager` similarity modes produce equivalent output.
  - verify peak RSS for similarity path is improved or justified.
- `v0.4.4`:
  - verify `--jobs 1` and `--jobs N` output parity.
  - run repeated parallel smoke to detect instability/deadlocks.
- `codex/benchmark-methodology` branch:
  - validate statistical report fields (`runs`, `median`, `p95`, `stddev`) without runtime behavior diffs.

## Asset and Model Attribution Requirements

When adding or updating fixtures under `tests/assets/**`, update
`docs/test_assets.md` in the same commit.

When adding or updating model entries in `models/manifest.json`, update
`docs/model_assets.md` in the same commit.
