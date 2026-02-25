#include "exiv2_wrapper.h"

#include <cstring>
#include <exiv2/exiv2.hpp>
#include <iostream>
#include <string>

extern "C" {

static double parse_gps_coordinate(const Exiv2::Value &value) {
  if (value.count() < 3)
    return 0.0;

  double d = value.toRational(0).first / (double)value.toRational(0).second;
  double m = value.toRational(1).first / (double)value.toRational(1).second;
  double s = value.toRational(2).first / (double)value.toRational(2).second;

  return d + (m / 60.0) + (s / 3600.0);
}

bool ExtractMetadataExiv2(const char *filepath, ImageMetadata *out_metadata) {
  if (!filepath || !out_metadata)
    return false;

  try {
    Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(filepath);
    if (image.get() == 0)
      return false;

    image->readMetadata();

    out_metadata->width = image->pixelWidth();
    out_metadata->height = image->pixelHeight();

    // Try getting dims from EXIF if missing
    if (out_metadata->width == 0 || out_metadata->height == 0) {
      Exiv2::ExifData &exifData = image->exifData();
      if (!exifData.empty()) {
        if (exifData.findKey(Exiv2::ExifKey("Exif.Photo.PixelXDimension")) !=
            exifData.end()) {
          out_metadata->width =
              std::stol(exifData["Exif.Photo.PixelXDimension"].toString());
        }
        if (exifData.findKey(Exiv2::ExifKey("Exif.Photo.PixelYDimension")) !=
            exifData.end()) {
          out_metadata->height =
              std::stol(exifData["Exif.Photo.PixelYDimension"].toString());
        }
      }
    }

    Exiv2::ExifData &exifData = image->exifData();
    if (exifData.empty()) {
      return true; // Dims extracted, no more EXIF
    }

    auto makePos = exifData.findKey(Exiv2::ExifKey("Exif.Image.Make"));
    if (makePos != exifData.end()) {
      std::string make = makePos->toString();
      strncpy(out_metadata->cameraMake, make.c_str(), METADATA_MAX_STRING - 1);
    }

    auto modelPos = exifData.findKey(Exiv2::ExifKey("Exif.Image.Model"));
    if (modelPos != exifData.end()) {
      std::string model = modelPos->toString();
      strncpy(out_metadata->cameraModel, model.c_str(),
              METADATA_MAX_STRING - 1);
    }

    auto datePos =
        exifData.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
    if (datePos != exifData.end()) {
      std::string date = datePos->toString();
      strncpy(out_metadata->dateTaken, date.c_str(), METADATA_MAX_STRING - 1);
    }

    auto orientPos = exifData.findKey(Exiv2::ExifKey("Exif.Image.Orientation"));
    if (orientPos != exifData.end()) {
      out_metadata->orientation = std::stol(orientPos->toString());
    }

    auto latPos = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLatitude"));
    auto latRef =
        exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLatitudeRef"));
    auto lonPos = exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLongitude"));
    auto lonRef =
        exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLongitudeRef"));

    if (latPos != exifData.end() && lonPos != exifData.end()) {
      double lat = parse_gps_coordinate(latPos->value());
      double lon = parse_gps_coordinate(lonPos->value());

      if (latRef != exifData.end() && latRef->toString() == "S")
        lat = -lat;
      if (lonRef != exifData.end() && lonRef->toString() == "W")
        lon = -lon;

      out_metadata->hasGps = true;
      out_metadata->gpsLatitude = lat;
      out_metadata->gpsLongitude = lon;
    }

    return true;
  } catch (Exiv2::Error &e) {
    // Failed
    return false;
  }
}

} // extern "C"
