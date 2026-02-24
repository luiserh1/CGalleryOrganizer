#ifndef METADATA_PARSER_H
#define METADATA_PARSER_H

#include <stdbool.h>

#include "gallery_cache.h"

ImageMetadata ExtractMetadata(const char *filepath);

#endif
