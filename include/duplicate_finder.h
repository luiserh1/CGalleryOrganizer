#ifndef DUPLICATE_FINDER_H
#define DUPLICATE_FINDER_H

#include "gallery_cache.h"

typedef struct {
  char original_path[1024];
  char **duplicate_paths;
  int duplicate_count;
} DuplicateGroup;

typedef struct {
  DuplicateGroup *groups;
  int group_count;
} DuplicateReport;

// Finds exact duplicates across the entire cache.
// Scans for MD5 collisions and verifies them cleanly using SHA-256.
DuplicateReport FindExactDuplicates(void);

// Frees the memory allocated for the duplicate report.
void FreeDuplicateReport(DuplicateReport *report);

#endif /* DUPLICATE_FINDER_H */
