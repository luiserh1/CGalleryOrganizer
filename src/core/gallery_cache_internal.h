#ifndef GALLERY_CACHE_INTERNAL_H
#define GALLERY_CACHE_INTERNAL_H

#include <stddef.h>

#include "cJSON.h"
#include "gallery_cache.h"

extern cJSON *g_cache_root;
extern char g_cache_path[1024];
extern CacheCompressionMode g_cache_compression_mode;
extern int g_cache_compression_level;
extern const size_t DEFAULT_AUTO_THRESHOLD_BYTES;

int CacheObjectSize(const cJSON *obj);
bool CachePopulateMetadataFromEntry(const cJSON *entry, const char *path,
                                    ImageMetadata *out_md);

#endif // GALLERY_CACHE_INTERNAL_H
