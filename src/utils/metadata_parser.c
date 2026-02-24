#include <string.h>

#include "metadata_parser.h"
#include "exif_parser.h"
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

static bool is_png(const char *ext) {
    return ext && strcmp(ext, ".png") == 0;
}

static bool is_webp(const char *ext) {
    return ext && strcmp(ext, ".webp") == 0;
}

ImageMetadata ExtractMetadata(const char *filepath) {
    ImageMetadata metadata = {0};

    if (!filepath)
        return metadata;

    const char *ext = get_extension(filepath);

    if (is_jpeg(ext)) {
        ExifData exif = ExifParse(filepath);
        if (exif.valid) {
            strncpy(metadata.dateTaken, exif.dateTaken, METADATA_MAX_STRING - 1);
            metadata.width = exif.width;
            metadata.height = exif.height;
            strncpy(metadata.cameraMake, exif.cameraMake, METADATA_MAX_STRING - 1);
            strncpy(metadata.cameraModel, exif.cameraModel, METADATA_MAX_STRING - 1);
            metadata.orientation = exif.orientation;
            metadata.hasGps = exif.hasGps;
            metadata.gpsLatitude = exif.gpsLatitude;
            metadata.gpsLongitude = exif.gpsLongitude;
        }
    } else if (is_png(ext)) {
        PngData png = PngParse(filepath);
        if (png.width > 0 && png.height > 0) {
            metadata.width = png.width;
            metadata.height = png.height;
        }
    } else if (is_webp(ext)) {
        WebpData webp = WebpParse(filepath);
        if (webp.width > 0 && webp.height > 0) {
            metadata.width = webp.width;
            metadata.height = webp.height;
        }
    }

    return metadata;
}
