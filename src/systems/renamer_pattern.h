#ifndef RENAMER_PATTERN_H
#define RENAMER_PATTERN_H

#include <stdbool.h>
#include <stddef.h>

#define RENAMER_PATTERN_TOKEN_MAX 64
#define RENAMER_FILENAME_COMPONENT_MAX 180

typedef struct {
  const char *date;
  const char *time;
  const char *datetime;
  const char *camera;
  const char *make;
  const char *model;
  const char *format;
  const char *gps_lat;
  const char *gps_lon;
  const char *location;
  const char *tags_manual;
  const char *tags_meta;
  const char *tags;
  int index;
} RenamerPatternContext;

const char *RenamerPatternDefault(void);

bool RenamerPatternValidate(const char *pattern, char *out_error,
                            size_t out_error_size);

bool RenamerPatternRender(const char *pattern, const RenamerPatternContext *ctx,
                          char *out_filename, size_t out_filename_size,
                          bool *out_truncated, char *out_warning,
                          size_t out_warning_size, char *out_error,
                          size_t out_error_size);

void RenamerPatternSanitizeSlug(char *text);

#endif // RENAMER_PATTERN_H
