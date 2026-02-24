#ifndef FS_UTILS_H
#define FS_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_PATH_LENGTH 1024

// Callback signature for directory traversal
// Return false to stop traversal, true to continue
typedef bool (*FsWalkCallback)(const char *absolute_path, void *user_data);

// Walks a directory recursively and calls the callback for each file
// Returns true on success, false on error
bool FsWalkDirectory(const char *root_path, FsWalkCallback callback,
                     void *user_data);

// Checks if a file has a supported media extension (e.g., .jpg, .png, .mp4)
bool FsIsSupportedMedia(const char *path);

// Gets the absolute path from a relative path
// Returns true on success
bool FsGetAbsolutePath(const char *path, char *out_path, size_t out_size);

#endif // FS_UTILS_H
