# Test Assets

This document catalogs image fixtures used by integration and robustness tests.

## Asset Registry

| Filename | Format | Relative Path | Expected Dimensions | EXIF Expectation |
|---|---|---|---|---|
| `sample_exif.jpg` | JPEG | `tests/assets/jpg/sample_exif.jpg` | 240x160 | Present |
| `sample_no_exif.jpg` | JPEG | `tests/assets/jpg/sample_no_exif.jpg` | 240x160 | Absent |
| `sample_deep_exif.jpg` | JPEG | `tests/assets/jpg/sample_deep_exif.jpg` | >0x>0 | Present (GPS validated) |
| `sample_no_exif.png` | PNG | `tests/assets/png/sample_no_exif.png` | 113x161 | Absent |
| `sample_exif.png` | PNG | `tests/assets/png/sample_exif.png` | 113x161 | Present |
| `fake.png` | PNG extension / JPEG content | `tests/assets/png/fake.png` | 240x160 | Parsed by magic bytes |
| `sample_no_exif.webp` | WebP | `tests/assets/webp/sample_no_exif.webp` | 400x400 | Absent |
| `sample_exif.webp` | WebP | `tests/assets/webp/sample_exif.webp` | 400x400 | Present |
| `sample_no_exif.gif` | GIF | `tests/assets/gif/sample_no_exif.gif` | 500x331 | Not expected |
| `sample_no_exif.bmp` | BMP | `tests/assets/bmp/sample_no_exif.bmp` | 256x151 | Not expected |
| `truncated.bmp` | BMP (truncated) | `tests/assets/bmp/truncated.bmp` | 256x151 | Robustness case |
| `sample_no_exif.heic` | HEIC | `tests/assets/heic/sample_no_exif.heic` | 256x192 | Absent |
| `sample_exif.heic` | HEIC | `tests/assets/heic/sample_exif.heic` | 256x192 | Limited/format-dependent |
| `bird*.JPG` set | JPEG variants | `tests/assets/duplicates/*` | N/A | Duplicate-detection fixtures |

## Sources and Licensing

Primary origins include Flickr and Wikimedia Commons assets with permissive or attribution-compatible licenses (public domain, CC BY, CC BY-SA). Modified variants were generated for metadata and corruption edge-case testing.

Any additional files under `tests/assets/` not listed above are considered derivative fixtures for duplicate, truncation, or metadata mutation scenarios.

## Adding New Assets

1. Place files under the appropriate `tests/assets/<format>/` directory.
2. Record filename, dimensions, and EXIF expectation in the registry.
3. Record source URL and license in this document.
4. Run `make test` and verify integration tests pass.
5. Commit both asset and documentation update together.
