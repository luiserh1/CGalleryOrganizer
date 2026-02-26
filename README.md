# CGalleryOrganizer

CGalleryOrganizer is a local-first C/C++ gallery organizer with dual frontends:
CLI (`gallery_organizer`) and a lightweight multiplatform GUI
(`gallery_organizer_gui`). Both frontends use the same backend app API.

## Key Features (v0.5.2)
- Recursive media scan with cache invalidation by file size and modification timestamp.
- Metadata extraction through Exiv2 (dimensions, date taken, camera, GPS, orientation).
- Optional exhaustive metadata capture with `--exhaustive`.
- Exact duplicate detection using MD5 with SHA-256 verification.
- Interactive organization plan/execute flow with `--preview`, `--organize`, and `--group-by`.
- Local ML enrichment with provider-agnostic API (`--ml-enrich`) for:
  - image classification
  - text detection
- Similarity workflow (`--similarity-report`) using embedding vectors and cosine similarity grouping.
- Manifest-driven model download and checksum validation (`make models`).
- Benchmark workloads with JSONL history output (`make benchmark`, `make benchmark-compare`).
- Optional whole-file cache compression (`--cache-compress none|zstd|auto`).
- Incremental similarity reuse toggle (`--sim-incremental on|off`, default `on`).
- Unified frontend backend contract (`include/app_api.h`, `include/app_api_types.h`).
- Canonical frontend API contract documentation (`docs/app_api.md`).
- GUI frontend with background tasks, progress/cancel, and persisted last-used
  gallery/environment paths.
- Functional fixed GUI baseline (`1280x820`, non-resizable, deterministic layout).

## Build

Engineering standards (style, modularity, low technical debt): see
`docs/CODE_STYLE.md`.

### Prerequisites
- Clang or GCC
- `make`
- Exiv2 development package (`brew install exiv2` on macOS)
- ONNX Runtime development package for ML provider (`brew install onnxruntime` on macOS)
- Raylib (GUI build only): `brew install raylib` on macOS

### Compile
```bash
make
```

### Run tests
```bash
make test
```

### Build GUI frontend
```bash
make gui
```

### Build Native App API Shared Library
```bash
make app-api-lib
```

### Run GUI frontend
```bash
make run-gui
```

Reset GUI saved paths explicitly:
```bash
./build/bin/gallery_organizer_gui --reset-state
```

## Model Setup (0.4.x)

Download model artifacts locally:
```bash
make models
```

Default install location: `build/models/`.
Override at runtime with `CGO_MODELS_ROOT=/custom/path`.

## CLI Usage

```bash
./build/bin/gallery_organizer <directory_to_scan> [env_dir] [options]
```

### Options
- `-h`, `--help`: show usage.
- `-e`, `--exhaustive`: include all EXIF/IPTC/XMP tags in cache (`allMetadataJson`).
- `--ml-enrich`: run local ML enrichment and store ML outputs in cache.
- `--similarity-report`: generate `<env_dir>/similarity_report.json` using embeddings.
- `--sim-incremental <on|off>`: reuse valid embeddings on similarity runs (default `on`).
- `--sim-memory-mode <eager|chunked>`: embedding decode strategy for similarity (default `chunked`).
- `--jobs <n|auto>`: scan/inference worker count (default `auto`, env override `CGO_JOBS`).
- `--sim-threshold <0..1>`: minimum cosine similarity (default `0.92`).
- `--sim-topk <n>`: max neighbors per anchor group (default `5`).
- `--cache-compress <mode>`: cache encoding mode (`none`, `zstd`, or `auto`).
- `--cache-compress-level <1..19>`: zstd level (default `3`).
- `--preview`: build and print organization plan without moving files.
- `--organize`: execute organization plan after confirmation.
- `--group-by <keys>`: grouping keys list. Allowed keys: `date,camera,format,orientation,resolution`.
- `--rollback`: restore moved files from `manifest.json`.

### Rollback invocation (both supported)
```bash
./build/bin/gallery_organizer <scan_dir> <env_dir> --rollback
./build/bin/gallery_organizer <env_dir> --rollback
```

## GUI Usage

The GUI exposes the same backend capabilities as the CLI:
- scan/cache (jobs, exhaustive, compression options)
- ML enrich
- similarity report
- organize preview/execute
- rollback
- duplicate analysis + move

Additional 0.5.2 behavior:
- fixed window shell (`1280x820`) focused on functional operation parity
- deterministic non-responsive panel geometry
- selected tabs/mode toggles are highlighted for clarity
- task action buttons are disabled while a background task is running

Initial GUI path inputs are manual text fields (no native file picker in 0.5.x).

## Examples

### Passive scan + cache
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env
```

### Exhaustive scan
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --exhaustive
```

### ML enrichment
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --ml-enrich
```

### Similarity report
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --similarity-report --sim-threshold 0.92 --sim-topk 5
```

### Compressed cache
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --cache-compress zstd --cache-compress-level 3
```

### Auto compression + incremental similarity
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --similarity-report --sim-incremental on --cache-compress auto
```

## Benchmarking

```bash
make benchmark
make benchmark-compare
make benchmark-stats
make benchmark-sim-memory-compare
```

Outputs:
- `build/benchmark_history.jsonl`
- `build/benchmark_last.json`
- `build/benchmark_compare.json` (when compare mode is used)

`benchmark_history.jsonl` records:
- `timestampUtc`, `gitCommit`, `profile`, `workload`
- `simMemoryMode` (`chunked` by default, `eager` for parity/diagnostics)
- `runIndex`, `runCount`, `warmupRuns`
- `datasetPath`, `datasetFileCount`
- `cachePath`, `cacheBytes`
- `timeMs`
- `rssStartBytes`, `rssEndBytes`, `rssDeltaBytes`, `peakRssBytes`
- `success`, `error`
- optional `similarityReportBytes` for `similarity_search`

`benchmark_runner` methodology flags:
- `--runs <N>`: number of measured runs per workload (default `1`).
- `--warmup-runs <N>`: number of pre-measurement warmup runs (default `0`).
- `--compare-profile <profile>`: execute baseline and candidate profiles in one run.
- `--comparison-path <path>`: output JSON comparison report path.

Suggested comparison rubric (zstd vs uncompressed):
- `win`: disk reduction >= 25% and latency increase <= 15%
- `mixed`: all other balanced outcomes
- `loss`: latency increase > 30% and disk gain < 15%

## Roadmap

- `v0.5.0`: unified app API + CLI/GUI dual frontends.
- `v0.5.1`: responsive GUI layout + scalable typography + persisted zoom/window state.
- `v0.5.2`: fixed functional GUI baseline + language-agnostic frontend contract docs.
- future: OS-specific frontends (e.g. SwiftUI) and additional frontend variants.

### Preview with compound grouping
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --preview --group-by camera,date,resolution
```

### Organize
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --organize --group-by date
```

## Project Layout
- `src/`: core implementation.
- `src/app/`: unified backend service API implementation for frontends.
- `src/gui/core/`: shared GUI runtime (event loop, worker integration, shell).
- `src/gui/frontends/functional/`: fixed functional GUI panel/layout layer.
- `include/`: public headers.
- `docs/app_api.md`: canonical frontend/backend API contract documentation.
- `docs/frontends.md`: frontend architecture and ownership boundaries.
- `models/`: tracked ML model manifest metadata (not model binaries).
- `tests/`: test framework and test suites.
- `docs/test_assets.md`: canonical attribution/license registry for test fixtures.
- `docs/model_assets.md`: canonical attribution/license registry for model assets.
- `vendor/`: bundled third-party C dependencies.
- `tools/cache_viewer/`: static dashboard for cache inspection.
- `tools/similarity_viewer/`: static dashboard for similarity report analysis.
- `docs/`: project and maintenance documentation.
