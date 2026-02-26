#ifndef CLI_SCAN_PIPELINE_H
#define CLI_SCAN_PIPELINE_H

#include <stdbool.h>

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
} AppScanContext;

typedef struct {
  int files_scanned;
  int files_cached;
} ScanRunStats;

bool CliRunScanPipeline(const char *target_dir, AppScanContext *ctx,
                        int resolved_jobs, ScanRunStats *out_stats);

#endif // CLI_SCAN_PIPELINE_H
