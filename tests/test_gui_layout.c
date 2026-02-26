#include <stdbool.h>

#include "gui/gui_layout.h"
#include "test_framework.h"

static int BuildRepresentativeScanLayout(const GuiLayoutMetrics *metrics,
                                         int width, int height,
                                         GuiLayoutRect *out_rects,
                                         int max_rects) {
  if (!metrics || !out_rects || max_rects <= 0) {
    return 0;
  }

  GuiLayoutRect bounds = {0, 0, (float)width, (float)height};
  GuiLayoutContext ctx = {0};
  GuiLayoutInit(&ctx, bounds, metrics);

  int count = 0;

  out_rects[count++] = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutMeasureTextWidth("Gallery Directory", metrics) + metrics->gap * 2,
      (float)metrics->label_h);
  out_rects[count++] =
      GuiLayoutPlaceFlex(&ctx, (float)metrics->min_text_input_w, (float)metrics->input_h);
  out_rects[count++] =
      GuiLayoutPlaceFixed(&ctx, GuiLayoutMeasureTextWidth("Environment Dir", metrics) +
                                    metrics->gap * 2,
                          (float)metrics->label_h);
  out_rects[count++] =
      GuiLayoutPlaceFlex(&ctx, (float)metrics->min_text_input_w, (float)metrics->input_h);

  GuiLayoutNextLine(&ctx);
  out_rects[count++] = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutButtonWidth("Exhaustive metadata", metrics, 210.0f),
      (float)metrics->label_h);

  GuiLayoutNextLine(&ctx);
  out_rects[count++] = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutMeasureTextWidth("Jobs", metrics) + metrics->gap * 2,
      (float)metrics->label_h);
  out_rects[count++] =
      GuiLayoutPlaceFixed(&ctx, (float)metrics->min_numeric_input_w, (float)metrics->input_h);
  out_rects[count++] = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutMeasureTextWidth("Cache compression", metrics) + metrics->gap * 2,
      (float)metrics->label_h);
  out_rects[count++] = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutButtonWidth("none", metrics, 64.0f), (float)metrics->button_h);
  out_rects[count++] = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutButtonWidth("zstd", metrics, 64.0f), (float)metrics->button_h);
  out_rects[count++] = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutButtonWidth("auto", metrics, 64.0f), (float)metrics->button_h);
  out_rects[count++] = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutMeasureTextWidth("Level", metrics) + metrics->gap * 2,
      (float)metrics->label_h);
  out_rects[count++] =
      GuiLayoutPlaceFixed(&ctx, (float)metrics->min_numeric_input_w, (float)metrics->input_h);

  GuiLayoutNextLine(&ctx);
  out_rects[count++] = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutButtonWidth("Scan/Cache", metrics, 120.0f),
      (float)metrics->button_h);
  out_rects[count++] = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutButtonWidth("ML Enrich", metrics, 120.0f),
      (float)metrics->button_h);
  out_rects[count++] = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutButtonWidth("Save Paths", metrics, 120.0f),
      (float)metrics->button_h);
  out_rects[count++] = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutButtonWidth("Reset Saved Paths", metrics, 160.0f),
      (float)metrics->button_h);

  if (count > max_rects) {
    return max_rects;
  }
  return count;
}

void test_gui_layout_scale_and_tokens(void) {
  ASSERT_EQ(100, GuiLayoutClampUiScalePercent(0));
  ASSERT_EQ(80, GuiLayoutClampUiScalePercent(40));
  ASSERT_EQ(160, GuiLayoutClampUiScalePercent(200));

  float s_small = GuiLayoutComputeEffectiveScale(850, 646, 100);
  float s_base = GuiLayoutComputeEffectiveScale(1000, 760, 100);
  float s_large = GuiLayoutComputeEffectiveScale(1920, 1080, 100);
  ASSERT_TRUE(s_small <= s_base);
  ASSERT_TRUE(s_large >= s_base);

  GuiLayoutMetrics m_small = {0};
  GuiLayoutMetrics m_large = {0};
  GuiLayoutComputeMetrics(&m_small, 900, 700, 100);
  GuiLayoutComputeMetrics(&m_large, 1920, 1080, 120);
  ASSERT_TRUE(m_large.font_size >= m_small.font_size);
  ASSERT_TRUE(m_large.pad >= m_small.pad);
}

void test_gui_layout_wrapping_in_bounds(void) {
  GuiLayoutMetrics metrics = {0};
  GuiLayoutComputeMetrics(&metrics, 900, 700, 100);

  GuiLayoutRect bounds = {0, 0, 420, 260};
  GuiLayoutContext ctx = {0};
  GuiLayoutInit(&ctx, bounds, &metrics);

  GuiLayoutRect rects[8] = {0};
  rects[0] = GuiLayoutPlaceFixed(&ctx, 180, (float)metrics.button_h);
  rects[1] = GuiLayoutPlaceFixed(&ctx, 180, (float)metrics.button_h);
  rects[2] = GuiLayoutPlaceFixed(&ctx, 180, (float)metrics.button_h);
  rects[3] = GuiLayoutPlaceFixed(&ctx, 180, (float)metrics.button_h);
  rects[4] = GuiLayoutPlaceFixed(&ctx, 180, (float)metrics.button_h);
  rects[5] = GuiLayoutPlaceFixed(&ctx, 180, (float)metrics.button_h);

  ASSERT_TRUE(GuiLayoutValidateNoOverlap(rects, 6, bounds));
}

void test_gui_layout_scan_shape_no_overlap_profiles(void) {
  int min_w = 0;
  int min_h = 0;
  GuiLayoutComputeMinWindowSize(&min_w, &min_h);

  const int widths[3] = {min_w, 1000, 1920};
  const int heights[3] = {min_h, 760, 1080};

  for (int i = 0; i < 3; i++) {
    GuiLayoutMetrics metrics = {0};
    GuiLayoutComputeMetrics(&metrics, widths[i], heights[i], 100);
    GuiLayoutRect rects[32] = {0};
    int count = BuildRepresentativeScanLayout(&metrics, widths[i] - metrics.pad * 2,
                                              heights[i] - metrics.pad * 2, rects, 32);
    ASSERT_TRUE(count > 0);
    GuiLayoutRect bounds = {0, 0, (float)(widths[i] - metrics.pad * 2),
                            (float)(heights[i] - metrics.pad * 2)};
    ASSERT_TRUE(GuiLayoutValidateNoOverlap(rects, count, bounds));
  }
}

void register_gui_layout_tests(void) {
  register_test("test_gui_layout_scale_and_tokens", test_gui_layout_scale_and_tokens,
                "unit");
  register_test("test_gui_layout_wrapping_in_bounds",
                test_gui_layout_wrapping_in_bounds, "unit");
  register_test("test_gui_layout_scan_shape_no_overlap_profiles",
                test_gui_layout_scan_shape_no_overlap_profiles, "unit");
}
