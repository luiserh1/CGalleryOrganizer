#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gallery_cache.h"
#include "systems/renamer_preview_internal.h"
void RenamerPreviewExtractTokensFromMetadata(const ImageMetadata *md, char *out_date,
                                      size_t out_date_size, char *out_time,
                                      size_t out_time_size,
                                      char *out_datetime,
                                      size_t out_datetime_size,
                                      char *out_camera,
                                      size_t out_camera_size,
                                      char *out_make,
                                      size_t out_make_size,
                                      char *out_model,
                                      size_t out_model_size,
                                      char *out_gps_lat,
                                      size_t out_gps_lat_size,
                                      char *out_gps_lon,
                                      size_t out_gps_lon_size,
                                      char *out_location,
                                      size_t out_location_size,
                                      char *out_format,
                                      size_t out_format_size) {
  if (!md) {
    return;
  }

  if (out_date && out_date_size > 0) {
    out_date[0] = '\0';
  }
  if (out_time && out_time_size > 0) {
    out_time[0] = '\0';
  }
  if (out_datetime && out_datetime_size > 0) {
    out_datetime[0] = '\0';
  }
  if (out_camera && out_camera_size > 0) {
    out_camera[0] = '\0';
  }
  if (out_make && out_make_size > 0) {
    out_make[0] = '\0';
  }
  if (out_model && out_model_size > 0) {
    out_model[0] = '\0';
  }
  if (out_gps_lat && out_gps_lat_size > 0) {
    out_gps_lat[0] = '\0';
  }
  if (out_gps_lon && out_gps_lon_size > 0) {
    out_gps_lon[0] = '\0';
  }
  if (out_location && out_location_size > 0) {
    out_location[0] = '\0';
  }
  if (out_format && out_format_size > 0) {
    out_format[0] = '\0';
  }

  if (md->dateTaken[0] != '\0' && strlen(md->dateTaken) >= 19) {
    if (out_date && out_date_size > 0) {
      snprintf(out_date, out_date_size, "%.4s%.2s%.2s", md->dateTaken,
               md->dateTaken + 5, md->dateTaken + 8);
    }
    if (out_time && out_time_size > 0) {
      snprintf(out_time, out_time_size, "%.2s%.2s%.2s", md->dateTaken + 11,
               md->dateTaken + 14, md->dateTaken + 17);
    }
    if (out_datetime && out_datetime_size > 0) {
      snprintf(out_datetime, out_datetime_size, "%.4s%.2s%.2s-%.2s%.2s%.2s",
               md->dateTaken, md->dateTaken + 5, md->dateTaken + 8,
               md->dateTaken + 11, md->dateTaken + 14, md->dateTaken + 17);
    }
  }

  if (out_make && out_make_size > 0) {
    strncpy(out_make, md->cameraMake, out_make_size - 1);
    out_make[out_make_size - 1] = '\0';
  }

  if (out_model && out_model_size > 0) {
    strncpy(out_model, md->cameraModel, out_model_size - 1);
    out_model[out_model_size - 1] = '\0';
  }

  if (out_camera && out_camera_size > 0) {
    if (md->cameraMake[0] != '\0' && md->cameraModel[0] != '\0') {
      snprintf(out_camera, out_camera_size, "%s-%s", md->cameraMake,
               md->cameraModel);
    } else if (md->cameraModel[0] != '\0') {
      strncpy(out_camera, md->cameraModel, out_camera_size - 1);
      out_camera[out_camera_size - 1] = '\0';
    } else if (md->cameraMake[0] != '\0') {
      strncpy(out_camera, md->cameraMake, out_camera_size - 1);
      out_camera[out_camera_size - 1] = '\0';
    }
  }

  if (md->hasGps) {
    if (out_gps_lat && out_gps_lat_size > 0) {
      snprintf(out_gps_lat, out_gps_lat_size, "%c%.6f",
               md->gpsLatitude < 0.0 ? 's' : 'n',
               md->gpsLatitude < 0.0 ? -md->gpsLatitude : md->gpsLatitude);
    }
    if (out_gps_lon && out_gps_lon_size > 0) {
      snprintf(out_gps_lon, out_gps_lon_size, "%c%.6f",
               md->gpsLongitude < 0.0 ? 'w' : 'e',
               md->gpsLongitude < 0.0 ? -md->gpsLongitude : md->gpsLongitude);
    }
    if (out_location && out_location_size > 0) {
      snprintf(out_location, out_location_size, "%s-%s",
               out_gps_lat && out_gps_lat[0] != '\0' ? out_gps_lat
                                                     : "unknown-gps-lat",
               out_gps_lon && out_gps_lon[0] != '\0' ? out_gps_lon
                                                     : "unknown-gps-lon");
    }
  }

  if (out_format && out_format_size > 0 && md->path) {
    const char *dot = strrchr(md->path, '.');
    if (dot && dot[1] != '\0') {
      for (size_t i = 1; dot[i] != '\0' && i < out_format_size; i++) {
        out_format[i - 1] = (char)tolower((unsigned char)dot[i]);
        out_format[i] = '\0';
      }
    }
  }
}

bool RenamerPreviewLoadMetadataForPath(const char *absolute_path,
                                bool *io_cache_dirty,
                                ImageMetadata *out_metadata,
                                char *out_error, size_t out_error_size) {
  if (!absolute_path || !io_cache_dirty || !out_metadata) {
    RenamerPreviewSetError(out_error, out_error_size,
             "invalid metadata load arguments for renamer preview");
    return false;
  }

  memset(out_metadata, 0, sizeof(*out_metadata));

  double mod_date = 0.0;
  long file_size = 0;
  if (!ExtractBasicMetadata(absolute_path, &mod_date, &file_size)) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to read file metadata for '%s'", absolute_path);
    return false;
  }

  ImageMetadata cached = {0};
  if (CacheGetValidEntry(absolute_path, mod_date, file_size, &cached)) {
    bool needs_refresh = false;
    if (cached.dateTaken[0] == '\0') {
      needs_refresh = true;
    }
    if (cached.cameraMake[0] == '\0' && cached.cameraModel[0] == '\0') {
      needs_refresh = true;
    }
    if (!cached.allMetadataJson || cached.allMetadataJson[0] == '\0') {
      needs_refresh = true;
    }

    if (!needs_refresh) {
      *out_metadata = cached;
      return true;
    }

    CacheFreeMetadata(&cached);
  }

  ImageMetadata fresh = ExtractMetadata(absolute_path, true);
  fresh.path = strdup(absolute_path);
  if (!fresh.path) {
    CacheFreeMetadata(&fresh);
    RenamerPreviewSetError(out_error, out_error_size,
             "out of memory while preparing metadata for '%s'", absolute_path);
    return false;
  }
  fresh.modificationDate = mod_date;
  fresh.fileSize = file_size;

  if (!CacheUpdateEntry(&fresh)) {
    CacheFreeMetadata(&fresh);
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to refresh cache entry for '%s'", absolute_path);
    return false;
  }

  *io_cache_dirty = true;
  *out_metadata = fresh;
  return true;
}
