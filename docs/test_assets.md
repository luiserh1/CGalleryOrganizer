# Test Assets

This document is the canonical location for attribution and licensing of image fixtures under `tests/assets/**`.

## 1. Fixture Registry (Technical)

| Fixture File | Format | Source Credit ID | Classification | Expected Dimensions | EXIF Expectation | Notes |
|---|---|---|---|---|---|---|
| `tests/assets/jpg/sample_exif.jpg` | JPEG | `SRC-KEW` | Third-party direct | 240x160 | Present | Base JPEG with metadata |
| `tests/assets/jpg/sample_no_exif.jpg` | JPEG | `SRC-KEW-MOD` | Third-party derivative | 240x160 | Absent | Metadata stripped |
| `tests/assets/jpg/sample_deep_exif.jpg` | JPEG | `SRC-KEW-MOD` | Third-party derivative | >0x>0 | Present (GPS validated) | Deep EXIF/GPS fixture |
| `tests/assets/png/sample_no_exif.png` | PNG | `SRC-PASSPORT` | Third-party direct | 113x161 | Absent | PNG metadata baseline |
| `tests/assets/png/sample_exif.png` | PNG | `SRC-PASSPORT-MOD` | Third-party derivative | 113x161 | Present | EXIF inserted for parser coverage |
| `tests/assets/png/fake.png` | PNG extension / JPEG content | `SRC-KEW-MOD` | Third-party derivative | 240x160 | N/A | Extension-mismatch robustness fixture |
| `tests/assets/webp/sample_no_exif.webp` | WebP | `SRC-DOG` | Third-party direct | 400x400 | Absent | WebP metadata baseline |
| `tests/assets/webp/sample_exif.webp` | WebP | `SRC-DOG-MOD` | Third-party derivative | 400x400 | Present | EXIF inserted for parser coverage |
| `tests/assets/gif/sample_no_exif.gif` | GIF | `SRC-NLIGHTS` | Third-party direct | 500x331 | Not expected | GIF parsing fixture |
| `tests/assets/bmp/sample_no_exif.bmp` | BMP | `SRC-RABBIT-MOD` | Third-party derivative | 256x151 | Not expected | Converted from source JPG |
| `tests/assets/bmp/truncated.bmp` | BMP (truncated) | `SRC-RABBIT-MOD` | Third-party derivative | 256x151 | Not expected | Corruption robustness fixture |
| `tests/assets/heic/sample_no_exif.heic` | HEIC | `SRC-FALLS-MOD` | Third-party derivative | 256x192 | Absent | Converted fixture |
| `tests/assets/heic/sample_exif.heic` | HEIC | `SRC-FALLS-MOD-EXIF` | Third-party derivative | 256x192 | Limited/format-dependent | Converted + EXIF-inserted fixture |
| `tests/assets/duplicates/bird.JPG` | JPEG | `SRC-KEW-MOD` | Third-party derivative | N/A | N/A | Duplicate-detection base image |
| `tests/assets/duplicates/bird_duplicate.JPG` | JPEG | `SRC-KEW-MOD` | Third-party derivative | N/A | N/A | Exact duplicate of `bird.JPG` |
| `tests/assets/duplicates/bird_duplicate_different_meta.JPG` | JPEG | `SRC-KEW-MOD` | Third-party derivative | N/A | N/A | Same visual content, metadata changed |
| `tests/assets/duplicates/bird_displaced.JPG` | JPEG | `SRC-KEW-MOD` | Third-party derivative | N/A | N/A | Pixel displacement variant |
| `tests/assets/duplicates/bird_duplicate_one_pixel_difference.JPG` | JPEG | `SRC-KEW-MOD` | Third-party derivative | N/A | N/A | Near-duplicate variant |

## 2. Sources and Credits (Legal)

### `SRC-KEW`
- Human-readable source: Flowers at Kew Gardens
- Source URL: <https://www.flickr.com/photos/adulau/8648707725/in/photostream/>
- Author/creator: Alexandre Dulaunoy
- License: CC BY-SA 2.0
- Modifications: None

### `SRC-KEW-MOD`
- Human-readable source: Flowers at Kew Gardens (derived variants)
- Source URL: <https://www.flickr.com/photos/adulau/8648707725/in/photostream/>
- Author/creator: Alexandre Dulaunoy
- License: CC BY-SA 2.0
- Modifications: Metadata edits/removal and additional robustness transformations for testing (e.g., duplicate set, extension mismatch, variant generation)

### `SRC-PASSPORT`
- Human-readable source: Cover of Papua New Guinean Passport
- Source URL: <https://commons.wikimedia.org/wiki/File:Cover_of_Papua_New_Guinean_Passport.png>
- Author/creator: Papua New Guinea government
- License: Public domain
- Modifications: None

### `SRC-PASSPORT-MOD`
- Human-readable source: Cover of Papua New Guinean Passport (EXIF variant)
- Source URL: <https://commons.wikimedia.org/wiki/File:Cover_of_Papua_New_Guinean_Passport.png>
- Author/creator: Papua New Guinea government
- License: Public domain
- Modifications: EXIF metadata inserted using `exiftool`

### `SRC-DOG`
- Human-readable source: Drawing of a Dog Home
- Source URL: <https://commons.wikimedia.org/wiki/File:Drawing-of-a-Dog-Home-400-x-400-1.webp>
- Author/creator: Mike Bruce
- License: CC BY-SA 4.0
- Modifications: None

### `SRC-DOG-MOD`
- Human-readable source: Drawing of a Dog Home (EXIF variant)
- Source URL: <https://commons.wikimedia.org/wiki/File:Drawing-of-a-Dog-Home-400-x-400-1.webp>
- Author/creator: Mike Bruce
- License: CC BY-SA 4.0
- Modifications: EXIF metadata inserted using `exiftool`

### `SRC-NLIGHTS`
- Human-readable source: Northern Lights timelapse
- Source URL: <https://commons.wikimedia.org/wiki/File:Northern_Lights_timelapse.gif>
- Author/creator: KristianPikner
- License: CC BY-SA 4.0
- Modifications: None

### `SRC-FALLS-MOD`
- Human-readable source: Shivanasamudram falls early morning view
- Source URL: <https://commons.wikimedia.org/wiki/File:Shivanasamudram_falls_early_morning_view.HEIC.jpg>
- Author/creator: Maskaravivek
- License: CC BY-SA 3.0
- Modifications: Converted from JPG to HEIC (test fixture conversion)

### `SRC-FALLS-MOD-EXIF`
- Human-readable source: Shivanasamudram falls early morning view (EXIF variant)
- Source URL: <https://commons.wikimedia.org/wiki/File:Shivanasamudram_falls_early_morning_view.HEIC.jpg>
- Author/creator: Maskaravivek
- License: CC BY-SA 3.0
- Modifications: Converted from JPG to HEIC, then EXIF metadata inserted using `exiftool`

### `SRC-RABBIT-MOD`
- Human-readable source: Checkered Giant rabbit
- Source URL: <https://commons.wikimedia.org/wiki/File:Checkered_Giant_rabbit.jpg>
- Author/creator: DestinationFearFan
- License: CC BY-SA 4.0
- Modifications: Converted from JPG to BMP; truncated variant generated for robustness tests

## 3. Attribution Coverage Rules

Every file under `tests/assets/**` must be one of:
1. Third-party direct (credited by Source Credit ID).
2. Third-party derivative (credited by Source Credit ID + modification notes).
3. Original by project maintainers (explicitly marked as such in the fixture registry).

Mapping rule for derived robustness fixtures:
- Duplicate/truncated/metadata-mutated/extension-mismatch files must point to a credited parent source via Source Credit ID.

## 4. Contribution and Review Checklist

### Contributor checklist
1. Verify incoming asset license is compatible (prefer CC0, CC BY, CC BY-SA, or public domain).
2. Add or reuse a Source Credit ID with author, source URL, and exact license.
3. Record the modification pipeline for derivative fixtures.
4. Add/update fixture rows in the registry in the same commit as the asset change.
5. Run `make test` before submitting changes.

### Reviewer checklist (PR guardrail)
1. No new file under `tests/assets/**` appears without a fixture registry entry.
2. No fixture references a missing Source Credit ID.
3. No third-party fixture lacks explicit author/source/license attribution.
4. Attribution detail is not collapsed into generic prose.
