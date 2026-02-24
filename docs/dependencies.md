# External Dependencies

This file documents all third-party code relied upon by the CGalleryOrganizer project.

**Rule:** All external dependencies MUST be stored in the `/vendor/` directory and must not be mixed with the core `src/` code.

---

## 1. cJSON
- **Purpose**: Parsing and writing the `gallery_cache.json` file.
- **Location**: `vendor/cJSON.c`, `vendor/cJSON.h`
- **License**: MIT License
- **Source**: [https://github.com/DaveGamble/cJSON](https://github.com/DaveGamble/cJSON)
- **Version**: TBD
- **Modifications**: None. Included as-is.

---

## Internal: EXIF Parser
- **Purpose**: Extracting EXIF metadata (date taken, camera model, GPS, dimensions) from JPEG files.
- **Location**: `include/exif_parser.h`, `src/utils/exif_parser.c`
- **Dependencies**: None. Zero-dependency, hand-written JPEG EXIF binary parser using only standard C library.
- **Note**: Only supports JPEG files. Non-JPEG formats (PNG, GIF, HEIC, MOV) do not have their metadata extracted via this parser.
