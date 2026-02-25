#ifndef EXIV2_WRAPPER_H
#define EXIV2_WRAPPER_H

#include "gallery_cache.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ExtractMetadataExiv2(const char *filepath, ImageMetadata *out_metadata,
                          bool exhaustive);

#ifdef __cplusplus
}
#endif

#endif // EXIV2_WRAPPER_H
