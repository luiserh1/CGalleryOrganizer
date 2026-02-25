#include <stdlib.h>
#include <unistd.h>

#include "test_framework.h"

#include "gallery_cache.h"

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
  ASSERT_TRUE(CacheInit("build/temp_cache.json"));

  // 2. Create entry
  ImageMetadata md = {0};
  md.path = strdup("/fake/path.jpg");
  md.fileSize = 100;
  md.modificationDate = 123456789.0;
  strcpy(md.mlPrimaryClass, "landscape_photo");
  md.mlPrimaryClassConfidence = 0.73f;
  md.mlHasText = true;
  md.mlTextBoxCount = 1;
  strcpy(md.mlModelClassification, "clf-default");
  strcpy(md.mlModelTextDetection, "text-default");
  strcpy(md.mlModelEmbedding, "embed-default");
  md.mlEmbeddingDim = 4;
  md.mlEmbeddingBase64 = strdup("AACAPwAAAEAAAEBAAACAQA==");
  md.mlRawJson = strdup("{\"demo\":true}");

  ASSERT_TRUE(CacheUpdateEntry(&md));
  CacheFreeMetadata(&md);

  // 3. Save cache
  ASSERT_TRUE(CacheSave());

  // 4. Shutdown and re-init to test loading
  CacheShutdown();
  ASSERT_TRUE(CacheInit("build/temp_cache.json"));

  // 5. Get entry (simulate file exists with same size and date)
  ImageMetadata loaded_md = {0};
  ASSERT_TRUE(
      CacheGetValidEntry("/fake/path.jpg", 123456789.0, 100, &loaded_md));

  // 6. Verify data
  ASSERT_EQ(100, loaded_md.fileSize);
  ASSERT_STR_EQ("landscape_photo", loaded_md.mlPrimaryClass);
  ASSERT_TRUE(loaded_md.mlPrimaryClassConfidence > 0.0f);
  ASSERT_TRUE(loaded_md.mlHasText);
  ASSERT_EQ(1, loaded_md.mlTextBoxCount);
  ASSERT_STR_EQ("clf-default", loaded_md.mlModelClassification);
  ASSERT_STR_EQ("embed-default", loaded_md.mlModelEmbedding);
  ASSERT_EQ(4, loaded_md.mlEmbeddingDim);
  ASSERT_TRUE(loaded_md.mlEmbeddingBase64 != NULL);
  ASSERT_TRUE(loaded_md.mlRawJson != NULL);
  CacheFreeMetadata(&loaded_md);

  // 7. Get invalid entry (size changed)
  ASSERT_FALSE(
      CacheGetValidEntry("/fake/path.jpg", 123456789.0, 200, &loaded_md));

  // 8. Get invalid entry (date changed)
  ASSERT_FALSE(
      CacheGetValidEntry("/fake/path.jpg", 999999999.0, 100, &loaded_md));

  // 9. Get missing entry
  ASSERT_FALSE(CacheGetValidEntry("/missing/path.jpg", 0, 0, &loaded_md));

  CacheShutdown();
  system("rm -f build/temp_cache.json");
}

void test_cache_entry_count_and_keys(void) {
  ASSERT_TRUE(CacheInit("build/temp_cache_count.json"));

  ImageMetadata md1 = {0};
  md1.path = strdup("/fake/a.jpg");
  ASSERT_TRUE(CacheUpdateEntry(&md1));
  CacheFreeMetadata(&md1);

  ImageMetadata md2 = {0};
  md2.path = strdup("/fake/b.jpg");
  ASSERT_TRUE(CacheUpdateEntry(&md2));
  CacheFreeMetadata(&md2);

  ImageMetadata md3 = {0};
  md3.path = strdup("/fake/c.jpg");
  ASSERT_TRUE(CacheUpdateEntry(&md3));
  CacheFreeMetadata(&md3);

  ASSERT_EQ(3, CacheGetEntryCount());

  int key_count = 0;
  char **keys = CacheGetAllKeys(&key_count);
  ASSERT_EQ(3, key_count);
  ASSERT_TRUE(keys != NULL);
  CacheFreeKeys(keys, key_count);

  CacheShutdown();
  system("rm -f build/temp_cache_count.json");
}

void register_gallery_cache_tests(void) {
  register_test("test_cache_extract_basic_metadata",
                test_cache_extract_basic_metadata, "cache");
  register_test("test_cache_flow", test_cache_flow, "cache");
  register_test("test_cache_entry_count_and_keys",
                test_cache_entry_count_and_keys, "cache");
}
