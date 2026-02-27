#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "test_framework.h"

#include "fs_utils.h"
#include "integration_test_helpers.h"

// Helper structure for WalkCallback counting
typedef struct {
  int count;
} WalkContext;

static bool TestWalkCallback(const char *absolute_path, void *user_data) {
  if (user_data) {
    WalkContext *ctx = (WalkContext *)user_data;
    ctx->count++;
  }
  // Simple verification that we are getting an absolute path
  if (absolute_path[0] != '/') {
    printf("  FAIL: Expected absolute path, got %s\n", absolute_path);
    g_fail_count++;
    return false;
  }
  return true;
}

static bool WriteDummyFile(const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }
  fputs("x", f);
  fclose(f);
  return true;
}

void test_fs_utils_is_supported_media_jpg(void) {
  ASSERT_TRUE(FsIsSupportedMedia("image.jpg"));
  ASSERT_TRUE(FsIsSupportedMedia("path/to/image.jpeg"));
  ASSERT_TRUE(FsIsSupportedMedia("IMAGE.JPG")); // Case insensitive
}

void test_fs_utils_is_supported_media_png(void) {
  ASSERT_TRUE(FsIsSupportedMedia("photo.png"));
  ASSERT_TRUE(FsIsSupportedMedia("PHOTO.PNG"));
}

void test_fs_utils_is_supported_media_video(void) {
  ASSERT_TRUE(FsIsSupportedMedia("video.mp4"));
  ASSERT_TRUE(FsIsSupportedMedia("clip.mov"));
  ASSERT_TRUE(FsIsSupportedMedia("movie.mkv"));
}

void test_fs_utils_is_supported_media_unsupported(void) {
  ASSERT_FALSE(FsIsSupportedMedia("document.pdf"));
  ASSERT_FALSE(FsIsSupportedMedia("text.txt"));
  ASSERT_FALSE(FsIsSupportedMedia("archive.zip"));
  ASSERT_FALSE(FsIsSupportedMedia("noextension"));
  ASSERT_FALSE(FsIsSupportedMedia(".hidden"));
}

void test_fs_utils_get_absolute_path(void) {
  char out_path[MAX_PATH_LENGTH];

  // Create a temporary file to test realpath
  ASSERT_TRUE(WriteDummyFile("temp_test_file.txt"));

  ASSERT_TRUE(
      FsGetAbsolutePath("temp_test_file.txt", out_path, sizeof(out_path)));
  ASSERT_EQ('/', out_path[0]); // Check it starts with root

  // Reject non-existent file
  ASSERT_FALSE(FsGetAbsolutePath("does_not_exist_at_all.test", out_path,
                                 sizeof(out_path)));

  // Clean up
  ASSERT_TRUE(FsDeleteFile("temp_test_file.txt"));
}

void test_fs_utils_walk_directory(void) {
  // Setup a temporary directory structure
  ASSERT_TRUE(RemovePathRecursiveForTest("temp_walk_test"));
  ASSERT_TRUE(FsMakeDirRecursive("temp_walk_test/sub"));
  ASSERT_TRUE(WriteDummyFile("temp_walk_test/file1.jpg"));
  ASSERT_TRUE(WriteDummyFile("temp_walk_test/file2.png"));
  ASSERT_TRUE(WriteDummyFile("temp_walk_test/sub/file3.mp4"));

  WalkContext ctx = {0};
  ASSERT_TRUE(FsWalkDirectory("temp_walk_test", TestWalkCallback, &ctx));

  // We expect exactly 3 files to be traversed
  ASSERT_EQ(3, ctx.count);

  // Clean up
  ASSERT_TRUE(RemovePathRecursiveForTest("temp_walk_test"));
}

void register_fs_utils_tests(void) {
  register_test("test_fs_utils_is_supported_media_jpg",
                test_fs_utils_is_supported_media_jpg, "fs");
  register_test("test_fs_utils_is_supported_media_png",
                test_fs_utils_is_supported_media_png, "fs");
  register_test("test_fs_utils_is_supported_media_video",
                test_fs_utils_is_supported_media_video, "fs");
  register_test("test_fs_utils_is_supported_media_unsupported",
                test_fs_utils_is_supported_media_unsupported, "fs");
  register_test("test_fs_utils_get_absolute_path",
                test_fs_utils_get_absolute_path, "fs");
  register_test("test_fs_utils_walk_directory", test_fs_utils_walk_directory,
                "fs");
}
