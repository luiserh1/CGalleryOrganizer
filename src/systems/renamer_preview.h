#ifndef RENAMER_PREVIEW_H
#define RENAMER_PREVIEW_H

#include <stdbool.h>
#include <stddef.h>

#include "fs_utils.h"
#include "systems/renamer_tags.h"

#define RENAMER_ID_MAX 64

typedef struct {
  char source_path[MAX_PATH_LENGTH];
  char candidate_filename[256];
  char candidate_path[MAX_PATH_LENGTH];
  char tags_manual[256];
  char tags_meta[256];
  char tags_merged[256];
  double source_mod_time;
  long source_size;
  bool truncated;
  bool collision_in_batch;
  bool collision_on_disk;
} RenamerPreviewItem;

typedef struct {
  char preview_id[RENAMER_ID_MAX];
  char created_at_utc[32];
  char env_dir[MAX_PATH_LENGTH];
  char target_dir[MAX_PATH_LENGTH];
  char pattern[256];
  char fingerprint[33];
  int file_count;
  int collision_group_count;
  int collision_count;
  int truncation_count;
  int metadata_field_count;
  char metadata_fields[RENAMER_META_FIELD_MAX][RENAMER_META_FIELD_KEY_MAX];
  bool requires_auto_suffix_acceptance;
  RenamerPreviewItem *items;
} RenamerPreviewArtifact;

typedef struct {
  const char *env_dir;
  const char *target_dir;
  const char *pattern;
  const char *tags_map_path;
  const char *tag_add_csv;
  const char *tag_remove_csv;
  const char *meta_tag_add_csv;
  const char *meta_tag_remove_csv;
  bool recursive;
} RenamerPreviewRequest;

bool RenamerPreviewLoadDefaultPattern(const char *env_dir, char *out_pattern,
                                      size_t out_pattern_size,
                                      char *out_error,
                                      size_t out_error_size);

bool RenamerPreviewSaveDefaultPattern(const char *env_dir, const char *pattern,
                                      char *out_error,
                                      size_t out_error_size);

bool RenamerPreviewBuild(const RenamerPreviewRequest *request,
                         RenamerPreviewArtifact *out_preview,
                         char *out_error, size_t out_error_size);

bool RenamerPreviewSaveArtifact(const RenamerPreviewArtifact *preview,
                                char *out_artifact_path,
                                size_t out_artifact_path_size,
                                char *out_error, size_t out_error_size);

bool RenamerPreviewLoadArtifact(const char *env_dir, const char *preview_id,
                                RenamerPreviewArtifact *out_preview,
                                char *out_error, size_t out_error_size);

bool RenamerPreviewRecomputeFingerprint(const RenamerPreviewArtifact *preview,
                                        char *out_fingerprint,
                                        size_t out_fingerprint_size,
                                        char *out_error,
                                        size_t out_error_size);

void RenamerPreviewFree(RenamerPreviewArtifact *preview);

#endif // RENAMER_PREVIEW_H
