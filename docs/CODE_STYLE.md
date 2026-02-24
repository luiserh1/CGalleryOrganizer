# Code Style Guidelines

## Overview

This document establishes the coding conventions for the CGalleryOrganizer project.

---

## 1. Include Paths

### Rule
Use header filename only (e.g., `"config.h"`), never relative paths (e.g., `"../core/config.h"`).

### Implementation
- Add `-Isrc` and `-Iinclude` to CFLAGS in Makefile
- Include order: system headers, project headers, local headers, vendor headers

### Tests Exception
Tests may use relative paths to include source headers (e.g., `"../src/core/config.h"`) to avoid complicating the source code.

---

## 2. Include Ordering

**Order (top to bottom):**

1. **System headers** (alphabetical): `<stdio.h>`, `<stdlib.h>`, `<string.h>`
2. **Project headers** (alphabetical): `"config.h"`
3. **Local headers**: `"fs_utils.h"`
4. **Vendor headers**: `"cJSON.h"`

### Example
```c
#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#include "fs_utils.h"

#include "cJSON.h"
```

---

## 3. One Definition Rule

- Declare in `.h`, implement in `.c` only
- No function definitions in header files (except `inline` functions)
- No variable definitions in headers (use `extern`)
- Guards against double inclusion (`#ifndef HEADER_H`)

---

## 4. Comment Style

- **Single-line:** `// Comment` (C99)
- **Multi-line:** `/* Multi-line comment */`
- **Avoid:** Inline comments explaining obvious code
- **Exception:** Complex algorithms, non-obvious logic
- If behavior is intentionally temporary, mark it with a clear `TODO` and owner/context.

---

## 5. Formatting

| Aspect | Rule |
|--------|------|
| **Indentation** | 4 spaces, no tabs |
| **Line length** | Max 100 characters |
| **Braces** | K&R style (`if () {\n`) |
| **PascalCase** | Public function names (module-prefixed) |
| **SCREAMING_SNAKE** | Macros and constants |
| **PascalCase** | Struct types (e.g., `GalleryCache`) |
| **Booleans** | Use `true`/`false` from `<stdbool.h>` |

---

## 6. File Organization

```
src/
├── core/           # Core models, cache management
├── systems/        # Similarity engine, Vision integration (lazy)
├── utils/          # FS utilities, hashing, generic helpers
└── main.c          # Entry point

tests/
├── bin/            # Test executables
├── test_*.c        # Test files
└── test_*.h        # Test framework

include/            # Public project headers

vendor/             # Third-party dependencies (strictly isolated)
```

### Helper Extraction Rule

- Keep helpers `static` in their `.c` module by default.
- Extract a helper only when all are true:
  - It has at least two real consumers in different modules.
  - Semantics are stable and domain-agnostic.
  - The extracted module has a focused responsibility.

### File Size Targets

- Soft target: keep `.c` files around `<= 350` lines.
- Hard warning threshold: review splitting any file over `500` lines.

---

## 7. Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Structs | PascalCase | `ImageMetadata`, `GalleryCache` |
| Functions | PascalCase (module-prefixed) | `FsWalkDirectory`, `CacheLoad` |
| Variables | snake_case | `file_path`, `text_density` |
| Constants | SCREAMING_SNAKE | `MAX_PATH_LENGTH`, `CACHE_VERSION` |
| Enums | SCREAMING_SNAKE | `MD_FILE_TYPE_IMAGE` |
| Enum values | SCREAMING_SNAKE | `CACHE_STATUS_OK` |

---

## 8. Testing Guidelines

- Unit tests for each module
- Integration tests for multi-component flows
- Tests use actual source files with `TESTING` preprocessor flag
- Run tests with `make test`

---

## 9. Dependency Management

- Third-party code belongs strictly in `vendor/`.
- All dependencies must be documented in `docs/dependencies.md`.
- Prefer single-file or header-only C libraries.
