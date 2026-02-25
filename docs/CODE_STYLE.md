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
- Consider splitting files that grow significantly beyond ~500 lines.

## 7. Testing Guidelines

- Add unit tests for module-level behavior.
- Add integration tests for end-to-end or cross-module flows.
- Run all tests with `make test`.

## 8. Build Strictness

- Keep clean builds under strict flags in `Makefile`:
  - `-Wall -Wextra -Werror -pedantic`
