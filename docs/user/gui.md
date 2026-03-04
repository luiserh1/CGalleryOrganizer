# GUI Guide

The GUI (`gallery_organizer_gui`) exposes the same backend workflows as the CLI
through a guided interface.

## Build and run

```bash
make gui
make run-gui
```

If the app starts with stale persisted state, reset it:
```bash
./build/bin/gallery_organizer_gui --reset-state
```

## What you can do in GUI

- Scan/cache with configurable jobs, exhaustive mode, and cache compression
- Optional model download and ML/similarity flow
- Organize preview/execute and rollback
- Duplicate report and explicit duplicate move
- Rename preview/apply/history/rollback with filters and redo

## Operational behavior

- Long-running operations execute in background worker tasks.
- Actions lock when required prerequisites are missing.
- Path fields are editable and also support picker actions where available.
- Persisted configuration can be saved and restored between sessions.

## Recommended usage pattern

1. Set source and environment paths.
2. Run scan (and optionally model install + similarity).
3. Run preview before any mutating operation.
4. Execute organize/rename/duplicate move only after preview review.
5. Use history/rollback tools for recovery when needed.

## Related docs

- Task recipes: [workflows.md](workflows.md)
- CLI parity and option details: [cli.md](cli.md)
- Frontend architecture: [../frontends.md](../frontends.md)
