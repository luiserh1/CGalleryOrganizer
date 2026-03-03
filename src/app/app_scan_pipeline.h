#ifndef APP_SCAN_PIPELINE_H
#define APP_SCAN_PIPELINE_H

#include <stdbool.h>

typedef bool (*ScanCancelCallback)(void *user_data);
typedef void (*ScanProgressCallback)(const char *stage, int current, int total,
                                     void *user_data);

typedef struct {
  bool exhaustive;
  bool ml_enrich;
  bool similarity_report;
  bool sim_incremental;
  bool ml_enabled;
  int ml_files_evaluated;
  int ml_files_classified;
  int ml_files_with_text;
  int ml_files_embedded;
  int ml_failures;
  bool cancelled;
  int total_files;
  ScanCancelCallback cancel_cb;
  ScanProgressCallback progress_cb;
  void *callback_user_data;
} AppScanContext;

typedef struct {
  int files_scanned;
  int files_cached;
} ScanRunStats;

bool AppRunScanPipeline(const char *target_dir, AppScanContext *ctx,
                        int resolved_jobs, ScanRunStats *out_stats);

#endif // APP_SCAN_PIPELINE_H
