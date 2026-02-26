# Frontend Architecture

## Overview

CGalleryOrganizer 0.5.0 uses a shared backend app service layer to support
multiple frontends without duplicating orchestration logic.

Frontends currently available:
- CLI: `build/bin/gallery_organizer`
- GUI: `build/bin/gallery_organizer_gui`

## Ownership Boundaries

- Public frontend/backend contract:
  - `include/app_api.h`
  - `include/app_api_types.h`
- App service implementation:
  - `src/app/*`
- Frontend code:
  - CLI: `src/main.c` + `src/cli/*` (argument parsing and presentation)
  - GUI: `src/gui/*`

Frontends do not orchestrate backend internals directly.

## Data Flow

1. Frontend collects user input/options.
2. Frontend builds typed app request structs.
3. Frontend calls app API operation.
4. App layer coordinates cache/ML/organizer/duplicate/similarity subsystems.
5. App layer returns typed result and status.
6. Frontend renders output (stdout or GUI widgets).

## Progress and Cancellation

Long-running operations accept operation hooks:
- progress callback: emits stage + counters
- cancel callback: polled by backend loops

GUI uses a worker thread with these hooks; CLI uses synchronous execution
without hooks.

## GUI Saved State

The GUI persists the last-used path pair:
- `galleryDir`
- `envDir`

Storage location (`gui_state.json`):
- macOS: `~/Library/Application Support/CGalleryOrganizer/`
- Linux: `$XDG_CONFIG_HOME/CGalleryOrganizer/` or `~/.config/CGalleryOrganizer/`
- Windows: `%APPDATA%/CGalleryOrganizer/`

Cleanup policy:
- explicit reset only (GUI action or `--reset-state`)
- not affected by `make clean`/`make clean-all`
