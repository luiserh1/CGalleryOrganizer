# CGalleryOrganizer - Test Documentation

## Overview

The repository uses a custom lightweight test framework in `tests/test_framework.h` and a single runner in `tests/test_runner.c`.

## Running Tests

```bash
# Build main binary + test runner and execute all tests
make test

# Run release checklist gates (tests + optional GUI + optional tag check)
./scripts/release_check.sh
./scripts/release_check.sh --expected-tag v0.6.9

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
- Dedicated rename workflow behavior (`src/systems/renamer_*.c`, `src/app/app_rename.c`)
- Metadata extraction integration using real assets (`tests/assets/**`)
- ML subsystem API/provider behavior (`src/ml/**`)
- Similarity engine behavior (`src/systems/similarity_codec.c`, `src/systems/similarity_math.c`, `src/systems/similarity_report.c`)
- CLI flow checks through the built binary (`build/bin/gallery_organizer`), including:
  - `--exhaustive`
  - rollback compatibility forms
  - `--group-by` validation
  - duplicate flow safety:
    - plain scan does not auto-move duplicates
    - `--duplicates-report` is non-mutating
    - `--duplicates-move` requires `env_dir` and performs explicit move
  - `--ml-enrich` success/failure paths
  - `--similarity-report` generation path
  - `--sim-incremental` reuse/force behavior
  - `--sim-memory-mode` validation + parity path coverage
  - `--jobs` and `CGO_JOBS` validation/override behavior
  - `--cache-compress auto` threshold selection
  - dedicated rename flow:
    - `--rename-init`
    - `--rename-bootstrap-tags-from-filename`
    - `--rename-preview`
    - `--rename-meta-tag-add` / `--rename-meta-tag-remove`
    - `--rename-meta-fields`
    - `--rename-preview-json` / `--rename-preview-json-out`
    - `--rename-apply` + `--rename-from-preview`
    - `--rename-apply-latest`
    - `--rename-preview-latest-id`
    - collision gate + `--rename-accept-auto-suffix`
    - `--rename-history`
    - `--rename-history-latest-id`
    - `--rename-history-detail`
    - `--rename-redo`
    - `--rename-rollback`
    - JSON tags-map ingest validation
- App API layer validation (`src/app/*`), including request validation and cancellation handling
- App API workflow coverage for duplicate and organize operations:
  - `AppFindDuplicates` + `AppMoveDuplicates`
  - `AppRunScan` and `AppFindDuplicates` keep duplicate-source files non-mutated
    until explicit move operation
  - `AppMoveDuplicates` requires explicit duplicate report input
  - `AppPreviewOrganize` + `AppExecuteOrganize` + `AppRollback`
- Cache profile persistence + strict match/rebuild logic (`src/app/app_cache_profile.c`)
- Runtime-state/model-management app API checks:
  - runtime prerequisite inspection
  - lightweight cache count behavior (`cache_entry_count_known`)
  - native model install manifest + checksum paths
- GUI state persistence and reset behavior (`src/gui/gui_state.c`)
- Functional GUI fixed-layout invariants (`src/gui/frontends/functional/gui_fixed_layout.c`)
- GUI action dependency rules (`src/gui/core/gui_action_rules.c`)
- GUI rename panel wiring (`src/gui/frontends/functional/gui_panels_rename.c`)
- GUI per-file tags-map upsert helper (`src/gui/core/gui_rename_map.c`)
- GUI rename preview filter/selection model (`src/gui/core/gui_rename_preview_model.c`)
- GUI path/file picker status handling and backend fallback logic (`src/gui/core/gui_path_picker.c`)
- GUI rename task integration workflows (`src/gui/gui_worker*.c`)
  - latest-id/history-detail/redo task coverage

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
- sidecar JSON may include optional `cacheEntryCount` after successful save.

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

### 5. Duplicate report (explicit, non-mutating)
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --duplicates-report
```
Expected:
- duplicate groups are printed as dry-run output.
- no files are moved from `build/smoke_source`.

### 6. Duplicate move (explicit)
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --duplicates-move
```
Expected:
- duplicate copies are moved into `build/smoke_env`.
- move only occurs when `--duplicates-move` is provided.

### 7. Preview
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --preview --group-by date
```
Expected: plan printed once per directory group, no file moves.

### 8. Organize
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --organize
```
Expected: confirmation prompt, files moved after `y`, `manifest.json` created.

### 9. Rollback (ergonomic)
```bash
./build/bin/gallery_organizer build/smoke_env --rollback
```
Expected: files restored according to manifest.

### 10. Similarity report
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report --sim-threshold 0.92 --sim-topk 5
```
Expected: `build/smoke_env/similarity_report.json` exists and includes `groupCount` + `groups`.

### 11. Incremental similarity smoke
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report --sim-incremental off
```
Expected:
- second run reuses embeddings (`ML evaluated: 0`).
- third run forces fresh embedding inference (`ML evaluated` increases).

### 12. Parallel jobs parity smoke
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report --jobs 1
cp build/smoke_env/similarity_report.json build/smoke_env/sim_jobs1_report.json
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report --jobs 4
```
Expected: `similarity_report.json` remains semantically equivalent to the `--jobs 1` output (ignoring `generatedAt` timestamp).

### 13. Similarity viewer smoke
```bash
python3 -m http.server 8000
```
Then open `/tools/similarity_viewer/`, click **Load Default** (after copying report to `build/similarity_report.json`) or upload `build/smoke_env/similarity_report.json`.

### 14. Benchmark smoke
```bash
BENCHMARK_DATASET=tests/assets ./build/tests/bin/benchmark_runner --profile uncompressed --workload cache_metadata_only
```
Expected:
- `build/benchmark_history.jsonl` appended with one JSON object.
- `build/benchmark_last.json` refreshed.
- each record includes `simMemoryMode` (`chunked` or `eager`).

### 15. Dedicated rename smoke
```bash
./build/bin/gallery_organizer build/smoke_source build/smoke_env --rename-init
./build/bin/gallery_organizer build/smoke_source build/smoke_env --rename-bootstrap-tags-from-filename
./build/bin/gallery_organizer build/smoke_source build/smoke_env --rename-preview --rename-pattern "pic-[tags]-[camera].[format]"
./build/bin/gallery_organizer build/smoke_source build/smoke_env --rename-preview --rename-pattern "pic-[location]-[tags]-[camera].[format]"
./build/bin/gallery_organizer build/smoke_source build/smoke_env --rename-preview --rename-pattern "pic-[tags_meta]-[camera].[format]" --rename-meta-tag-add "frag-a,frag-b" --rename-meta-fields
./build/bin/gallery_organizer build/smoke_env --rename-preview-latest-id
```
Expected:
- rename init validates paths and creates rename cache layout directories.
- bootstrap command writes `build/smoke_env/.cache/rename_tags_bootstrap.json`.
- preview id and preview artifact path are printed.
- preview artifact exists under `build/smoke_env/.cache/rename_previews/`.
- GPS/location tokens should resolve when metadata has GPS, otherwise deterministic
  fallback values are used (`unknown-gps-lat`, `unknown-gps-lon`, `unknown-location`).
- metadata-tag edit flags should persist into sidecar (`metaTagAdds`) and affect
  `tagsMeta` values in preview results.

Apply from preview id:
```bash
./build/bin/gallery_organizer build/smoke_env --rename-apply --rename-from-preview <preview_id> --rename-accept-auto-suffix
```
Expected:
- operation id is printed.
- rename history artifacts are written under `build/smoke_env/.cache/rename_history/`.

Apply from latest preview shortcut:
```bash
./build/bin/gallery_organizer build/smoke_env --rename-apply-latest --rename-accept-auto-suffix
```
Expected:
- latest preview id is resolved and printed before apply.
- apply succeeds with the same validation semantics as explicit preview id.

List and rollback:
```bash
./build/bin/gallery_organizer build/smoke_env --rename-history
./build/bin/gallery_organizer build/smoke_env --rename-history-latest-id
./build/bin/gallery_organizer build/smoke_env --rename-history-detail <operation_id>
./build/bin/gallery_organizer build/smoke_env --rename-redo <operation_id>
./build/bin/gallery_organizer build/smoke_env --rename-rollback <operation_id>
```
Expected:
- history output contains operation id rows.
- latest-id/detail/redo helpers complete without validation errors for valid ids.
- rollback reports restored/skipped/failed counts.
- rename tag sidecar path keys are updated on apply and rollback.

Optional reproducibility command:
```bash
BENCHMARK_DATASET=tests/assets ./build/tests/bin/benchmark_runner --profile uncompressed --workload cache_metadata_only --runs 5 --warmup-runs 1 --compare-profile zstd-l3 --comparison-path build/benchmark_compare.json
```
Expected:
- `build/benchmark_last.json` contains median/p95/min/max/stddev stats per workload.
- `build/benchmark_compare.json` contains per-workload `deltaPct` fields for median metrics.

### 16. GUI smoke
```bash
make gui
./build/bin/gallery_organizer_gui
```
Expected:
- gallery/env path inputs are editable and expose picker buttons on supported platforms.
- picker flows normalize outcomes:
  - selected path updates field value
  - user cancel shows informational banner (not error)
  - unavailable backend shows manual-paste informational guidance
  - backend runtime failures surface error banner
- scan, ML enrich, similarity, organize, rollback, duplicate actions are invokable.
- rename preview/apply/history/rollback actions are invokable from Rename tab.
- Rename tab exposes tags-map picker and **Bootstrap Tags** action.
- rename preview table renders source/candidate/manual-tags rows with:
  - collision-only and warnings-only filters
  - selectable rows
  - per-file selected-row manual tag apply action
  - per-file selected-row metadata tag apply action
- Rename tab exposes bulk metadata add/remove inputs for preview scope edits.
- background tasks show progress and can be cancelled.
- active tab and selected mode controls are visually highlighted.
- while a task is running, task-start action buttons are disabled until completion/cancel.

Deterministic GUI rename regression script (manual):
```bash
rm -rf build/gui_rename_reg_src build/gui_rename_reg_env
mkdir -p build/gui_rename_reg_src build/gui_rename_reg_env
cp tests/assets/png/sample_no_exif.png build/gui_rename_reg_src/a.png
cp tests/assets/png/sample_no_exif.png build/gui_rename_reg_src/b.png
make gui
./build/bin/gallery_organizer_gui
```
In GUI:
- set `Gallery Directory` to `build/gui_rename_reg_src`.
- set `Environment Dir` to `build/gui_rename_reg_env`.
- open `Rename` tab.
- run preview with pattern `same.[format]` and verify collision flags are shown.
- run apply without enabling accept-auto-suffix and verify rejection.
- enable accept-auto-suffix and re-run apply; verify success + operation id.
- run history and verify latest operation appears.
- run rollback using operation id and verify success.

Expected:
- preview row table shows two items with collision/warning status.
- apply is blocked unless auto-suffix acceptance is enabled for collision preview.
- history output lists the new operation id.
- rollback restores `a.png` and `b.png` in `build/gui_rename_reg_src`.
- actions with unmet prerequisites are disabled and report an explicit reason when clicked.
- each panel exposes hover tooltip help for inputs/actions.
- Scan panel includes **Download Models** and completes model install workflow.
- jobs/compression level/threshold/topK fields enforce documented numeric ranges.
- **Save State** persists full functional GUI configuration (paths + scan/similarity/organize/rename inputs).
- **Reset Saved State** clears persisted GUI configuration and restores defaults in-session.
- GUI window starts fixed at `1280x820` (non-resizable functional baseline).
- panel controls remain aligned with no overlaps in the fixed shell.
- saved GUI configuration persists between runs when explicitly saved.
- scan-like actions show a rebuild confirmation dialog when profile mismatch would rebuild an existing cache.
- `--reset-state` clears saved GUI state.

### 17. GUI idle performance smoke (large cache)
Precondition: point GUI `Environment Dir` at an env containing a large cache file.

Expected:
- leaving GUI idle for 15-30s does not cause periodic freezes/stutters.
- tabs, hover tooltips, and status text remain responsive while idle.
- malformed cache file should not crash runtime checks; count can be unknown.

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
