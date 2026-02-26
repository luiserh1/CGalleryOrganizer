# CGalleryOrganizer

CGalleryOrganizer is a local-first C/C++ CLI for scanning media folders, extracting metadata, caching results, finding exact duplicates, organizing files, enriching metadata with local ML inference, and generating similarity reports.

## Key Features (v0.4.2)
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

## Build

### Prerequisites
- Clang or GCC
- `make`
- Exiv2 development package (`brew install exiv2` on macOS)
- ONNX Runtime development package for ML provider (`brew install onnxruntime` on macOS)

### Compile
```bash
make
```

### Run tests
```bash
make test
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
```

Outputs:
- `build/benchmark_history.jsonl`
- `build/benchmark_last.json`

`benchmark_history.jsonl` records:
- `timestampUtc`, `gitCommit`, `profile`, `workload`
- `datasetPath`, `datasetFileCount`
- `cachePath`, `cacheBytes`
- `timeMs`
- `rssStartBytes`, `rssEndBytes`, `rssDeltaBytes`, `peakRssBytes`
- `success`, `error`
- optional `similarityReportBytes` for `similarity_search`

Suggested comparison rubric (zstd vs uncompressed):
- `win`: disk reduction >= 25% and latency increase <= 15%
- `mixed`: all other balanced outcomes
- `loss`: latency increase > 30% and disk gain < 15%

## 0.4.x Roadmap

- `v0.4.2`: incremental similarity + cache compression `auto` mode.
- `v0.4.3`: similarity memory optimization (`chunked` default).
- `v0.4.4`: parallel scan/inference pipeline (`--jobs`, `CGO_JOBS`).
- `codex/benchmark-methodology`: benchmark-only improvements merged without version bump.

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
- `include/`: public headers.
- `models/`: tracked ML model manifest metadata (not model binaries).
- `tests/`: test framework and test suites.
- `docs/test_assets.md`: canonical attribution/license registry for test fixtures.
- `docs/model_assets.md`: canonical attribution/license registry for model assets.
- `vendor/`: bundled third-party C dependencies.
- `tools/cache_viewer/`: static dashboard for cache inspection.
- `tools/similarity_viewer/`: static dashboard for similarity report analysis.
- `docs/`: project and maintenance documentation.
