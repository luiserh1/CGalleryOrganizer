#ifndef WEBP_PARSER_H
#define WEBP_PARSER_H

#include "exif_parser.h"
#include <stdbool.h>

typedef struct {
  int width;
  int height;
  bool has_exif;
  ExifData exif;
} WebpData;

WebpData WebpParse(const char *filepath);

bool WebpGetDimensions(const char *filepath, int *width, int *height);

#endif
