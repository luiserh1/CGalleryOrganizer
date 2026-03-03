#ifndef RENAMER_TAGS_H
#define RENAMER_TAGS_H

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"

#define RENAMER_TAG_TEXT_MAX 256
#define RENAMER_META_FIELD_MAX 16
#define RENAMER_META_FIELD_KEY_MAX 96

typedef struct {
  char manual[RENAMER_TAG_TEXT_MAX];
  char meta[RENAMER_TAG_TEXT_MAX];
  char merged[RENAMER_TAG_TEXT_MAX];
} RenamerResolvedTags;

bool RenamerTagsLoadSidecar(const char *env_dir, cJSON **out_root,
                            char *out_error, size_t out_error_size);

bool RenamerTagsSaveSidecar(const char *env_dir, const cJSON *root,
                            char *out_error, size_t out_error_size);

bool RenamerTagsApplyBulkCsv(cJSON *root, const char **absolute_paths,
                             int path_count, const char *tag_add_csv,
                             const char *tag_remove_csv,
                             const char *meta_tag_add_csv,
                             const char *meta_tag_remove_csv, char *out_error,
                             size_t out_error_size);

bool RenamerTagsApplyMapFile(cJSON *root, const char *map_json_path,
                             const char **absolute_paths, int path_count,
                             char *out_error, size_t out_error_size);

bool RenamerTagsResolve(const cJSON *root, const char *absolute_path,
                        const char *all_metadata_json,
                        RenamerResolvedTags *out_tags, char *out_error,
                        size_t out_error_size);

bool RenamerTagsCollectMetadataFields(
    const char *all_metadata_json,
    char out_fields[][RENAMER_META_FIELD_KEY_MAX], int max_fields,
    int *io_count, char *out_error, size_t out_error_size);

bool RenamerTagsMovePathKey(cJSON *root, const char *old_path,
                            const char *new_path);

#endif // RENAMER_TAGS_H
