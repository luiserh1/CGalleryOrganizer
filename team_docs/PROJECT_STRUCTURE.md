# Project Structure

## Top-Level Layout
- `src/`: implementation code.
- `include/`: public headers/contracts.
- `tests/`: test framework and suites.
- `docs/`: product/engineering documentation.
- `tools/`: static viewers and helper tooling.
- `models/`: model manifest metadata.
- `scripts/`: acquisition/install scripts.
- `vendor/`: bundled third-party source.

## Source Architecture
- `src/app/`: backend orchestration layer behind public API.
- `src/cli/`: CLI parsing and scan pipeline helpers.
- `src/gui/`: GUI runtime and presentation modules.
- `src/core/`: cache/core model primitives.
- `src/systems/`: organizer/duplicate/similarity systems.
- `src/utils/`: filesystem/hash/metadata utilities.
- `src/ml/`: provider-agnostic ML API and provider implementations.

## Contract Boundaries
- Public frontend/backend boundary:
  - `include/app_api.h`
  - `include/app_api_types.h`
- Frontends should not orchestrate low-level subsystems directly.

## Test Organization
- Unit and integration tests live under `tests/test_*.c`.
- `tests/test_runner.c` is the single suite entrypoint.
- Integration helpers live in `tests/test_integration_helpers.c`.

## Team Docs Package
- Audit records: `team_docs/audit/`
- Role playbooks: `team_docs/roles/`
- Governance and lifecycle docs: `team_docs/*.md`
