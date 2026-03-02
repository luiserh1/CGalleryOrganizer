#ifndef RENAMER_APPLY_H
#define RENAMER_APPLY_H

#include <stdbool.h>
#include <stddef.h>

#include "systems/renamer_preview.h"

typedef struct {
  const char *env_dir;
  const RenamerPreviewArtifact *preview;
  bool accept_auto_suffix;
} RenamerApplyRequest;

typedef struct {
  char operation_id[64];
  char created_at_utc[32];
  int renamed_count;
  int skipped_count;
  int failed_count;
  int collision_resolved_count;
  int truncation_count;
  bool auto_suffix_applied;
} RenamerApplyResult;

bool RenamerApplyExecute(const RenamerApplyRequest *request,
                         RenamerApplyResult *out_result,
                         char *out_error, size_t out_error_size);

#endif // RENAMER_APPLY_H
