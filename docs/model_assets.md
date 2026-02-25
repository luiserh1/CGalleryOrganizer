# Model Assets Registry

This document is the canonical attribution and license registry for ML model assets defined in `models/manifest.json`.

## Model Table

| Model ID | Task | Filename | Version | Author | Source URL | License | SHA256 |
|---|---|---|---|---|---|---|---|
| `clf-default` | classification | `clf-default.onnx` | `0.3.0-bootstrap` | CGalleryOrganizer maintainers | <https://github.com/luiserh1/CGalleryOrganizer> | CC0-1.0 | `59f2723d7445b89a523ed98a386ffe18f0a5cd0b481376dbb49269a788a3c036` |
| `text-default` | text_detection | `text-default.onnx` | `0.3.0-bootstrap` | CGalleryOrganizer maintainers | <https://github.com/luiserh1/CGalleryOrganizer> | CC0-1.0 | `c23369c4fe0df8a5b17fb44ad8c323440bee9dd1efa9ae5c606365b6d670482b` |
| `embed-default` | embedding | `embed-default.onnx` | `0.4.0-bootstrap` | CGalleryOrganizer maintainers | <https://github.com/luiserh1/CGalleryOrganizer> | CC0-1.0 | `aa58f055a95e018ff961711508452074475ba94f36c4311fbee935f365d6c9e3` |

## Notes

- Entries above mirror `models/manifest.json`.
- `scripts/download_models.sh` enforces required attribution/license fields before download.
- 0.4.0 uses bootstrap placeholder ONNX artifacts to validate local pipeline wiring, including embedding support for similarity reports.

## Contributor Checklist

1. Every model entry must include: `license_name`, `license_url`, `author`, `source_url`, `credit_text`.
2. Every model file must include a pinned SHA256 checksum.
3. Update `models/manifest.json` and this document in the same commit.
4. Verify downloader success with `make models`.

## Reviewer Checklist

1. No new manifest entry without complete attribution metadata.
2. No checksum placeholders or missing hashes.
3. `docs/model_assets.md` stays in sync with `models/manifest.json`.
