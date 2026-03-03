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

## v0.4.3 Scope: Similarity Memory Optimization (Completed)

### Primary Goal
Reduce peak RSS during similarity computation by avoiding eager full-decoding of embeddings.

### Features
- Similarity engine refactor to bounded-memory processing (`chunked` mode).
- Optional expert flag:
  - `--sim-memory-mode <eager|chunked>` (default `chunked`)
- Preserve report schema and ranking behavior.

---

## v0.4.4 Scope: Parallel Scan/Inference Pipeline (Completed)

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

## v0.4.5 Scope: Modularity + Technical Debt Cleanup (Completed)

### Primary Goal
Reduce maintenance risk by decomposing oversized files, formalizing engineering
guardrails, and preserving runtime behavior contracts.

### Features
- Decompose CLI monolith (`src/main.c`) into dedicated `src/cli/*` modules.
- Split oversized benchmark and core/system implementation files while keeping
  public contracts stable.
- Formalize low technical debt and modularity rules in `docs/CODE_STYLE.md`.
- Improve test maintainability by splitting oversized integration test suites.
- Keep CLI/cache/report behavior stable (cleanup-only milestone).

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

## v0.5.0 Scope: Unified Frontend API + CLI/GUI Dual Frontends (Completed)

### Primary Goal
Introduce a pure frontend-facing backend API used by both CLI and a new
cross-platform GUI frontend, with shared orchestration and no duplicated
backend workflow logic.

### Features
- Added public app service contract:
  - `include/app_api.h`
  - `include/app_api_types.h`
- Added app service implementation layer (`src/app/*`) for:
  - runtime context init/shutdown
  - scan/cache workflows
  - ML/similarity report generation
  - organize preview/execute
  - rollback
  - duplicate discovery/move
- Migrated CLI orchestration to call app API operations while preserving
  existing flags and behavior.
- Added initial multiplatform GUI frontend (`src/gui/*`) using raylib + raygui:
  - manual gallery/env path inputs
  - scan/cache controls
  - ML enrich and similarity report actions
  - organize preview/execute and rollback actions
  - duplicate analysis/move actions
  - background worker execution with progress + cancel
- Added GUI persisted state for last-used path pair:
  - `galleryDir`
  - `envDir`
  - stored in user config directory (`gui_state.json`)
  - explicit reset behavior (not tied to `make clean` targets)

### Release Notes
- Behavior changes:
  - introduced GUI frontend binary (`gallery_organizer_gui`)
  - CLI orchestration now routes through app API service layer
- Migration/compat notes:
  - CLI flags/flows remain compatible with prior versions
  - GUI state is stored outside repository build artifacts
- Benchmark impact summary:
  - no benchmark methodology/schema changes in this milestone

---

## v0.5.1 Scope: Responsive GUI Layout + Scalable Typography (Completed)

### Primary Goal
Make the GUI robust under resizing by replacing fixed geometry with responsive
layout primitives and scalable typography while preserving existing feature
behavior.

### Features
- Added responsive GUI layout engine (`src/gui/gui_layout.[ch]`) with:
  - row/stack placement
  - wrapping for narrow widths
  - shared runtime layout metrics derived from effective scale
- Added deterministic UI scaling model:
  - `effective_scale = auto_scale(window_size) * user_zoom_scale`
  - auto scale based on baseline `1000x760`
  - user zoom persisted as `uiScalePercent` (`80..160`)
- Enabled resizable window with enforced minimum size based on worst-case panel
  requirements.
- Migrated GUI panels and shell to dynamic sizing/placement:
  - scan, similarity, organize, duplicates
  - header tabs and status/log area
- Added GUI zoom controls in header:
  - `A-`, `A+`, `Reset`, and current `%` display
- Extended GUI state persistence (`gui_state.json`) with:
  - `uiScalePercent`
  - `windowWidth`
  - `windowHeight`
  - optional `updatedAt`
- Added automated tests for layout math and state migration/defaults:
  - `tests/test_gui_layout.c`
  - extended `tests/test_gui_state.c`

### Release Notes
- Behavior changes:
  - GUI now resizes safely without control overlap.
  - GUI persists zoom level and last window dimensions in addition to paths.
- Migration/compat notes:
  - Existing `gui_state.json` files without new fields load with defaults.
  - `--reset-state` clears the full GUI state file.
- Benchmark impact summary:
  - no benchmark methodology/schema changes in this milestone.

---

## v0.5.2 Scope: Functional GUI Simplification + Language-Agnostic Frontend Foundation (Completed)

### Primary Goal
Simplify GUI presentation to a deterministic functional baseline while
strengthening app API contract clarity for future multi-language frontend
integrations (native and web adapter paths).

### Features
- Added canonical frontend API documentation:
  - `docs/app_api.md`
  - function purpose, request/response semantics, lifecycle/call order,
    dependency matrix, and ownership rules.
- Clarified multi-language frontend strategy:
  - native in-process frontends (CLI/current GUI/future Swift bridge)
  - out-of-process adapter path for future web frontends.
- Added concise API contract comments in:
  - `include/app_api.h`
  - `include/app_api_types.h`
- Simplified GUI shell/runtime to fixed functional baseline:
  - fixed window `1280x820`
  - non-resizable runtime
  - removed zoom/responsive runtime behavior
- Reorganized GUI implementation for clearer boundaries:
  - runtime core: `src/gui/core/*`
  - functional presentation: `src/gui/frontends/functional/*`
- Replaced responsive layout module with deterministic fixed layout helpers:
  - `src/gui/frontends/functional/gui_fixed_layout.[ch]`
- Simplified GUI state schema to path-focused persistence:
  - persisted: `galleryDir`, `envDir`, optional `updatedAt`
  - legacy size/zoom fields tolerated on load and omitted on write.
- Added optional native-consumption build artifact:
  - `make app-api-lib`
  - outputs shared app API library for native frontend bindings.

### Release Notes
- Behavior changes:
  - GUI now uses fixed functional geometry rather than responsive/zoom behavior.
  - GUI state persists only last-used gallery/env paths (+ optional timestamp).
- Migration/compat notes:
  - old GUI state files with `uiScalePercent/windowWidth/windowHeight` still load
    paths and are rewritten to simplified schema on next save.
  - CLI behavior and flags remain unchanged.
- Benchmark impact summary:
  - no benchmark methodology/schema changes in this milestone.

---

## v0.5.3 Scope: Guided Functional GUI + In-App Model Management (Completed)

### Primary Goal
Keep the fixed functional GUI baseline while improving guided usability:
clear action prerequisites, built-in model installation flow, contextual help,
and tighter numeric validation feedback.

### Features
- Added runtime inspection API for frontend dependency gating:
  - `AppInspectRuntimeState()`
  - reports cache/manifest presence, model availability, logical cores, and
    recommended jobs.
- Added native model installation API:
  - `AppInstallModels()`
  - manifest field validation
  - SHA-256 verification
  - lockfile write to `<models_root>/.installed.json`.
- Improved ML readiness error handling:
  - missing-model errors now surfaced through app status/error channels for
    frontend rendering.
  - removed provider console-only noise for missing model paths.
- Added GUI action dependency locking:
  - invalid actions are disabled until prerequisites are met
  - blocked actions provide explicit reason text.
- Added GUI contextual help system:
  - hover tooltips for fields/actions.
- Added GUI **Download Models** task:
  - wired to app API installer
  - reports installed/skipped counts and lockfile path.
- Polished numeric controls:
  - strict parse + clamped range enforcement
  - inline/banner validation feedback for constrained fields.

### Release Notes
- Behavior changes:
  - guided action gating and contextual help are now part of the functional GUI
    baseline.
  - models can be installed directly from the GUI via app API.
- Migration/compat notes:
  - CLI behavior remains unchanged.
  - GUI window/layout/state model from 0.5.2 remains intact.
- Benchmark impact summary:
  - no benchmark methodology/schema changes in this milestone.

---

## v0.5.4 Scope: Persistent Cache Profile + Parameter-Aware Recompute (Completed)

### Primary Goal
Make cache reuse deterministic across sessions by persisting scan semantics and
automatically rebuilding cache only when tracked parameters change.

### Features
- Added persistent cache profile sidecar:
  - `<env_dir>/.cache/gallery_cache.profile.json`
  - fallback path when env dir omitted: `.cache/gallery_cache.profile.json`
- Added strict profile comparison before scan:
  - tracked fields:
    - absolute source root
    - exhaustive mode
    - ML enrichment requested
    - similarity-prep requested
    - model fingerprint (SHA-256 over required model files)
  - non-tracked fields:
    - jobs
    - cache compression mode/level
    - similarity incremental flag
- Added automatic rebuild behavior on profile mismatch:
  - release active cache state
  - remove cache artifact/profile sidecar
  - reinitialize empty cache and continue scan
- Added model fingerprint support for semantic ML/similarity profile checks:
  - ML enrich path: classification + text model files
  - similarity prep path: embedding model file
- Extended scan result contract:
  - `cache_profile_matched`
  - `cache_profile_rebuilt`
  - `cache_profile_reason`
- Added non-mutating scan profile preflight API:
  - `AppInspectScanProfile(...)`
  - exposes `AppScanProfileDecision` for frontend rebuild confirmation UX
- Added lightweight runtime cache-count contract for frontend gating:
  - `cache_entry_count_known` indicates whether `cache_entry_count` is reliable
  - runtime inspection uses active in-memory cache count or sidecar last-known
    count instead of decoding/parsing large cache files during idle GUI refresh
- Extended profile sidecar with optional informational field:
  - `cacheEntryCount` (last-known count; excluded from strict profile match)
- Updated GUI idle refresh policy for safer cadence:
  - event-driven refresh plus slower idle poll
  - post-refresh timestamp update to avoid bursty back-to-back refresh cycles
- Updated GUI cache-dependent action gating:
  - unknown entry count no longer blocks cache-dependent actions when cache exists
- Expanded GUI saved state contract (explicit-save only):
  - persisted fields now include full functional configuration
    (paths + scan/similarity/organize inputs)
  - save button persists full state; exit prompts on unsaved configuration changes
- Added pre-scan rebuild confirmation guard in GUI:
  - scan-like tasks run profile preflight
  - when cache exists and mismatch implies rebuild, user must confirm continue/cancel
- Added focused tests for profile roundtrip/match/mismatch rules and
  non-semantic parameter tolerance.

### Release Notes
- Behavior changes:
  - scan now performs profile-aware cache reuse/rebuild automatically.
  - missing or malformed profile sidecar is treated as mismatch and rebuilt safely.
  - GUI/runtime state inspection remains responsive even with very large cache files.
  - GUI now saves/restores full functional configuration when explicitly saved.
  - GUI scan-like actions prompt for confirmation before rebuilding an existing cache.
- Migration/compat notes:
  - no CLI flag changes required.
  - cache profile write is attempted only after successful cache save.
  - `cacheEntryCount` in sidecar is optional and load-tolerant.
- Benchmark impact summary:
  - no benchmark methodology/schema changes in this milestone.

---

## v0.6.0 Scope: Dedicated Pattern-Based Renaming + Tagging (Completed)

### Primary Goal
Introduce a dedicated in-place renaming workflow (separate from organize) with
pattern-based naming, hybrid metadata/manual tagging, collision-safe execution,
and multi-step rollback by operation id.

### Features
- Add dedicated rename app API operations:
  - preview rename plan from pattern + tag inputs
  - apply rename plan through preview handshake
  - list rename history
  - rollback rename operation by id
- Add renamer subsystem modules for:
  - pattern parsing and token rendering
  - metadata/manual tag resolution and persistence
  - preview artifact generation
  - apply/rollback execution and history persistence
- Support curated token set for v1:
  - `[date]`, `[time]`, `[datetime]`, `[camera]`, `[make]`, `[model]`,
    `[format]`, `[index]`, `[tags_manual]`, `[tags_meta]`, `[tags]`
- Add per-environment manual-tag sidecar:
  - `<env_dir>/.cache/rename_tags.json`
- Add preview artifacts:
  - `<env_dir>/.cache/rename_previews/<preview_id>.json`
- Add rename history store:
  - `<env_dir>/.cache/rename_history/index.json`
  - `<env_dir>/.cache/rename_history/<operation_id>.json`
- Add CLI rename flow:
  - preview/apply/history/rollback commands
  - explicit preview-id handshake and collision acceptance gate for apply
- Add GUI rename panel:
  - pattern inputs
  - preview/apply controls
  - tag editing actions (map + bulk add/remove)
  - history + rollback action wiring
- Keep existing organize workflow and semantics unchanged.

### Release Notes
- Behavior changes:
  - Added a dedicated in-place rename workflow, independent from organize
    semantics, with pattern-based preview/apply/history/rollback.
  - Added tokenized naming engine with strict allowlist, escape support,
    deterministic fallback values, sanitization, and truncate+hash policy.
  - Added collision-gated apply handshake requiring explicit acceptance for
    deterministic auto-suffix resolution.
  - Added CLI and GUI rename surfaces with preview id handshake and operation-id
    rollback.
- Migration/compat notes:
  - Existing organize/rollback/duplicate workflows remain unchanged.
  - New rename data is stored under:
    - `<env_dir>/.cache/rename_tags.json`
    - `<env_dir>/.cache/rename_previews/*.json`
    - `<env_dir>/.cache/rename_history/index.json` and operation manifests
  - Rename history retention is capped to the latest 200 operations.
- Benchmark impact summary:
  - No benchmark schema or benchmark command changes in this milestone.

---

## v0.6.1 Scope: CLI Rename Onboarding + Usability (Completed)

### Primary Goal
Reduce first-run friction in the dedicated rename CLI workflow while keeping
existing rename safety semantics (preview/apply handshake, collision controls,
and rollback history) unchanged.

### Features
- Add explicit rename environment preflight command:
  - `--rename-init` validates target/env paths, resolves canonical absolute
    paths, creates required cache subdirectories, and prints readiness summary.
- Add built-in tag bootstrap from filenames:
  - `--rename-bootstrap-tags-from-filename` writes a valid rename tags map JSON
    from numeric tokens detected in current filenames.
- Improve path error diagnostics for rename preview:
  - unresolved directory errors include nearby sibling suggestions when
    available (e.g. typo/case mismatch hints).
- Improve preview output ergonomics:
  - default preview output remains concise summary-first.
  - add explicit full JSON output controls:
    - `--rename-preview-json`
    - `--rename-preview-json-out <path>`
- Add apply shortcut for latest preview:
  - `--rename-apply-latest` resolves latest preview id in env and reuses
    existing apply/fingerprint validation rules.
- Add integration coverage for new CLI contracts and guardrails.
- Sync documentation for CLI usage, examples, and smoke tests.

### Release Notes
- Behavior changes:
  - Added `--rename-init` to validate target/env paths, create rename cache
    layout, and print readiness summary.
  - Added `--rename-bootstrap-tags-from-filename` to generate a JSON manual tag
    map from numeric filename tokens.
  - Added rename target typo/case suggestion hints when preview target path is
    unresolved.
  - Preview output is now summary-first by default; full preview JSON is
    optional via:
    - `--rename-preview-json`
    - `--rename-preview-json-out <path>`
  - Added `--rename-apply-latest` as a shortcut for applying the newest preview
    artifact in env while preserving existing apply validation semantics.
- Migration/compat notes:
  - Existing rename flags remain valid and backward compatible.
  - `--rename-apply` still requires `--rename-from-preview`; the new
    `--rename-apply-latest` path is additive.
  - No cache schema changes; new bootstrap output defaults to:
    - `<env_dir>/.cache/rename_tags_bootstrap.json`
- Benchmark impact summary:
  - No benchmark schema or benchmark methodology changes in this milestone.

---

## v0.6.2 Scope: GUI Rename Workflow Usability (Planned)

### Primary Goal
Improve the dedicated Rename tab usability for first-run and iterative batch
rename workflows without changing backend rename semantics.

### Features
- Add GUI path pickers for gallery, environment, and tags-map fields.
- Add GUI action to bootstrap manual tags from filename numeric tokens.
- Add preview table UI (source -> candidate) with collision/warning filters.
- Add per-file manual tag editor plus bulk add/remove controls in-panel.
- Improve guided flow affordances:
  - clearer preview/apply/rollback progression
  - better preview/apply/history result summaries for large batches
- Keep CLI/API contracts from v0.6.1 and v0.6.0 unchanged.

### Release Notes
- Behavior changes:
  - TBD at completion: finalized GUI rename panel improvements.
- Migration/compat notes:
  - TBD at completion: GUI state compatibility and defaults.
- Benchmark impact summary:
  - TBD at completion: expected no benchmark schema/methodology changes.
