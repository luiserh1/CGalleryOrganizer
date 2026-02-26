# Frontend Architecture

## Overview

CGalleryOrganizer uses a single backend service contract (`app_api`) shared by all
frontends.

Current shipped frontends:
- CLI: `build/bin/gallery_organizer`
- Functional GUI (C + raylib/raygui): `build/bin/gallery_organizer_gui`

## Ownership Boundaries

- Public frontend/backend contract:
  - `include/app_api.h`
  - `include/app_api_types.h`
- Contract documentation:
  - `docs/app_api.md`
- Backend service implementation:
  - `src/app/*`
- Frontend presentation layers:
  - CLI: `src/main.c` + `src/cli/*`
  - GUI runtime core: `src/gui/core/*`
  - GUI functional presentation: `src/gui/frontends/functional/*`

Frontends must not orchestrate backend internals (`Cache*`, `Ml*`, `Organizer*`,
`Similarity*`) directly.

## Integration Modes

## 1) Native in-process frontends

Examples: CLI, current C GUI, future Swift/ObjC bridge.

- Link to `app_api` directly.
- Optional shared library artifact for native bindings: `make app-api-lib`.
- Build typed requests.
- Execute synchronous `App*` calls.
- Render frontend-specific output.

## 2) Out-of-process / web frontends

Examples: browser or remote UI.

- Must use an adapter service/bridge process.
- Must not call C APIs directly from browser code.
- Adapter owns translation between transport formats and `App*` types.

No adapter service is shipped in 0.5.3; this is the required boundary contract
for future work.

## Data Flow

1. Frontend collects user input/options.
2. Frontend builds typed app request structs.
3. Frontend calls app API operation.
4. App layer coordinates cache/ML/organizer/duplicate/similarity subsystems.
5. App layer returns typed result + status.
6. Frontend renders output.

## Progress and Cancellation

Long-running operations expose callbacks through `AppOperationHooks`.

- GUI runs synchronous API calls in a worker thread.
- CLI runs calls on main thread.

## Functional GUI Baseline (0.5.3)

The GUI intentionally targets a functional baseline:
- fixed window size: `1280x820`
- no responsive resizing behavior
- no runtime zoom controls
- full operation parity with CLI workflows
- dependency-gated actions with explicit reason feedback
- always-visible hints + hover tooltips
- in-app model installation action (`Download Models`)
- strict numeric validation for constrained fields

Persisted GUI state:
- `galleryDir`
- `envDir`
- optional `updatedAt`

State storage (`gui_state.json`):
- macOS: `~/Library/Application Support/CGalleryOrganizer/`
- Linux: `$XDG_CONFIG_HOME/CGalleryOrganizer/` or `~/.config/CGalleryOrganizer/`
- Windows: `%APPDATA%/CGalleryOrganizer/`

Cleanup policy:
- explicit reset only (`--reset-state` or GUI reset action)
- not affected by `make clean` / `make clean-all`
