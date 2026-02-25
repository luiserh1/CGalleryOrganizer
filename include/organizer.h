#ifndef ORGANIZER_H
#define ORGANIZER_H

#include <stdbool.h>

// Initialize the organization session.
// env_dir: The target output directory where the manifest will be stored.
bool OrganizerInit(const char *env_dir);

// Record a planned move.
// original_path: The absolute path of the original file.
// new_path: The absolute path where it will be moved.
bool OrganizerRecordMove(const char *original_path, const char *new_path);

// Save the recorded moves into env_dir/manifest.json.
bool OrganizerSaveManifest(void);

// Shut down the organization session and free memory.
void OrganizerShutdown(void);

// Executes a rollback by reading env_dir/manifest.json and moving files back
// to their original paths.
// Returns the number of files successfully rolled back, or -1 on failure.
int OrganizerRollback(const char *env_dir);

// Represents a single planned move operation.
typedef struct {
  char original_path[1024];
  char new_path[1024];
} OrganizerMove;

// Represents the overall execution plan for the gallery.
typedef struct {
  OrganizerMove *moves;
  int count;
  int capacity;
} OrganizerPlan;

// Computes the reorganization plan based on the current cache state.
// Groups files by _YYYY/_MM/ extracted from DateTaken, or _Unknown.
// The plan must be freed with OrganizerFreePlan.
OrganizerPlan *OrganizerComputePlan(const char *env_dir);

// Print the computed plan to stdout as a visual tree outline.
void OrganizerPrintPlan(OrganizerPlan *plan);

// Execute the final restructuring plan, optionally committing to disk
// and triggering the manifest write.
bool OrganizerExecutePlan(OrganizerPlan *plan);

// Free the allocated memory for an OrganizerPlan.
void OrganizerFreePlan(OrganizerPlan *plan);

#endif // ORGANIZER_H
