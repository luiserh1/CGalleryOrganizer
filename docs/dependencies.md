# Dependencies

This file documents all third-party code relied upon by the CGalleryOrganizer project, as well as the self-contained internally developed functionality that is not part of the core of the project.

---

## External Dependencies

**Rule:** All external dependencies MUST be stored in the `/vendor/` directory and must not be mixed with the core `src/` code.

### 1. cJSON
- **Purpose**: Parsing and writing the `gallery_cache.json` file.
- **Location**: `vendor/cJSON.c`, `vendor/cJSON.h`
- **License**: MIT License
- **Source**: [https://github.com/DaveGamble/cJSON](https://github.com/DaveGamble/cJSON)
- **Version**: TBD
- **Modifications**: None. Included as-is.

---

## Internal Dependencies

### 1. EXIF Parser
- **Purpose**: Extracting EXIF metadata (date taken, camera model, GPS, dimensions) from JPEG files.
- **Location**: `include/exif_parser.h`, `src/utils/exif_parser.c`
- **Dependencies**: None. Zero-dependency, hand-written JPEG EXIF binary parser using only standard C library.
- **Note**: Only supports JPEG files. Non-JPEG formats (PNG, GIF, HEIC, MOV) do not have their metadata extracted via this parser.

### 2. PNG Parser
- **Purpose**: Extracting metadata (dimensions) from PNG files.
- **Location**: `include/png_parser.h`, `src/utils/png_parser.c`
- **Dependencies**: None. Zero-dependency, hand-written PNG binary parser using only standard C library.
- **Note**: Extracts width and height from PNG IHDR chunk. Does not extract text metadata (tEXt chunks).

### 3. WebP Parser
- **Purpose**: Extracting metadata (dimensions) from WebP files.
- **Location**: `include/webp_parser.h`, `src/utils/webp_parser.c`
- **Dependencies**: None. Zero-dependency, hand-written WebP binary parser using only standard C library.
- **Note**: Extracts width and height from WebP VP8/VP8L chunks. Supports both lossy and lossless WebP formats.

### 4. GIF Parser
- **Purpose**: Extracting metadata (dimensions) from GIF files.
- **Location**: `include/gif_parser.h`, `src/utils/gif_parser.c`
- **Dependencies**: None. Zero-dependency, hand-written GIF binary parser using only standard C library.
- **Note**: Extracts width and height from GIF header. Supports both GIF87a and GIF89a formats.

### 5. HEIC Parser
- **Purpose**: Extracting metadata (dimensions) from HEIC files.
- **Location**: `include/heic_parser.h`, `src/utils/heic_parser.c`
- **Dependencies**: None. Zero-dependency, hand-written HEIC binary parser using only standard C library.
- **Note**: Extracts width and height from moov/mvhd atoms. Supports HEIC files using ISOBMFF container format.
