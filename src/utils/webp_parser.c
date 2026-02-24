#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp_parser.h"

#define RIFF_SIGNATURE_SIZE 4
static const char RIFF_SIGNATURE[RIFF_SIGNATURE_SIZE] = {'R', 'I', 'F', 'F'};
static const char WEBP_SIGNATURE[4] = {'W', 'E', 'B', 'P'};

#define CHUNK_VP8 0x56503820
#define CHUNK_VP8L 0x5650384C
#define CHUNK_VP8X 0x56503858

typedef struct {
    char signature[4];
    unsigned int file_size;
    char webp[4];
} RiffHeader;

typedef struct {
    unsigned int chunk_type;
    unsigned int chunk_size;
} ChunkHeader;

static unsigned int read_u32_le(const unsigned char *buf) {
    return ((unsigned int)buf[0]) | ((unsigned int)buf[1] << 8) |
           ((unsigned int)buf[2] << 16) | ((unsigned int)buf[3] << 24);
}

static int read_riff_header(FILE *f, RiffHeader *header) {
    unsigned char buf[12];
    if (fread(buf, 1, 12, f) != 12)
        return -1;

    memcpy(header->signature, buf, 4);
    header->file_size = read_u32_le(buf + 4);
    memcpy(header->webp, buf + 8, 4);

    return 0;
}

static int read_chunk_header(FILE *f, ChunkHeader *header) {
    unsigned char buf[8];
    if (fread(buf, 1, 8, f) != 8)
        return -1;

    header->chunk_type = read_u32_le(buf);
    header->chunk_size = read_u32_le(buf + 4);

    return 0;
}

static int parse_vp8_chunk(FILE *f, int *width, int *height) {
    unsigned char frame_tag[3];
    if (fread(frame_tag, 1, 3, f) != 3)
        return -1;

    int key_frame = !(frame_tag[0] & 1);
    if (!key_frame)
        return -1;

    // For key frames, check start code (0x9d 0x01 0x2a)
    unsigned char start_code[3];
    if (fread(start_code, 1, 3, f) != 3)
        return -1;
    
    if (start_code[0] != 0x9d || start_code[1] != 0x01 || start_code[2] != 0x2a)
        return -1;

    // Read dimensions (2 bytes each, little endian)
    unsigned char width_bytes[2];
    unsigned char height_bytes[2];
    if (fread(width_bytes, 1, 2, f) != 2)
        return -1;
    if (fread(height_bytes, 1, 2, f) != 2)
        return -1;

    *width = width_bytes[0] | (width_bytes[1] << 8);
    *height = height_bytes[0] | (height_bytes[1] << 8);

    return 0;
}

static int parse_vp8l_chunk(FILE *f, int *width, int *height) {
    unsigned char signature;
    if (fread(&signature, 1, 1, f) != 1)
        return -1;

    if (signature != 0x2F)
        return -1;

    unsigned char bits;
    if (fread(&bits, 1, 1, f) != 1)
        return -1;

    *width = (read_u32_le(&bits) & 0x3FFF) + 1;
    *height = ((read_u32_le(&bits) >> 14) & 0x3FFF) + 1;

    return 0;
}

static int parse_vp8x_chunk(FILE *f, unsigned int chunk_size, int *width,
                            int *height) {
    fseek(f, chunk_size, SEEK_CUR);

    ChunkHeader inner_chunk;
    if (read_chunk_header(f, &inner_chunk) != 0)
        return -1;

    if (inner_chunk.chunk_type == CHUNK_VP8) {
        return parse_vp8_chunk(f, width, height);
    } else if (inner_chunk.chunk_type == CHUNK_VP8L) {
        return parse_vp8l_chunk(f, width, height);
    }

    return -1;
}

WebpData WebpParse(const char *filepath) {
    WebpData metadata = {0};

    if (!filepath)
        return metadata;

    FILE *f = fopen(filepath, "rb");
    if (!f)
        return metadata;

    RiffHeader riff;
    if (read_riff_header(f, &riff) != 0) {
        fclose(f);
        return metadata;
    }

    if (memcmp(riff.signature, RIFF_SIGNATURE, RIFF_SIGNATURE_SIZE) != 0 ||
        memcmp(riff.webp, WEBP_SIGNATURE, 4) != 0) {
        fclose(f);
        return metadata;
    }

    ChunkHeader chunk;
    while (read_chunk_header(f, &chunk) == 0) {
        if (chunk.chunk_type == CHUNK_VP8X) {
            if (parse_vp8x_chunk(f, chunk.chunk_size, &metadata.width,
                                &metadata.height) == 0) {
                fclose(f);
                return metadata;
            }
        } else if (chunk.chunk_type == CHUNK_VP8) {
            if (parse_vp8_chunk(f, &metadata.width, &metadata.height) == 0) {
                fclose(f);
                return metadata;
            }
        } else if (chunk.chunk_type == CHUNK_VP8L) {
            if (parse_vp8l_chunk(f, &metadata.width, &metadata.height) == 0) {
                fclose(f);
                return metadata;
            }
        }

        if (chunk.chunk_size % 2 != 0)
            chunk.chunk_size++;
        fseek(f, chunk.chunk_size, SEEK_CUR);
    }

    fclose(f);

    return metadata;
}

bool WebpGetDimensions(const char *filepath, int *width, int *height) {
    if (!filepath || !width || !height)
        return false;

    WebpData metadata = WebpParse(filepath);
    if (metadata.width <= 0 || metadata.height <= 0)
        return false;

    *width = metadata.width;
    *height = metadata.height;

    return true;
}
