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
}
