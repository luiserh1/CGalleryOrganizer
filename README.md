# CGalleryOrganizer

CGalleryOrganizer is a local-first C/C++ gallery organizer with dual frontends:
CLI (`gallery_organizer`) and a lightweight multiplatform GUI
(`gallery_organizer_gui`). Both frontends use the same backend app API.

## Key Features (v0.6.11)
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
- Persistent cache computation profile sidecar (`.cache/gallery_cache.profile.json`).
- Automatic cache rebuild on profile mismatch (strict equality on semantic parameters).
- Unified frontend backend contract (`include/app_api.h`, `include/app_api_types.h`).
- Canonical frontend API contract documentation (`docs/app_api.md`).
- GUI frontend with background tasks, progress/cancel, and explicit persisted
  functional configuration state (paths + scan/similarity/organize inputs).
- Functional fixed GUI baseline (`1280x820`, non-resizable, deterministic layout).
- Runtime inspection API for frontend action gating (`AppInspectRuntimeState`).
- Lightweight runtime cache-count contract:
  - `cache_entry_count_known` indicates whether `cache_entry_count` is reliable
  - runtime inspection avoids full cache decode/parse on idle GUI checks
- Native model install API with manifest validation and checksum verification (`AppInstallModels`).
- Guided GUI behavior:
  - action dependency locking with explicit reason messages
  - hover tooltips for fields/actions
  - in-app **Download Models** workflow
  - strict numeric range validation with inline clamp feedback
- Dedicated pattern-based rename workflow (CLI + GUI):
  - preview/apply/history/rollback API-backed flow
  - curated token set:
    - `[date]`, `[time]`, `[datetime]`, `[camera]`, `[make]`, `[model]`,
      `[format]`, `[gps_lat]`, `[gps_lon]`, `[location]`, `[index]`,
      `[tags_manual]`, `[tags_meta]`, `[tags]`
  - manual + metadata tag merge model with sidecar persistence
  - explicit metadata-tag edit intents:
    - sidecar-backed `metaTagAdds` + `suppressedMetaTags`
    - preview metadata field discovery (`metadataTagFields`)
  - preview handshake and explicit collision acceptance for auto suffixing
  - deterministic overlength truncate+hash naming policy
  - GUI rename usability improvements:
    - path pickers for gallery/env/tags-map fields with normalized status
      handling (`selected`, `cancelled`, `unavailable`, `error`)
    - in-panel bootstrap action for filename-derived tags map generation
    - preview table with collision/warning filters and row selection
    - selected-row manual tag edit + persistence helper
  - GUI rename integration/e2e confidence improvements:
    - worker-driven integration tests for preview/apply/history/rollback
    - negative-path coverage for invalid tags map, missing ids, and collision guard
    - headless filter/selection model tests for deterministic preview-table behavior
  - CLI onboarding/usability improvements:
    - `--rename-init` for target/env preflight and cache layout setup
    - `--rename-bootstrap-tags-from-filename` to build tag map JSON from
      filename numeric tokens
    - `--rename-apply-latest` to apply from latest preview artifact
    - `--rename-meta-tag-add` / `--rename-meta-tag-remove` for bulk metadata
      tag edits in preview scope
    - `--rename-meta-fields` to print discovered editable metadata fields in
      preview scope
    - optional full preview JSON controls:
      - `--rename-preview-json`
      - `--rename-preview-json-out <path>`
    - path typo hints for unresolved rename target directory
- Release hardening (v0.6.7):
  - cross-platform CI matrix:
    - Linux/macOS build + test
    - Windows (MSYS2) build + test
    - macOS GUI build
  - deterministic pre-release checklist script:
    - `scripts/release_check.sh`
    - optional tag verification: `--expected-tag vX.Y.Z`
- Rename history ergonomics (v0.6.8):
  - latest id helper commands:
    - `--rename-preview-latest-id`
    - `--rename-history-latest-id`
  - operation deep inspection:
    - `--rename-history-detail <operation_id>`
  - safe re-apply shortcut:
    - `--rename-redo <operation_id>`
    - resolves operation -> preview and reuses apply safety checks
- GUI rename history ergonomics (v0.6.9):
  - latest id helpers from Rename tab:
    - latest preview id
    - latest operation id
  - history detail action (operation summary + manifest JSON)
  - redo action (operation -> preview -> apply path with existing safeguards)
- Rename history management + audit export (v0.6.10):
  - history filters in CLI/GUI:
    - operation id prefix
    - rollback status (`any|yes|no`)
    - created-at date range (`from`/`to`)
  - history export to JSON audit payload:
    - filtered entries + manifest availability/item summaries
  - explicit history prune action:
    - manual keep-count cleanup in CLI/GUI
  - rollback preflight:
    - non-mutating feasibility validation before rollback
- CI stabilization + coverage ratchet (v0.6.11):
  - Linux strict-C99 build compatibility (`strdup` declaration path)
  - Windows filesystem portability hardening (`_fullpath`, `_mkdir` path)
  - raylib decoupling from non-rendering test headers (`make test` does not require raylib)
  - coverage reporting pipeline (`make coverage`) with regression-only ratchet gate

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

Release checklist:
```bash
./scripts/release_check.sh
./scripts/release_check.sh --expected-tag v0.6.11
```

Coverage run:
```bash
make coverage
python3 ./scripts/coverage_gate.py --baseline tests/coverage/baseline.json --summary build/coverage/summary.json
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

Reset GUI saved state explicitly:
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
The GUI also supports in-app model installation via **Download Models** on the
Scan tab.

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
- `--duplicates-report`: analyze duplicate groups without moving files.
- `--duplicates-move`: move duplicate copies into `<env_dir>`.
- `--rollback`: restore moved files from `manifest.json`.
- `--rename-init`: validate target/env and create rename cache layout.
- `--rename-bootstrap-tags-from-filename`: generate manual tag map from filename numeric tokens.
- `--rename-preview`: build dedicated pattern-based rename preview.
- `--rename-apply`: apply rename from preview id.
- `--rename-apply-latest`: apply rename from latest preview artifact in env.
- `--rename-preview-latest-id`: print latest preview id from env preview cache.
- `--rename-pattern <pattern>`: set pattern for rename preview.
- `--rename-tags-map <json_path>`: ingest per-file manual tags map.
- `--rename-tag-add <csv_tags>`: bulk add manual tags for preview scope.
- `--rename-tag-remove <csv_tags>`: bulk remove/suppress tags for preview scope.
- `--rename-meta-tag-add <csv_tags>`: bulk add metadata tags for preview scope.
- `--rename-meta-tag-remove <csv_tags>`: bulk remove/suppress metadata tags for preview scope.
- `--rename-meta-fields`: print discovered editable metadata fields in current preview scope.
- `--rename-preview-json`: print full preview JSON payload.
- `--rename-preview-json-out <path>`: write full preview JSON payload to file.
- `--rename-from-preview <preview_id>`: required handshake id for `--rename-apply`.
- `--rename-accept-auto-suffix`: accept deterministic collision suffixing (`_1`, `_2`, ...).
- `--rename-history`: list dedicated rename operation history.
- `--rename-history-id-prefix <prefix>`: filter history by operation-id prefix.
- `--rename-history-rollback <any|yes|no>`: filter history by rollback status.
- `--rename-history-from <date>`: lower history date bound (`YYYY-MM-DD` or UTC timestamp).
- `--rename-history-to <date>`: upper history date bound (`YYYY-MM-DD` or UTC timestamp).
- `--rename-history-export <json_path>`: export filtered history audit JSON.
- `--rename-history-prune <keep_count>`: keep latest N history entries and prune older ones.
- `--rename-history-latest-id`: print latest rename operation id.
- `--rename-history-detail <operation_id>`: print detailed history summary + operation manifest.
- `--rename-redo <operation_id>`: resolve operation preview id and re-apply rename via standard apply path.
- `--rename-rollback-preflight <operation_id>`: validate rollback feasibility without file mutation.
- `--rename-rollback <operation_id>`: rollback dedicated rename operation.

Notes:
- plain scan/cache runs do not move duplicate files automatically.
- duplicate moves are explicit-only via `--duplicates-move`.
- rename preview output is summary-first; use `--rename-preview-json` or
  `--rename-preview-json-out` for full JSON payloads.

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
- dedicated rename preview/apply/history/rollback with pattern + tag controls

Additional 0.5.4 behavior:
- fixed window shell (`1280x820`) focused on functional operation parity
- deterministic non-responsive panel geometry
- selected tabs/mode toggles are highlighted for clarity
- guided action locking:
  - actions that need prerequisites are disabled until satisfied
  - blocked clicks surface explicit reasons in status/help text
- task action buttons are disabled while a background task is running
- in-app model installation button (**Download Models**) wired to backend API
- hover tooltips for fields/actions
- strict numeric validation for jobs/threshold/topK/compression level with range hints
- scan profile persistence and automatic cache rebuild on semantic mismatch
- explicit save-only state behavior:
  - **Save State** persists the full functional configuration
  - unsaved configuration changes prompt Save/Discard/Cancel on exit
- pre-scan rebuild guard:
  - scan-like actions run profile preflight first
  - when cache exists and profile mismatches, GUI asks confirmation before rebuild
- idle runtime refresh uses event-driven updates plus a slow poll cadence
- rename tab usability additions (v0.6.2):
  - tags-map picker + bootstrap action
  - preview table with collision/warning filtering
  - selected-row per-file manual tag update action
- rename metadata-tag additions (v0.6.4):
  - bulk metadata add/remove controls
  - selected-row metadata tag update action
- cross-platform picker expansion (v0.6.5):
  - macOS: `osascript`
  - Linux: `zenity -> kdialog -> yad` fallback chain
  - Windows: `powershell -> pwsh` fallback chain
  - picker cancel/unavailable paths are informational; only backend failures are
    treated as errors
- GUI rename integration coverage expansion (v0.6.6):
  - preview/apply/history/rollback task wiring covered by integration tests
  - tag-update + preview-refresh workflow covered by integration tests
  - collisions, missing ids, and invalid tags-map paths covered as negative flows
- GUI rename history ergonomics (v0.6.9):
  - latest preview id + latest operation id lookup actions
  - history detail action with summary + manifest output
  - redo action from operation id with existing apply safeguards
- Rename history management additions (v0.6.10):
  - rollback-status/date filters for history list/export
  - JSON export action for filtered history audit payloads
  - manual history prune action with keep-count input
  - rollback preflight action (`Rollback Check`) before mutation

GUI path fields remain directly editable and now also provide picker actions on
supported platforms.

Cache profile behavior:
- sidecar file: `<env_dir>/.cache/gallery_cache.profile.json`
- strict tracked fields:
  - absolute source root path
  - `--exhaustive` mode
  - `--ml-enrich` requested
  - similarity-prep requested (`--similarity-report`)
  - model fingerprint (SHA-256 over required model files)
- non-tracked fields (do not force rebuild):
  - `--jobs`
  - cache compression mode/level
  - `--sim-incremental`
- optional informational field:
  - `cacheEntryCount` (last-known cache entry count, not part of strict profile match)
- if profile differs from the current scan request, cache is automatically rebuilt before scan
- missing/malformed profile is treated as mismatch and rebuilt safely

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

### Duplicate analysis (non-mutating)
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --duplicates-report
```

### Duplicate move (explicit)
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --duplicates-move
```

### Rename preview (pattern-based)
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --rename-preview --rename-pattern "pic-[tags]-[camera].[format]"
```

### Rename init + bootstrap tags map
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --rename-init
./build/bin/gallery_organizer /path/to/source /path/to/env --rename-bootstrap-tags-from-filename
```

### Rename preview JSON export
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --rename-preview --rename-pattern "pic-[tags]-[camera].[format]" --rename-preview-json-out /path/to/env/preview.json
```

### Rename preview with metadata tag edits
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --rename-preview --rename-pattern "pic-[tags_meta]-[camera].[format]" --rename-meta-tag-add "frag-a,frag-b" --rename-meta-fields
```

### Rename apply (preview handshake)
```bash
./build/bin/gallery_organizer /path/to/env --rename-apply --rename-from-preview <preview_id> --rename-accept-auto-suffix
```

### Rename apply (latest preview shortcut)
```bash
./build/bin/gallery_organizer /path/to/env --rename-apply-latest --rename-accept-auto-suffix
```

### Rename history and rollback
```bash
./build/bin/gallery_organizer /path/to/env --rename-history
./build/bin/gallery_organizer /path/to/env --rename-history --rename-history-rollback no --rename-history-from 2026-01-01
./build/bin/gallery_organizer /path/to/env --rename-history-export /path/to/env/history_audit.json --rename-history-id-prefix rop-2026
./build/bin/gallery_organizer /path/to/env --rename-history-prune 200
./build/bin/gallery_organizer /path/to/env --rename-history-latest-id
./build/bin/gallery_organizer /path/to/env --rename-history-detail <operation_id>
./build/bin/gallery_organizer /path/to/env --rename-rollback-preflight <operation_id>
./build/bin/gallery_organizer /path/to/env --rename-rollback <operation_id>
```

### Rename redo and latest preview helper
```bash
./build/bin/gallery_organizer /path/to/env --rename-preview-latest-id
./build/bin/gallery_organizer /path/to/env --rename-redo <operation_id> --rename-accept-auto-suffix
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
- `v0.5.3`: guided functional GUI (dependency-locked actions, help hints/tooltips, in-app model install).
- `v0.5.4`: persistent cache profile with parameter-aware automatic recompute.
- `v0.6.0`: dedicated pattern-based rename workflow with hybrid tagging and operation history rollback.
- `v0.6.1`: CLI rename onboarding/usability (init/bootstrap/apply-latest/preview JSON controls).
- `v0.6.2`: GUI rename UX improvements (pickers, preview table, guided per-file tagging).
- `v0.6.3`: extended rename tokens with GPS/location support (`[gps_lat]`, `[gps_lon]`, `[location]`).
- `v0.6.4`: metadata tag editing workflow (CLI + GUI + resolver persistence).
- `v0.6.5`: cross-platform GUI picker expansion.
- `v0.6.6`: GUI rename integration/E2E coverage.
- `v0.6.7`: release hardening (cross-platform CI matrix + release checklist script).
- `v0.6.8`: rename history ergonomics (detail inspection, redo path, latest-id helpers).
- `v0.6.9`: GUI rename history ergonomics (latest-id/detail/redo actions).
- `v0.6.10`: rename history management + audit export (filters/export/prune/preflight across CLI/GUI).
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
