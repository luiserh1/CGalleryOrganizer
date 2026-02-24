#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "png_parser.h"

#define PNG_SIGNATURE_SIZE 8
static const unsigned char PNG_SIGNATURE[PNG_SIGNATURE_SIZE] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

#define CHUNK_IHDR 0x49484452
#define CHUNK_IEND 0x49454E44
#define CHUNK_TEXt 0x74455874
#define CHUNK_ZTXt 0x7A545874
#define CHUNK_EXIF 0x65584966 // "eXIf"

typedef struct {
  unsigned int length;
  unsigned int type;
} PngChunkHeader;

static unsigned int read_u32_be(const unsigned char *buf) {
  return ((unsigned int)buf[0] << 24) | ((unsigned int)buf[1] << 16) |
         ((unsigned int)buf[2] << 8) | (unsigned int)buf[3];
}

static int read_chunk_header(FILE *f, PngChunkHeader *header) {
  unsigned char len_buf[4];
  if (fread(len_buf, 1, 4, f) != 4)
    return -1;

  header->length = read_u32_be(len_buf);

  unsigned char type_buf[4];
  if (fread(type_buf, 1, 4, f) != 4)
    return -1;

  header->type = ((unsigned int)type_buf[0] << 24) |
                 ((unsigned int)type_buf[1] << 16) |
                 ((unsigned int)type_buf[2] << 8) | (unsigned int)type_buf[3];

  return 0;
}

static void skip_chunk_data(FILE *f, unsigned int length) {
  fseek(f, length, SEEK_CUR);
  unsigned char crc[4];
  fread(crc, 1, 4, f);
}

static void read_ihdr_data(FILE *f, int *width, int *height) {
  unsigned char ihdr_data[13];
  if (fread(ihdr_data, 1, 13, f) != 13)
    return;

  *width = ((int)ihdr_data[0] << 24) | ((int)ihdr_data[1] << 16) |
           ((int)ihdr_data[2] << 8) | (int)ihdr_data[3];
  *height = ((int)ihdr_data[4] << 24) | ((int)ihdr_data[5] << 16) |
            ((int)ihdr_data[6] << 8) | (int)ihdr_data[7];
}

PngData PngParse(const char *filepath) {
  PngData metadata = {0};

  if (!filepath)
    return metadata;

  FILE *f = fopen(filepath, "rb");
  if (!f)
    return metadata;

  unsigned char signature[PNG_SIGNATURE_SIZE];
  if (fread(signature, 1, PNG_SIGNATURE_SIZE, f) != PNG_SIGNATURE_SIZE) {
    fclose(f);
    return metadata;
  }

  if (memcmp(signature, PNG_SIGNATURE, PNG_SIGNATURE_SIZE) != 0) {
    fclose(f);
    return metadata;
  }

  PngChunkHeader chunk;
  while (read_chunk_header(f, &chunk) == 0) {
    if (chunk.type == CHUNK_IHDR) {
      read_ihdr_data(f, &metadata.width, &metadata.height);
      skip_chunk_data(f, chunk.length);
    } else if (chunk.type == CHUNK_IEND) {
      break;
    } else if (chunk.type == CHUNK_EXIF) {
      unsigned char *exif_buf = malloc(chunk.length);
      if (exif_buf) {
        if (fread(exif_buf, 1, chunk.length, f) == chunk.length) {
          ExifData parsed_exif = ParseRawExifBlock(exif_buf, chunk.length);
          if (parsed_exif.valid) {
            metadata.has_exif = true;
            metadata.exif = parsed_exif;
          }
        }
        free(exif_buf);
      }
      unsigned char crc[4];
      fread(crc, 1, 4, f);
    } else {
      skip_chunk_data(f, chunk.length);
    }
  }

  fclose(f);

  return metadata;
}

bool PngGetDimensions(const char *filepath, int *width, int *height) {
  if (!filepath || !width || !height)
    return false;

  PngData metadata = PngParse(filepath);
  if (metadata.width <= 0 || metadata.height <= 0)
    return false;

  *width = metadata.width;
  *height = metadata.height;

  return true;
}
