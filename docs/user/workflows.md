# Common Workflows

These flows focus on safe, repeatable usage.

## 1) Scan and inspect

```bash
./build/bin/gallery_organizer /path/source /path/env
```

What this does:
- Reads media recursively
- Extracts metadata
- Writes/updates cache in `/path/env/.cache/`

## 2) Organize with preview-first safety

Preview:
```bash
./build/bin/gallery_organizer /path/source /path/env --preview --group-by date,camera
```

Execute:
```bash
./build/bin/gallery_organizer /path/source /path/env --organize --group-by date,camera
```

Rollback (if needed):
```bash
./build/bin/gallery_organizer /path/env --rollback
```

## 3) Duplicate review and explicit move

Report only:
```bash
./build/bin/gallery_organizer /path/source /path/env --duplicates-report
```

Move duplicates:
```bash
./build/bin/gallery_organizer /path/source /path/env --duplicates-move
```

## 4) Rename with preview/apply handshake

Initialize:
```bash
./build/bin/gallery_organizer /path/source /path/env --rename-init
```

Preview:
```bash
./build/bin/gallery_organizer /path/source /path/env --rename-preview --rename-pattern "[date]-[camera]-[index].[format]"
```

Apply latest preview:
```bash
./build/bin/gallery_organizer /path/env --rename-apply-latest --rename-accept-auto-suffix
```

History and rollback:
```bash
./build/bin/gallery_organizer /path/env --rename-history
./build/bin/gallery_organizer /path/env --rename-rollback <operation_id>
```

## 5) Similarity analysis

Install models:
```bash
make models
```

Generate report:
```bash
./build/bin/gallery_organizer /path/source /path/env --similarity-report --sim-threshold 0.92 --sim-topk 5
```

Inspect in viewer:
- [../../tools/similarity_viewer/README.md](../../tools/similarity_viewer/README.md)

## 6) GUI-first operation

```bash
make run-gui
```

Recommended sequence in GUI:
1. Set paths
2. Run scan
3. Preview operation
4. Execute operation
5. Use history/rollback if needed
