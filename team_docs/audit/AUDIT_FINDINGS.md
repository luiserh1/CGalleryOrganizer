# Audit Findings Matrix

## Severity Scale
- `P0`: data-loss or integrity risk.
- `P1`: user-visible incorrect behavior or contract drift.
- `P2`: maintainability/testability debt with near-term delivery risk.
- `P3`: hygiene/documentation quality issues.

## Findings

| ID | Priority | Area | Evidence | Impact | Owner | Status |
| --- | --- | --- | --- | --- | --- | --- |
| F-001 | P0 | CLI safety | Default scan branch in `src/main.c` previously called duplicate move path implicitly | Unintended file moves during normal scan | API Maintainer + Back Developer | Fixed |
| F-002 | P1 | CLI contract | Missing explicit duplicate action flags in CLI interface | Ambiguous mutating behavior | API Maintainer | Fixed |
| F-003 | P1 | Versioning | `src/main.c` banner reported `v0.5.1` while current docs/GUI were `v0.5.4` | User confusion and release mismatch | Main Maintainer | Fixed |
| F-004 | P1 | Tool docs | Cache viewer referenced `build/perf_cache.json`/`make stress` with no valid producer/target | Broken default UX and stale guidance | Main Maintainer | Fixed |
| F-005 | P2 | Modularity | `src/gui/core/gui_ui_state.c` and `src/gui/gui_worker.c` exceeded style guidance | Slower changes, harder review/debug | Front Developer + Back Developer | Fixed (split) |
| F-006 | P2 | Test coverage | App duplicate/organize/rollback workflows lacked direct app-layer tests | Regression risk in critical workflows | QA + API Maintainer | Fixed |
| F-007 | P3 | Test fragility | Broad use of `system()`/`popen()` in tests | Platform/path sensitivity and brittle failures | QA | Open (tracked) |

## Open Remediation Backlog

### B-001 (`P2`) Shell Fragility Reduction
- Replace high-value shell calls in tests with direct helper APIs first in:
  - `tests/test_cli_core.c`
  - `tests/test_cli_ml_similarity.c`
  - `tests/test_benchmark_output.c`
- Keep shell-based integration tests only where process-level behavior is the subject under test.

### B-002 (`P2`) CI Hardening
- Add CI jobs for:
  - `make test`
  - `make gui`
  - markdown lint/link check for docs.
- Block merges on failed quality gates.

### B-003 (`P3`) Repository Branch Hygiene
- Standardize lifecycle of release and feature branches.
- Require branch cleanup after release merge/tag cut.
