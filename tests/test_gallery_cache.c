#include "gallery_cache.h"
#include "test_framework.h"
#include <stdlib.h>
#include <unistd.h>

void test_cache_extract_basic_metadata(void) {
  system("echo 'hello' > temp_meta.txt");

  double mod_date = 0;
  long size = 0;

  ASSERT_TRUE(ExtractBasicMetadata("temp_meta.txt", &mod_date, &size));
  ASSERT_EQ(6, size); // "hello\n" is 6 bytes
  ASSERT_TRUE(mod_date > 0.0);

  system("rm temp_meta.txt");
}

void test_cache_flow(void) {
  // 1. Init cache
  ASSERT_TRUE(CacheInit("temp_cache.json"));

  // 2. Create entry
  ImageMetadata md = {0};
  md.path = strdup("/fake/path.jpg");
  md.fileSize = 100;
  md.modificationDate = 123456789.0;

  ASSERT_TRUE(CacheUpdateEntry(&md));

  // md.path is now owned by the cache, do not free it.

  // 3. Save cache
  ASSERT_TRUE(CacheSave());

  // 4. Shutdown and re-init to test loading
  CacheShutdown();
  ASSERT_TRUE(CacheInit("temp_cache.json"));

  // 5. Get entry (simulate file exists with same size and date)
  ImageMetadata loaded_md = {0};
  ASSERT_TRUE(
      CacheGetValidEntry("/fake/path.jpg", 123456789.0, 100, &loaded_md));

  // 6. Verify data
  ASSERT_EQ(100, loaded_md.fileSize);

  // 7. Get invalid entry (size changed)
  ASSERT_FALSE(
      CacheGetValidEntry("/fake/path.jpg", 123456789.0, 200, &loaded_md));

  // 8. Get invalid entry (date changed)
  ASSERT_FALSE(
      CacheGetValidEntry("/fake/path.jpg", 999999999.0, 100, &loaded_md));

  // 9. Get missing entry
  ASSERT_FALSE(CacheGetValidEntry("/missing/path.jpg", 0, 0, &loaded_md));

  CacheShutdown();
  system("rm temp_cache.json");
}

void register_gallery_cache_tests(void) {
  register_test("test_cache_extract_basic_metadata",
                test_cache_extract_basic_metadata, "cache");
  register_test("test_cache_flow", test_cache_flow, "cache");
}
