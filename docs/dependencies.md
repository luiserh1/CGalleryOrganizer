# Dependencies

This document lists third-party and system dependencies used by CGalleryOrganizer.

**Last consulted:** 2026-02-25

## Dependency Philosophy

Priority order for new integrations:
1. Ad-hoc implementation in core project code.
2. Vendored dependency (auditable, minimal footprint).
3. System dependency (when complexity and maintenance cost justify it).

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

## System Dependencies

### 1. Exiv2 (C++ library)
- Purpose: metadata extraction across JPEG/PNG/WebP/GIF/BMP/HEIC and related formats.
- Installation:
  - macOS: `brew install exiv2`
  - Linux (Debian/Ubuntu): `apt install libexiv2-dev`
- Integration point: `include/exiv2_wrapper.h`, `src/utils/exiv2_wrapper.cpp`

### 2. ONNX Runtime (C API)
- Purpose: local ML inference provider backend for ML tasks (`classification`, `text_detection`, `embedding`).
- Installation:
  - macOS: `brew install onnxruntime`
  - Linux: install `onnxruntime` C runtime and headers from distro/vendor package.
- Integration points:
  - Public API: `include/ml_api.h`
  - Provider abstraction: `include/ml_provider.h`
  - ONNX provider: `src/ml/providers/onnx_provider.c`

### 3. zstd (C library, optional)
- Purpose: optional whole-file cache compression (`--cache-compress zstd`).
- Installation:
  - macOS: `brew install zstd`
  - Linux (Debian/Ubuntu): `apt install libzstd-dev`
- Integration points:
  - Cache codec: `src/core/cache_codec.c`
  - Cache runtime: `src/core/gallery_cache.c`

## Model Assets (Not in Git)

Model binaries are downloaded to `build/models/` and are not tracked in git.

- Manifest: `models/manifest.json`
- Downloader: `scripts/download_models.sh`
- Helper target: `make models`
- Attribution registry: `docs/model_assets.md`

Mandatory model metadata fields (enforced by downloader):
- `id`, `task`, `url`, `sha256`, `license_name`, `license_url`, `author`, `source_url`, `credit_text`, `version`, `filename`

Supported `task` values:
- `classification`
- `text_detection`
- `embedding`

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
