#ifndef GALLERY_CACHE_H
#define GALLERY_CACHE_H

#include <stdbool.h>
#include <stddef.h>

#define METADATA_MAX_STRING 256

typedef struct {
  char *path;              // Absolute path (owned dynamically)
  double modificationDate; // Unix timestamp (from filesystem)
  long fileSize;           // File size in bytes

  // EXIF fields (populated when available)
  char dateTaken[METADATA_MAX_STRING]; // "YYYY:MM:DD HH:MM:SS" or empty
  int width;
  int height;
  char cameraMake[METADATA_MAX_STRING];  // e.g. "Apple"
  char cameraModel[METADATA_MAX_STRING]; // e.g. "iPhone 14 Pro"
  bool hasGps;
  double gpsLatitude;
  double gpsLongitude;
  int orientation; // 1-8 per EXIF spec, 0 if unknown

  // Exact duplicate detection
  char exactHashMd5[33];

  // Dynamic Metadata (exhaustive capture)
  char *
      allMetadataJson; // Stringified JSON of all tags found (owned dynamically)

  // ML inference (v0.3.0)
  char mlPrimaryClass[128];
  float mlPrimaryClassConfidence;
  int mlTextBoxCount;
  bool mlHasText;
  char mlModelClassification[64];
  char mlModelTextDetection[64];
  char mlModelEmbedding[64];
  int mlEmbeddingDim;
  char *mlEmbeddingBase64; // Base64-encoded embedding bytes (owned dynamically)
  char *mlRawJson; // Provider raw payload (owned dynamically)
} ImageMetadata;

// Caching interface
bool CacheInit(const char *cache_path);
void CacheShutdown(void);

// Saves the entire cache to disk
bool CacheSave(void);

// Adds or updates an entry in the cache. Takes ownership of the char* path
// inside data if success
bool CacheUpdateEntry(const ImageMetadata *data);

// Retrieves metadata for a specific path if valid. Populates out_md.
// Returns false if cache missed or invalid (e.g. file size/date mismatch with
// current FS)
bool CacheGetValidEntry(const char *absolute_path, double current_mod_date,
                        long current_size, ImageMetadata *out_md);

// Retrieves metadata without validating against the filesystem
bool CacheGetRawEntry(const char *key, ImageMetadata *out_md);

// Retrieves the total number of cached entries
int CacheGetEntryCount(void);

// Helper to extract basic metadata from disk
bool ExtractBasicMetadata(const char *absolute_path, double *out_mod_date,
                          long *out_size);

// Gets a list of all raw cache keys (allocated array of allocated strings)
char **CacheGetAllKeys(int *out_count);
void CacheFreeKeys(char **keys, int count);

// Frees dynamically allocated members of ImageMetadata
void CacheFreeMetadata(ImageMetadata *md);

#endif // GALLERY_CACHE_H
