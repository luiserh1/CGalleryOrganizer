#include <stdio.h>
#include <string.h>

#include "bmp_parser.h"

#define BMP_SIGNATURE_SIZE 2
static const char BMP_SIGNATURE[BMP_SIGNATURE_SIZE] = {'B', 'M'};

typedef struct {
    unsigned int bi_size;
    int bi_width;
    int bi_height;
} BmpInfoHeader;

static unsigned int read_u32_le(const unsigned char *buf) {
    return ((unsigned int)buf[0]) | ((unsigned int)buf[1] << 8) |
           ((unsigned int)buf[2] << 16) | ((unsigned int)buf[3] << 24);
}

static int read_s32_le(const unsigned char *buf) {
    return (int)read_u32_le(buf);
}

BmpData BmpParse(const char *filepath) {
    BmpData metadata = {0};

    if (!filepath)
        return metadata;

    FILE *f = fopen(filepath, "rb");
    if (!f)
        return metadata;

    unsigned char signature[2];
    if (fread(signature, 1, 2, f) != 2) {
        fclose(f);
        return metadata;
    }

    if (memcmp(signature, BMP_SIGNATURE, BMP_SIGNATURE_SIZE) != 0) {
        fclose(f);
        return metadata;
    }

    fseek(f, 10, SEEK_SET);

    unsigned char info_header[4];
    if (fread(info_header, 1, 4, f) != 4) {
        fclose(f);
        return metadata;
    }

    unsigned int header_size = read_u32_le(info_header);

    if (header_size < 12) {
        fclose(f);
        return metadata;
    }

    fseek(f, 14 + header_size, SEEK_SET);

    unsigned char dims[8];
    if (fread(dims, 1, 8, f) != 8) {
        fclose(f);
        return metadata;
    }

    if (header_size >= 40) {
        metadata.width = read_s32_le(dims);
        metadata.height = read_s32_le(dims + 4);
    } else {
        metadata.width = (int)(unsigned short)dims[0] | ((int)(unsigned short)dims[1] << 8);
        metadata.height = (int)(unsigned short)dims[2] | ((int)(unsigned short)dims[3] << 8);
    }

    if (metadata.height < 0)
        metadata.height = -metadata.height;

    fclose(f);

    return metadata;
}

bool BmpGetDimensions(const char *filepath, int *width, int *height) {
    if (!filepath || !width || !height)
        return false;

    BmpData metadata = BmpParse(filepath);
    if (metadata.width <= 0 || metadata.height <= 0)
        return false;

    *width = metadata.width;
    *height = metadata.height;

    return true;
}
