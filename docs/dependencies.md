# Dependencies

This file documents all third-party code relied upon by the CGalleryOrganizer project, as well as the self-contained internally developed functionality that is not part of the core of the project.

**Last consulted:** 2026-02-25

---

## External Dependencies

**Rule:** All lightweight external C dependencies MUST be stored in the `/vendor/` directory and must not be mixed with the core `src/` code. System-level heavy libraries (like `exiv2`) are exempted and should be installed via the hosting OS package manager natively.

### 1. cJSON
- **Purpose**: Parsing and writing the `gallery_cache.json` file.
- **Location**: `vendor/cJSON.c`, `vendor/cJSON.h`
- **License**: MIT License
- **Source**: [https://github.com/DaveGamble/cJSON](https://github.com/DaveGamble/cJSON)
- **Version**: TBD
- **Modifications**: None. Included as-is.

### 2. MD5 Hashing Utility
- **Purpose**: Fast cryptographic hashing for exact file duplication detection.
- **Location**: `vendor/md5.c`, `vendor/md5.h`
- **License**: Public Domain / Standard Open Source
- **Modifications**: None. Included as-is.

### 3. SHA-256 Hashing Utility
- **Purpose**: Secure cryptographic hashing as a fallback for MD5 duplicates verification.
- **Location**: `vendor/sha256.c`, `vendor/sha256.h`
- **License**: Public Domain / Standard Open Source
- **Modifications**: None. Included as-is.

---

## System Dependencies

### 1. Exiv2 (Native C++ Library)
- **Purpose**: Unified, robust metadata and EXIF extraction for all standard image formats (JPEG, PNG, WebP, GIF, BMP, HEIC, etc.). Replaced the strictly limited legacy hand-written ad-hoc binary parsers.
- **Installation**: `brew install exiv2` (macOS) / `apt install libexiv2-dev` (Linux)
- **License**: GPL
- **Version**: v0.28+ Target
- **Integration**: The project utilizes a pure C interface facade (`include/exiv2_wrapper.h` & `src/utils/exiv2_wrapper.cpp`) compiled via `clang++` to bridge the C++ Exiv2 objects safely back to the core C metadata structures.
