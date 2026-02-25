# CGalleryOrganizer

CGalleryOrganizer is a local-first C/C++ CLI for scanning media folders, extracting metadata, caching results, finding exact duplicates, and organizing files into metadata-based directory trees.

## Key Features (v0.2.4)
- Recursive media scan with cache invalidation by file size and modification timestamp.
- Metadata extraction through Exiv2 (dimensions, date taken, camera, GPS, orientation).
- Optional exhaustive metadata capture with `--exhaustive`.
- Exact duplicate detection using MD5 with SHA-256 verification.
- Interactive organization plan/execute flow with `--preview`, `--organize`, and `--group-by`.
- Rollback via `manifest.json` with backward-compatible and ergonomic invocation.

## Build

### Prerequisites
- Clang or GCC
- `make`
- Exiv2 development package (`brew install exiv2` on macOS)

### Compile
```bash
make
```

### Run tests
```bash
make test
```

## CLI Usage

```bash
./build/bin/gallery_organizer <directory_to_scan> [env_dir] [options]
```

### Options
- `-h`, `--help`: show usage.
- `-e`, `--exhaustive`: include all EXIF/IPTC/XMP tags in cache (`allMetadataJson`).
- `--preview`: build and print organization plan without moving files.
- `--organize`: execute organization plan after confirmation.
- `--group-by <keys>`: grouping keys list. Allowed keys: `date,camera,format,orientation,resolution`.
- `--rollback`: restore moved files from `manifest.json`.

### Rollback invocation (both supported)
```bash
./build/bin/gallery_organizer <scan_dir> <env_dir> --rollback
./build/bin/gallery_organizer <env_dir> --rollback
```

## Examples

### Passive scan + cache
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env
```

### Exhaustive scan
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --exhaustive
```

### Preview with compound grouping
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --preview --group-by camera,date,resolution
```

### Organize
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env --organize --group-by date
```

## Project Layout
- `src/`: core implementation.
- `include/`: public headers.
- `tests/`: test framework and test suites.
- `vendor/`: bundled third-party C dependencies.
- `tools/cache_viewer/`: static dashboard for cache inspection.
- `docs/`: project and maintenance documentation.
