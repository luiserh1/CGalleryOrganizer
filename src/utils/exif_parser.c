#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exif_parser.h"

// EXIF Tag IDs
#define TAG_IMAGE_WIDTH 0x0100
#define TAG_IMAGE_HEIGHT 0x0101
#define TAG_ORIENTATION 0x0112
#define TAG_MAKE 0x010F
#define TAG_MODEL 0x0110
#define TAG_DATE_TIME_ORIGINAL 0x9003
#define TAG_EXIF_WIDTH 0xA002
#define TAG_EXIF_HEIGHT 0xA003
#define TAG_GPS_IFD 0x8825
#define TAG_EXIF_IFD 0x8769

// GPS sub-tags
#define GPS_TAG_LATITUDE_REF 0x0001
#define GPS_TAG_LATITUDE 0x0002
#define GPS_TAG_LONGITUDE_REF 0x0003
#define GPS_TAG_LONGITUDE 0x0004

// EXIF data types
#define EXIF_TYPE_BYTE 1
#define EXIF_TYPE_ASCII 2
#define EXIF_TYPE_SHORT 3
#define EXIF_TYPE_LONG 4
#define EXIF_TYPE_RATIONAL 5

// Read helpers that respect endianness
static uint16_t read_u16(const unsigned char *buf, bool big_endian) {
  if (big_endian) {
    return (uint16_t)((buf[0] << 8) | buf[1]);
  }
  return (uint16_t)((buf[1] << 8) | buf[0]);
}

static uint32_t read_u32(const unsigned char *buf, bool big_endian) {
  if (big_endian) {
    return (uint32_t)((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
  }
  return (uint32_t)((buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0]);
}

// Convert GPS rational degrees/minutes/seconds to decimal degrees
static double gps_to_decimal(const unsigned char *data, bool big_endian) {
  uint32_t deg_num = read_u32(data, big_endian);
  uint32_t deg_den = read_u32(data + 4, big_endian);
  uint32_t min_num = read_u32(data + 8, big_endian);
  uint32_t min_den = read_u32(data + 12, big_endian);
  uint32_t sec_num = read_u32(data + 16, big_endian);
  uint32_t sec_den = read_u32(data + 20, big_endian);

  double degrees = (deg_den > 0) ? (double)deg_num / deg_den : 0.0;
  double minutes = (min_den > 0) ? (double)min_num / min_den : 0.0;
  double seconds = (sec_den > 0) ? (double)sec_num / sec_den : 0.0;

  return degrees + (minutes / 60.0) + (seconds / 3600.0);
}

// Parse one IFD entry and populate ExifData
static void parse_ifd_entry(const unsigned char *tiff_base, size_t tiff_len,
                            const unsigned char *entry, bool big_endian,
                            ExifData *out) {
  uint16_t tag = read_u16(entry, big_endian);
  uint16_t type = read_u16(entry + 2, big_endian);
  uint32_t count = read_u32(entry + 4, big_endian);

  // Value or offset (4 bytes at entry+8)
  const unsigned char *value_area = entry + 8;

  // For data > 4 bytes, value_area is an offset into tiff_base
  size_t data_size = 0;
  switch (type) {
  case EXIF_TYPE_BYTE:
    data_size = count;
    break;
  case EXIF_TYPE_ASCII:
    data_size = count;
    break;
  case EXIF_TYPE_SHORT:
    data_size = count * 2;
    break;
  case EXIF_TYPE_LONG:
    data_size = count * 4;
    break;
  case EXIF_TYPE_RATIONAL:
    data_size = count * 8;
    break;
  default:
    return; // Unknown type, skip
  }

  const unsigned char *data_ptr;
  if (data_size <= 4) {
    data_ptr = value_area;
  } else {
    uint32_t offset = read_u32(value_area, big_endian);
    if (offset + data_size > tiff_len)
      return; // Bounds check
    data_ptr = tiff_base + offset;
  }

  switch (tag) {
  case TAG_MAKE:
    if (type == EXIF_TYPE_ASCII && count < EXIF_MAX_STRING) {
      memcpy(out->cameraMake, data_ptr, count);
      out->cameraMake[count] = '\0';
    }
    break;
  case TAG_MODEL:
    if (type == EXIF_TYPE_ASCII && count < EXIF_MAX_STRING) {
      memcpy(out->cameraModel, data_ptr, count);
      out->cameraModel[count] = '\0';
    }
    break;
  case TAG_DATE_TIME_ORIGINAL:
    if (type == EXIF_TYPE_ASCII && count < EXIF_MAX_STRING) {
      memcpy(out->dateTaken, data_ptr, count);
      out->dateTaken[count] = '\0';
    }
    break;
  case TAG_ORIENTATION:
    if (type == EXIF_TYPE_SHORT) {
      out->orientation = read_u16(data_ptr, big_endian);
    }
    break;
  case TAG_IMAGE_WIDTH:
  case TAG_EXIF_WIDTH:
    if (type == EXIF_TYPE_SHORT) {
      out->width = read_u16(data_ptr, big_endian);
    } else if (type == EXIF_TYPE_LONG) {
      out->width = (int)read_u32(data_ptr, big_endian);
    }
    break;
  case TAG_IMAGE_HEIGHT:
  case TAG_EXIF_HEIGHT:
    if (type == EXIF_TYPE_SHORT) {
      out->height = read_u16(data_ptr, big_endian);
    } else if (type == EXIF_TYPE_LONG) {
      out->height = (int)read_u32(data_ptr, big_endian);
    }
    break;
  default:
    break;
  }
}

// Parse a GPS IFD
static void parse_gps_ifd(const unsigned char *tiff_base, size_t tiff_len,
                          uint32_t gps_offset, bool big_endian, ExifData *out) {
  if (gps_offset + 2 > tiff_len)
    return;

  const unsigned char *ifd_ptr = tiff_base + gps_offset;
  uint16_t num_entries = read_u16(ifd_ptr, big_endian);
  ifd_ptr += 2;

  char lat_ref = 0;
  char lon_ref = 0;
  double lat = 0.0;
  double lon = 0.0;
  bool has_lat = false;
  bool has_lon = false;

  for (int i = 0; i < num_entries; i++) {
    if ((size_t)(ifd_ptr - tiff_base) + 12 > tiff_len)
      break;

    uint16_t tag = read_u16(ifd_ptr, big_endian);
    uint16_t type = read_u16(ifd_ptr + 2, big_endian);
    uint32_t count = read_u32(ifd_ptr + 4, big_endian);
    const unsigned char *value_area = ifd_ptr + 8;

    switch (tag) {
    case GPS_TAG_LATITUDE_REF:
      if (type == EXIF_TYPE_ASCII && count >= 1) {
        lat_ref = (char)value_area[0];
      }
      break;
    case GPS_TAG_LATITUDE:
      if (type == EXIF_TYPE_RATIONAL && count == 3) {
        uint32_t offset = read_u32(value_area, big_endian);
        if (offset + 24 <= tiff_len) {
          lat = gps_to_decimal(tiff_base + offset, big_endian);
          has_lat = true;
        }
      }
      break;
    case GPS_TAG_LONGITUDE_REF:
      if (type == EXIF_TYPE_ASCII && count >= 1) {
        lon_ref = (char)value_area[0];
      }
      break;
    case GPS_TAG_LONGITUDE:
      if (type == EXIF_TYPE_RATIONAL && count == 3) {
        uint32_t offset = read_u32(value_area, big_endian);
        if (offset + 24 <= tiff_len) {
          lon = gps_to_decimal(tiff_base + offset, big_endian);
          has_lon = true;
        }
      }
      break;
    }
    ifd_ptr += 12;
  }

  if (has_lat && has_lon) {
    out->hasGps = true;
    out->gpsLatitude = (lat_ref == 'S') ? -lat : lat;
    out->gpsLongitude = (lon_ref == 'W') ? -lon : lon;
  }
}

// Parse an IFD (Image File Directory)
static void parse_ifd(const unsigned char *tiff_base, size_t tiff_len,
                      uint32_t ifd_offset, bool big_endian, ExifData *out,
                      bool follow_sub_ifds) {
  if (ifd_offset + 2 > tiff_len)
    return;

  const unsigned char *ifd_ptr = tiff_base + ifd_offset;
  uint16_t num_entries = read_u16(ifd_ptr, big_endian);
  ifd_ptr += 2;

  for (int i = 0; i < num_entries; i++) {
    if ((size_t)(ifd_ptr - tiff_base) + 12 > tiff_len)
      break;

    uint16_t tag = read_u16(ifd_ptr, big_endian);

    if (tag == TAG_EXIF_IFD && follow_sub_ifds) {
      // Follow the EXIF sub-IFD
      uint32_t sub_offset = read_u32(ifd_ptr + 8, big_endian);
      parse_ifd(tiff_base, tiff_len, sub_offset, big_endian, out, false);
    } else if (tag == TAG_GPS_IFD && follow_sub_ifds) {
      // Follow the GPS sub-IFD
      uint32_t gps_off = read_u32(ifd_ptr + 8, big_endian);
      parse_gps_ifd(tiff_base, tiff_len, gps_off, big_endian, out);
    } else {
      parse_ifd_entry(tiff_base, tiff_len, ifd_ptr, big_endian, out);
    }

    ifd_ptr += 12;
  }
}

ExifData ParseRawExifBlock(const unsigned char *data, size_t len) {
  ExifData result;
  memset(&result, 0, sizeof(ExifData));

  if (!data || len < 8)
    return result;

  const unsigned char *tiff_base = data;
  size_t tiff_len = len;

  // Check for standard "Exif\0\0" prefix used in APP1 and some other containers
  if (len >= 14 && data[0] == 'E' && data[1] == 'x' && data[2] == 'i' &&
      data[3] == 'f' && data[4] == 0x00 && data[5] == 0x00) {
    tiff_base = data + 6;
    tiff_len = len - 6;
  }

  // Determine byte order
  bool big_endian;
  bool order_valid = true;
  if (tiff_base[0] == 'M' && tiff_base[1] == 'M') {
    big_endian = true;
  } else if (tiff_base[0] == 'I' && tiff_base[1] == 'I') {
    big_endian = false;
  } else {
    order_valid = false; // Invalid byte order
  }

  if (order_valid) {
    // Verify TIFF magic (42)
    uint16_t magic = read_u16(tiff_base + 2, big_endian);
    if (magic == 42) {
      // Get IFD0 offset
      uint32_t ifd0_offset = read_u32(tiff_base + 4, big_endian);

      // Parse IFD0 (links to EXIF sub-IFD and GPS IFD)
      parse_ifd(tiff_base, tiff_len, ifd0_offset, big_endian, &result, true);
      result.valid = true;
    }
  }

  return result;
}

ExifData ExifParse(const char *filepath) {
  ExifData result;
  memset(&result, 0, sizeof(ExifData));

  if (!filepath)
    return result;

  FILE *f = fopen(filepath, "rb");
  if (!f)
    return result;

  // Read first 64KB — EXIF is always near the start
  size_t buf_size = 65536;
  unsigned char *buf = malloc(buf_size);
  if (!buf) {
    fclose(f);
    return result;
  }

  size_t bytes_read = fread(buf, 1, buf_size, f);
  fclose(f);

  if (bytes_read < 12) {
    free(buf);
    return result;
  }

  // Verify JPEG SOI marker
  if (buf[0] != 0xFF || buf[1] != 0xD8) {
    free(buf);
    return result; // Not a JPEG
  }

  // Search for APP1 (EXIF) marker
  size_t pos = 2;
  while (pos + 4 < bytes_read) {
    if (buf[pos] != 0xFF) {
      pos++;
      continue;
    }

    uint8_t marker = buf[pos + 1];
    uint16_t segment_len = (uint16_t)((buf[pos + 2] << 8) | buf[pos + 3]);

    if (marker == 0xE1) { // APP1
      // Check for "Exif\0\0" header
      if (pos + 10 < bytes_read && buf[pos + 4] == 'E' && buf[pos + 5] == 'x' &&
          buf[pos + 6] == 'i' && buf[pos + 7] == 'f' && buf[pos + 8] == 0x00 &&
          buf[pos + 9] == 0x00) {

        if (segment_len >= 2) {
          size_t payload_len = segment_len - 2;
          if (pos + 4 + payload_len <= bytes_read) {
            ExifData parsed = ParseRawExifBlock(buf + pos + 4, payload_len);
            if (parsed.valid) {
              result = parsed;
            }
          }
        }
      }
    } else if (marker >= 0xC0 && marker <= 0xC3 && marker != 0xC4 &&
               marker != 0xC8 && marker != 0xCC) { // SOF0, SOF1, SOF2, etc
      // Found SOF marker, extract basic dimensions
      if (pos + 8 < bytes_read) {
        int height = (buf[pos + 5] << 8) | buf[pos + 6];
        int width = (buf[pos + 7] << 8) | buf[pos + 8];
        if (result.width == 0 && result.height == 0) {
          result.width = width;
          result.height = height;
        }
      }
      // If we only needed dimensions, we could break here, but we keep scanning
      // for EXIF if not found yet
      if (result.valid)
        break;
    }

    // Skip to next segment
    pos += 2 + segment_len;
  }

  free(buf);
  return result;
}
