#include <stdio.h>
#include <string.h>

#include "gif_parser.h"

#define GIF_SIGNATURE_SIZE 6
static const char GIF87_SIGNATURE[GIF_SIGNATURE_SIZE] = "GIF87a";
static const char GIF89_SIGNATURE[GIF_SIGNATURE_SIZE] = "GIF89a";

GifData GifParse(const char *filepath) {
    GifData metadata = {0};

    if (!filepath)
        return metadata;

    FILE *f = fopen(filepath, "rb");
    if (!f)
        return metadata;

    unsigned char header[10];
    if (fread(header, 1, 10, f) != 10) {
        fclose(f);
        return metadata;
    }

    if (memcmp(header, GIF87_SIGNATURE, GIF_SIGNATURE_SIZE) != 0 &&
        memcmp(header, GIF89_SIGNATURE, GIF_SIGNATURE_SIZE) != 0) {
        fclose(f);
        return metadata;
    }

    metadata.width = (int)header[6] | ((int)header[7] << 8);
    metadata.height = (int)header[8] | ((int)header[9] << 8);

    fclose(f);

    return metadata;
}

bool GifGetDimensions(const char *filepath, int *width, int *height) {
    if (!filepath || !width || !height)
        return false;

    GifData metadata = GifParse(filepath);
    if (metadata.width <= 0 || metadata.height <= 0)
        return false;

    *width = metadata.width;
    *height = metadata.height;

    return true;
}
