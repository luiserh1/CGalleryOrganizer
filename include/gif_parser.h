#ifndef GIF_PARSER_H
#define GIF_PARSER_H

#include <stdbool.h>

#include "gallery_cache.h"

typedef ImageMetadata GifData;

GifData GifParse(const char *filepath);

bool GifGetDimensions(const char *filepath, int *width, int *height);

#endif
