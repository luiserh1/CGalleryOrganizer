#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdbool.h>

#include "heic_parser.h"

#define BOX_ftyp 0x66747970
#define BOX_moov 0x6D6F6F76
#define BOX_mvhd 0x6D766864
#define BOX_trak 0x7472616B
#define BOX_tkhd 0x746B6864
#define BOX_avc1 0x61766331
#define BOX_avc3 0x61766333
#define BOX_hvc1 0x68766331
#define BOX_hvc3 0x68766333
#define BOX_encv 0x656E6376
#define BOX_ispe 0x69737065

typedef struct {
  unsigned int box_size;
  unsigned int box_type;
} BoxHeader;

static unsigned int read_u32_be(const unsigned char *buf) {
  return ((unsigned int)buf[0] << 24) | ((unsigned int)buf[1] << 16) |
         ((unsigned int)buf[2] << 8) | (unsigned int)buf[3];
}

static int read_box_header(FILE *f, BoxHeader *header) {
  unsigned char buf[8];
  if (fread(buf, 1, 8, f) != 8)
    return -1;

  header->box_size = read_u32_be(buf);
  header->box_type = read_u32_be(buf + 4);

  return 0;
}

static int find_ispe_box(FILE *f, unsigned int box_size, int *width,
                         int *height) {
  unsigned int bytes_read = 0;
  while (bytes_read + 8 <= box_size) {
    BoxHeader child;
    if (read_box_header(f, &child) != 0)
      return -1;

    bytes_read += 8;

    unsigned int child_size = child.box_size;
    if (child_size == 0) {
      // Goes to end of file
      child_size = box_size - bytes_read + 8;
    } else if (child_size < 8) {
      return -1;
    }

    if (child.box_type == BOX_ispe) {
      unsigned char data[8];
      if (fread(data, 1, 8, f) != 8)
        return -1;

      // ispe is a FullBox, so 4 bytes version+flags, then 4 bytes width, 4
      // bytes height
      fseek(f, -8, SEEK_CUR); // rewind to read it properly
      unsigned char full_data[12];
      if (fread(full_data, 1, 12, f) != 12)
        return -1;

      *width = (int)read_u32_be(full_data + 4);
      *height = (int)read_u32_be(full_data + 8);
      return 0;
    } else if (child.box_type == 0x69707270 /*'iprp'*/ ||
               child.box_type == 0x6970636F /*'ipco'*/) {
      // Recursively search container boxes
      off_t current_pos = ftell(f);
      if (find_ispe_box(f, child_size - 8, width, height) == 0) {
        return 0;
      }
      fseek(f, current_pos, SEEK_SET);
    }

    fseek(f, child_size - 8, SEEK_CUR);
    bytes_read += child_size - 8;
  }

  return -1;
}

static void scan_for_exif(FILE *f, HeicData *metadata) {
  long original_pos = ftell(f);
  fseek(f, 0, SEEK_SET);

  const size_t CHUNK_SIZE = 65536;
  const size_t OVERLAP = 16;
  unsigned char *buf = malloc(CHUNK_SIZE);
  if (!buf)
    return;

  size_t bytes_read = fread(buf, 1, CHUNK_SIZE, f);
  while (bytes_read > 0) {
    for (size_t i = 0; i + 6 <= bytes_read; i++) {
      if (buf[i] == 'E' && buf[i + 1] == 'x' && buf[i + 2] == 'i' &&
          buf[i + 3] == 'f' && buf[i + 4] == 0 && buf[i + 5] == 0) {
        long exif_file_offset = ftell(f) - bytes_read + i;
        long current_search_pos = ftell(f);

        fseek(f, exif_file_offset, SEEK_SET);
        unsigned char exif_buf[65536];
        size_t exif_len = fread(exif_buf, 1, sizeof(exif_buf), f);

        ExifData parsed_exif = ParseRawExifBlock(exif_buf, exif_len);
        if (parsed_exif.valid) {
          metadata->has_exif = true;
          metadata->exif = parsed_exif;
          free(buf);
          fseek(f, original_pos, SEEK_SET);
          return;
        }

        fseek(f, current_search_pos, SEEK_SET);
      }
    }

    if (bytes_read < CHUNK_SIZE)
      break;

    fseek(f, -OVERLAP, SEEK_CUR);
    bytes_read = fread(buf, 1, CHUNK_SIZE, f);
  }

  free(buf);
  fseek(f, original_pos, SEEK_SET);
}

HeicData HeicParse(const char *filepath) {
  HeicData metadata = {0};

  if (!filepath)
    return metadata;

  FILE *f = fopen(filepath, "rb");
  if (!f)
    return metadata;

  unsigned char signature[12];
  if (fread(signature, 1, 12, f) != 12) {
    fclose(f);
    return metadata;
  }

  int is_heic = 0;
  if (memcmp(signature + 4, "ftyp", 4) == 0) {
    if (memcmp(signature + 8, "heic", 4) == 0 ||
        memcmp(signature + 8, "mif1", 4) == 0 ||
        memcmp(signature + 8, "msf1", 4) == 0) {
      is_heic = 1;
    }
  }

  if (!is_heic) {
    fseek(f, 0, SEEK_SET);
    if (fread(signature, 1, 8, f) == 8) {
      if (memcmp(signature, "heic", 4) == 0)
        is_heic = 1;
    }
  }

  if (!is_heic) {
    fclose(f);
    return metadata;
  }

  fseek(f, 0, SEEK_SET);
  BoxHeader box;
  int width = 0, height = 0;

  while (read_box_header(f, &box) == 0) {
    if (box.box_type == 0x6D657461) { // 'meta'
      // 'meta' is a FullBox, skip version and flags (4 bytes)
      fseek(f, 4, SEEK_CUR);

      unsigned int meta_payload_size = box.box_size - 12;
      if (find_ispe_box(f, meta_payload_size, &width, &height) == 0) {
        metadata.width = width;
        metadata.height = height;
        break;
      }
    }

    if (box.box_size < 8)
      break;

    fseek(f, box.box_size - 8, SEEK_CUR);
  }

  // After processing everything, if we haven't found exif yet (or just to be
  // sure), scan for it.
  if (!metadata.has_exif) {
    scan_for_exif(f, &metadata);
  }

  fclose(f);

  return metadata;
}

bool HeicGetDimensions(const char *filepath, int *width, int *height) {
  if (!filepath || !width || !height)
    return false;

  HeicData metadata = HeicParse(filepath);
  if (metadata.width <= 0 || metadata.height <= 0)
    return false;

  *width = metadata.width;
  *height = metadata.height;

  return true;
}
