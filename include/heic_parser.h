#ifndef HEIC_PARSER_H
#define HEIC_PARSER_H

#include "exif_parser.h"
#include <stdbool.h>

#include "gallery_cache.h"

typedef struct {
  int width;
  int height;
  bool has_exif;
  ExifData exif;
} HeicData;

HeicData HeicParse(const char *filepath);

bool HeicGetDimensions(const char *filepath, int *width, int *height);

#endif
