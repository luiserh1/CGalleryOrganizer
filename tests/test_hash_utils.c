#include "hash_utils.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TEMP_FILE = "tmp_hash_test.txt";

static void setup(void) {
  FILE *f = fopen(TEMP_FILE, "wb");
  if (f) {
    fwrite("hello world", 1, 11, f);
    fclose(f);
  }
}

static void teardown(void) { remove(TEMP_FILE); }

void test_compute_md5(void) {
  setup();

  char md5_hex[MD5_HASH_LEN + 1] = {0};
  bool success = ComputeFileMd5(TEMP_FILE, md5_hex);

  ASSERT_TRUE(success);
  // MD5("hello world") = 5eb63bbbe01eeed093cb22bb8f5acdc3
  ASSERT_STR_EQ("5eb63bbbe01eeed093cb22bb8f5acdc3", md5_hex);

  teardown();
}

void test_compute_sha256(void) {
  setup();

  char sha256_hex[SHA256_HASH_LEN + 1] = {0};
  bool success = ComputeFileSha256(TEMP_FILE, sha256_hex);

  ASSERT_TRUE(success);
  // SHA256("hello world") =
  // b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9
  ASSERT_STR_EQ(
      "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9",
      sha256_hex);

  teardown();
}

void test_hash_utils_nonexistent_file(void) {
  char md5_hex[MD5_HASH_LEN + 1] = {0};
  bool success = ComputeFileMd5("this_file_does_not_exist.txt", md5_hex);
  ASSERT_FALSE(success);
}

void register_hash_utils_tests(void) {
  register_test("test_compute_md5", test_compute_md5, "Hash Utils");
  register_test("test_compute_sha256", test_compute_sha256, "Hash Utils");
  register_test("test_hash_utils_nonexistent_file",
                test_hash_utils_nonexistent_file, "Hash Utils");
}
