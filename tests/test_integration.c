#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "cJSON.h"
#include "duplicate_finder.h"
#include "fs_utils.h"
#include "gallery_cache.h"
#include "hash_utils.h"
#include "metadata_parser.h"
#include "core/cache_codec.h"
#include "test_framework.h"

static int RunCommandCapture(const char *cmd, char *output, size_t output_size) {
  if (!cmd || !output || output_size == 0) {
    return -1;
  }

  output[0] = '\0';
  FILE *pipe = popen(cmd, "r");
  if (!pipe) {
    return -1;
  }

  size_t used = 0;
  while (!feof(pipe) && used < output_size - 1) {
    size_t n = fread(output + used, 1, output_size - 1 - used, pipe);
    if (n == 0) {
      break;
    }
    used += n;
  }
  output[used] = '\0';

  int status = pclose(pipe);
  if (status == -1) {
    return -1;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return -1;
}

static bool LoadFileText(const char *path, char **out_text) {
  if (!path || !out_text) {
    return false;
  }
  *out_text = NULL;

  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return false;
  }
  rewind(f);

  char *buf = malloc((size_t)size + 1);
  if (!buf) {
    fclose(f);
    return false;
  }
  size_t n = fread(buf, 1, (size_t)size, f);
  fclose(f);
  buf[n] = '\0';
  *out_text = buf;
  return true;
}

static bool ReportsEquivalentIgnoringTimestamp(const char *left_path,
                                               const char *right_path) {
  char *left_text = NULL;
  char *right_text = NULL;
  if (!LoadFileText(left_path, &left_text) || !LoadFileText(right_path, &right_text)) {
    free(left_text);
    free(right_text);
    return false;
  }

  cJSON *left = cJSON_Parse(left_text);
  cJSON *right = cJSON_Parse(right_text);
  free(left_text);
  free(right_text);
  if (!left || !right) {
    cJSON_Delete(left);
    cJSON_Delete(right);
    return false;
  }

  cJSON_DeleteItemFromObjectCaseSensitive(left, "generatedAt");
  cJSON_DeleteItemFromObjectCaseSensitive(right, "generatedAt");

  char *left_norm = cJSON_PrintUnformatted(left);
  char *right_norm = cJSON_PrintUnformatted(right);
  cJSON_Delete(left);
  cJSON_Delete(right);
  if (!left_norm || !right_norm) {
    free(left_norm);
    free(right_norm);
    return false;
  }

  bool same = strcmp(left_norm, right_norm) == 0;
  free(left_norm);
  free(right_norm);
  return same;
}

static bool WriteRollbackManifest(const char *env_dir) {
  char orig_dir[1024];
  char new_dir[1024];
  snprintf(orig_dir, sizeof(orig_dir), "%s/orig", env_dir);
  snprintf(new_dir, sizeof(new_dir), "%s/new", env_dir);

  if (!FsMakeDirRecursive(orig_dir) || !FsMakeDirRecursive(new_dir)) {
    return false;
  }

  char orig_file[1024];
  char new_file[1024];
  snprintf(orig_file, sizeof(orig_file), "%s/file.jpg", orig_dir);
  snprintf(new_file, sizeof(new_file), "%s/file.jpg", new_dir);

  FILE *f = fopen(new_file, "w");
  if (!f) {
    return false;
  }
  fputs("dummy", f);
  fclose(f);

  f = fopen(orig_file, "w");
  if (!f) {
    return false;
  }
  fputs("dummy", f);
  fclose(f);

  char abs_orig[1024] = {0};
  char abs_new[1024] = {0};
  if (!FsGetAbsolutePath(orig_file, abs_orig, sizeof(abs_orig)) ||
      !FsGetAbsolutePath(new_file, abs_new, sizeof(abs_new))) {
    return false;
  }

  remove(orig_file);

  char manifest_path[1024];
  snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", env_dir);
  f = fopen(manifest_path, "w");
  if (!f) {
    return false;
  }
  fprintf(f, "[{\"original\":\"%s\",\"new\":\"%s\"}]", abs_orig, abs_new);
  fclose(f);
  return true;
}

static bool WriteBootstrapModels(const char *models_dir) {
  if (!FsMakeDirRecursive(models_dir)) {
    return false;
  }

  char clf_path[1024];
  char text_path[1024];
  char embed_path[1024];
  snprintf(clf_path, sizeof(clf_path), "%s/clf-default.onnx", models_dir);
  snprintf(text_path, sizeof(text_path), "%s/text-default.onnx", models_dir);
  snprintf(embed_path, sizeof(embed_path), "%s/embed-default.onnx", models_dir);

  FILE *f = fopen(clf_path, "w");
  if (!f) {
    return false;
  }
  fputs("dummy-clf", f);
  fclose(f);

  f = fopen(text_path, "w");
  if (!f) {
    return false;
  }
  fputs("dummy-text", f);
  fclose(f);

  f = fopen(embed_path, "w");
  if (!f) {
    return false;
  }
  fputs("dummy-embed", f);
  fclose(f);

  return true;
}

void test_metadata_png_support(void) {
  const char *filepath = "tests/assets/png/sample_no_exif.png";
  double mod_date;
  long size;
  ASSERT_TRUE(ExtractBasicMetadata(filepath, &mod_date, &size));

  ImageMetadata metadata = ExtractMetadata(filepath, false);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_png_with_exif(void) {
  const char *filepath = "tests/assets/png/sample_exif.png";
  ImageMetadata metadata = ExtractMetadata(filepath, false);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
  ASSERT_TRUE(metadata.cameraMake[0] != '\0');
}

void test_metadata_gif_support(void) {
  const char *filepath = "tests/assets/gif/sample_no_exif.gif";
  double mod_date;
  long size;
  ASSERT_TRUE(ExtractBasicMetadata(filepath, &mod_date, &size));

  ImageMetadata metadata = ExtractMetadata(filepath, false);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_bmp_support(void) {
  const char *filepath = "tests/assets/bmp/sample_no_exif.bmp";
  double mod_date;
  long size;
  ASSERT_TRUE(ExtractBasicMetadata(filepath, &mod_date, &size));

  ImageMetadata metadata = ExtractMetadata(filepath, false);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_webp_support(void) {
  const char *filepath = "tests/assets/webp/sample_no_exif.webp";
  double mod_date;
  long size;
  ASSERT_TRUE(ExtractBasicMetadata(filepath, &mod_date, &size));

  ImageMetadata metadata = ExtractMetadata(filepath, false);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_webp_with_exif(void) {
  const char *filepath = "tests/assets/webp/sample_exif.webp";
  ImageMetadata metadata = ExtractMetadata(filepath, false);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
  ASSERT_TRUE(metadata.cameraMake[0] != '\0');
}

void test_metadata_heic_dimensions(void) {
  const char *filepath = "tests/assets/heic/sample_no_exif.heic";
  double mod_date;
  long size;
  ASSERT_TRUE(ExtractBasicMetadata(filepath, &mod_date, &size));

  ImageMetadata metadata = ExtractMetadata(filepath, false);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_heic_with_exif(void) {
  const char *filepath = "tests/assets/heic/sample_exif.heic";
  ImageMetadata metadata = ExtractMetadata(filepath, false);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
  // Exiv2's strict ISOBMFF parser ignores our raw exiftool injection for this
  // asset
  ASSERT_FALSE(metadata.hasGps);
}

void test_metadata_jpeg_with_exif(void) {
  const char *filepath = "tests/assets/jpg/sample_exif.jpg";

  ImageMetadata metadata = ExtractMetadata(filepath, false);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_jpeg_no_exif(void) {
  const char *filepath = "tests/assets/jpg/sample_no_exif.jpg";

  ImageMetadata metadata = ExtractMetadata(filepath, false);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_png_fake_extension(void) {
  const char *filepath = "tests/assets/png/fake.png";
  double mod_date;
  long size;
  ASSERT_TRUE(ExtractBasicMetadata(filepath, &mod_date, &size));

  ImageMetadata metadata = ExtractMetadata(filepath, false);

  // Fake extension: Exiv2 uses magic bytes, correctly identifying this as a
  // 240x160 JPEG despite the .png extension.
  ASSERT_EQ(240, metadata.width);
  ASSERT_EQ(160, metadata.height);
}

void test_metadata_bmp_truncated(void) {
  const char *filepath = "tests/assets/bmp/truncated.bmp";
  ImageMetadata metadata = ExtractMetadata(filepath, false);

  // Truncated file: Exiv2 robustly reads the header without crashing and
  // returns the intended dimensions.
  ASSERT_EQ(256, metadata.width);
  ASSERT_EQ(151, metadata.height);
}

void test_metadata_jpeg_deep_exif(void) {
  const char *filepath = "tests/assets/jpg/sample_deep_exif.jpg";
  ImageMetadata metadata = ExtractMetadata(filepath, false);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
  ASSERT_TRUE(metadata.hasGps);

  // Verify Eiffel Tower coordinate extraction (48.8584 N, 2.2945 E)
  double lat_diff = metadata.gpsLatitude - 48.8584;
  if (lat_diff < 0)
    lat_diff = -lat_diff;
  ASSERT_TRUE(lat_diff < 0.001);

  double lon_diff = metadata.gpsLongitude - 2.2945;
  if (lon_diff < 0)
    lon_diff = -lon_diff;
  ASSERT_TRUE(lon_diff < 0.001);
}

void test_unified_metadata_dispatcher(void) {
  const char *png_path = "tests/assets/png/sample_no_exif.png";
  const char *gif_path = "tests/assets/gif/sample_no_exif.gif";
  const char *bmp_path = "tests/assets/bmp/sample_no_exif.bmp";

  ImageMetadata png = ExtractMetadata(png_path, false);
  ImageMetadata gif = ExtractMetadata(gif_path, false);
  ImageMetadata bmp = ExtractMetadata(bmp_path, false);

  ASSERT_TRUE(png.width > 0);
  ASSERT_TRUE(gif.width > 0);
  ASSERT_TRUE(bmp.width > 0);
}

static bool ScanCallbackIntegrations(const char *absolute_path,
                                     void *user_data) {
  (void)user_data;
  double mod_date;
  long size;

  if (ExtractBasicMetadata(absolute_path, &mod_date, &size)) {
    ImageMetadata md = {0};

    if (CacheGetValidEntry(absolute_path, mod_date, size, &md)) {
      if (md.exactHashMd5[0] == '\0') {
        ComputeFileMd5(absolute_path, md.exactHashMd5);
        CacheUpdateEntry(&md);
      }
      return true;
    }

    md.path = strdup(absolute_path);
    md.modificationDate = mod_date;
    md.fileSize = size;

    if (FsIsSupportedMedia(absolute_path)) {
      ImageMetadata parsed = ExtractMetadata(absolute_path, false);
      md.width = parsed.width;
      md.height = parsed.height;
      strncpy(md.dateTaken, parsed.dateTaken, METADATA_MAX_STRING - 1);
      strncpy(md.cameraMake, parsed.cameraMake, METADATA_MAX_STRING - 1);
      strncpy(md.cameraModel, parsed.cameraModel, METADATA_MAX_STRING - 1);
      md.orientation = parsed.orientation;
      md.hasGps = parsed.hasGps;
      md.gpsLatitude = parsed.gpsLatitude;
      md.gpsLongitude = parsed.gpsLongitude;
      CacheFreeMetadata(&parsed);
    }

    ComputeFileMd5(absolute_path, md.exactHashMd5);
    CacheUpdateEntry(&md);
    CacheFreeMetadata(&md);
  }
  return true;
}

void test_exhaustive_metadata_capture(void) {
  const char *filepath = "tests/assets/jpg/sample_exif.jpg";

  // 1. Regular extraction should have NULL allMetadataJson
  ImageMetadata regular = ExtractMetadata(filepath, false);
  ASSERT_TRUE(regular.allMetadataJson == NULL);
  CacheFreeMetadata(&regular);

  // 2. Exhaustive extraction should NOT be NULL
  ImageMetadata exhaustive = ExtractMetadata(filepath, true);
  ASSERT_TRUE(exhaustive.allMetadataJson != NULL);

  // 3. Verify it contains expected JSON-like structure
  ASSERT_TRUE(strstr(exhaustive.allMetadataJson, "Exif.Image.Artist") != NULL);
  ASSERT_TRUE(strstr(exhaustive.allMetadataJson, "\"Alexandre Dulaunoy\"") !=
              NULL);

  CacheFreeMetadata(&exhaustive);
}

void test_duplicate_integration(void) {
  system("rm -f build/test_dup_integ_cache.json");
  ASSERT_TRUE(CacheInit("build/test_dup_integ_cache.json"));

  // 1. Scan the whole duplicate asset tree
  ASSERT_TRUE(FsWalkDirectory("./tests/assets/duplicates",
                              ScanCallbackIntegrations, NULL));

  // 2. Compute exact duplicates directly from cache
  DuplicateReport rep = FindExactDuplicates();

  // Expect exactly ONE true duplicate group (bird.JPG and bird_duplicate.JPG)
  ASSERT_EQ(1, rep.group_count);
  ASSERT_EQ(1, rep.groups[0].duplicate_count);

  char abs_orig1[1024] = {0};
  char abs_orig2[1024] = {0};
  FsGetAbsolutePath("./tests/assets/duplicates/bird.JPG", abs_orig1,
                    sizeof(abs_orig1));
  FsGetAbsolutePath("./tests/assets/duplicates/bird_duplicate.JPG", abs_orig2,
                    sizeof(abs_orig2));

  bool is_original = strcmp(rep.groups[0].original_path, abs_orig1) == 0 ||
                     strcmp(rep.groups[0].original_path, abs_orig2) == 0;
  ASSERT_TRUE(is_original);

  FreeDuplicateReport(&rep);
  CacheShutdown();
  system("rm -f build/test_dup_integ_cache.json");
}

void test_cli_exhaustive_flag_and_cache_enrichment(void) {
  system("rm -rf build/test_cli_exhaustive_src build/test_cli_exhaustive_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_exhaustive_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_exhaustive_env"));
  ASSERT_EQ(0, system("cp tests/assets/jpg/sample_exif.jpg "
                      "build/test_cli_exhaustive_src/a.jpg"));

  char output[4096];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_exhaustive_src "
      "build/test_cli_exhaustive_env --exhaustive 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Exhaustive: ON") != NULL);

  FILE *f = fopen("build/test_cli_exhaustive_env/.cache/gallery_cache.json", "r");
  ASSERT_TRUE(f != NULL);
  char cache_buf[8192] = {0};
  size_t bytes = fread(cache_buf, 1, sizeof(cache_buf) - 1, f);
  cache_buf[bytes] = '\0';
  fclose(f);
  ASSERT_TRUE(strstr(cache_buf, "\"allMetadataJson\"") != NULL);

  system("rm -rf build/test_cli_exhaustive_src build/test_cli_exhaustive_env");
}

void test_cli_group_by_empty_rejected(void) {
  system("rm -rf build/test_cli_groupby_empty_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_groupby_empty_env"));
  char output[4096];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/jpg "
      "build/test_cli_groupby_empty_env --preview --group-by '' 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "cannot include empty keys") != NULL);
  system("rm -rf build/test_cli_groupby_empty_env");
}

void test_cli_group_by_invalid_rejected(void) {
  system("rm -rf build/test_cli_groupby_invalid_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_groupby_invalid_env"));
  char output[4096];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/jpg "
      "build/test_cli_groupby_invalid_env --preview --group-by foo 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "Invalid --group-by key") != NULL);
  system("rm -rf build/test_cli_groupby_invalid_env");
}

void test_cli_rollback_single_positional(void) {
  const char *env_dir = "build/test_cli_rollback_single";
  system("rm -rf build/test_cli_rollback_single");
  ASSERT_TRUE(WriteRollbackManifest(env_dir));

  char output[4096];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rollback_single --rollback "
      "2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Rollback complete") != NULL);

  FILE *f = fopen("build/test_cli_rollback_single/orig/file.jpg", "r");
  ASSERT_TRUE(f != NULL);
  if (f) {
    fclose(f);
  }

  system("rm -rf build/test_cli_rollback_single");
}

void test_cli_rollback_legacy_two_positional(void) {
  const char *env_dir = "build/test_cli_rollback_legacy";
  system("rm -rf build/test_cli_rollback_legacy");
  ASSERT_TRUE(WriteRollbackManifest(env_dir));

  char output[4096];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer /nonexistent/path "
      "build/test_cli_rollback_legacy --rollback 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Rollback complete") != NULL);

  FILE *f = fopen("build/test_cli_rollback_legacy/orig/file.jpg", "r");
  ASSERT_TRUE(f != NULL);
  if (f) {
    fclose(f);
  }

  system("rm -rf build/test_cli_rollback_legacy");
}

void test_cli_ml_enrich_missing_models(void) {
  system("rm -rf build/test_cli_ml_missing_src build/test_cli_ml_missing_env "
         "build/test_cli_ml_missing_models");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_ml_missing_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_ml_missing_env"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_ml_missing_src/a.png"));

  char output[4096];
  int code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_ml_missing_models "
      "./build/bin/gallery_organizer build/test_cli_ml_missing_src "
      "build/test_cli_ml_missing_env --ml-enrich 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "Failed to initialize ML runtime") != NULL);

  system("rm -rf build/test_cli_ml_missing_src build/test_cli_ml_missing_env "
         "build/test_cli_ml_missing_models");
}

void test_cli_ml_enrich_updates_cache(void) {
  system("rm -rf build/test_cli_ml_src build/test_cli_ml_env build/test_cli_models");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_ml_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_ml_env"));
  ASSERT_TRUE(WriteBootstrapModels("build/test_cli_models"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_ml_src/a.png"));

  char output[4096];
  int code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_models "
      "./build/bin/gallery_organizer build/test_cli_ml_src "
      "build/test_cli_ml_env --ml-enrich 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "ML evaluated:") != NULL);

  FILE *f = fopen("build/test_cli_ml_env/.cache/gallery_cache.json", "r");
  ASSERT_TRUE(f != NULL);
  char cache_buf[8192] = {0};
  size_t bytes = fread(cache_buf, 1, sizeof(cache_buf) - 1, f);
  cache_buf[bytes] = '\0';
  fclose(f);
  ASSERT_TRUE(strstr(cache_buf, "\"mlPrimaryClass\"") != NULL);
  ASSERT_TRUE(strstr(cache_buf, "\"mlModelClassification\"") != NULL);

  system("rm -rf build/test_cli_ml_src build/test_cli_ml_env build/test_cli_models");
}

void test_cli_similarity_report_generates_json(void) {
  system("rm -rf build/test_cli_sim_src build/test_cli_sim_env build/test_cli_sim_models");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_sim_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_sim_env"));
  ASSERT_TRUE(WriteBootstrapModels("build/test_cli_sim_models"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png build/test_cli_sim_src/a.png"));
  ASSERT_EQ(0, system("cp tests/assets/jpg/sample_exif.jpg build/test_cli_sim_src/b.jpg"));

  char output[4096];
  int code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_sim_models "
      "./build/bin/gallery_organizer build/test_cli_sim_src "
      "build/test_cli_sim_env --similarity-report --sim-threshold 0.1 --sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Similarity report generated:") != NULL);

  FILE *f = fopen("build/test_cli_sim_env/similarity_report.json", "r");
  ASSERT_TRUE(f != NULL);
  char report_buf[8192] = {0};
  size_t bytes = fread(report_buf, 1, sizeof(report_buf) - 1, f);
  report_buf[bytes] = '\0';
  fclose(f);
  ASSERT_TRUE(strstr(report_buf, "\"groupCount\"") != NULL);
  ASSERT_TRUE(strstr(report_buf, "\"groups\"") != NULL);

  system("rm -rf build/test_cli_sim_src build/test_cli_sim_env build/test_cli_sim_models");
}

void test_cli_similarity_report_requires_env(void) {
  char output[2048];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/png --similarity-report 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "requires an environment directory") != NULL);
}

void test_cli_similarity_memory_mode_validation(void) {
  char output[2048];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/png build/test_cli_sim_mode "
      "--similarity-report --sim-memory-mode invalid 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "--sim-memory-mode must be eager|chunked") != NULL);
}

void test_cli_jobs_validation(void) {
  char output[2048];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/png build/test_cli_jobs_env "
      "--jobs 0 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output,
                     "--jobs/CGO_JOBS must be 'auto' or a positive integer") !=
              NULL);
}

void test_cli_jobs_env_and_override(void) {
  system("rm -rf build/test_cli_jobs_src build/test_cli_jobs_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_jobs_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_jobs_env"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_jobs_src/a.png"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "CGO_JOBS=3 ./build/bin/gallery_organizer build/test_cli_jobs_src "
      "build/test_cli_jobs_env 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Jobs: 3") != NULL);

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "CGO_JOBS=3 ./build/bin/gallery_organizer build/test_cli_jobs_src "
      "build/test_cli_jobs_env --jobs 1 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Jobs: 1") != NULL);

  system("rm -rf build/test_cli_jobs_src build/test_cli_jobs_env");
}

void test_cli_similarity_memory_mode_parity(void) {
  system("rm -rf build/test_cli_sim_mode_src build/test_cli_sim_mode_env "
         "build/test_cli_sim_mode_models");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_sim_mode_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_sim_mode_env"));
  ASSERT_TRUE(WriteBootstrapModels("build/test_cli_sim_mode_models"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_sim_mode_src/a.png"));
  ASSERT_EQ(0, system("cp tests/assets/jpg/sample_exif.jpg "
                      "build/test_cli_sim_mode_src/b.jpg"));
  ASSERT_EQ(0, system("cp tests/assets/jpg/sample_no_exif.jpg "
                      "build/test_cli_sim_mode_src/c.jpg"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_sim_mode_models "
      "./build/bin/gallery_organizer build/test_cli_sim_mode_src "
      "build/test_cli_sim_mode_env --similarity-report --sim-memory-mode eager "
      "--sim-threshold 0.1 --sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_EQ(0, system("cp build/test_cli_sim_mode_env/similarity_report.json "
                      "build/test_cli_sim_mode_env/eager_report.json"));

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_sim_mode_models "
      "./build/bin/gallery_organizer build/test_cli_sim_mode_src "
      "build/test_cli_sim_mode_env --similarity-report --sim-memory-mode chunked "
      "--sim-threshold 0.1 --sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);

  ASSERT_TRUE(ReportsEquivalentIgnoringTimestamp(
      "build/test_cli_sim_mode_env/eager_report.json",
      "build/test_cli_sim_mode_env/similarity_report.json"));

  system("rm -rf build/test_cli_sim_mode_src build/test_cli_sim_mode_env "
         "build/test_cli_sim_mode_models");
}

void test_cli_cache_compress_flag_validation(void) {
  char output[2048];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/png "
      "build/test_cli_cache_mode --cache-compress bogus 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "Invalid --cache-compress mode") != NULL);

  code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/png "
      "build/test_cli_cache_mode --cache-compress-level 3 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "only valid with --cache-compress zstd|auto") !=
              NULL);
}

void test_cli_cache_compress_zstd_roundtrip_or_skip(void) {
  if (!CacheCodecIsAvailable(CACHE_COMPRESSION_ZSTD)) {
    return;
  }

  system("rm -rf build/test_cli_cache_zstd_src build/test_cli_cache_zstd_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_cache_zstd_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_cache_zstd_env"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_cache_zstd_src/a.png"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_cache_zstd_src "
      "build/test_cli_cache_zstd_env --cache-compress zstd "
      "--cache-compress-level 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);

  FILE *f = fopen("build/test_cli_cache_zstd_env/.cache/gallery_cache.json", "rb");
  ASSERT_TRUE(f != NULL);
  char magic[9] = {0};
  ASSERT_TRUE(fread(magic, 1, 8, f) == 8);
  fclose(f);
  ASSERT_TRUE(memcmp(magic, CACHE_CODEC_MAGIC, 8) == 0);

  code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_cache_zstd_src "
      "build/test_cli_cache_zstd_env 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);

  system("rm -rf build/test_cli_cache_zstd_src build/test_cli_cache_zstd_env");
}

void test_cli_similarity_incremental_toggle_behavior(void) {
  system("rm -rf build/test_cli_sim_inc_src build/test_cli_sim_inc_env "
         "build/test_cli_sim_inc_models");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_sim_inc_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_sim_inc_env"));
  ASSERT_TRUE(WriteBootstrapModels("build/test_cli_sim_inc_models"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_sim_inc_src/a.png"));
  ASSERT_EQ(0, system("cp tests/assets/jpg/sample_exif.jpg "
                      "build/test_cli_sim_inc_src/b.jpg"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_sim_inc_models "
      "./build/bin/gallery_organizer build/test_cli_sim_inc_src "
      "build/test_cli_sim_inc_env --similarity-report --sim-threshold 0.1 "
      "--sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "ML evaluated: 2") != NULL);

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_sim_inc_models "
      "./build/bin/gallery_organizer build/test_cli_sim_inc_src "
      "build/test_cli_sim_inc_env --similarity-report --sim-threshold 0.1 "
      "--sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "ML evaluated: 0") != NULL);

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_sim_inc_models "
      "./build/bin/gallery_organizer build/test_cli_sim_inc_src "
      "build/test_cli_sim_inc_env --similarity-report --sim-incremental off "
      "--sim-threshold 0.1 --sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "ML evaluated: 2") != NULL);

  system("rm -rf build/test_cli_sim_inc_src build/test_cli_sim_inc_env "
         "build/test_cli_sim_inc_models");
}

void test_cli_cache_compress_auto_threshold_behavior(void) {
  if (!CacheCodecIsAvailable(CACHE_COMPRESSION_ZSTD)) {
    return;
  }

  system("rm -rf build/test_cli_cache_auto_src build/test_cli_cache_auto_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_cache_auto_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_cache_auto_env"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_cache_auto_src/a.png"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "CGO_CACHE_AUTO_THRESHOLD_BYTES=999999999 "
      "./build/bin/gallery_organizer build/test_cli_cache_auto_src "
      "build/test_cli_cache_auto_env --cache-compress auto 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);

  FILE *f = fopen("build/test_cli_cache_auto_env/.cache/gallery_cache.json", "rb");
  ASSERT_TRUE(f != NULL);
  char head[8] = {0};
  ASSERT_TRUE(fread(head, 1, sizeof(head), f) == sizeof(head));
  fclose(f);
  ASSERT_TRUE(memcmp(head, CACHE_CODEC_MAGIC, sizeof(head)) != 0);

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "CGO_CACHE_AUTO_THRESHOLD_BYTES=1 "
      "./build/bin/gallery_organizer build/test_cli_cache_auto_src "
      "build/test_cli_cache_auto_env --cache-compress auto 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);

  f = fopen("build/test_cli_cache_auto_env/.cache/gallery_cache.json", "rb");
  ASSERT_TRUE(f != NULL);
  char magic[9] = {0};
  ASSERT_TRUE(fread(magic, 1, 8, f) == 8);
  fclose(f);
  ASSERT_TRUE(memcmp(magic, CACHE_CODEC_MAGIC, 8) == 0);

  system("rm -rf build/test_cli_cache_auto_src build/test_cli_cache_auto_env");
}

void register_integration_tests(void) {
  register_test("test_metadata_png_support", test_metadata_png_support,
                "integration");
  register_test("test_metadata_gif_support", test_metadata_gif_support,
                "integration");
  register_test("test_metadata_bmp_support", test_metadata_bmp_support,
                "integration");
  register_test("test_metadata_webp_support", test_metadata_webp_support,
                "integration");
  register_test("test_metadata_webp_with_exif", test_metadata_webp_with_exif,
                "integration");
  register_test("test_metadata_heic_dimensions", test_metadata_heic_dimensions,
                "integration");
  register_test("test_metadata_heic_with_exif", test_metadata_heic_with_exif,
                "integration");
  register_test("test_metadata_jpeg_with_exif", test_metadata_jpeg_with_exif,
                "integration");
  register_test("test_metadata_jpeg_no_exif", test_metadata_jpeg_no_exif,
                "integration");
  register_test("test_metadata_jpeg_deep_exif", test_metadata_jpeg_deep_exif,
                "integration");
  register_test("test_metadata_png_fake_extension",
                test_metadata_png_fake_extension, "integration");
  register_test("test_metadata_bmp_truncated", test_metadata_bmp_truncated,
                "integration");
  register_test("test_unified_metadata_dispatcher",
                test_unified_metadata_dispatcher, "integration");
  register_test("test_exhaustive_metadata_capture",
                test_exhaustive_metadata_capture, "integration");
  register_test("test_duplicate_integration", test_duplicate_integration,
                "integration");
  register_test("test_cli_exhaustive_flag_and_cache_enrichment",
                test_cli_exhaustive_flag_and_cache_enrichment, "integration");
  register_test("test_cli_group_by_empty_rejected",
                test_cli_group_by_empty_rejected, "integration");
  register_test("test_cli_group_by_invalid_rejected",
                test_cli_group_by_invalid_rejected, "integration");
  register_test("test_cli_rollback_single_positional",
                test_cli_rollback_single_positional, "integration");
  register_test("test_cli_rollback_legacy_two_positional",
                test_cli_rollback_legacy_two_positional, "integration");
  register_test("test_cli_ml_enrich_missing_models",
                test_cli_ml_enrich_missing_models, "integration");
  register_test("test_cli_ml_enrich_updates_cache",
                test_cli_ml_enrich_updates_cache, "integration");
  register_test("test_cli_similarity_report_generates_json",
                test_cli_similarity_report_generates_json, "integration");
  register_test("test_cli_similarity_report_requires_env",
                test_cli_similarity_report_requires_env, "integration");
  register_test("test_cli_similarity_memory_mode_validation",
                test_cli_similarity_memory_mode_validation, "integration");
  register_test("test_cli_jobs_validation", test_cli_jobs_validation,
                "integration");
  register_test("test_cli_jobs_env_and_override",
                test_cli_jobs_env_and_override, "integration");
  register_test("test_cli_similarity_memory_mode_parity",
                test_cli_similarity_memory_mode_parity, "integration");
  register_test("test_cli_cache_compress_flag_validation",
                test_cli_cache_compress_flag_validation, "integration");
  register_test("test_cli_cache_compress_zstd_roundtrip_or_skip",
                test_cli_cache_compress_zstd_roundtrip_or_skip,
                "integration");
  register_test("test_cli_similarity_incremental_toggle_behavior",
                test_cli_similarity_incremental_toggle_behavior,
                "integration");
  register_test("test_cli_cache_compress_auto_threshold_behavior",
                test_cli_cache_compress_auto_threshold_behavior,
                "integration");
}
