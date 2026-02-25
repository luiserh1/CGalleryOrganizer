#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fs_utils.h"
#include "gallery_cache.h"
#include "organizer.h"
#include "test_framework.h"

void test_organizer_plan_generation(void) {
  const char *env_dir = "build/test_env_plan";
  FsMakeDirRecursive(env_dir);

  char cache_path[1024];
  snprintf(cache_path, sizeof(cache_path), "%s/gallery_cache.json", env_dir);

  FsDeleteFile(cache_path);
  ASSERT_TRUE(CacheInit(cache_path));

  ImageMetadata md1 = {0};
  md1.path = strdup("/fake/photo1.jpg");
  strcpy(md1.dateTaken, "2024:05:12 10:00:00");
  md1.fileSize = 1000;
  md1.modificationDate = 12345.0;
  ASSERT_TRUE(CacheUpdateEntry(&md1));

  ImageMetadata md2 = {0};
  md2.path = strdup("/fake/photo2.jpg");
  md2.dateTaken[0] = '\0'; // Unknown date
  md2.fileSize = 2000;
  md2.modificationDate = 12345.0;
  ASSERT_TRUE(CacheUpdateEntry(&md2));

  const char *group_keys[] = {"date"};
  OrganizerPlan *plan = OrganizerComputePlan(env_dir, group_keys, 1);
  ASSERT_TRUE(plan != NULL);
  ASSERT_EQ(2, plan->count);

  bool found_known = false;
  bool found_unknown = false;

  for (int i = 0; i < plan->count; i++) {
    if (strcmp(plan->moves[i].original_path, "/fake/photo1.jpg") == 0) {
      ASSERT_TRUE(strstr(plan->moves[i].new_path, "_2024/_05") != NULL);
      found_known = true;
    } else if (strcmp(plan->moves[i].original_path, "/fake/photo2.jpg") == 0) {
      ASSERT_TRUE(strstr(plan->moves[i].new_path, "_Unknown") != NULL);
      found_unknown = true;
    }
  }

  ASSERT_TRUE(found_known);
  ASSERT_TRUE(found_unknown);

  OrganizerFreePlan(plan);
  CacheFreeMetadata(&md1);
  CacheFreeMetadata(&md2);
  CacheShutdown();
}

void test_organizer_compound_grouping(void) {
  const char *env_dir = "build/test_env_compound";
  FsMakeDirRecursive(env_dir);

  char cache_path[1024];
  snprintf(cache_path, sizeof(cache_path), "%s/gallery_cache.json", env_dir);

  FsDeleteFile(cache_path);
  ASSERT_TRUE(CacheInit(cache_path));

  ImageMetadata md1 = {0};
  md1.path = strdup("/fake/photo1.jpg");
  strcpy(md1.dateTaken, "2024:08:15 14:30:00");
  strcpy(md1.cameraMake, "Sony");
  strcpy(md1.cameraModel, "ILCE-7RM3");
  md1.width = 4000;
  md1.height = 3000;
  md1.fileSize = 1000;
  md1.modificationDate = 12345.0;
  ASSERT_TRUE(CacheUpdateEntry(&md1));

  const char *group_keys[] = {"camera", "date", "resolution", "orientation"};
  OrganizerPlan *plan = OrganizerComputePlan(env_dir, group_keys, 4);
  ASSERT_TRUE(plan != NULL);
  ASSERT_EQ(1, plan->count);

  // Expected Path:
  // _Sony_ILCE-7RM3/_2024/_08/_4K_UHD/_Landscape/20240815_143000_ILCE-7RM3.jpg
  // Note: 4000x3000 max_dim=4000, 4000 >= 3840 -> _4K_UHD, 4000 > 3000 ->
  // _Landscape
  ASSERT_TRUE(strstr(plan->moves[0].new_path,
                     "_Sony_ILCE-7RM3/_2024/_08/_4K_UHD/_Landscape/"
                     "20240815_143000_ILCE-7RM3.jpg") != NULL);

  OrganizerFreePlan(plan);
  CacheFreeMetadata(&md1);
  CacheShutdown();
}

void test_organizer_manifest_rw(void) {
  const char *env_dir = "build/test_env_manifest";
  FsMakeDirRecursive(env_dir);

  system("mkdir -p build/test_env_manifest/orig build/test_env_manifest/new");
  system("touch build/test_env_manifest/new/file.jpg"); // Put the file in 'new'

  ASSERT_TRUE(OrganizerInit(env_dir));

  // We need to use system-resolved paths for rollback to work perfectly since
  // it moves stuff Wait, no, FsGetAbsolutePath requires the file to exist, but
  // "orig/file.jpg" doesn't exist yet! It's okay, FsGetAbsolutePath uses
  // realpath which might fail if the file doesn't exist... Let's create it
  // first, get its realpath, then move it.

  system("touch build/test_env_manifest/orig/file.jpg");
  char abs_orig[1024];
  FsGetAbsolutePath("build/test_env_manifest/orig/file.jpg", abs_orig,
                    sizeof(abs_orig));

  char abs_new[1024];
  FsGetAbsolutePath("build/test_env_manifest/new/file.jpg", abs_new,
                    sizeof(abs_new));

  // Now "move" it manually to 'new' so it is missing from 'orig'
  system("rm build/test_env_manifest/orig/file.jpg");

  ASSERT_TRUE(OrganizerRecordMove(abs_orig, abs_new));
  ASSERT_TRUE(OrganizerSaveManifest());
  OrganizerShutdown();

  int restored = OrganizerRollback(env_dir);
  ASSERT_EQ(1, restored);

  // Verify it's back in orig
  FILE *f_check = fopen("build/test_env_manifest/orig/file.jpg", "r");
  ASSERT_TRUE(f_check != NULL);
  if (f_check)
    fclose(f_check);

  // Verify it's removed from new
  f_check = fopen("build/test_env_manifest/new/file.jpg", "r");
  ASSERT_TRUE(f_check == NULL);
}

void test_organizer_print_plan_no_duplicate_last_group(void) {
  const char *env_dir = "build/test_env_print";
  FsMakeDirRecursive(env_dir);

  char cache_path[1024];
  snprintf(cache_path, sizeof(cache_path), "%s/gallery_cache.json", env_dir);

  FsDeleteFile(cache_path);
  ASSERT_TRUE(CacheInit(cache_path));

  ImageMetadata md = {0};
  md.path = strdup("/fake/only_photo.jpg");
  md.fileSize = 100;
  md.modificationDate = 12345.0;
  ASSERT_TRUE(CacheUpdateEntry(&md));
  CacheFreeMetadata(&md);

  const char *group_keys[] = {"date"};
  OrganizerPlan *plan = OrganizerComputePlan(env_dir, group_keys, 1);
  ASSERT_TRUE(plan != NULL);
  ASSERT_EQ(1, plan->count);

  const char *capture_file = "build/test_plan_output.txt";
  fflush(stdout);
  int saved_stdout = dup(fileno(stdout));
  ASSERT_TRUE(saved_stdout >= 0);
  FILE *redirect = freopen(capture_file, "w", stdout);
  ASSERT_TRUE(redirect != NULL);

  OrganizerPrintPlan(plan);
  fflush(stdout);

  ASSERT_TRUE(dup2(saved_stdout, fileno(stdout)) >= 0);
  close(saved_stdout);

  FILE *f = fopen(capture_file, "r");
  ASSERT_TRUE(f != NULL);
  char buffer[2048] = {0};
  size_t bytes = fread(buffer, 1, sizeof(buffer) - 1, f);
  buffer[bytes] = '\0';
  fclose(f);

  int unknown_lines = 0;
  const char *needle = "_Unknown";
  const char *p = buffer;
  while ((p = strstr(p, needle)) != NULL) {
    unknown_lines++;
    p += strlen(needle);
  }
  ASSERT_EQ(1, unknown_lines);

  OrganizerFreePlan(plan);
  CacheShutdown();
  FsDeleteFile(capture_file);
}

void register_organizer_tests(void) {
  register_test("test_organizer_plan_generation",
                test_organizer_plan_generation, "organizer");
  register_test("test_organizer_compound_grouping",
                test_organizer_compound_grouping, "organizer");
  register_test("test_organizer_manifest_rw", test_organizer_manifest_rw,
                "organizer");
  register_test("test_organizer_print_plan_no_duplicate_last_group",
                test_organizer_print_plan_no_duplicate_last_group, "organizer");
}
