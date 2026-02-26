#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cli/cli_args.h"

static const int DEFAULT_MAX_JOBS = 8;
static const char *GROUP_KEYS[] = {"date", "camera", "format", "orientation",
                                   "resolution"};

void CliPrintUsage(const char *argv0) {
  printf("Usage: %s <directory_to_scan> [env_dir] [options]\n", argv0);
  printf("\n");
  printf("Options:\n");
  printf("  -h, --help        Show this help message and exit\n");
  printf("  -e, --exhaustive  Extract all metadata tags (larger cache)\n");
  printf("  --ml-enrich       Run local ML enrichment (classification + text detection)\n");
  printf("  --similarity-report Build similarity report using embeddings\n");
  printf("  --sim-incremental <on|off> Reuse valid embeddings on similarity runs "
         "(default: on)\n");
  printf("  --sim-memory-mode <eager|chunked> Similarity embedding decode mode "
         "(default: chunked)\n");
  printf("  --sim-threshold <0..1> Similarity threshold (default: 0.92)\n");
  printf("  --sim-topk <n>    Max neighbors per anchor (default: 5)\n");
  printf("  --jobs <n|auto>   Worker count for scan/inference (default: auto)\n");
  printf("  --cache-compress <mode> Cache compression mode: none|zstd|auto\n");
  printf("  --cache-compress-level <1..19> Compression level when using zstd "
         "(default: 3)\n");
  printf("  --organize        Execute metadata-based restructuring\n");
  printf("  --group-by <keys> Set grouping fields (e.g. 'camera,date'). "
         "Default: date\n");
  printf("  --preview         Print restructuring plan without executing\n");
  printf("  --rollback        Undo a restructuring operation using the manifest\n");
  printf("\nRollback usage:\n");
  printf("  %s <scan_dir> <env_dir> --rollback\n", argv0);
  printf("  %s <env_dir> --rollback\n", argv0);
}

static bool IsValidGroupKey(const char *key) {
  for (size_t i = 0; i < sizeof(GROUP_KEYS) / sizeof(GROUP_KEYS[0]); i++) {
    if (strcmp(key, GROUP_KEYS[i]) == 0) {
      return true;
    }
  }
  return false;
}

static char *TrimInPlace(char *s) {
  while (*s != '\0' && isspace((unsigned char)*s)) {
    s++;
  }

  if (*s == '\0') {
    return s;
  }

  char *end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end)) {
    *end = '\0';
    end--;
  }
  return s;
}

bool CliParseGroupByKeys(const char *group_by_arg, const char **out_keys,
                         int max_keys, int *out_count,
                         char **out_owned_buffer) {
  if (!out_keys || !out_count || !out_owned_buffer || max_keys <= 0) {
    return false;
  }

  *out_count = 0;
  *out_owned_buffer = NULL;

  if (!group_by_arg) {
    out_keys[0] = "date";
    *out_count = 1;
    return true;
  }

  char *owned = strdup(group_by_arg);
  if (!owned) {
    return false;
  }

  char *cursor = owned;
  while (true) {
    char *comma = strchr(cursor, ',');
    if (comma) {
      *comma = '\0';
    }

    char *trimmed = TrimInPlace(cursor);
    if (*trimmed == '\0') {
      printf("Error: --group-by cannot include empty keys. "
             "Allowed keys: date,camera,format,orientation,resolution\n");
      free(owned);
      return false;
    }

    if (!IsValidGroupKey(trimmed)) {
      printf("Error: Invalid --group-by key '%s'. "
             "Allowed keys: date,camera,format,orientation,resolution\n",
             trimmed);
      free(owned);
      return false;
    }

    if (*out_count >= max_keys) {
      printf("Error: Too many --group-by keys (max %d).\n", max_keys);
      free(owned);
      return false;
    }

    out_keys[*out_count] = trimmed;
    (*out_count)++;

    if (!comma) {
      break;
    }
    cursor = comma + 1;
  }

  *out_owned_buffer = owned;
  return true;
}

const char *CliResolveRollbackEnvDir(const char *first_positional,
                                     const char *second_positional) {
  if (second_positional && second_positional[0] != '\0') {
    return second_positional;
  }
  if (first_positional && first_positional[0] != '\0') {
    return first_positional;
  }
  return NULL;
}

int CliResolveJobsFromString(const char *raw_value, bool *out_valid) {
  if (out_valid) {
    *out_valid = true;
  }
  if (!raw_value || raw_value[0] == '\0' || strcmp(raw_value, "auto") == 0) {
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    int resolved = (cores > 0) ? (int)cores : 1;
    if (resolved > DEFAULT_MAX_JOBS) {
      resolved = DEFAULT_MAX_JOBS;
    }
    if (resolved < 1) {
      resolved = 1;
    }
    return resolved;
  }

  char *endptr = NULL;
  long parsed = strtol(raw_value, &endptr, 10);
  if (!endptr || *endptr != '\0' || parsed <= 0 || parsed > 256) {
    if (out_valid) {
      *out_valid = false;
    }
    return 1;
  }
  return (int)parsed;
}
