# Development Lifecycle

## 1. Intake
- PM defines goal, scope, success criteria, and constraints.
- PM assigns primary/secondary role ownership.

## 2. Design
- API Maintainer validates contract impact.
- Back/Front Developers produce implementation notes.
- QA drafts acceptance test scenarios.

## 3. Implementation
- Execute smallest safe increments.
- Keep changes role-scoped.
- Run local checks continuously.

## 4. Validation
- Required: `make test`.
- GUI-related: `make gui`.
- Workflow-related: relevant smoke path from `docs/tests.md`.

## 5. Documentation Sync
- Update docs and examples in same PR as behavior change.
- Main Maintainer approves doc accuracy and clarity.

## 6. Review and Merge
- PM verifies acceptance criteria and dependency order.
- QA confirms regression coverage.
- Merge only when all gates are green.

## 7. Post-Merge
- Update audit backlog if residual risk remains.
- Record follow-up actions by owner and target milestone.
