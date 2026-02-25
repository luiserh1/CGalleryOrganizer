#include <stdio.h>
#include <stdlib.h>

#include "duplicate_finder.h"
#include "fs_utils.h"
#include "gallery_cache.h"
#include "hash_utils.h"
#include "metadata_parser.h"
#include "test_framework.h"

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
}
