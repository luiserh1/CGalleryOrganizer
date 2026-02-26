# Frontend Architecture

## Overview

CGalleryOrganizer 0.5.1 uses a shared backend app service layer to support
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

## GUI Responsive Layout (0.5.1)

GUI layout uses internal layout math (`src/gui/gui_layout.[ch]`) instead of
hardcoded pixel coordinates:
- deterministic effective scale:
  - `auto_scale = clamp(min(window_w/1000, window_h/760), 0.85, 1.75)`
  - `user_zoom_scale = clamp(uiScalePercent, 80..160)/100`
  - `effective_scale = auto_scale * user_zoom_scale`
- row/stack layout with wrapping to avoid control overlap.
- minimum window size derived from worst-case panel requirements and enforced
  at runtime.

## GUI Saved State

The GUI persists:
- `galleryDir`
- `envDir`
- `uiScalePercent`
- `windowWidth`
- `windowHeight`
- optional `updatedAt`

Storage location (`gui_state.json`):
- macOS: `~/Library/Application Support/CGalleryOrganizer/`
- Linux: `$XDG_CONFIG_HOME/CGalleryOrganizer/` or `~/.config/CGalleryOrganizer/`
- Windows: `%APPDATA%/CGalleryOrganizer/`

Cleanup policy:
- explicit reset only (GUI action or `--reset-state`)
- not affected by `make clean`/`make clean-all`
