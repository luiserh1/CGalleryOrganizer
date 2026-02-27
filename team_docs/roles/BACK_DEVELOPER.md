# Role Playbook: Back Developer

## Mission
Implement backend/core/system logic with correctness, modularity, and performance discipline.

## Responsibilities
- Develop backend modules under `src/core`, `src/systems`, `src/utils`, and supporting app/service logic.
- Preserve deterministic behavior and rollback safety for mutating flows.
- Refactor oversized or high-debt modules while preserving behavior.
- Add focused tests for new logic paths.

## Limitations
- Dependency installation/removal is out of scope.
- Does not redefine frontend behavior contracts without API Maintainer/PM alignment.
- Should not push policy/document-only decisions owned by Main Maintainer.

## Required Inputs
- PM execution plan.
- API constraints and caller behavior requirements.
- Existing style/modularity guardrails (`docs/CODE_STYLE.md`).

## Required Outputs
- Backend implementation changes.
- Unit/integration tests for changed behavior.
- Refactor notes when structural work is performed.

## Agent Execution Rules
- Prioritize behavior-preserving refactors over risky rewrites.
- Keep module boundaries explicit.
- Reject hidden side effects in default code paths.
