# CGalleryOrganizer

A high-performance, privacy-first tool written in C for analyzing, classifying, and organizing large image and video galleries. 

## 🚀 Overview

**CGalleryOrganizer** is designed for users who want to manage massive photo libraries locally, without relying on cloud services. It uses a JSON-backed caching system to ensure that expensive metadata extraction and analysis are only performed once per file.

### Key Features (v0.1.5)
- **Extreme Performance**: Recursive directory scanning for 10k+ files in seconds.
- **Rich Metadata Extraction**:
    - **Basic FS Data**: Size, modification timestamps, absolute paths.
    - **JPEG EXIF Parsing**: Hand-written, zero-dependency parser for Date Taken, Image Dimensions, Camera Model, and GPS Coordinates.
    - **PNG Support**: Extracts Image Dimensions from PNG files.
    - **WebP Support**: Extracts Image Dimensions from WebP files.
    - **GIF Support**: Extracts Image Dimensions from GIF files.
    - **HEIC Support**: Extracts Image Dimensions from HEIC files.
- **Smart JSON Caching**: Incremental updates and auto-invalidation when files are modified.
- **Test-Driven**: 100% test coverage for core filesystem and caching logic.
- **Zero-Dependency Core**: Minimalist implementation with only `cJSON` as a vendored dependency.

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
./bin/gallery_organizer /path/to/your/photos
```
This will generate a `gallery_cache.json` file in the current directory containing all extracted metadata.

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
