# Project Handbook

## Handover Context
You are taking ownership of a mature but still evolving C/C++ codebase with dual frontends and a shared app API. Treat this as production-grade software: safety first, deterministic behavior second, and feature work third.

## Project Mission
CGalleryOrganizer is a local-first media organizer with:
- scan/cache pipeline
- metadata extraction
- duplicate analysis
- organize/rollback
- optional local ML enrichment and similarity reporting
- CLI + GUI frontends over one backend contract.

## Operating Principles
1. Never introduce implicit mutating behavior.
2. Keep frontend/backend boundaries strict (`app_api`).
3. Preserve test green status before and after every change.
4. Update docs in the same change set as behavior changes.
5. Prefer small modules with clear ownership boundaries.

## Team Model
- Work is divided across role playbooks in `team_docs/roles/`.
- PM owns sequencing, scope, and acceptance criteria only (no coding/commit authority).
- Main Maintainer owns dependency policy, docs quality, build/CI policy, and release tagging/changelog.

## Mandatory Quality Gates
- `make test` must pass.
- For GUI-affecting changes: `make gui` must pass.
- For contract changes: docs sync required in `README.md` and relevant `docs/*.md`.

## Key Safety Rules
- File-moving actions must be explicit user intent.
- Rollback paths must remain operational for any organizing/move flow.
- Never merge behavior drift against documented CLI/API contracts.

## Implementation Plan After Audit
Use `team_docs/IMPLEMENTATION_PLAN_POST_AUDIT.md` as the canonical execution sequence and ownership map.
