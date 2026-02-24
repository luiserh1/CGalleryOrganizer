#ifndef EXIF_PARSER_H
#define EXIF_PARSER_H

#include <stdbool.h>
#include <stddef.h>

#define EXIF_MAX_STRING 256

// Parsed EXIF data from a JPEG file
typedef struct {
  bool valid; // Whether EXIF data was found

  // Date & Time
  char dateTaken[EXIF_MAX_STRING]; // "YYYY:MM:DD HH:MM:SS" or empty

  // Image Dimensions
  int width;
  int height;

  // Device Info
  char cameraMake[EXIF_MAX_STRING];  // e.g. "Apple"
  char cameraModel[EXIF_MAX_STRING]; // e.g. "iPhone 14 Pro"

  // GPS
  bool hasGps;
  double gpsLatitude;  // Decimal degrees (negative = South)
  double gpsLongitude; // Decimal degrees (negative = West)

  // Orientation
  int orientation; // 1-8 per EXIF spec, 0 if unknown
} ExifData;

// Parses raw EXIF data block (starting either with "Exif\0\0" or TIFF header
// directly) Returns an ExifData struct. Check .valid to see if parsing
// succeeded.
ExifData ParseRawExifBlock(const unsigned char *data, size_t len);

// Parses EXIF data from a JPEG file.
// Returns an ExifData struct. Check .valid to see if parsing succeeded.
ExifData ExifParse(const char *filepath);

#endif // EXIF_PARSER_H
