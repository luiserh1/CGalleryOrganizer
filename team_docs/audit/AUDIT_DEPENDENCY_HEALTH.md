# Audit Dependency Health

## Dependency Model
The project follows a layered model:
1. Project-native implementation preferred.
2. Vendored dependencies for lightweight audited code.
3. System dependencies for heavy platform libraries.

## Current Status

### Vendored (`vendor/`)
- `cJSON`: cache and report serialization support.
- `md5` + `sha256`: duplicate hashing and checksum validation.
- `raygui.h`: GUI helper shim.

### System
- `exiv2`: metadata extraction wrapper path (`src/utils/exiv2_wrapper.cpp`).
- `onnxruntime`: optional ML inference provider.
- `zstd`: optional cache compression.
- `raylib`: GUI frontend runtime.

## Observations
- Dependency docs are present and detailed in `docs/dependencies.md`.
- ONNX Runtime availability varies across hosts; CLI/app behavior already reports missing-model/runtime errors clearly.
- Build flags are strict (`-Wall -Wextra -Werror -pedantic`) which helps dependency integration safety.

## Risks
- Environment mismatch across machines (especially `onnxruntime` pkg-config availability).
- Optional components can silently be unused if not validated in CI matrix.

## Recommendations
1. Add CI matrix lane with ML-enabled host where `onnxruntime` is resolvable.
2. Keep dependency verification checklist in release process:
   - compile
   - test
   - model install (`make models`)
   - optional GUI build.
3. Maintain explicit dependency ownership:
   - Main Maintainer approves add/remove/major-version changes.
