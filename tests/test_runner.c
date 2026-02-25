#include <stdio.h>
#include <string.h>

#include "test_framework.h"

extern void register_fs_utils_tests(void);
extern void register_gallery_cache_tests(void);
extern void register_integration_tests(void);
extern void register_hash_utils_tests(void);
extern void register_duplicate_finder_tests(void);
extern void register_organizer_tests(void);
extern void register_ml_api_tests(void);
extern void register_similarity_engine_tests(void);

int main(int argc, char **argv) {
  printf("=== CGalleryOrganizer Test Suite ===\n\n");

  const char *filter_suite = NULL;

  // Simple arg parsing
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--suite") == 0 && i + 1 < argc) {
      filter_suite = argv[i + 1];
      i++;
    }
  }

  // Register tests
  register_fs_utils_tests();
  register_gallery_cache_tests();
  register_hash_utils_tests();
  register_duplicate_finder_tests();
  register_organizer_tests();
  register_ml_api_tests();
  register_similarity_engine_tests();
  register_integration_tests();

  int run_count = 0;
  for (int i = 0; i < g_test_count; i++) {
    if (filter_suite && strcmp(g_tests[i].category, filter_suite) != 0) {
      continue;
    }

    printf("RUN  %s\n", g_tests[i].name);
    int start_fails = g_fail_count;
    g_tests[i].func();
    if (g_fail_count == start_fails) {
      printf("PASS %s\n", g_tests[i].name);
    }
    run_count++;
  }

  printf("\nTotal tests run: %d\n", run_count);
  if (g_fail_count > 0) {
    printf("=== Results: %d/%d passed (%d failures) ===\n",
           run_count - g_fail_count, run_count, g_fail_count);
    return 1;
  } else {
    printf("=== Results: %d/%d passed ===\n", run_count, run_count);
    return 0;
  }
}
