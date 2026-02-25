# Dependencies

This document lists third-party and system dependencies used by CGalleryOrganizer.

**Last consulted:** 2026-02-25

## Vendored Dependencies (`vendor/`)

### 1. cJSON
- Purpose: cache JSON parsing/writing (`gallery_cache.json`).
- Location: `vendor/cJSON.c`, `vendor/cJSON.h`
- License: MIT
- Source: <https://github.com/DaveGamble/cJSON>
- Modifications: none.

### 2. MD5 utility
- Purpose: fast content hashing for duplicate grouping.
- Location: `vendor/md5.c`, `vendor/md5.h`
- License: bundled upstream license.
- Modifications: none.

### 3. SHA-256 utility
- Purpose: secondary hash verification for exact duplicate confirmation.
- Location: `vendor/sha256.c`, `vendor/sha256.h`
- License: bundled upstream license.
- Modifications: none.

## System Dependency

### Exiv2 (C++ library)
- Purpose: metadata extraction across JPEG/PNG/WebP/GIF/BMP/HEIC and related formats.
- Installation:
  - macOS: `brew install exiv2`
  - Linux (Debian/Ubuntu): `apt install libexiv2-dev`
- Integration point: `include/exiv2_wrapper.h`, `src/utils/exiv2_wrapper.cpp`

## Optional Stress-Test Utilities

These are only needed for `make stress` dataset acquisition and benchmark runs.

### Tools
- `curl`
- `unzip`

### Dataset source used by script
- URL host: `images.cocodataset.org`
- Default dataset: COCO 2017 validation zip (`val2017.zip`, ~815MB compressed, ~5k images)
- Heavy option: COCO 2017 training zip (`train2017.zip`, ~19GB compressed)
- Script: `scripts/download_stress_dataset.sh`
