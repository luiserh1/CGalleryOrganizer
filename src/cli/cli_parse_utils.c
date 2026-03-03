#include <stdlib.h>
#include <string.h>

#include "cli/cli_parse_utils.h"

bool CliParseSimIncrementalValue(const char *value, bool *out_sim_incremental) {
  if (!value || !out_sim_incremental) {
    return false;
  }

  if (strcmp(value, "on") == 0) {
    *out_sim_incremental = true;
    return true;
  }
  if (strcmp(value, "off") == 0) {
    *out_sim_incremental = false;
    return true;
  }
  return false;
}

bool CliParseSimilarityMemoryModeValue(const char *mode,
                                       AppSimilarityMemoryMode *out_mode) {
  if (!mode || !out_mode) {
    return false;
  }

  if (strcmp(mode, "eager") == 0) {
    *out_mode = APP_SIM_MEMORY_EAGER;
    return true;
  }
  if (strcmp(mode, "chunked") == 0) {
    *out_mode = APP_SIM_MEMORY_CHUNKED;
    return true;
  }
  return false;
}

bool CliParseCacheCompressionModeValue(const char *mode,
                                       AppCacheCompressionMode *out_mode) {
  if (!mode || !out_mode) {
    return false;
  }

  if (strcmp(mode, "none") == 0) {
    *out_mode = APP_CACHE_COMPRESSION_NONE;
    return true;
  }
  if (strcmp(mode, "zstd") == 0) {
    *out_mode = APP_CACHE_COMPRESSION_ZSTD;
    return true;
  }
  if (strcmp(mode, "auto") == 0) {
    *out_mode = APP_CACHE_COMPRESSION_AUTO;
    return true;
  }
  return false;
}

bool CliParseLongInRange(const char *raw, long min_value, long max_value,
                         long *out_value) {
  if (!raw || !out_value || min_value > max_value) {
    return false;
  }

  char *endptr = NULL;
  long parsed = strtol(raw, &endptr, 10);
  if (!endptr || *endptr != '\0' || parsed < min_value || parsed > max_value) {
    return false;
  }

  *out_value = parsed;
  return true;
}
