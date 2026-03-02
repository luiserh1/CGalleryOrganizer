#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cli/cli_args.h"

static const int DEFAULT_MAX_JOBS = 8;

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
  printf("  --duplicates-report Analyze duplicate groups without moving files\n");
  printf("  --duplicates-move Move duplicates into env_dir after scan\n");
  printf("  --rollback        Undo a restructuring operation using the manifest\n");
  printf("  --rename-preview  Build dedicated pattern-based rename preview\n");
  printf("  --rename-apply    Apply dedicated rename from preview id\n");
  printf("  --rename-pattern <pattern> Pattern template used for rename preview\n");
  printf("  --rename-tags-map <json_path> Per-file manual tags map (JSON)\n");
  printf("  --rename-tag-add <csv_tags> Bulk add manual tags for preview scope\n");
  printf("  --rename-tag-remove <csv_tags> Bulk remove/suppress tags for preview "
         "scope\n");
  printf("  --rename-from-preview <preview_id> Preview id required by "
         "--rename-apply\n");
  printf("  --rename-accept-auto-suffix Accept deterministic _N collision "
         "suffixing\n");
  printf("  --rename-history  List dedicated rename operation history\n");
  printf("  --rename-rollback <operation_id> Roll back dedicated rename "
         "operation\n");
  printf("\nRollback usage:\n");
  printf("  %s <scan_dir> <env_dir> --rollback\n", argv0);
  printf("  %s <env_dir> --rollback\n", argv0);
  printf("\nRename apply/history/rollback usage:\n");
  printf("  %s <env_dir> --rename-history\n", argv0);
  printf("  %s <env_dir> --rename-apply --rename-from-preview <id>\n", argv0);
  printf("  %s <env_dir> --rename-rollback <operation_id>\n", argv0);
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
