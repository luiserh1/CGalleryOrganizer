#include <stdbool.h>
#include <string.h>

#include "exiv2_wrapper.h"
#include "fs_utils.h"
#include "gallery_cache.h"
#include "metadata_parser.h"

ImageMetadata ExtractMetadata(const char *filepath, bool exhaustive) {
  ImageMetadata metadata = {0};

  if (!filepath)
    return metadata;

  if (FsIsSupportedMedia(filepath)) {
    ExtractMetadataExiv2(filepath, &metadata, exhaustive);
  }

  return metadata;
}
