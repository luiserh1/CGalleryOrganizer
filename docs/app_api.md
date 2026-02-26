# Frontend App API Contract

## Purpose

`include/app_api.h` and `include/app_api_types.h` define the canonical backend
contract for all frontends.

The API is frontend-framework-neutral and language-agnostic:
- CLI and C GUI call it in-process.
- Future native frontends (SwiftUI, etc.) should call it through native bindings.
- Future web frontends should call a dedicated adapter service (not browser-direct C calls).

## Lifecycle

1. Build `AppRuntimeOptions`.
2. Call `AppContextCreate()`.
3. Execute one or more `App*` operations.
4. Read status/result values.
5. On failures, read `AppGetLastError()`.
6. Free API-owned dynamic outputs (`AppFree*`).
7. Call `AppContextDestroy()`.

## Error Handling

Every operation returns `AppStatus`.

- `APP_STATUS_OK`: success.
- `APP_STATUS_INVALID_ARGUMENT`: missing/invalid request fields.
- `APP_STATUS_IO_ERROR`: filesystem read/write failure.
- `APP_STATUS_CACHE_ERROR`: cache load/parse/save failure.
- `APP_STATUS_ML_ERROR`: model/provider/inference failure.
- `APP_STATUS_CANCELLED`: caller cancellation hook requested stop.
- `APP_STATUS_INTERNAL_ERROR`: unexpected internal failure.

Use `AppStatusToString()` for user text and `AppGetLastError()` for detail.

## Progress and Cancellation Hooks

Requests that include `AppOperationHooks` can report progress and be cancelled.

- `progress_cb`: receives stage/counters/messages.
- `cancel_cb`: polled by long operations.
- `user_data`: caller-owned opaque pointer passed back to callbacks.

The API remains synchronous. Frontends can run calls in worker threads.

## Dependency Matrix

| Operation | Required prior state | Typical predecessor |
| --- | --- | --- |
| `AppInspectRuntimeState` | none (optional `env_dir` for cache/manifest probes) | frontend startup or before task dispatch |
| `AppInstallModels` | valid manifest and writable models root | frontend startup or before ML/similarity tasks |
| `AppInspectScanProfile` | same request semantics as `AppRunScan`; no scan execution | before scan-like actions requiring rebuild confirmation |
| `AppRunScan` | valid `target_dir`, `env_dir`; cache profile sidecar may trigger automatic rebuild | none |
| `AppGenerateSimilarityReport` | cache in `env_dir`; embeddings available or producible from cache state | `AppRunScan` with similarity/ML path enabled |
| `AppPreviewOrganize` | cache in `env_dir` | `AppRunScan` |
| `AppExecuteOrganize` | cache in `env_dir` | `AppRunScan`, optionally `AppPreviewOrganize` |
| `AppRollback` | `manifest.json` from prior organize execution | `AppExecuteOrganize` |
| `AppFindDuplicates` | cache in `env_dir` | `AppRunScan` |
| `AppMoveDuplicates` | duplicate report generated in memory | `AppFindDuplicates` |

## Typical Call Sequences

### Guided GUI startup

1. `AppInspectRuntimeState(...)`
2. if required models are missing: `AppInstallModels(...)`
3. refresh `AppInspectRuntimeState(...)`
4. enable/disable frontend actions based on returned state

### Scan -> Similarity

1. `AppRunScan(..., .similarity_report=true or .ml_enrich=true)`
2. `AppGenerateSimilarityReport(...)`

### Cache profile reuse flow

1. (optional frontend preflight) `AppInspectScanProfile(...)`.
2. `AppRunScan(...)` with desired semantic parameters.
3. API compares requested profile with stored sidecar
   (`<env_dir>/.cache/gallery_cache.profile.json`).
4. If profile matches, cache is reused.
5. If profile is missing/malformed/mismatched, cache is rebuilt automatically
   before scan and a new sidecar is written after successful save.

### Scan -> Organize -> Rollback

1. `AppRunScan(...)`
2. `AppPreviewOrganize(...)` (optional but recommended)
3. `AppExecuteOrganize(...)`
4. `AppRollback(...)` (if revert needed)

### Scan -> Duplicate Analysis -> Move

1. `AppRunScan(...)`
2. `AppFindDuplicates(...)`
3. `AppMoveDuplicates(...)`
4. `AppFreeDuplicateReport(...)`

## Ownership and Lifetime Rules

- Context:
  - created by `AppContextCreate()`
  - destroyed by `AppContextDestroy()`
- Error string from `AppGetLastError()`:
  - owned by context
  - invalidated by subsequent API calls or context destruction
- `AppOrganizePlanResult.plan_text`:
  - owned by API on success
  - release with `AppFreeOrganizePlanResult()`
- `AppDuplicateReport` dynamic fields:
  - owned by API on success
  - release with `AppFreeDuplicateReport()`
- All request pointers/strings:
  - owned by caller
  - must remain valid for the duration of the call

## Function Reference

## Runtime

### `AppContext *AppContextCreate(const AppRuntimeOptions *options)`
- Initializes runtime state and optional model-root override.
- Returns `NULL` on failure.

### `void AppContextDestroy(AppContext *ctx)`
- Releases runtime state.
- Safe to call with `NULL`.

### `const char *AppStatusToString(AppStatus status)`
- Returns stable status label.

### `const char *AppGetLastError(const AppContext *ctx)`
- Returns latest context-local error detail string.

### `AppStatus AppInspectRuntimeState(AppContext *ctx, const AppRuntimeStateRequest *request, AppRuntimeState *out_state)`
- Lightweight runtime probe used by frontends for dependency gating.
- Optional probes:
  - cache and manifest presence/counts when `request->env_dir` is provided
  - models root and required model availability
- Always reports:
  - `logical_cores`
  - `recommended_jobs`
- Cache count semantics:
  - `cache_entry_count_known=true`: `cache_entry_count` is reliable
  - `cache_entry_count_known=false`: count is unknown/stale and must not be treated as empty
  - runtime inspect uses lightweight sources (in-memory active cache or profile sidecar last-known count),
    not full cache decode/parse in idle GUI paths
- Side effects: none on cache/ML/organizer state.

## Scan/Cache and ML

### `AppStatus AppRunScan(AppContext *ctx, const AppScanRequest *request, AppScanResult *out_result)`
- Scans media, updates cache, and optionally runs ML/similarity preparation.
- Required request fields: `target_dir`, `env_dir`.
- `out_result` returns scan/cache/ML counters and cache profile status:
  - `cache_profile_matched`
  - `cache_profile_rebuilt`
  - `cache_profile_reason`

### `AppStatus AppInspectScanProfile(AppContext *ctx, const AppScanRequest *request, AppScanProfileDecision *out_decision)`
- Non-mutating preflight for scan-like requests.
- Uses the same strict cache-profile decision logic as `AppRunScan(...)`.
- Typical frontend use:
  - inspect before starting a scan-like task,
  - if `will_rebuild_cache=true` and cache exists, ask for user confirmation.
- `out_decision` fields:
  - `profile_present`: sidecar exists and parsed correctly.
  - `profile_match`: requested semantics exactly match stored profile.
  - `will_rebuild_cache`: scan would rebuild cache before pipeline execution.
  - `reason`: human-readable summary of match/mismatch.

#### Cache Profile Contract (v1)

Sidecar path:
- `<env_dir>/.cache/gallery_cache.profile.json`
- default (no env dir): `.cache/gallery_cache.profile.json`

Tracked strict-equality fields:
- `sourceRootAbs` (absolute source path)
- `exhaustive`
- `mlEnrichRequested`
- `similarityPrepRequested`
- `modelsFingerprint` (SHA-256 fingerprint computed from required model files)
  - ML enrich requested: `clf-default.onnx`, `text-default.onnx`
  - similarity prep requested: `embed-default.onnx`

Optional informational field (excluded from strict match):
- `cacheEntryCount` (last-known cache entry count)

Non-tracked scan request fields:
- `jobs`
- cache compression mode/level
- `sim_incremental`

Behavior:
- missing or malformed sidecar is treated as mismatch.
- mismatch triggers automatic full cache rebuild before scan.
- sidecar write happens only after successful cache save.

## Similarity

### `AppStatus AppGenerateSimilarityReport(AppContext *ctx, const AppSimilarityRequest *request, AppSimilarityResult *out_result)`
- Generates similarity JSON report from cached embeddings.
- Required request field: `env_dir`.
- `out_result->report_path` contains final output file path.

## Organize

### `AppStatus AppPreviewOrganize(AppContext *ctx, const AppOrganizePlanRequest *request, AppOrganizePlanResult *out_result)`
- Builds organize plan text without moving files.
- Required request field: `env_dir`.
- `out_result->plan_text` must be released with `AppFreeOrganizePlanResult()`.

### `AppStatus AppExecuteOrganize(AppContext *ctx, const AppOrganizeExecuteRequest *request, AppOrganizeExecuteResult *out_result)`
- Executes organize moves and writes rollback manifest.
- Required request field: `env_dir`.

### `AppStatus AppRollback(AppContext *ctx, const AppRollbackRequest *request, AppRollbackResult *out_result)`
- Restores files from manifest history.
- Required request field: `env_dir`.

## Duplicates

### `AppStatus AppFindDuplicates(AppContext *ctx, const AppDuplicateFindRequest *request, AppDuplicateReport *out_report)`
- Computes duplicate groups from cache.
- Required request field: `env_dir`.
- Use `AppFreeDuplicateReport()` after use.

### `AppStatus AppMoveDuplicates(AppContext *ctx, const AppDuplicateMoveRequest *request, AppDuplicateMoveResult *out_result)`
- Moves duplicate files to `target_dir` based on the supplied report.
- `request->report` must come from `AppFindDuplicates()`.

## Model Management

### `AppStatus AppInstallModels(AppContext *ctx, const AppModelInstallRequest *request, AppModelInstallResult *out_result)`
- Validates manifest shape and required license/attribution fields.
- Installs model files into resolved models root.
- Verifies SHA-256 for each installed artifact.
- Writes lockfile metadata at `<models_root>/.installed.json`.
- Supports cancellation/progress via `AppOperationHooks`.
- Returns counts for manifest entries, installed files, and skipped files.

Note:
- In this build, native app install currently supports manifest entries using
  `data:application/octet-stream;base64,...` URLs.
- Non-data URLs should be handled by external tooling (e.g., `make models`)
  until HTTP download support is introduced in app-level installer.

## Free Helpers

### `void AppFreeOrganizePlanResult(AppOrganizePlanResult *result)`
- Frees `plan_text` and resets result object fields.

### `void AppFreeDuplicateReport(AppDuplicateReport *report)`
- Frees report group arrays and strings, then resets report fields.
