#ifndef HEIC_PARSER_H
#define HEIC_PARSER_H

#include <stdbool.h>

#include "gallery_cache.h"

typedef ImageMetadata HeicData;

HeicData HeicParse(const char *filepath);

bool HeicGetDimensions(const char *filepath, int *width, int *height);

#endif
