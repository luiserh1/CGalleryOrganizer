#ifndef PNG_PARSER_H
#define PNG_PARSER_H

#include <stdbool.h>

#include "gallery_cache.h"

typedef ImageMetadata PngData;

PngData PngParse(const char *filepath);

bool PngGetDimensions(const char *filepath, int *width, int *height);

#endif
