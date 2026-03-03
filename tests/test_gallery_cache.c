#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "test_framework.h"

#include "fs_utils.h"
#include "gallery_cache.h"
#include "integration_test_helpers.h"

static bool WriteTextFile(const char *path, const char *content) {
  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }
  if (content && fputs(content, f) == EOF) {
    fclose(f);
    return false;
  }
  fclose(f);
  return true;
}

void test_cache_extract_basic_metadata(void) {
  ASSERT_TRUE(WriteTextFile("temp_meta.txt", "hello\n"));

  double mod_date = 0;
  long size = 0;

  ASSERT_TRUE(ExtractBasicMetadata("temp_meta.txt", &mod_date, &size));
  ASSERT_EQ(6, size); // "hello\n" is 6 bytes
  ASSERT_TRUE(mod_date > 0.0);

  ASSERT_TRUE(FsDeleteFile("temp_meta.txt"));
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
  ASSERT_TRUE(RemovePathRecursiveForTest("build/temp_cache.json"));
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
  ASSERT_TRUE(RemovePathRecursiveForTest("build/temp_cache_count.json"));
}

void test_cache_get_raw_entry_returns_independent_owned_strings(void) {
  const char *cache_path = "build/temp_cache_owned_strings.json";
  ASSERT_TRUE(CacheInit(cache_path));

  ImageMetadata md = {0};
  md.path = strdup("/fake/owned.jpg");
  md.modificationDate = 42.0;
  md.fileSize = 7;
  md.mlEmbeddingDim = 4;
  md.mlEmbeddingBase64 = strdup("AACAPwAAAEAAAEBAAACAQA==");
  ASSERT_TRUE(CacheUpdateEntry(&md));
  CacheFreeMetadata(&md);
  ASSERT_TRUE(CacheSave());

  ImageMetadata first = {0};
  ImageMetadata second = {0};
  ASSERT_TRUE(CacheGetRawEntry("/fake/owned.jpg", &first));
  ASSERT_TRUE(CacheGetRawEntry("/fake/owned.jpg", &second));

  ASSERT_TRUE(first.path != NULL);
  ASSERT_TRUE(second.path != NULL);
  ASSERT_STR_EQ("/fake/owned.jpg", first.path);
  ASSERT_STR_EQ("/fake/owned.jpg", second.path);
  ASSERT_TRUE(first.path != second.path);
  ASSERT_TRUE(first.mlEmbeddingBase64 != NULL);
  ASSERT_TRUE(second.mlEmbeddingBase64 != NULL);
  ASSERT_TRUE(first.mlEmbeddingBase64 != second.mlEmbeddingBase64);

  first.path[0] = 'X';
  first.mlEmbeddingBase64[0] = 'Z';

  ImageMetadata third = {0};
  ASSERT_TRUE(CacheGetRawEntry("/fake/owned.jpg", &third));
  ASSERT_STR_EQ("/fake/owned.jpg", third.path);
  ASSERT_STR_EQ("AACAPwAAAEAAAEBAAACAQA==", third.mlEmbeddingBase64);

  CacheFreeMetadata(&first);
  CacheFreeMetadata(&second);
  CacheFreeMetadata(&third);
  CacheShutdown();
  ASSERT_TRUE(RemovePathRecursiveForTest(cache_path));
}

void register_gallery_cache_tests(void) {
  register_test("test_cache_extract_basic_metadata",
                test_cache_extract_basic_metadata, "cache");
  register_test("test_cache_flow", test_cache_flow, "cache");
  register_test("test_cache_entry_count_and_keys",
                test_cache_entry_count_and_keys, "cache");
  register_test("test_cache_get_raw_entry_returns_independent_owned_strings",
                test_cache_get_raw_entry_returns_independent_owned_strings,
                "cache");
}
