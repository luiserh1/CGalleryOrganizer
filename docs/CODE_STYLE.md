# Code Style Guidelines

## Overview

This document defines practical coding conventions used in CGalleryOrganizer.

## 1. Includes

- Use project headers by name (e.g., `"gallery_cache.h"`) with include paths from `Makefile`.
- Prefer include ordering:
  1. System headers
  2. Project headers
  3. Vendor headers

## 2. Header and Definition Rules

- Declarations in `.h`, implementations in `.c`/`.cpp`.
- No global variable definitions in headers.
- Every header must use include guards.

## 3. Formatting

- Indentation: 2 spaces (current repository convention).
- Braces: K&R style.
- Keep lines readable (target near 100 chars where practical).
- Avoid unnecessary comments; document only non-obvious behavior.

## 4. Naming

- Structs/types: PascalCase (e.g., `ImageMetadata`).
- Functions: PascalCase with module context (e.g., `CacheInit`, `FsWalkDirectory`).
- Variables: snake_case.
- Macros/constants/enums: SCREAMING_SNAKE.

## 5. File Organization

- `src/core`: cache/core model logic.
- `src/systems`: organizer/duplicate workflows.
- `src/utils`: filesystem, hashing, metadata wrappers.
- `include`: public headers.
- `vendor`: bundled third-party code only.

## 6. Size and Modularity Guidance

- Prefer small focused helpers (`static` unless shared).
- Avoid god-files; keep module boundaries explicit and single-purpose.
- Prefer module-private headers for internal contracts.
- Production files (`.c/.cpp`) target <= 500 LOC; hard warning above 650.
- Test files target <= 800 LOC; hard warning above 1000.

## 7. Low Technical Debt Policy

- No dead code or abandoned paths in mainline.
- No avoidable duplication: extract shared helpers when logic repeats.
- Be explicit about ownership/lifetime for allocated memory in interfaces.
- Prefer behavior-preserving refactors over quick patches when complexity rises.
- Keep internals modular even when public API remains stable.

## 8. Testing Guidelines

- Add unit tests for module-level behavior.
- Add integration tests for end-to-end or cross-module flows.
- Run all tests with `make test`.

## 9. Build Strictness

- Keep clean builds under strict flags in `Makefile`:
  - `-Wall -Wextra -Werror -pedantic`

## 10. Frontend Boundary Rules

- The backend-facing contract for frontends is the public app API:
  - `include/app_api.h`
  - `include/app_api_types.h`
- API behavior documentation must be kept in:
  - `docs/app_api.md`
- Frontend entrypoints (`src/main.c`, `src/gui/*`) must call backend flows via
  app API operations, not by directly orchestrating `Cache*`, `Organizer*`,
  `Similarity*`, `Ml*`, or other subsystem internals.
- Headers under `include/` must remain declarative and pure contracts:
  no behavior implementation in headers.
- Shared workflow logic must live once in backend/app service modules, then be
  reused by all frontends.
- App API contracts must stay language/framework neutral so future frontends
  (SwiftUI/web adapters/etc.) can consume the same backend behavior.
- Browser/web frontends must integrate through a dedicated adapter/service
  boundary; do not introduce browser-direct calls to C backend internals.

## 11. GUI Layout Rules

- `v0.5.2` functional GUI intentionally uses fixed-size geometry.
- Keep functional GUI geometry deterministic and centralized in a dedicated
  layout helper module (`src/gui/frontends/functional/gui_fixed_layout.[ch]`).
- New controls in functional GUI must preserve no-overlap and in-bounds
  guarantees for the fixed shell (`1280x820`).
- If/when responsive or user-oriented GUI variants are added in future versions,
  they must live in separate frontend modules and must not duplicate backend
  orchestration logic.
