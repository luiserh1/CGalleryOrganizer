#ifndef ORGANIZER_INTERNAL_H
#define ORGANIZER_INTERNAL_H

#include <stddef.h>

#include "fs_utils.h"
#include "gallery_cache.h"
#include "organizer.h"

void OrganizerCopyStringBounded(char *dst, size_t dst_size, const char *src);
void OrganizerSanitizeFilename(char *name);
void OrganizerBuildNewFilename(ImageMetadata *md, char *out_buffer,
                               size_t out_size);
void OrganizerResolveGroupCriteria(ImageMetadata *md, const char *key,
                                   char *out_buffer, size_t out_size);
void OrganizerRemoveEmptyParents(const char *file_path, const char *stop_dir);

#endif // ORGANIZER_INTERNAL_H
