#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "duplicate_finder.h"
#include "fs_utils.h"
#include "gallery_cache.h"
#include "hash_utils.h"
#include "metadata_parser.h"
#include "ml_api.h"
#include "organizer.h"
#include "similarity_engine.h"

static int g_files_scanned = 0;
static int g_files_cached = 0;

static const char *k_group_keys[] = {"date", "camera", "format", "orientation",
                                     "resolution"};

typedef struct {
  bool exhaustive;
  bool ml_enrich;
  bool similarity_report;
  bool ml_enabled;
  int ml_files_evaluated;
  int ml_files_classified;
  int ml_files_with_text;
  int ml_files_embedded;
  int ml_failures;
} AppScanContext;

static void PrintUsage(const char *argv0) {
  printf("Usage: %s <directory_to_scan> [env_dir] [options]\n", argv0);
  printf("\n");
  printf("Options:\n");
  printf("  -h, --help        Show this help message and exit\n");
  printf("  -e, --exhaustive  Extract all metadata tags (larger cache)\n");
  printf("  --ml-enrich       Run local ML enrichment (classification + text detection)\n");
  printf("  --similarity-report Build similarity report using embeddings\n");
  printf("  --sim-threshold <0..1> Similarity threshold (default: 0.92)\n");
  printf("  --sim-topk <n>    Max neighbors per anchor (default: 5)\n");
  printf("  --cache-compress <mode> Cache compression mode: none|zstd\n");
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
  for (size_t i = 0; i < sizeof(k_group_keys) / sizeof(k_group_keys[0]); i++) {
    if (strcmp(key, k_group_keys[i]) == 0) {
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

static bool ParseGroupByKeys(const char *group_by_arg, const char **out_keys,
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

static const char *ResolveRollbackEnvDir(const char *first_positional,
                                         const char *second_positional) {
  if (second_positional && second_positional[0] != '\0') {
    return second_positional;
  }
  if (first_positional && first_positional[0] != '\0') {
    return first_positional;
  }
  return NULL;
}

static void ApplyMlResultToMetadata(ImageMetadata *md, const MlResult *ml) {
  if (!md || !ml) {
    return;
  }

  if (ml->has_classification && ml->topk_count > 0) {
    strncpy(md->mlPrimaryClass, ml->topk[0].label,
            sizeof(md->mlPrimaryClass) - 1);
    md->mlPrimaryClass[sizeof(md->mlPrimaryClass) - 1] = '\0';
    md->mlPrimaryClassConfidence = ml->topk[0].confidence;
  }

  md->mlHasText = ml->has_text_detection && ml->text_box_count > 0;
  md->mlTextBoxCount = ml->text_box_count;

  if (ml->model_id_classification[0] != '\0') {
    strncpy(md->mlModelClassification, ml->model_id_classification,
            sizeof(md->mlModelClassification) - 1);
    md->mlModelClassification[sizeof(md->mlModelClassification) - 1] = '\0';
  }

  if (ml->model_id_text_detection[0] != '\0') {
    strncpy(md->mlModelTextDetection, ml->model_id_text_detection,
            sizeof(md->mlModelTextDetection) - 1);
    md->mlModelTextDetection[sizeof(md->mlModelTextDetection) - 1] = '\0';
  }

  if (ml->provider_raw_json) {
    if (md->mlRawJson) {
      free(md->mlRawJson);
    }
    md->mlRawJson = strdup(ml->provider_raw_json);
  }

  if (ml->has_embedding && ml->embedding && ml->embedding_dim > 0) {
    char *encoded = NULL;
    if (SimilarityEncodeEmbeddingBase64(ml->embedding, ml->embedding_dim,
                                        &encoded)) {
      if (md->mlEmbeddingBase64) {
        free(md->mlEmbeddingBase64);
      }
      md->mlEmbeddingBase64 = encoded;
      md->mlEmbeddingDim = ml->embedding_dim;
    }
  }

  if (ml->model_id_embedding[0] != '\0') {
    strncpy(md->mlModelEmbedding, ml->model_id_embedding,
            sizeof(md->mlModelEmbedding) - 1);
    md->mlModelEmbedding[sizeof(md->mlModelEmbedding) - 1] = '\0';
  }
}

static void RunMlInferenceIfRequested(const char *absolute_path, ImageMetadata *md,
                                      AppScanContext *ctx) {
  if (!ctx || !md || !ctx->ml_enabled ||
      (!ctx->ml_enrich && !ctx->similarity_report)) {
    return;
  }

  bool need_classification =
      ctx->ml_enrich && md->mlModelClassification[0] == '\0';
  bool need_text = ctx->ml_enrich && md->mlModelTextDetection[0] == '\0';
  bool need_embedding =
      ctx->similarity_report &&
      (md->mlEmbeddingDim <= 0 || md->mlEmbeddingBase64 == NULL ||
       md->mlModelEmbedding[0] == '\0');

  if (!need_classification && !need_text && !need_embedding) {
    return;
  }

  ctx->ml_files_evaluated++;

  MlFeature requested[3] = {0};
  int requested_count = 0;
  if (need_classification) {
    requested[requested_count++] = ML_FEATURE_CLASSIFICATION;
  }
  if (need_text) {
    requested[requested_count++] = ML_FEATURE_TEXT_DETECTION;
  }
  if (need_embedding) {
    requested[requested_count++] = ML_FEATURE_EMBEDDING;
  }

  MlResult result = {0};

  if (!MlInferImage(absolute_path, requested, requested_count, &result)) {
    ctx->ml_failures++;
    return;
  }

  ApplyMlResultToMetadata(md, &result);
  if (result.has_classification && result.topk_count > 0) {
    ctx->ml_files_classified++;
  }
  if (result.has_text_detection && result.text_box_count > 0) {
    ctx->ml_files_with_text++;
  }
  if (result.has_embedding && result.embedding_dim > 0) {
    ctx->ml_files_embedded++;
  }

  MlFreeResult(&result);
}

static bool BuildSimilarityReportFromCache(const char *report_path,
                                           float threshold, int topk) {
  if (!report_path) {
    return false;
  }

  int key_count = 0;
  char **keys = CacheGetAllKeys(&key_count);
  if (key_count <= 0) {
    SimilarityReport empty = {0};
    if (!SimilarityBuildReport("embed-default", threshold, topk, NULL, 0,
                               &empty)) {
      CacheFreeKeys(keys, key_count);
      return false;
    }
    bool written = SimilarityWriteReportJson(report_path, &empty);
    SimilarityFreeReport(&empty);
    CacheFreeKeys(keys, key_count);
    return written;
  }
  if (!keys) {
    return false;
  }

  ImageMetadata *entries = calloc((size_t)key_count, sizeof(ImageMetadata));
  if (!entries) {
    CacheFreeKeys(keys, key_count);
    return false;
  }

  int usable_count = 0;
  for (int i = 0; i < key_count; i++) {
    ImageMetadata md = {0};
    if (!CacheGetRawEntry(keys[i], &md)) {
      continue;
    }
    if (md.mlEmbeddingDim > 0 && md.mlEmbeddingBase64 &&
        md.mlModelEmbedding[0] != '\0') {
      entries[usable_count++] = md;
    } else {
      CacheFreeMetadata(&md);
    }
  }

  SimilarityReport report = {0};
  const char *model_id =
      usable_count > 0 ? entries[0].mlModelEmbedding : "embed-default";
  bool ok = SimilarityBuildReport(model_id, threshold, topk, entries,
                                  usable_count, &report);
  if (ok) {
    ok = SimilarityWriteReportJson(report_path, &report);
  }

  SimilarityFreeReport(&report);
  for (int i = 0; i < usable_count; i++) {
    CacheFreeMetadata(&entries[i]);
  }

  free(entries);
  CacheFreeKeys(keys, key_count);
  return ok;
}

static bool ScanCallback(const char *absolute_path, void *user_data) {
  AppScanContext *ctx = (AppScanContext *)user_data;
  g_files_scanned++;

  if (FsIsSupportedMedia(absolute_path)) {
    double mod_date = 0;
    long size = 0;

    if (ExtractBasicMetadata(absolute_path, &mod_date, &size)) {
      ImageMetadata md = {0};

      bool exhaustive = (ctx != NULL) ? ctx->exhaustive : false;

      if (CacheGetValidEntry(absolute_path, mod_date, size, &md)) {
        if (md.exactHashMd5[0] == '\0' ||
            (exhaustive && md.allMetadataJson == NULL)) {
          ImageMetadata full = ExtractMetadata(absolute_path, exhaustive);
          md.width = full.width;
          md.height = full.height;
          strncpy(md.dateTaken, full.dateTaken, METADATA_MAX_STRING - 1);
          md.dateTaken[METADATA_MAX_STRING - 1] = '\0';
          strncpy(md.cameraMake, full.cameraMake, METADATA_MAX_STRING - 1);
          md.cameraMake[METADATA_MAX_STRING - 1] = '\0';
          strncpy(md.cameraModel, full.cameraModel, METADATA_MAX_STRING - 1);
          md.cameraModel[METADATA_MAX_STRING - 1] = '\0';
          md.orientation = full.orientation;
          md.hasGps = full.hasGps;
          md.gpsLatitude = full.gpsLatitude;
          md.gpsLongitude = full.gpsLongitude;
          if (full.allMetadataJson) {
            if (md.allMetadataJson) {
              free(md.allMetadataJson);
            }
            md.allMetadataJson = strdup(full.allMetadataJson);
          }
          if (md.exactHashMd5[0] == '\0') {
            ComputeFileMd5(absolute_path, md.exactHashMd5);
          }
          CacheFreeMetadata(&full);
        }

        RunMlInferenceIfRequested(absolute_path, &md, ctx);
        CacheUpdateEntry(&md);

        g_files_cached++;
        CacheFreeMetadata(&md);
        return true;
      }

      md.path = strdup(absolute_path);
      if (!md.path) {
        return true;
      }

      md.modificationDate = mod_date;
      md.fileSize = size;

      ImageMetadata extracted = ExtractMetadata(absolute_path, exhaustive);
      if (extracted.width > 0) {
        md.width = extracted.width;
      }
      if (extracted.height > 0) {
        md.height = extracted.height;
      }
      if (extracted.dateTaken[0] != '\0') {
        strncpy(md.dateTaken, extracted.dateTaken, METADATA_MAX_STRING - 1);
        md.dateTaken[METADATA_MAX_STRING - 1] = '\0';
      }
      if (extracted.cameraMake[0] != '\0') {
        strncpy(md.cameraMake, extracted.cameraMake, METADATA_MAX_STRING - 1);
        md.cameraMake[METADATA_MAX_STRING - 1] = '\0';
      }
      if (extracted.cameraModel[0] != '\0') {
        strncpy(md.cameraModel, extracted.cameraModel, METADATA_MAX_STRING - 1);
        md.cameraModel[METADATA_MAX_STRING - 1] = '\0';
      }
      md.orientation = extracted.orientation;
      md.hasGps = extracted.hasGps;
      md.gpsLatitude = extracted.gpsLatitude;
      md.gpsLongitude = extracted.gpsLongitude;

      if (extracted.allMetadataJson) {
        md.allMetadataJson = strdup(extracted.allMetadataJson);
      }

      ComputeFileMd5(absolute_path, md.exactHashMd5);
      RunMlInferenceIfRequested(absolute_path, &md, ctx);

      if (CacheUpdateEntry(&md)) {
        g_files_cached++;
      }
      CacheFreeMetadata(&md);
      CacheFreeMetadata(&extracted);
    }
  }

  return true;
}

int main(int argc, char **argv) {
  printf("CGalleryOrganizer v0.4.1\n");

  bool exhaustive = false;
  bool ml_enrich = false;
  bool similarity_report = false;
  bool organize = false;
  bool preview = false;
  bool rollback = false;
  float sim_threshold = 0.92f;
  int sim_topk = 5;
  CacheCompressionMode cache_compression_mode = CACHE_COMPRESSION_NONE;
  int cache_compression_level = 3;
  bool cache_compression_level_set = false;
  const char *target_dir = NULL;
  const char *env_dir = NULL;
  const char *group_by_arg = NULL;
  bool cache_initialized = false;
  bool ml_initialized = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      PrintUsage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-e") == 0 ||
               strcmp(argv[i], "--exhaustive") == 0) {
      exhaustive = true;
    } else if (strcmp(argv[i], "--ml-enrich") == 0) {
      ml_enrich = true;
    } else if (strcmp(argv[i], "--similarity-report") == 0) {
      similarity_report = true;
    } else if (strcmp(argv[i], "--sim-threshold") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-threshold requires a value in [0,1].\n");
        return 1;
      }
      char *endptr = NULL;
      sim_threshold = strtof(argv[++i], &endptr);
      if (!endptr || *endptr != '\0' || sim_threshold < 0.0f ||
          sim_threshold > 1.0f) {
        printf("Error: --sim-threshold must be a number in [0,1].\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--sim-topk") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-topk requires a positive integer.\n");
        return 1;
      }
      char *endptr = NULL;
      long parsed = strtol(argv[++i], &endptr, 10);
      if (!endptr || *endptr != '\0' || parsed <= 0 || parsed > 1000) {
        printf("Error: --sim-topk must be a positive integer.\n");
        return 1;
      }
      sim_topk = (int)parsed;
    } else if (strcmp(argv[i], "--cache-compress") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --cache-compress requires a mode: none|zstd.\n");
        return 1;
      }
      const char *mode = argv[++i];
      if (strcmp(mode, "none") == 0) {
        cache_compression_mode = CACHE_COMPRESSION_NONE;
      } else if (strcmp(mode, "zstd") == 0) {
        cache_compression_mode = CACHE_COMPRESSION_ZSTD;
      } else {
        printf("Error: Invalid --cache-compress mode '%s'. Allowed: none|zstd\n",
               mode);
        return 1;
      }
    } else if (strcmp(argv[i], "--cache-compress-level") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --cache-compress-level requires a value in [1,19].\n");
        return 1;
      }
      char *endptr = NULL;
      long parsed = strtol(argv[++i], &endptr, 10);
      if (!endptr || *endptr != '\0' || parsed < 1 || parsed > 19) {
        printf("Error: --cache-compress-level must be in [1,19].\n");
        return 1;
      }
      cache_compression_level = (int)parsed;
      cache_compression_level_set = true;
    } else if (strcmp(argv[i], "--organize") == 0) {
      organize = true;
    } else if (strcmp(argv[i], "--group-by") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --group-by requires a comma-separated key list.\n");
        return 1;
      }
      group_by_arg = argv[++i];
    } else if (strcmp(argv[i], "--preview") == 0) {
      preview = true;
    } else if (strcmp(argv[i], "--rollback") == 0) {
      rollback = true;
    } else if (argv[i][0] == '-') {
      printf("Error: Unknown option '%s'.\n", argv[i]);
      PrintUsage(argv[0]);
      return 1;
    } else if (target_dir == NULL) {
      target_dir = argv[i];
    } else if (env_dir == NULL) {
      env_dir = argv[i];
    } else {
      printf("Error: Unexpected positional argument '%s'.\n", argv[i]);
      PrintUsage(argv[0]);
      return 1;
    }
  }

  if (rollback && (organize || preview)) {
    printf("Error: Cannot specify --rollback with --organize or --preview.\n");
    return 1;
  }
  if (rollback && similarity_report) {
    printf("Error: Cannot specify --rollback with --similarity-report.\n");
    return 1;
  }
  if (organize && preview) {
    printf("Error: Cannot specify --organize and --preview together.\n");
    return 1;
  }
  if (similarity_report && (organize || preview)) {
    printf("Error: --similarity-report cannot be combined with --organize/--preview.\n");
    return 1;
  }
  if (cache_compression_level_set &&
      cache_compression_mode != CACHE_COMPRESSION_ZSTD) {
    printf("Error: --cache-compress-level is only valid with --cache-compress "
           "zstd.\n");
    return 1;
  }

  if (rollback) {
    const char *rollback_env = ResolveRollbackEnvDir(target_dir, env_dir);
    if (!rollback_env) {
      printf("Error: --rollback requires an environment directory.\n");
      PrintUsage(argv[0]);
      return 1;
    }

    printf("\n[*] Initiating Rollback from %s...\n", rollback_env);
    int restored = OrganizerRollback(rollback_env);
    if (restored >= 0) {
      printf("[*] Rollback complete. Restored %d files.\n", restored);
      return 0;
    }

    printf("[!] Rollback failed to execute properly.\n");
    return 1;
  }

  if (target_dir == NULL) {
    PrintUsage(argv[0]);
    return 1;
  }
  if (similarity_report && !env_dir) {
    printf("Error: --similarity-report requires an environment directory.\n");
    return 1;
  }

  char cache_dir[1024];
  char cache_path[1024];
  if (env_dir) {
    snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", env_dir);
    snprintf(cache_path, sizeof(cache_path), "%s/.cache/gallery_cache.json",
             env_dir);
  } else {
    snprintf(cache_dir, sizeof(cache_dir), ".cache");
    snprintf(cache_path, sizeof(cache_path), ".cache/gallery_cache.json");
  }

  FsMakeDirRecursive(cache_dir);
  if (!CacheSetCompression(cache_compression_mode, cache_compression_level)) {
    if (cache_compression_mode == CACHE_COMPRESSION_ZSTD) {
      printf("Error: zstd cache compression is unavailable or invalid.\n");
      printf("Hint: install zstd development package or use "
             "--cache-compress none.\n");
    } else {
      printf("Error: Invalid cache compression configuration.\n");
    }
    return 1;
  }
  if (!CacheInit(cache_path)) {
    printf("Error: Failed to initialize cache.\n");
    return 1;
  }
  cache_initialized = true;

  char models_root[1024] = {0};
  const char *models_override = getenv("CGO_MODELS_ROOT");
  if (models_override && models_override[0] != '\0') {
    strncpy(models_root, models_override, sizeof(models_root) - 1);
  } else {
    strncpy(models_root, "build/models", sizeof(models_root) - 1);
  }

  if (ml_enrich || similarity_report) {
    if (!MlInit(models_root)) {
      printf("Error: Failed to initialize ML runtime from %s.\n", models_root);
      printf("Hint: run scripts/download_models.sh first or set CGO_MODELS_ROOT.\n");
      CacheShutdown();
      return 1;
    }
    ml_initialized = true;
  }

  printf("Scanning directory: %s (Exhaustive: %s)\n", target_dir,
         exhaustive ? "ON" : "OFF");

  AppScanContext scan_ctx = {.exhaustive = exhaustive,
                             .ml_enrich = ml_enrich,
                             .similarity_report = similarity_report,
                             .ml_enabled = ml_initialized,
                             .ml_files_evaluated = 0,
                             .ml_files_classified = 0,
                             .ml_files_with_text = 0,
                             .ml_files_embedded = 0,
                             .ml_failures = 0};

  if (!FsWalkDirectory(target_dir, ScanCallback, &scan_ctx)) {
    printf("Error: Failed to walk directory '%s'.\n", target_dir);
    if (ml_initialized) {
      MlShutdown();
    }
    CacheShutdown();
    return 1;
  }

  if (!CacheSave()) {
    printf("Error: Failed to save cache.\n");
  }

  printf("Scan complete.\n");
  printf("Files scanned: %d\n", g_files_scanned);
  printf("Media files cached: %d\n", g_files_cached);
  if (ml_enrich || similarity_report) {
    printf("ML evaluated: %d\n", scan_ctx.ml_files_evaluated);
    if (ml_enrich) {
      printf("ML classified: %d\n", scan_ctx.ml_files_classified);
      printf("ML with text: %d\n", scan_ctx.ml_files_with_text);
    }
    if (similarity_report) {
      printf("ML embedded: %d\n", scan_ctx.ml_files_embedded);
    }
    printf("ML failures/skips: %d\n", scan_ctx.ml_failures);
  }
  printf("\n");

  if (similarity_report) {
    char report_path[1024] = {0};
    snprintf(report_path, sizeof(report_path), "%s/similarity_report.json",
             env_dir);
    if (!BuildSimilarityReportFromCache(report_path, sim_threshold, sim_topk)) {
      printf("Error: Failed to generate similarity report at %s.\n",
             report_path);
      if (ml_initialized) {
        MlShutdown();
      }
      CacheShutdown();
      return 1;
    }
    printf("Similarity report generated: %s\n", report_path);
  }

  if (preview || organize) {
    if (!env_dir) {
      printf("Error: --organize and --preview require an environment "
             "directory.\n");
      if (ml_initialized) {
        MlShutdown();
      }
      CacheShutdown();
      return 1;
    }

    const char *group_keys[16] = {0};
    int group_key_count = 0;
    char *group_keys_owned = NULL;
    if (!ParseGroupByKeys(group_by_arg, group_keys, 16, &group_key_count,
                          &group_keys_owned)) {
      if (ml_initialized) {
        MlShutdown();
      }
      CacheShutdown();
      return 1;
    }

    printf("\n[*] Calculating Gallery Reorganization Plan...\n");

    if (!OrganizerInit(env_dir)) {
      printf("[!] Failed to initialize organizer state.\n");
      free(group_keys_owned);
      if (ml_initialized) {
        MlShutdown();
      }
      CacheShutdown();
      return 1;
    }

    OrganizerPlan *plan =
        OrganizerComputePlan(env_dir, group_keys, group_key_count);
    if (!plan) {
      printf("[!] Failed to compute reorganization plan.\n");
    } else {
      if (preview) {
        OrganizerPrintPlan(plan);
        printf("\n[*] Preview mode: No files were moved.\n");
      } else {
        OrganizerPrintPlan(plan);
        printf("\nExecute plan? [y/N]: ");
        fflush(stdout);
        char buf[16];
        if (fgets(buf, sizeof(buf), stdin)) {
          if (buf[0] == 'y' || buf[0] == 'Y') {
            OrganizerExecutePlan(plan);
          } else {
            printf("[*] Operation cancelled.\n");
          }
        }
      }
      OrganizerFreePlan(plan);
    }

    OrganizerShutdown();
    free(group_keys_owned);
  } else if (!similarity_report) {
    DuplicateReport rep = FindExactDuplicates();
    int moved_count = 0;

    if (rep.group_count > 0) {
      if (env_dir) {
        printf(
            "Found %d groups of exact duplicates. Moving copies to '%s'...\n",
            rep.group_count, env_dir);
        for (int i = 0; i < rep.group_count; i++) {
          for (int j = 0; j < rep.groups[i].duplicate_count; j++) {
            char moved_path[1024] = {0};
            if (FsMoveFile(rep.groups[i].duplicate_paths[j], env_dir,
                           moved_path, sizeof(moved_path))) {
              printf("  Moved: %s -> %s\n", rep.groups[i].duplicate_paths[j],
                     moved_path);
              moved_count++;
            } else {
              printf("  Failed to move: %s\n",
                     rep.groups[i].duplicate_paths[j]);
            }
          }
        }
        printf("Successfully moved %d duplicate files.\n", moved_count);
      } else {
        printf("Found %d groups of exact duplicates (dry-run):\n",
               rep.group_count);
        for (int i = 0; i < rep.group_count; i++) {
          printf("  Group %d (Original: %s):\n", i + 1,
                 rep.groups[i].original_path);
          for (int j = 0; j < rep.groups[i].duplicate_count; j++) {
            printf("    Duplicate: %s\n", rep.groups[i].duplicate_paths[j]);
          }
        }
        printf("\nTo automatically move these duplicates, provide a target "
               "directory as the second argument.\n");
      }
    } else {
      printf("No exact duplicates found.\n");
    }
    FreeDuplicateReport(&rep);
  }

  if (ml_initialized) {
    MlShutdown();
  }

  if (cache_initialized) {
    CacheShutdown();
  }

  return 0;
}
