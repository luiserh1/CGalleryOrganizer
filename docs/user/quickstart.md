# Quickstart

This guide gets you from clone to first useful output quickly.

## 1. Install prerequisites

See [../dependencies.md](../dependencies.md) for full details by platform.

Minimum:
- C compiler (Clang or GCC)
- `make`
- Exiv2 development package

Optional:
- ONNX Runtime (for ML/similarity enrichment)
- raylib (for GUI build/run)

## 2. Build

```bash
make
```

## 3. Run tests (sanity check)

```bash
make test
```

## 4. Prepare source and environment folders

- Source directory: media files you want to analyze.
- Environment directory: output/cache/history state for CGalleryOrganizer.

Example:
```bash
mkdir -p /tmp/cgo_source /tmp/cgo_env
```

## 5. First scan

```bash
./build/bin/gallery_organizer /tmp/cgo_source /tmp/cgo_env
```

Expected outputs include cache files under:
- `/tmp/cgo_env/.cache/`

## 6. Optional: install models and run similarity

```bash
make models
./build/bin/gallery_organizer /tmp/cgo_source /tmp/cgo_env --similarity-report
```

Expected report:
- `/tmp/cgo_env/similarity_report.json`

## 7. Optional: use GUI frontend

```bash
make gui
make run-gui
```

If GUI state gets stuck, reset it:
```bash
./build/bin/gallery_organizer_gui --reset-state
```

## Next

- Task recipes: [workflows.md](workflows.md)
- CLI reference: [cli.md](cli.md)
- GUI guide: [gui.md](gui.md)
