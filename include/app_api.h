#ifndef APP_API_H
#define APP_API_H

#include "app_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AppContext AppContext;

AppContext *AppContextCreate(const AppRuntimeOptions *options);
void AppContextDestroy(AppContext *ctx);

const char *AppStatusToString(AppStatus status);
const char *AppGetLastError(const AppContext *ctx);

AppStatus AppRunScan(AppContext *ctx, const AppScanRequest *request,
                     AppScanResult *out_result);

AppStatus AppGenerateSimilarityReport(AppContext *ctx,
                                      const AppSimilarityRequest *request,
                                      AppSimilarityResult *out_result);

AppStatus AppPreviewOrganize(AppContext *ctx,
                             const AppOrganizePlanRequest *request,
                             AppOrganizePlanResult *out_result);

AppStatus AppExecuteOrganize(AppContext *ctx,
                             const AppOrganizeExecuteRequest *request,
                             AppOrganizeExecuteResult *out_result);

AppStatus AppRollback(AppContext *ctx, const AppRollbackRequest *request,
                      AppRollbackResult *out_result);

AppStatus AppFindDuplicates(AppContext *ctx,
                            const AppDuplicateFindRequest *request,
                            AppDuplicateReport *out_report);

AppStatus AppMoveDuplicates(AppContext *ctx,
                            const AppDuplicateMoveRequest *request,
                            AppDuplicateMoveResult *out_result);

void AppFreeOrganizePlanResult(AppOrganizePlanResult *result);
void AppFreeDuplicateReport(AppDuplicateReport *report);

#ifdef __cplusplus
}
#endif

#endif // APP_API_H
