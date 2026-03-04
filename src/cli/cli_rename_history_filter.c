#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/cli_rename_common.h"
#include "cli/cli_rename_utils.h"

static bool IsDigits(const char *text, size_t len) {
  if (!text) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    if (!isdigit((unsigned char)text[i])) {
      return false;
    }
  }
  return true;
}

bool CliRenameParseRollbackFilter(const char *raw_value,
                                  CliRenameRollbackFilter *out_filter,
                                  char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!out_filter) {
    CliRenameCommonSetError(out_error, out_error_size, "output rollback filter is required");
    return false;
  }

  if (!raw_value || raw_value[0] == '\0' || strcmp(raw_value, "any") == 0) {
    *out_filter = CLI_RENAME_ROLLBACK_FILTER_ANY;
    return true;
  }
  if (strcmp(raw_value, "yes") == 0 || strcmp(raw_value, "true") == 0) {
    *out_filter = CLI_RENAME_ROLLBACK_FILTER_YES;
    return true;
  }
  if (strcmp(raw_value, "no") == 0 || strcmp(raw_value, "false") == 0) {
    *out_filter = CLI_RENAME_ROLLBACK_FILTER_NO;
    return true;
  }

  CliRenameCommonSetError(out_error, out_error_size,
           "invalid rollback filter '%s' (allowed: any|yes|no)", raw_value);
  return false;
}

bool CliRenameNormalizeHistoryDateBound(const char *raw_value, bool is_upper_bound,
                                        char *out_utc, size_t out_utc_size,
                                        char *out_error,
                                        size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!out_utc || out_utc_size == 0) {
    CliRenameCommonSetError(out_error, out_error_size, "output date buffer is required");
    return false;
  }

  out_utc[0] = '\0';
  if (!raw_value || raw_value[0] == '\0') {
    return true;
  }

  size_t len = strlen(raw_value);
  if (len == 10) {
    if (!IsDigits(raw_value, 4) || raw_value[4] != '-' ||
        !IsDigits(raw_value + 5, 2) || raw_value[7] != '-' ||
        !IsDigits(raw_value + 8, 2)) {
      CliRenameCommonSetError(out_error, out_error_size,
               "invalid date '%s' (expected YYYY-MM-DD)", raw_value);
      return false;
    }
    snprintf(out_utc, out_utc_size, "%sT%sZ", raw_value,
             is_upper_bound ? "23:59:59" : "00:00:00");
    return true;
  }

  if (len == 20 && IsDigits(raw_value, 4) && raw_value[4] == '-' &&
      IsDigits(raw_value + 5, 2) && raw_value[7] == '-' &&
      IsDigits(raw_value + 8, 2) && raw_value[10] == 'T' &&
      IsDigits(raw_value + 11, 2) && raw_value[13] == ':' &&
      IsDigits(raw_value + 14, 2) && raw_value[16] == ':' &&
      IsDigits(raw_value + 17, 2) && raw_value[19] == 'Z') {
    strncpy(out_utc, raw_value, out_utc_size - 1);
    out_utc[out_utc_size - 1] = '\0';
    return true;
  }

  CliRenameCommonSetError(out_error, out_error_size,
           "invalid date '%s' (expected YYYY-MM-DD or YYYY-MM-DDTHH:MM:SSZ)",
           raw_value);
  return false;
}

static bool HistoryEntryMatchesFilter(const AppRenameHistoryEntry *entry,
                                      const CliRenameHistoryFilter *filter) {
  if (!entry || !filter) {
    return false;
  }

  if (filter->operation_id_prefix[0] != '\0') {
    size_t prefix_len = strlen(filter->operation_id_prefix);
    if (strncmp(entry->operation_id, filter->operation_id_prefix, prefix_len) !=
        0) {
      return false;
    }
  }

  if (filter->rollback_filter == CLI_RENAME_ROLLBACK_FILTER_YES &&
      !entry->rollback_performed) {
    return false;
  }
  if (filter->rollback_filter == CLI_RENAME_ROLLBACK_FILTER_NO &&
      entry->rollback_performed) {
    return false;
  }

  if (filter->created_from_utc[0] != '\0') {
    if (entry->created_at_utc[0] == '\0' ||
        strcmp(entry->created_at_utc, filter->created_from_utc) < 0) {
      return false;
    }
  }
  if (filter->created_to_utc[0] != '\0') {
    if (entry->created_at_utc[0] == '\0' ||
        strcmp(entry->created_at_utc, filter->created_to_utc) > 0) {
      return false;
    }
  }

  return true;
}

bool CliRenameBuildHistoryFilterIndex(const AppRenameHistoryEntry *entries,
                                      int entry_count,
                                      const CliRenameHistoryFilter *filter,
                                      int **out_indices,
                                      int *out_filtered_count,
                                      char *out_error,
                                      size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!entries || entry_count < 0 || !filter || !out_indices ||
      !out_filtered_count) {
    CliRenameCommonSetError(out_error, out_error_size,
             "entries, filter, and output pointers are required");
    return false;
  }

  *out_indices = NULL;
  *out_filtered_count = 0;
  if (entry_count == 0) {
    return true;
  }

  int *indices = calloc((size_t)entry_count, sizeof(int));
  if (!indices) {
    CliRenameCommonSetError(out_error, out_error_size,
             "out of memory while building filtered history index");
    return false;
  }

  int written = 0;
  for (int i = 0; i < entry_count; i++) {
    if (HistoryEntryMatchesFilter(&entries[i], filter)) {
      indices[written++] = i;
    }
  }

  if (written == 0) {
    free(indices);
    indices = NULL;
  }

  *out_indices = indices;
  *out_filtered_count = written;
  return true;
}

void CliRenameFreeHistoryFilterIndex(int *indices) { free(indices); }
