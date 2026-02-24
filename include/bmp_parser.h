#ifndef BMP_PARSER_H
#define BMP_PARSER_H

#include <stdbool.h>

#include "gallery_cache.h"

typedef ImageMetadata BmpData;

BmpData BmpParse(const char *filepath);

bool BmpGetDimensions(const char *filepath, int *width, int *height);

#endif
