# CGalleryOrganizer

A high-performance, privacy-first tool written in C for analyzing, classifying, and organizing large image and video galleries. 

## 🚀 Overview

**CGalleryOrganizer** is designed for users who want to manage massive photo libraries locally, without relying on cloud services. It uses a JSON-backed caching system to ensure that expensive metadata extraction and analysis are only performed once per file.

### Key Features (v0.2.3)
- **Compound Directory Grouping**: Reorganize your media library dynamically using multi-level configurable tags (e.g., `--group-by camera,date,resolution`) with automatic incremental index file renaming (`20240815_143000_CameraModel_1.jpg`) to guarantee filesystem uniqueness.
- **Rollback Engine**: Safely rollback entire restructuring moves via a tracking manifest (`--rollback`).
- **Rich Metadata Extraction**:
    - **Basic FS Data**: Size, modification timestamps, absolute paths.
    - **Universal Format Parsing**: Fully integrated with the native C++ **Exiv2** library to robustly extract dimensions, dates, models, GPS, and orientation data from JPEG, PNG, WebP, GIF, BMP, HEIC, and others. Exiv2's magic-byte architecture effortlessly bypasses fake file extensions and corrupted headers without triggering buffer crashes.
- **Smart JSON Caching**: Incremental updates and auto-invalidation when files are modified.
- **Test-Driven**: 100% test coverage for core filesystem and caching logic.
- **Zero-Dependency Core**: Pure C core utilizing only `cJSON`, cryptographic hashing vendors, and a single system-bound Exiv2 bridge.

## 🛠 Build & Run

### Prerequisites
- A C compiler (GCC or Clang)
- `make`

### Installation
```bash
git clone https://github.com/luiserh1/CGalleryOrganizer.git
cd CGalleryOrganizer
make
```

### Usage

**1. Standard Passive Scan (Metadata Caching Only):**
Run the scanner against any directory:
```bash
./build/bin/gallery_organizer /path/to/your/photos
```

**2. Preview Reorganization Plan:**
To see exactly how files will be organized into `_YYYY/_MM` folders without moving anything:
```bash
./build/bin/gallery_organizer /path/to/your/photos /path/to/target/env --preview
```

**3. Execute Organization (Interactive):**
To move files based on EXIF dates:
```bash
./build/bin/gallery_organizer /path/to/your/photos /path/to/target/env --organize
```
*Note: This creates a `.cache` array and a `manifest.json` tracker inside the target environment.*

**4. Compound Organizing:**
You can chain dynamic metadata tags such as `camera`, `date`, `format`, `resolution`, and `orientation` using the `--group-by` argument natively to nest directories mathematically:
```bash
./build/bin/gallery_organizer /path/to/your/photos /path/to/target/env --organize --group-by camera,date,orientation
```

**5. Rollback execution (Undo):**
If you want to revert a recent organization action back to the original directories:
```bash
./build/bin/gallery_organizer /path/to/target/env /path/to/target/env --rollback
```

### Run Tests
```bash
make test
```

## 🗺 Roadmap

- **v0.2.0**: Exact duplicate detection via file hashing and automated action routines (rename/move).
- **v0.3.0**: Machine Learning integration (OCR, Face Detection) and "Vision Analyzer" heuristic tree.
- **v0.4.0**: Visual similarity engine (Near-duplicate detection).
- **v0.5.0+**: Interactive macOS native / Raylib UI.

## ⚖️ License
TBD
