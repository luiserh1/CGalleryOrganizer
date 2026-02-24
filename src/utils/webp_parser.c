#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdbool.h>

#include "exif_parser.h"
#include "webp_parser.h"

#define RIFF_SIGNATURE_SIZE 4
static const char RIFF_SIGNATURE[RIFF_SIGNATURE_SIZE] = {'R', 'I', 'F', 'F'};
static const char WEBP_SIGNATURE[4] = {'W', 'E', 'B', 'P'};

// FourCC strings in little-endian uint32
#define CHUNK_VP8 0x20385056  // "VP8 "
#define CHUNK_VP8L 0x4C385056 // "VP8L"
#define CHUNK_VP8X 0x58385056 // "VP8X"
#define CHUNK_EXIF 0x46495845 // "EXIF"

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

  unsigned int raw_width =
      ((unsigned int)width_bytes[0]) | ((unsigned int)width_bytes[1] << 8);
  unsigned int raw_height =
      ((unsigned int)height_bytes[0]) | ((unsigned int)height_bytes[1] << 8);

  *width = raw_width & 0x3FFF;
  *height = raw_height & 0x3FFF;

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
  if (chunk_size < 10) {
    fseek(f, chunk_size, SEEK_CUR);
    return -1;
  }

  unsigned char buf[10];
  if (fread(buf, 1, 10, f) != 10)
    return -1;

  // Width is at offset 4 (24-bit little endian)
  unsigned int canvas_width = ((unsigned int)buf[4]) |
                              ((unsigned int)buf[5] << 8) |
                              ((unsigned int)buf[6] << 16);
  *width = canvas_width + 1;

  // Height is at offset 7 (24-bit little endian)
  unsigned int canvas_height = ((unsigned int)buf[7]) |
                               ((unsigned int)buf[8] << 8) |
                               ((unsigned int)buf[9] << 16);
  *height = canvas_height + 1;

  // Skip the rest of the chunk if it's larger than 10 bytes
  if (chunk_size > 10) {
    fseek(f, chunk_size - 10, SEEK_CUR);
  }

  return 0;
}

WebpData WebpParse(const char *filepath) {
  WebpData result;
  memset(&result, 0, sizeof(WebpData));

  if (!filepath)
    return result;

  FILE *f = fopen(filepath, "rb");
  if (!f)
    return result;

  RiffHeader header;
  if (read_riff_header(f, &header) != 0) {
    fclose(f);
    return result;
  }

  if (memcmp(header.signature, RIFF_SIGNATURE, RIFF_SIGNATURE_SIZE) != 0 ||
      memcmp(header.webp, WEBP_SIGNATURE, 4) != 0) {
    fclose(f);
    return result;
  }

  bool found_dimensions = false;
  bool found_exif = false;

  ChunkHeader chunk;
  while (read_chunk_header(f, &chunk) == 0) {
    long next_chunk_pos = ftell(f) + chunk.chunk_size;
    if (chunk.chunk_size % 2 != 0)
      next_chunk_pos++;

    if (chunk.chunk_type == CHUNK_VP8X && !found_dimensions) {
      if (parse_vp8x_chunk(f, chunk.chunk_size, &result.width,
                           &result.height) == 0) {
        found_dimensions = true;
      }
    } else if (chunk.chunk_type == CHUNK_VP8 && !found_dimensions) {
      if (parse_vp8_chunk(f, &result.width, &result.height) == 0) {
        found_dimensions = true;
      }
    } else if (chunk.chunk_type == CHUNK_VP8L && !found_dimensions) {
      if (parse_vp8l_chunk(f, &result.width, &result.height) == 0) {
        found_dimensions = true;
      }
    } else if (chunk.chunk_type == CHUNK_EXIF && !found_exif) {
      unsigned char *exif_buf = malloc(chunk.chunk_size);
      if (exif_buf) {
        if (fread(exif_buf, 1, chunk.chunk_size, f) == chunk.chunk_size) {
          ExifData parsed_exif = ParseRawExifBlock(exif_buf, chunk.chunk_size);
          if (parsed_exif.valid) {
            result.has_exif = true;
            result.exif = parsed_exif;
          }
          found_exif = true;
        }
        free(exif_buf);
      }
    }

    fseek(f, next_chunk_pos, SEEK_SET);
  }

  fclose(f);
  return result;
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
