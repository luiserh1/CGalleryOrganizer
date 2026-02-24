#include "duplicate_finder.h"
#include "gallery_cache.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void create_file(const char *path, const char *content) {
  FILE *f = fopen(path, "wb");
  if (f) {
    if (content)
      fputs(content, f);
    fclose(f);
  }
}

void test_find_exact_duplicates_success(void) {
  // 1. Setup mock files
  create_file("dup_test_1.txt", "hello duplicate");
  create_file("dup_test_2.txt", "hello duplicate");
  create_file("dup_test_3.txt", "unique content!");

  // 2. Init cache and populate it
  CacheInit("test_dup_cache.json");

  ImageMetadata md1 = {0};
  md1.path = strdup("dup_test_1.txt");
  strcpy(md1.exactHashMd5, "md5_mocked_but_same");
  CacheUpdateEntry(&md1);
  free(md1.path);

  ImageMetadata md2 = {0};
  md2.path = strdup("dup_test_2.txt");
  strcpy(md2.exactHashMd5, "md5_mocked_but_same");
  CacheUpdateEntry(&md2);
  free(md2.path);

  ImageMetadata md3 = {0};
  md3.path = strdup("dup_test_3.txt");
  strcpy(md3.exactHashMd5, "md5_unique");
  CacheUpdateEntry(&md3);
  free(md3.path);

  // Run the system
  DuplicateReport rep = FindExactDuplicates();

  ASSERT_EQ(1, rep.group_count);
  ASSERT_EQ(1, rep.groups[0].duplicate_count);

  // Original might be 1 or 2 due to array sort/loop order, but one must be dup
  // of the other
  bool is_1_orig = strcmp(rep.groups[0].original_path, "dup_test_1.txt") == 0;
  bool is_2_orig = strcmp(rep.groups[0].original_path, "dup_test_2.txt") == 0;
  ASSERT_TRUE(is_1_orig || is_2_orig);

  FreeDuplicateReport(&rep);
  CacheShutdown();
  remove("test_dup_cache.json");
  remove("dup_test_1.txt");
  remove("dup_test_2.txt");
  remove("dup_test_3.txt");
}

void register_duplicate_finder_tests(void) {
  register_test("test_find_exact_duplicates_success",
                test_find_exact_duplicates_success, "Duplicate Finder");
}
