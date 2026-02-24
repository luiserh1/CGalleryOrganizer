#ifndef PNG_PARSER_H
#define PNG_PARSER_H

#include "exif_parser.h"
#include <stdbool.h>

typedef struct {
  int width;
  int height;
  bool has_exif;
  ExifData exif;
} PngData;

PngData PngParse(const char *filepath);

bool PngGetDimensions(const char *filepath, int *width, int *height);

#endif
