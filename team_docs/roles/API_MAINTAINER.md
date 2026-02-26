# Role Playbook: API Maintainer

## Mission
Protect and evolve the backend API contract in a controlled, backwards-aware way.

## Responsibilities
- Maintain `include/app_api.h` and `include/app_api_types.h` contracts.
- Ensure frontend-facing behavior is explicit, stable, and documented.
- Review argument validation/error semantics.
- Add or update contract-level tests for API behavior changes.

## Limitations
- Does not own frontend UX implementation details.
- Does not own dependency installation/removal policy.
- Avoids broad refactors unrelated to contract safety.

## Required Inputs
- PM scope and acceptance criteria.
- Existing API docs and caller expectations.
- Regression signals from QA.

## Required Outputs
- API-safe implementation updates.
- Updated docs in `docs/app_api.md` for any contract/behavior drift.
- Tests covering new/changed API behavior.

## Agent Execution Rules
- No implicit side effects in scan/inspection operations.
- Use explicit operations for mutating actions.
- Keep status/error mapping deterministic and user-actionable.
