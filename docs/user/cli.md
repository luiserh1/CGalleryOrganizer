# CLI Guide

## Command shape

```bash
./build/bin/gallery_organizer <source_dir> [env_dir] [options]
```

Notes:
- Most workflows use both `<source_dir>` and `<env_dir>`.
- Some environment-only operations can use `<env_dir>` as the first argument
  (for example rollback/history actions).

## High-value workflows

Scan and cache:
```bash
./build/bin/gallery_organizer /path/source /path/env
```

Scan with exhaustive metadata:
```bash
./build/bin/gallery_organizer /path/source /path/env --exhaustive
```

Preview organization (non-mutating):
```bash
./build/bin/gallery_organizer /path/source /path/env --preview --group-by date,camera
```

Execute organization:
```bash
./build/bin/gallery_organizer /path/source /path/env --organize --group-by date
```

Find duplicates (report only):
```bash
./build/bin/gallery_organizer /path/source /path/env --duplicates-report
```

Move duplicates explicitly:
```bash
./build/bin/gallery_organizer /path/source /path/env --duplicates-move
```

Generate similarity report (requires models):
```bash
./build/bin/gallery_organizer /path/source /path/env --similarity-report --sim-threshold 0.92 --sim-topk 5
```

## Rename workflow (dedicated)

Initialize rename environment:
```bash
./build/bin/gallery_organizer /path/source /path/env --rename-init
```

Preview rename plan:
```bash
./build/bin/gallery_organizer /path/source /path/env --rename-preview --rename-pattern "pic-[tags]-[camera].[format]"
```

Apply latest preview with collision suffix acceptance:
```bash
./build/bin/gallery_organizer /path/env --rename-apply-latest --rename-accept-auto-suffix
```

Inspect history and rollback:
```bash
./build/bin/gallery_organizer /path/env --rename-history
./build/bin/gallery_organizer /path/env --rename-rollback <operation_id>
```

## Option Families

This section is intentionally curated for common workflows and is not an
exhaustive flag catalog.

Scan/cache:
- `--exhaustive`
- `--jobs <n|auto>`
- `--cache-compress <none|zstd|auto>`
- `--cache-compress-level <1..19>`

ML/similarity:
- `--ml-enrich`
- `--similarity-report`
- `--sim-incremental <on|off>`
- `--sim-memory-mode <eager|chunked>`
- `--sim-threshold <0..1>`
- `--sim-topk <n>`

Organize:
- `--preview`
- `--organize`
- `--group-by <keys>`
- `--rollback`

Duplicates:
- `--duplicates-report`
- `--duplicates-move`

Rename:
- `--rename-init`
- `--rename-bootstrap-tags-from-filename`
- `--rename-preview`
- `--rename-pattern <pattern>`
- `--rename-tags-map <json_path>`
- `--rename-tag-add <csv_tags>`
- `--rename-tag-remove <csv_tags>`
- `--rename-meta-tag-add <csv_tags>`
- `--rename-meta-tag-remove <csv_tags>`
- `--rename-meta-fields`
- `--rename-preview-json`
- `--rename-preview-json-out <path>`
- `--rename-apply`
- `--rename-apply-latest`
- `--rename-accept-auto-suffix`
- `--rename-history`
- `--rename-history-detail <operation_id>`
- `--rename-history-export <json_path>`
- `--rename-history-prune <keep_count>`
- `--rename-history-latest-id`
- `--rename-redo <operation_id>`
- `--rename-rollback-preflight <operation_id>`
- `--rename-rollback <operation_id>`

## Full Reference

Use:
```bash
./build/bin/gallery_organizer --help
```

For API-level behavior and guarantees behind these operations, see
[../app_api.md](../app_api.md).
