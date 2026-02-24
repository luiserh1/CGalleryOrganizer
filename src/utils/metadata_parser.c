#include <string.h>

#include <stdbool.h>

#include "bmp_parser.h"
#include "exif_parser.h"
#include "gif_parser.h"
#include "heic_parser.h"
#include "metadata_parser.h"
#include "png_parser.h"
#include "webp_parser.h"

static const char *get_extension(const char *path) {
  if (!path)
    return NULL;

  const char *ext = strrchr(path, '.');
  if (ext)
    return ext;

  return NULL;
}

static bool is_jpeg(const char *ext) {
  return ext && (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0);
}

static bool is_png(const char *ext) { return ext && strcmp(ext, ".png") == 0; }

static bool is_webp(const char *ext) {
  return ext && strcmp(ext, ".webp") == 0;
}

static bool is_gif(const char *ext) { return ext && strcmp(ext, ".gif") == 0; }

static bool is_heic(const char *ext) {
  return ext && strcmp(ext, ".heic") == 0;
}

static bool is_bmp(const char *ext) { return ext && strcmp(ext, ".bmp") == 0; }

static void copy_exif_to_metadata(ImageMetadata *metadata,
                                  const ExifData *exif) {
  if (!exif || !exif->valid)
    return;
  strncpy(metadata->dateTaken, exif->dateTaken, METADATA_MAX_STRING - 1);
  strncpy(metadata->cameraMake, exif->cameraMake, METADATA_MAX_STRING - 1);
  strncpy(metadata->cameraModel, exif->cameraModel, METADATA_MAX_STRING - 1);
  metadata->orientation = exif->orientation;
  metadata->hasGps = exif->hasGps;
  metadata->gpsLatitude = exif->gpsLatitude;
  metadata->gpsLongitude = exif->gpsLongitude;
}

ImageMetadata ExtractMetadata(const char *filepath) {
  ImageMetadata metadata = {0};

  if (!filepath)
    return metadata;

  const char *ext = get_extension(filepath);

  if (is_jpeg(ext)) {
    ExifData exif = ExifParse(filepath);
    if (exif.width > 0 && exif.height > 0) {
      metadata.width = exif.width;
      metadata.height = exif.height;
    }
    copy_exif_to_metadata(&metadata, &exif);
  } else if (is_png(ext)) {
    PngData png = PngParse(filepath);
    if (png.width > 0 && png.height > 0) {
      metadata.width = png.width;
      metadata.height = png.height;
    }
    copy_exif_to_metadata(&metadata, &png.exif);
  } else if (is_webp(ext)) {
    WebpData webp = WebpParse(filepath);
    if (webp.width > 0 && webp.height > 0) {
      metadata.width = webp.width;
      metadata.height = webp.height;
    }
    copy_exif_to_metadata(&metadata, &webp.exif);
  } else if (is_gif(ext)) {
    GifData gif = GifParse(filepath);
    if (gif.width > 0 && gif.height > 0) {
      metadata.width = gif.width;
      metadata.height = gif.height;
    }
  } else if (is_heic(ext)) {
    HeicData heic = HeicParse(filepath);
    if (heic.width > 0 && heic.height > 0) {
      metadata.width = heic.width;
      metadata.height = heic.height;
    }
    copy_exif_to_metadata(&metadata, &heic.exif);
  } else if (is_bmp(ext)) {
    BmpData bmp = BmpParse(filepath);
    if (bmp.width > 0 && bmp.height > 0) {
      metadata.width = bmp.width;
      metadata.height = bmp.height;
    }
  }

  return metadata;
}
