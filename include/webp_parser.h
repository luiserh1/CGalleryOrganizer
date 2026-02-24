#ifndef WEBP_PARSER_H
#define WEBP_PARSER_H

#include <stdbool.h>

#include "gallery_cache.h"

typedef ImageMetadata WebpData;

WebpData WebpParse(const char *filepath);

bool WebpGetDimensions(const char *filepath, int *width, int *height);

#endif
