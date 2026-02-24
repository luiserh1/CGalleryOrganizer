#ifndef GALLERY_CACHE_H
#define GALLERY_CACHE_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_FEATURE_PRINT 128

typedef struct {
  char *path;              // Absolute path (owned dynamically)
  double modificationDate; // Unix timestamp
  long fileSize;           // File size in bytes
  // ML Heuristics (v0.2.0+)
  double textDensity;
  double featurePrint[MAX_FEATURE_PRINT];
  int featurePrintSize;
  bool isNature;
  bool hasPeopleOrAnimals;
  double maxFaceArea;
  bool hasBarcode;
  double barcodeArea;
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

// Helper to extract basic metadata from disk
bool ExtractBasicMetadata(const char *absolute_path, double *out_mod_date,
                          long *out_size);

#endif // GALLERY_CACHE_H
