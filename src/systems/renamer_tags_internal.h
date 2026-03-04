#ifndef RENAMER_TAGS_INTERNAL_H
#define RENAMER_TAGS_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"
#include "systems/renamer_tags.h"

#define RENAMER_TAG_LIMIT 128

void RenamerTagsSetError(char *out_error, size_t out_error_size,
                         const char *fmt, ...);

bool RenamerTagsLoadFileText(const char *path, char **out_text);

bool RenamerTagsSaveFileText(const char *path, const char *text);

bool RenamerTagsTagListContains(char **tags, int count, const char *candidate);

bool RenamerTagsTagListAppend(char **tags, int *io_count, const char *tag);

void RenamerTagsTagListFree(char **tags, int count);

void RenamerTagsTagListJoin(char **tags, int count, char *out_text,
                            size_t out_text_size, const char *fallback);

bool RenamerTagsJSONArrayContainsTag(const cJSON *array, const char *tag);

void RenamerTagsJSONArrayRemoveTag(cJSON *array, const char *tag);

bool RenamerTagsJSONArrayAddTag(cJSON *array, const char *tag);

bool RenamerTagsParseCsvTags(const char *csv, char **out_tags, int *out_count);

bool RenamerTagsParseTagsFromNode(const cJSON *node, char **out_tags,
                                  int *out_count);

void RenamerTagsNowUtc(char *out_text, size_t out_text_size);

cJSON *RenamerTagsEnsurePathEntry(cJSON *root, const char *absolute_path);

#endif // RENAMER_TAGS_INTERNAL_H
