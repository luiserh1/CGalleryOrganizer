# CGalleryOrganizer - Scope And Roadmap

## Overview

**Project Name:** CGalleryOrganizer  
**Technology:** C  
**Platforms:** macOS (validated via Objective-C/Swift bridging later), Linux/Windows (optional for CLI)

CGalleryOrganizer is a local, privacy-first application designed to analyze, classify, and organize a directory of images and videos. The project is a C-based port of the "Unified Gallery Organizer" concept, aiming for extreme performance, minimal dependencies, and a granular integration of Machine Learning heuristics.

---

## Core Design Principles

- **Zero-to-Minimal Vendor Dependencies**: Third-party C header packages (JSON parsers, ML frameworks) are heavily isolated in `vendor/`. Heavy system-level libraries (like `exiv2` for robust format parsing) are integrated natively via the host OS package manager and securely abstracted behind pure C wrappers. All dependencies are documented in `docs/dependencies.md`.
- **Granular Evolution**: Features are strictly bounded by minor version bumps (0.1.0 -> 0.2.0, etc.).
- **Strict Data Contract**: The application revolves around a JSON-backed cache (`.cache/gallery_cache.json`) to avoid re-computing expensive heuristics.
- **TDD Driven**: All logic (FS parsing, caching, hashing, math) is tested via a custom, lightweight framework without relying on external testing libraries.

---

## v0.1.0 Scope: Core Foundation & Basic Metadata (Completed)

### Primary Goal
Establish the project structure, testing framework, code style, and implement basic directory traversal and metadata extraction.

### Core Architecture
- Directories for `src/`, `include/`, `tests/`, `vendor/`, and `docs/`.
- Custom, lightweight testing framework.

### Features
- Recursive directory traversal for supported extensions (images/videos).
- Basic metadata extraction:
  - Absolute Path
  - File Size (bytes)
  - Modification Date (Unix Timestamp)
- JSON Cache integration (`.cache/gallery_cache.json`) using a lightweight vendor library (e.g., `cJSON`).
  - Cache invalidation logic based on file size and timestamp.

---

## v0.2.0 Scope: Exact Duplicates & Actions (Completed)

### Primary Goal
Implement file hashing to detect exact duplicates and introduce action routines based on caching results.

### Features
- Exact duplicate detection via fast file hashing (e.g., MD5 or SHA-256).
- Abstracted file action operations:
  - Renaming (e.g., to timestamp pattern).
  - Moving/Reordering into subdirectories based on metadata.
- Grouping logic for exact hashes.

---

## v0.2.1 Scope: Dynamic Metadata & Dashboard (Completed)

### Primary Goal
Expand metadata extraction with exhaustive tags and evolve the visualization cache dashboard.

### Features
- Hybrid C metadata struct supporting dynamic JSON extraction for EXIF, IPTC, and XMP.
- Exiv2 C++ wrapper iterative parsing controlled via a `--exhaustive` flag.
- Evolved cache viewer dashboard with row expansion and `tag:value` deep search capabilities.
- Persistent logging of benchmark results (`build/benchmark_history.log`).

---

## v0.2.2 Scope: Active Gallery Organization (Completed)

### Primary Goal
Transition from a passive analyzer to an active organizer. Introduce interactive CLI execution, robust environment management (rollback manifest), and metadata-based folder staging.

### Features
- Interactive CLI with `--organize`, `--preview`, and `--rollback` arguments.
- Environment Directory paradigm (hosts caches and manifest).
- Rollback manifest (`history.json`) for safe reversal of filesystem changes.
- Virtual tree generation and metadata-based configurable renaming mapping (e.g., Year/Month).

---

## v0.2.3 Scope: Compound Grouping & File Renaming (Completed)

### Primary Goal
Advance the organization engine horizontally to support complex, multi-level folder hierarchies using sequential metadata keys while guaranteeing filesystem uniqueness via incremental collision-proof renaming.

### Features
- New CLI argument `--group-by <keys>` (e.g., `camera,date`).
- Compound path building in the `organizer` (e.g., `_Sony/_2013/_03`).
- Dynamic renaming engine resolving to `YYYYMMDD_HHMMSS_CameraModel.ext`.
- Fallback collision safeguards using incremental `_1`, `_2` indexes.

---

## v0.2.4 Scope: Stability Remediation (Completed)

### Primary Goal
Stabilize the CLI and organizer runtime behavior, remove noisy debug output, and align documentation and tests with implemented behavior.

### Features
- Fixed `--exhaustive` parsing and validation paths.
- Added backward-compatible ergonomic rollback invocation (`<env_dir> --rollback`).
- Hardened `--group-by` validation for empty/invalid keys.
- Fixed organizer preview duplicate-line bug and improved defensive memory/path handling.
- Removed unconditional debug logs from cache/duplicate flows.
- Replaced fragile cache object-size assumptions with deterministic key counting.
- Corrected cache viewer column sorting mappings.
- Expanded tests for CLI flows, rollback compatibility, and cache key counting.

---

## v0.3.0 Scope: Local ML Foundation (Completed)

### Primary Goal
Introduce a modular local ML foundation with a provider-agnostic API so the core application can consume inference results without coupling to a specific runtime implementation.

### Features
- Added a stable ML API boundary (`include/ml_api.h`) and provider abstraction (`include/ml_provider.h`).
- Implemented first local provider module (`src/ml/providers/onnx_provider.c`) behind the abstraction layer.
- Introduced two minimal capabilities in the 0.3.0 API surface:
  - Image classification
  - Text detection
- Added CLI smoke workflow: `--ml-enrich` to enrich cache entries with ML outputs.
- Extended cache schema with typed ML fields and raw provider payload (`mlRaw`).
- Added manifest-driven model delivery (`models/manifest.json`) and downloader (`scripts/download_models.sh`) with required metadata fields and SHA256 verification.
- Added model attribution registry (`docs/model_assets.md`) and documentation guardrails.

---

## v0.4.0 Scope: Similarity Engine + Dedicated Similarity Tool (Completed)

### Primary Goal
Deliver a minimal but complete similarity workflow on top of the local ML foundation: embeddings, report generation, and dedicated report exploration UI.

### Features
- Extend ML API/provider with `ML_FEATURE_EMBEDDING`.
- Persist canonical embedding fields in cache (`mlModelEmbedding`, `mlEmbeddingDim`, `mlEmbeddingBase64`).
- Add CLI workflow:
  - `--similarity-report`
  - `--sim-threshold <0..1>` (default `0.92`)
  - `--sim-topk <n>` (default `5`)
- Generate stable report contract at `<env_dir>/similarity_report.json`:
  - `generatedAt`, `modelId`, `threshold`, `topK`, `groupCount`, `groups[]`
- Add dedicated viewer tool at `tools/similarity_viewer/`:
  - group list + pair table UX
  - default load from `build/similarity_report.json`
  - upload/filter/search/sort/export
- Add embedding model entry in `models/manifest.json` and attribution in `docs/model_assets.md`.

---

## v0.4.1 Scope: Performance Baseline + Optional Cache Compression (Completed)

### Primary Goal
Measure and compare performance characteristics across key workloads, then introduce optional cache compression with quantified tradeoffs.

### Features
- Dedicated benchmark runner with three workloads:
  - `cache_metadata_only`
  - `cache_full_with_embeddings`
  - `similarity_search`
- Structured JSONL benchmark history:
  - `build/benchmark_history.jsonl`
  - `build/benchmark_last.json`
- Metrics captured per workload:
  - time
  - disk cost
  - main-memory cost (RSS start/end/delta + peak)
- Optional cache compression:
  - `--cache-compress none|zstd`
  - `--cache-compress-level 1..19`
- Compare modes via:
  - `make benchmark`
  - `make benchmark-compare`

---

## v0.4.2 Scope: Incremental Similarity + Compression Auto Mode (Completed)

### Primary Goal
Speed up repeated similarity workflows by reusing valid embeddings, and simplify compression behavior with an automatic mode.

### Features
- Incremental similarity generation:
  - reuse embeddings for unchanged files
  - infer only missing/stale embeddings
  - regenerate report with reused + new embeddings
- Compression ergonomics:
  - `--cache-compress auto` with size-threshold policy
  - keep explicit `none` and `zstd`
- CLI additions:
  - `--sim-incremental <on|off>` (default `on`)
  - `--cache-compress <none|zstd|auto>`
- Default policy constants:
  - `CACHE_AUTO_THRESHOLD_BYTES = 8 MiB`
  - `SIM_INCREMENTAL_DEFAULT = on`

---

## v0.4.3 Scope: Similarity Memory Optimization (Planned)

### Primary Goal
Reduce peak RSS during similarity computation by avoiding eager full-decoding of embeddings.

### Features
- Similarity engine refactor to bounded-memory processing (`chunked` mode).
- Optional expert flag:
  - `--sim-memory-mode <eager|chunked>` (default `chunked`)
- Preserve report schema and ranking behavior.

---

## v0.4.4 Scope: Parallel Scan/Inference Pipeline (Planned)

### Primary Goal
Improve throughput via parallel scan->metadata->inference processing while preserving deterministic output.

### Features
- Worker-pool pipeline with bounded queue.
- Aggregated serialized writes for cache/report safety.
- CLI additions:
  - `--jobs <n>` (default `auto`)
  - `CGO_JOBS` override
- Deterministic output parity between `--jobs 1` and `--jobs N`.

---

## Benchmark Methodology Branch (Non-Versioned)

### Branch
- `codex/benchmark-methodology`

### Scope
- Benchmark-only improvements (no runtime behavior changes):
  - repeated runs (`--runs N`)
  - summary stats (median/p95/min/max/stddev)
  - optional warmup runs
  - structured comparison report export

### Merge Policy
- May merge to `main` without creating a release tag.
- If behavior changes leak into runtime paths, defer merge to the next functional version branch.

---

## Documentation Governance

For every merged milestone branch:
1. Update this file (`docs/scope.md`) status transitions (`Planned` -> `Completed`) with exact version.
2. Keep `README.md` CLI/options/examples synchronized with parser behavior.
3. Keep `docs/tests.md` smoke and benchmark flows synchronized.
4. If benchmark schema changes, update schema docs in the same commit.
5. Add version release-notes section in docs for:
   - behavior changes
   - migration/compat notes
   - benchmark impact summary

---

## Version Mapping (Corrected)

- `v0.4.2`: incremental similarity + compression auto mode
- `v0.4.3`: similarity memory optimization
- `v0.4.4`: parallel pipeline
- benchmark-methodology branch: non-versioned merge path

---

## v0.5.0+ Scope: Interactive UI

### Primary Goal
Provide a user-friendly way to visualize results, review discards, and manage the gallery.

### Features
- Integrate a C-compatible GUI (Raylib, SDL) or macOS native UI via Objective-C.
- Interactive grid/list for review.
