#ifndef APP_API_H
#define APP_API_H

#include "app_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AppContext AppContext;

// Create runtime context used by all App* operations.
// Must be paired with AppContextDestroy().
AppContext *AppContextCreate(const AppRuntimeOptions *options);

// Release runtime context and owned resources.
void AppContextDestroy(AppContext *ctx);

// Stable mapping for AppStatus values.
const char *AppStatusToString(AppStatus status);

// Returns the latest error string produced by an App* call on this context.
// Pointer is owned by context and is invalid after the next App* call.
const char *AppGetLastError(const AppContext *ctx);

// Inspect cache/model/runtime prerequisites for frontend task gating.
// This call does not execute scan/ML/organize actions.
AppStatus AppInspectRuntimeState(AppContext *ctx,
                                 const AppRuntimeStateRequest *request,
                                 AppRuntimeState *out_state);

// Scan media in target_dir and update env_dir cache.
// Optional ML/similarity pre-work can be requested via request flags.
// Callers should run this before organize/duplicates/similarity operations when
// cache freshness is required.
AppStatus AppRunScan(AppContext *ctx, const AppScanRequest *request,
                     AppScanResult *out_result);

// Build similarity report from embeddings in env_dir cache.
// Requires valid cache entries with embeddings; run AppRunScan with similarity
// support enabled beforehand when needed.
AppStatus AppGenerateSimilarityReport(AppContext *ctx,
                                      const AppSimilarityRequest *request,
                                      AppSimilarityResult *out_result);

// Build text preview for organize plan using env_dir cache and group_by settings.
// On success out_result->plan_text is heap-owned by API and must be released by
// AppFreeOrganizePlanResult().
AppStatus AppPreviewOrganize(AppContext *ctx,
                             const AppOrganizePlanRequest *request,
                             AppOrganizePlanResult *out_result);

// Execute organize moves from env_dir cache and write manifest for rollback.
AppStatus AppExecuteOrganize(AppContext *ctx,
                             const AppOrganizeExecuteRequest *request,
                             AppOrganizeExecuteResult *out_result);

// Roll back organize operations using env_dir manifest history.
AppStatus AppRollback(AppContext *ctx, const AppRollbackRequest *request,
                      AppRollbackResult *out_result);

// Analyze duplicate groups from env_dir cache.
// On success out_report owns heap memory and must be released by
// AppFreeDuplicateReport().
AppStatus AppFindDuplicates(AppContext *ctx,
                            const AppDuplicateFindRequest *request,
                            AppDuplicateReport *out_report);

// Move duplicates reported by AppFindDuplicates() into target_dir.
AppStatus AppMoveDuplicates(AppContext *ctx,
                            const AppDuplicateMoveRequest *request,
                            AppDuplicateMoveResult *out_result);

// Download/install model artifacts from manifest into models_root.
AppStatus AppInstallModels(AppContext *ctx,
                           const AppModelInstallRequest *request,
                           AppModelInstallResult *out_result);

// Free memory owned by an AppOrganizePlanResult produced by AppPreviewOrganize().
void AppFreeOrganizePlanResult(AppOrganizePlanResult *result);

// Free memory owned by an AppDuplicateReport produced by AppFindDuplicates().
void AppFreeDuplicateReport(AppDuplicateReport *report);

#ifdef __cplusplus
}
#endif

#endif // APP_API_H
