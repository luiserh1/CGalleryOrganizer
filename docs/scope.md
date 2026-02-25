# CGalleryOrganizer - Scope And Roadmap

## Overview

**Project Name:** CGalleryOrganizer  
**Technology:** C  
**Platforms:** macOS (validated via Objective-C/Swift bridging later), Linux/Windows (optional for CLI)

CGalleryOrganizer is a local, privacy-first application designed to analyze, classify, and organize a directory of images and videos. The project is a C-based port of the "Unified Gallery Organizer" concept, aiming for extreme performance, minimal dependencies, and a granular integration of Machine Learning heuristics.

---

## Core Design Principles

- **Zero-to-Minimal Vendor Dependencies**: Third-party C header packages (JSON parsers, ML frameworks) are heavily isolated in `vendor/`. Heavy system-level libraries (like `exiv2` for robust format parsing) are integrated natively via the host OS package manager and securely abstracted behind pure C wrappers. All dependencies are documented in `docs/dependencies.md`.
- **Granular Evolution**: Features are strictly bounded by minor version bumps (0.1.0 -> 0.2.0, etc.).
- **Strict Data Contract**: The application revolves around a JSON-backed cache (`.cache/gallery_cache.json`) to avoid re-computing expensive heuristics.
- **TDD Driven**: All logic (FS parsing, caching, hashing, math) is tested via a custom, lightweight framework without relying on external testing libraries.

---

## v0.1.0 Scope: Core Foundation & Basic Metadata (Completed)

### Primary Goal
Establish the project structure, testing framework, code style, and implement basic directory traversal and metadata extraction.

### Core Architecture
- Directories for `src/`, `include/`, `tests/`, `vendor/`, and `docs/`.
- Custom, lightweight testing framework.

### Features
- Recursive directory traversal for supported extensions (images/videos).
- Basic metadata extraction:
  - Absolute Path
  - File Size (bytes)
  - Modification Date (Unix Timestamp)
- JSON Cache integration (`.cache/gallery_cache.json`) using a lightweight vendor library (e.g., `cJSON`).
  - Cache invalidation logic based on file size and timestamp.

---

## v0.2.0 Scope: Exact Duplicates & Actions (Completed)

### Primary Goal
Implement file hashing to detect exact duplicates and introduce action routines based on caching results.

### Features
- Exact duplicate detection via fast file hashing (e.g., MD5 or SHA-256).
- Abstracted file action operations:
  - Renaming (e.g., to timestamp pattern).
  - Moving/Reordering into subdirectories based on metadata.
- Grouping logic for exact hashes.

---

## v0.2.1 Scope: Dynamic Metadata & Dashboard (Completed)

### Primary Goal
Expand metadata extraction with exhaustive tags and evolve the visualization cache dashboard.

### Features
- Hybrid C metadata struct supporting dynamic JSON extraction for EXIF, IPTC, and XMP.
- Exiv2 C++ wrapper iterative parsing controlled via a `--exhaustive` flag.
- Evolved cache viewer dashboard with row expansion and `tag:value` deep search capabilities.
- Persistent logging of benchmark results (`build/benchmark_history.log`).

---

## v0.2.2 Scope: Active Gallery Organization (Completed)

### Primary Goal
Transition from a passive analyzer to an active organizer. Introduce interactive CLI execution, robust environment management (rollback manifest), and metadata-based folder staging.

### Features
- Interactive CLI with `--organize`, `--preview`, and `--rollback` arguments.
- Environment Directory paradigm (hosts caches and manifest).
- Rollback manifest (`history.json`) for safe reversal of filesystem changes.
- Virtual tree generation and metadata-based configurable renaming mapping (e.g., Year/Month).

---

## v0.2.3 Scope: Compound Grouping & File Renaming (Working On)

### Primary Goal
Advance the organization engine horizontally to support complex, multi-level folder hierarchies using sequential metadata keys while guaranteeing filesystem uniqueness via incremental collision-proof renaming.

### Features
- New CLI argument `--group-by <keys>` (e.g., `camera,date`).
- Compound path building in the `organizer` (e.g., `_Sony/_2013/_03`).
- Dynamic renaming engine resolving to `YYYYMMDD_HHMMSS_CameraModel.ext`.
- Fallback collision safeguards using incremental `_1`, `_2` indexes.

---

## v0.3.0 Scope: ML Integration & Vision Analyzer

### Primary Goal
Integrate Machine Learning capabilities (OCR, Object Detection, Image Classification) to evaluate the heuristic decision tree.

### Features
- Evaluate and integrate a C-compatible ML runtime (e.g., ONNX Runtime or `ggml`, heavily abstracted).
- Populate the heuristic fields in the cache:
  - `textDensity`, `isNature`, `hasPeopleOrAnimals`, `maxFaceArea`, `hasBarcode`, `barcodeArea`.
- Implement the "Vision Analyzer" Decision Tree to classify images as `KEEP` (Gallery Worthy) or `DISCARD` (Meme, Document, Text Heavy).

---

## v0.4.0 Scope: Similarity Engine

### Primary Goal
Compute visual similarities to find near-duplicates (bursts, crops).

### Features
- Extract `featurePrint` (embedding vector) using the ML models.
- Implement Cosine Similarity math engine.
- Grouping and threshold logic for near-duplicates.

---

## v0.5.0+ Scope: Interactive UI

### Primary Goal
Provide a user-friendly way to visualize results, review discards, and manage the gallery.

### Features
- Integrate a C-compatible GUI (Raylib, SDL) or macOS native UI via Objective-C.
- Interactive grid/list for review.
