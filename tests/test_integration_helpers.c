#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "cJSON.h"
#include "fs_utils.h"

#include "integration_test_helpers.h"

int RunCommandCapture(const char *cmd, char *output, size_t output_size) {
  if (!cmd || !output || output_size == 0) {
    return -1;
  }

  output[0] = '\0';
  FILE *pipe = popen(cmd, "r");
  if (!pipe) {
    return -1;
  }

  size_t used = 0;
  while (!feof(pipe) && used < output_size - 1) {
    size_t n = fread(output + used, 1, output_size - 1 - used, pipe);
    if (n == 0) {
      break;
    }
    used += n;
  }
  output[used] = '\0';

  int status = pclose(pipe);
  if (status == -1) {
    return -1;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return -1;
}

static bool LoadFileText(const char *path, char **out_text) {
  if (!path || !out_text) {
    return false;
  }
  *out_text = NULL;

  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return false;
  }
  rewind(f);

  char *buf = malloc((size_t)size + 1);
  if (!buf) {
    fclose(f);
    return false;
  }
  size_t n = fread(buf, 1, (size_t)size, f);
  fclose(f);
  buf[n] = '\0';
  *out_text = buf;
  return true;
}

bool ReportsEquivalentIgnoringTimestamp(const char *left_path,
                                        const char *right_path) {
  char *left_text = NULL;
  char *right_text = NULL;
  if (!LoadFileText(left_path, &left_text) ||
      !LoadFileText(right_path, &right_text)) {
    free(left_text);
    free(right_text);
    return false;
  }

  cJSON *left = cJSON_Parse(left_text);
  cJSON *right = cJSON_Parse(right_text);
  free(left_text);
  free(right_text);
  if (!left || !right) {
    cJSON_Delete(left);
    cJSON_Delete(right);
    return false;
  }

  cJSON_DeleteItemFromObjectCaseSensitive(left, "generatedAt");
  cJSON_DeleteItemFromObjectCaseSensitive(right, "generatedAt");

  char *left_norm = cJSON_PrintUnformatted(left);
  char *right_norm = cJSON_PrintUnformatted(right);
  cJSON_Delete(left);
  cJSON_Delete(right);
  if (!left_norm || !right_norm) {
    free(left_norm);
    free(right_norm);
    return false;
  }

  bool same = strcmp(left_norm, right_norm) == 0;
  free(left_norm);
  free(right_norm);
  return same;
}

bool WriteRollbackManifest(const char *env_dir) {
  char orig_dir[1024];
  char new_dir[1024];
  snprintf(orig_dir, sizeof(orig_dir), "%s/orig", env_dir);
  snprintf(new_dir, sizeof(new_dir), "%s/new", env_dir);

  if (!FsMakeDirRecursive(orig_dir) || !FsMakeDirRecursive(new_dir)) {
    return false;
  }

  char orig_file[1024];
  char new_file[1024];
  snprintf(orig_file, sizeof(orig_file), "%s/file.jpg", orig_dir);
  snprintf(new_file, sizeof(new_file), "%s/file.jpg", new_dir);

  FILE *f = fopen(new_file, "w");
  if (!f) {
    return false;
  }
  fputs("dummy", f);
  fclose(f);

  f = fopen(orig_file, "w");
  if (!f) {
    return false;
  }
  fputs("dummy", f);
  fclose(f);

  char abs_orig[1024] = {0};
  char abs_new[1024] = {0};
  if (!FsGetAbsolutePath(orig_file, abs_orig, sizeof(abs_orig)) ||
      !FsGetAbsolutePath(new_file, abs_new, sizeof(abs_new))) {
    return false;
  }

  remove(orig_file);

  char manifest_path[1024];
  snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", env_dir);
  f = fopen(manifest_path, "w");
  if (!f) {
    return false;
  }
  fprintf(f, "[{\"original\":\"%s\",\"new\":\"%s\"}]", abs_orig,
          abs_new);
  fclose(f);
  return true;
}

bool WriteBootstrapModels(const char *models_dir) {
  if (!FsMakeDirRecursive(models_dir)) {
    return false;
  }

  char clf_path[1024];
  char text_path[1024];
  char embed_path[1024];
  snprintf(clf_path, sizeof(clf_path), "%s/clf-default.onnx", models_dir);
  snprintf(text_path, sizeof(text_path), "%s/text-default.onnx", models_dir);
  snprintf(embed_path, sizeof(embed_path), "%s/embed-default.onnx", models_dir);

  FILE *f = fopen(clf_path, "w");
  if (!f) {
    return false;
  }
  fputs("dummy-clf", f);
  fclose(f);

  f = fopen(text_path, "w");
  if (!f) {
    return false;
  }
  fputs("dummy-text", f);
  fclose(f);

  f = fopen(embed_path, "w");
  if (!f) {
    return false;
  }
  fputs("dummy-embed", f);
  fclose(f);

  return true;
}
