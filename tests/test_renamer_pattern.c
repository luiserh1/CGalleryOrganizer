#include <string.h>

#include "systems/renamer_pattern.h"
#include "test_framework.h"

void test_renamer_pattern_unknown_token_rejected(void) {
  char error[256] = {0};
  ASSERT_FALSE(RenamerPatternValidate("[unknown].[format]", error,
                                      sizeof(error)));
  ASSERT_TRUE(strstr(error, "unknown token") != NULL);
}

void test_renamer_pattern_extended_gps_tokens_validate(void) {
  char error[256] = {0};
  ASSERT_TRUE(RenamerPatternValidate("[location]-[gps_lat]-[gps_lon].[format]",
                                     error, sizeof(error)));
}

void test_renamer_pattern_escape_and_render(void) {
  RenamerPatternContext ctx = {
      .format = "JPG",
      .index = 7,
  };

  char output[256] = {0};
  bool truncated = false;
  char warning[128] = {0};
  char error[128] = {0};
  ASSERT_TRUE(RenamerPatternRender("Pic-[[tags]]-[index].[format]", &ctx, output,
                                   sizeof(output), &truncated, warning,
                                   sizeof(warning), error, sizeof(error)));
  ASSERT_FALSE(truncated);
  ASSERT_STR_EQ("pic-tags-7.jpg", output);
}

void test_renamer_pattern_missing_values_use_fallbacks(void) {
  RenamerPatternContext ctx = {0};

  char output[256] = {0};
  bool truncated = false;
  char warning[128] = {0};
  char error[128] = {0};
  ASSERT_TRUE(RenamerPatternRender(
      "[date]-[camera]-[location]-[gps_lat]-[gps_lon]-[tags].[format]", &ctx,
      output, sizeof(output), &truncated, warning, sizeof(warning), error,
      sizeof(error)));
  ASSERT_FALSE(truncated);
  ASSERT_STR_EQ(
      "unknown-date-unknown-camera-unknown-location-unknown-gps-lat-unknown-gps-lon-untagged.unknown-format",
      output);
}

void test_renamer_pattern_renders_location_tokens_when_present(void) {
  RenamerPatternContext ctx = {
      .location = "n40.416775-w3.703790",
      .gps_lat = "n40.416775",
      .gps_lon = "w3.703790",
      .format = "jpg",
  };

  char output[256] = {0};
  bool truncated = false;
  char warning[128] = {0};
  char error[128] = {0};
  ASSERT_TRUE(RenamerPatternRender("[location]-[gps_lat]-[gps_lon].[format]",
                                   &ctx, output, sizeof(output), &truncated,
                                   warning, sizeof(warning), error,
                                   sizeof(error)));
  ASSERT_FALSE(truncated);
  ASSERT_STR_EQ("n40.416775-w3.703790-n40.416775-w3.703790.jpg", output);
}

void test_renamer_pattern_truncate_hash_is_deterministic(void) {
  char long_tag[512] = {0};
  for (int i = 0; i < (int)sizeof(long_tag) - 1; i++) {
    long_tag[i] = (i % 3 == 0) ? 'a' : ((i % 3 == 1) ? 'b' : 'c');
  }
  long_tag[sizeof(long_tag) - 1] = '\0';

  RenamerPatternContext ctx = {
      .tags = long_tag,
      .format = "png",
      .index = 1,
  };

  char output_a[512] = {0};
  char output_b[512] = {0};
  bool trunc_a = false;
  bool trunc_b = false;
  char warning_a[128] = {0};
  char warning_b[128] = {0};
  char error[128] = {0};

  ASSERT_TRUE(RenamerPatternRender("x-[tags].[format]", &ctx, output_a,
                                   sizeof(output_a), &trunc_a, warning_a,
                                   sizeof(warning_a), error, sizeof(error)));
  ASSERT_TRUE(RenamerPatternRender("x-[tags].[format]", &ctx, output_b,
                                   sizeof(output_b), &trunc_b, warning_b,
                                   sizeof(warning_b), error, sizeof(error)));

  ASSERT_TRUE(trunc_a);
  ASSERT_TRUE(trunc_b);
  ASSERT_STR_EQ(output_a, output_b);
}

void register_renamer_pattern_tests(void) {
  register_test("test_renamer_pattern_unknown_token_rejected",
                test_renamer_pattern_unknown_token_rejected, "unit");
  register_test("test_renamer_pattern_extended_gps_tokens_validate",
                test_renamer_pattern_extended_gps_tokens_validate, "unit");
  register_test("test_renamer_pattern_escape_and_render",
                test_renamer_pattern_escape_and_render, "unit");
  register_test("test_renamer_pattern_missing_values_use_fallbacks",
                test_renamer_pattern_missing_values_use_fallbacks, "unit");
  register_test("test_renamer_pattern_renders_location_tokens_when_present",
                test_renamer_pattern_renders_location_tokens_when_present,
                "unit");
  register_test("test_renamer_pattern_truncate_hash_is_deterministic",
                test_renamer_pattern_truncate_hash_is_deterministic, "unit");
}
