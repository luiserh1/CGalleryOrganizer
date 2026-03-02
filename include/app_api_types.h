#ifndef APP_API_TYPES_H
#define APP_API_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#define APP_MAX_PATH 1024
#define APP_MAX_ERROR 512

typedef enum {
  APP_STATUS_OK = 0,
  APP_STATUS_INVALID_ARGUMENT = 1,
  APP_STATUS_IO_ERROR = 2,
  APP_STATUS_CACHE_ERROR = 3,
  APP_STATUS_ML_ERROR = 4,
  APP_STATUS_CANCELLED = 5,
  APP_STATUS_INTERNAL_ERROR = 6
} AppStatus;

// Cache serialization mode for env_dir .cache artifact.
typedef enum {
  APP_CACHE_COMPRESSION_NONE = 0,
  APP_CACHE_COMPRESSION_ZSTD = 1,
  APP_CACHE_COMPRESSION_AUTO = 2
} AppCacheCompressionMode;

// Embedding decode strategy for similarity generation.
typedef enum {
  APP_SIM_MEMORY_EAGER = 0,
  APP_SIM_MEMORY_CHUNKED = 1
} AppSimilarityMemoryMode;

typedef struct {
  const char *stage;
  int current;
  int total;
  const char *message;
} AppProgressEvent;

typedef void (*AppProgressCallback)(const AppProgressEvent *event,
                                    void *user_data);
typedef bool (*AppCancelCallback)(void *user_data);

typedef struct {
  AppProgressCallback progress_cb;
  AppCancelCallback cancel_cb;
  void *user_data;
} AppOperationHooks;

typedef struct {
  // Optional models root override. NULL uses project defaults.
  const char *models_root;
} AppRuntimeOptions;

typedef struct {
  // Optional cache environment root. If missing, cache/manifest probes are skipped.
  const char *env_dir;
  // Optional models root override. NULL uses context/default root.
  const char *models_root_override;
} AppRuntimeStateRequest;

typedef struct {
  bool classification_present;
  bool text_detection_present;
  bool embedding_present;
  int missing_count;
  char missing_ids[3][32];
} AppModelAvailability;

typedef struct {
  bool cache_exists;
  // True when cache_entry_count is reliable for the current env/cache state.
  bool cache_entry_count_known;
  int cache_entry_count;
  bool rollback_manifest_exists;
  int logical_cores;
  int recommended_jobs;
  char models_root[APP_MAX_PATH];
  AppModelAvailability models;
} AppRuntimeState;

typedef struct {
  // Required for scan-like operations.
  const char *target_dir;
  // Required cache environment root.
  const char *env_dir;
  bool exhaustive;
  bool ml_enrich;
  bool similarity_report;
  bool sim_incremental;
  int jobs;
  AppCacheCompressionMode cache_compression_mode;
  int cache_compression_level;
  const char *models_root_override;
  AppOperationHooks hooks;
} AppScanRequest;

typedef struct {
  int files_scanned;
  int files_cached;
  int ml_files_evaluated;
  int ml_files_classified;
  int ml_files_with_text;
  int ml_files_embedded;
  int ml_failures;
  // True when persisted cache profile exactly matches request semantics.
  bool cache_profile_matched;
  // True when cache was rebuilt due to missing/malformed/mismatched profile.
  bool cache_profile_rebuilt;
  // Human-readable reason for match/mismatch/rebuild decision.
  char cache_profile_reason[APP_MAX_ERROR];
} AppScanResult;

typedef struct {
  // True when profile sidecar exists and could be parsed.
  bool profile_present;
  // True when requested profile semantics exactly match stored profile.
  bool profile_match;
  // True when scan would rebuild cache before pipeline execution.
  bool will_rebuild_cache;
  // Human-readable summary of profile decision.
  char reason[APP_MAX_ERROR];
} AppScanProfileDecision;

typedef struct {
  // Required cache environment root.
  const char *env_dir;
  AppCacheCompressionMode cache_compression_mode;
  int cache_compression_level;
  float threshold;
  int topk;
  AppSimilarityMemoryMode memory_mode;
  const char *output_path_override;
  AppOperationHooks hooks;
} AppSimilarityRequest;

typedef struct {
  // Absolute or env-relative path to generated similarity report.
  char report_path[APP_MAX_PATH];
} AppSimilarityResult;

typedef struct {
  // Required cache environment root.
  const char *env_dir;
  AppCacheCompressionMode cache_compression_mode;
  int cache_compression_level;
  const char *group_by_arg;
  AppOperationHooks hooks;
} AppOrganizePlanRequest;

typedef struct {
  // Heap-owned string, release with AppFreeOrganizePlanResult().
  char *plan_text;
  int planned_moves;
} AppOrganizePlanResult;

typedef struct {
  // Required cache environment root.
  const char *env_dir;
  AppCacheCompressionMode cache_compression_mode;
  int cache_compression_level;
  const char *group_by_arg;
  AppOperationHooks hooks;
} AppOrganizeExecuteRequest;

typedef struct {
  int planned_moves;
  int executed_moves;
} AppOrganizeExecuteResult;

typedef struct {
  // Required target directory to traverse recursively in-place.
  const char *target_dir;
  // Required cache environment root.
  const char *env_dir;
  // Optional pattern. If NULL/empty, env default is used.
  const char *pattern;
  // Optional JSON path for per-file manual tag map ingest.
  const char *tags_map_json_path;
  // Optional CSV tags added to every file in preview scope.
  const char *tag_add_csv;
  // Optional CSV tags removed from every file in preview scope.
  const char *tag_remove_csv;
  AppOperationHooks hooks;
} AppRenamePreviewRequest;

typedef struct {
  char preview_id[64];
  char preview_path[APP_MAX_PATH];
  char effective_pattern[256];
  char fingerprint[64];
  int file_count;
  int collision_group_count;
  int collision_count;
  int truncation_count;
  bool requires_auto_suffix_acceptance;
  // Heap-owned preview details JSON for CLI/GUI rendering.
  char *details_json;
} AppRenamePreviewResult;

typedef struct {
  // Required cache environment root.
  const char *env_dir;
  // Required preview identifier returned by AppPreviewRename().
  const char *preview_id;
  // Must be true when preview reported collisions.
  bool accept_auto_suffix;
  AppOperationHooks hooks;
} AppRenameApplyRequest;

typedef struct {
  char operation_id[64];
  char created_at_utc[32];
  int renamed_count;
  int skipped_count;
  int failed_count;
  int collision_resolved_count;
  int truncation_count;
  bool auto_suffix_applied;
} AppRenameApplyResult;

typedef struct {
  // Required cache environment root.
  const char *env_dir;
  // Required operation identifier from rename history.
  const char *operation_id;
  AppOperationHooks hooks;
} AppRenameRollbackRequest;

typedef struct {
  int restored_count;
  int skipped_count;
  int failed_count;
} AppRenameRollbackResult;

typedef struct {
  char operation_id[64];
  char preview_id[64];
  char created_at_utc[32];
  int renamed_count;
  int skipped_count;
  int failed_count;
  int collision_resolved_count;
  int truncation_count;
  bool rollback_performed;
  int rollback_restored_count;
  int rollback_skipped_count;
  int rollback_failed_count;
} AppRenameHistoryEntry;

typedef struct {
  // Required cache environment root with manifest history.
  const char *env_dir;
} AppRollbackRequest;

typedef struct {
  int restored_count;
} AppRollbackResult;

typedef struct {
  // Required cache environment root.
  const char *env_dir;
  AppCacheCompressionMode cache_compression_mode;
  int cache_compression_level;
} AppDuplicateFindRequest;

typedef struct {
  char *original_path;
  char **duplicate_paths;
  int duplicate_count;
} AppDuplicateGroup;

typedef struct {
  // Heap-owned array allocated by AppFindDuplicates().
  AppDuplicateGroup *groups;
  int group_count;
} AppDuplicateReport;

typedef struct {
  // Destination directory where duplicate files are moved.
  const char *target_dir;
  // Must reference report previously returned by AppFindDuplicates().
  const AppDuplicateReport *report;
} AppDuplicateMoveRequest;

typedef struct {
  int moved_count;
  int failed_count;
} AppDuplicateMoveResult;

typedef struct {
  // Optional manifest override. NULL defaults to models/manifest.json.
  const char *manifest_path_override;
  // Optional models root override. NULL uses context/default root.
  const char *models_root_override;
  bool force_redownload;
  AppOperationHooks hooks;
} AppModelInstallRequest;

typedef struct {
  int manifest_model_count;
  int installed_count;
  int skipped_count;
  char models_root[APP_MAX_PATH];
  char lockfile_path[APP_MAX_PATH];
} AppModelInstallResult;

#endif // APP_API_TYPES_H
