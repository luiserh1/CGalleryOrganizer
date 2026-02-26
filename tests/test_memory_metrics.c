#include "utils/memory_metrics.h"
#include "test_framework.h"

void test_memory_metrics_peak_non_negative(void) {
  long long peak = GetPeakRssBytes();
  ASSERT_TRUE(peak >= -1);
}

void test_memory_metrics_current_non_negative_or_unavailable(void) {
  long long current = GetCurrentRssBytes();
  ASSERT_TRUE(current >= -1);
}

void register_memory_metrics_tests(void) {
  register_test("test_memory_metrics_peak_non_negative",
                test_memory_metrics_peak_non_negative, "metrics");
  register_test("test_memory_metrics_current_non_negative_or_unavailable",
                test_memory_metrics_current_non_negative_or_unavailable,
                "metrics");
}
