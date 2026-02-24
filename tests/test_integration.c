#include <stdio.h>
#include <stdlib.h>

#include "test_framework.h"

#include "bmp_parser.h"
#include "exif_parser.h"
#include "gif_parser.h"
#include "heic_parser.h"
#include "metadata_parser.h"
#include "png_parser.h"
#include "webp_parser.h"

#include "duplicate_finder.h"
#include "fs_utils.h"
#include "hash_utils.h"

void test_metadata_png_dimensions(void) {
  const char *filepath = "tests/assets/png/sample.png";

  ImageMetadata metadata = ExtractMetadata(filepath);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_png_with_exif(void) {
  const char *filepath = "tests/assets/png/sample_exif.png";
  ImageMetadata metadata = ExtractMetadata(filepath);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
  ASSERT_TRUE(metadata.cameraMake[0] != '\0');
}

void test_metadata_gif_dimensions(void) {
  const char *filepath = "tests/assets/gif/sample.gif";

  ImageMetadata metadata = ExtractMetadata(filepath);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_bmp_dimensions(void) {
  const char *filepath = "tests/assets/bmp/sample.bmp";

  ImageMetadata metadata = ExtractMetadata(filepath);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_webp_dimensions(void) {
  const char *filepath = "tests/assets/webp/sample.webp";

  ImageMetadata metadata = ExtractMetadata(filepath);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_webp_with_exif(void) {
  const char *filepath = "tests/assets/webp/sample_exif.webp";
  ImageMetadata metadata = ExtractMetadata(filepath);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
  ASSERT_TRUE(metadata.cameraMake[0] != '\0');
}

void test_metadata_heic_dimensions(void) {
  const char *filepath = "tests/assets/heic/sample.heic";

  ImageMetadata metadata = ExtractMetadata(filepath);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_heic_with_exif(void) {
  const char *filepath = "tests/assets/heic/sample_exif.heic";
  ImageMetadata metadata = ExtractMetadata(filepath);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
  ASSERT_TRUE(metadata.cameraMake[0] != '\0');
}

void test_metadata_jpeg_with_exif(void) {
  const char *filepath = "tests/assets/jpg/sample_exif.jpg";

  ImageMetadata metadata = ExtractMetadata(filepath);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_metadata_jpeg_no_exif(void) {
  const char *filepath = "tests/assets/jpg/sample_no_exif.jpg";

  ImageMetadata metadata = ExtractMetadata(filepath);

  ASSERT_TRUE(metadata.width > 0);
  ASSERT_TRUE(metadata.height > 0);
}

void test_unified_metadata_dispatcher(void) {
  const char *png_path = "tests/assets/png/sample.png";
  const char *gif_path = "tests/assets/gif/sample.gif";
  const char *bmp_path = "tests/assets/bmp/sample.bmp";

  ImageMetadata png = ExtractMetadata(png_path);
  ImageMetadata gif = ExtractMetadata(gif_path);
  ImageMetadata bmp = ExtractMetadata(bmp_path);

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
      ImageMetadata parsed = ExtractMetadata(absolute_path);
      md.width = parsed.width;
      md.height = parsed.height;
      strncpy(md.dateTaken, parsed.dateTaken, METADATA_MAX_STRING - 1);
      strncpy(md.cameraMake, parsed.cameraMake, METADATA_MAX_STRING - 1);
      strncpy(md.cameraModel, parsed.cameraModel, METADATA_MAX_STRING - 1);
      md.orientation = parsed.orientation;
      md.hasGps = parsed.hasGps;
      md.gpsLatitude = parsed.gpsLatitude;
      md.gpsLongitude = parsed.gpsLongitude;
    }

    ComputeFileMd5(absolute_path, md.exactHashMd5);
    CacheUpdateEntry(&md);
    free(md.path);
  }
  return true;
}

void test_duplicate_integration(void) {
  system("rm -f test_dup_integ_cache.json");
  ASSERT_TRUE(CacheInit("test_dup_integ_cache.json"));

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
  system("rm -f test_dup_integ_cache.json");
}

void register_integration_tests(void) {
  register_test("test_metadata_png_dimensions", test_metadata_png_dimensions,
                "integration");
  register_test("test_metadata_gif_dimensions", test_metadata_gif_dimensions,
                "integration");
  register_test("test_metadata_bmp_dimensions", test_metadata_bmp_dimensions,
                "integration");
  register_test("test_metadata_webp_dimensions", test_metadata_webp_dimensions,
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
  register_test("test_unified_metadata_dispatcher",
                test_unified_metadata_dispatcher, "integration");
  register_test("test_duplicate_integration", test_duplicate_integration,
                "integration");
}
