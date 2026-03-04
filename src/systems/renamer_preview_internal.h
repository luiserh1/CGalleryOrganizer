#ifndef RENAMER_PREVIEW_INTERNAL_H
#define RENAMER_PREVIEW_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "metadata_parser.h"
#include "systems/renamer_preview.h"

typedef struct {
  char **paths;
  int count;
  int capacity;
} RenamerPreviewPathList;

void RenamerPreviewSetError(char *out_error, size_t out_error_size,
                            const char *fmt, ...);

void RenamerPreviewNowUtc(char *out_text, size_t out_text_size);

bool RenamerPreviewEnsureRenameCachePaths(const char *env_dir,
                                          char *out_config_path,
                                          size_t out_config_path_size,
                                          char *out_preview_dir,
                                          size_t out_preview_dir_size);

bool RenamerPreviewLoadFileText(const char *path, char **out_text);

bool RenamerPreviewSaveFileText(const char *path, const char *text);

bool RenamerPreviewBuildFingerprint(const char *target_dir,
                                    const char *pattern,
                                    const RenamerPreviewItem *items,
                                    int item_count,
                                    char *out_fingerprint,
                                    size_t out_fingerprint_size);

void RenamerPreviewExtractTokensFromMetadata(
    const ImageMetadata *md, char *out_date, size_t out_date_size,
    char *out_time, size_t out_time_size, char *out_datetime,
    size_t out_datetime_size, char *out_camera, size_t out_camera_size,
    char *out_make, size_t out_make_size, char *out_model,
    size_t out_model_size, char *out_gps_lat, size_t out_gps_lat_size,
    char *out_gps_lon, size_t out_gps_lon_size, char *out_location,
    size_t out_location_size, char *out_format, size_t out_format_size);

bool RenamerPreviewLoadMetadataForPath(const char *absolute_path,
                                       bool *io_cache_dirty,
                                       ImageMetadata *out_metadata,
                                       char *out_error,
                                       size_t out_error_size);

void RenamerPreviewPathListFree(RenamerPreviewPathList *list);

bool RenamerPreviewCollectFilesRecursive(const char *root,
                                         RenamerPreviewPathList *out_list,
                                         char *out_error,
                                         size_t out_error_size);

void RenamerPreviewBuildPreviewId(const char *fingerprint, char *out_preview_id,
                                  size_t out_preview_id_size);

bool RenamerPreviewBuildParentDir(const char *path, char *out_dir,
                                  size_t out_dir_size);

void RenamerPreviewDetectCollisions(RenamerPreviewArtifact *preview);

#endif // RENAMER_PREVIEW_INTERNAL_H
