# CGalleryOrganizer

A high-performance, privacy-first tool written in C for analyzing, classifying, and organizing large image and video galleries. 

## 🚀 Overview

**CGalleryOrganizer** is designed for users who want to manage massive photo libraries locally, without relying on cloud services. It uses a JSON-backed caching system to ensure that expensive metadata extraction and analysis are only performed once per file.

### Key Features (v0.2.1)
- **Extreme Performance**: Recursive directory scanning for 10k+ files in seconds.
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
Run the scanner against any directory:
```bash
./build/bin/gallery_organizer /path/to/your/photos
```
This will generate a `.cache/gallery_cache.json` file inside the project directory containing all extracted metadata.

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
