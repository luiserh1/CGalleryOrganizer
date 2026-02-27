# Scope and Change Control

## Scope Ownership
- PM owns in-scope/out-of-scope definitions.
- API Maintainer owns API contract change approval.
- Main Maintainer owns dependency and build policy changes.
- Main Maintainer owns CI policy, release tags, and changelog governance.

## Change Categories
1. **Behavioral**: user-visible runtime/CLI/API behavior.
2. **Structural**: module decomposition/refactors with no behavior change.
3. **Operational**: docs/policies/build/dependency updates.

## Rules
- Behavioral changes require:
  - explicit acceptance criteria
  - test additions/updates
  - documentation synchronization.
- Structural changes require:
  - no behavior regression proof
  - maintainability rationale.
- Dependency changes require Main Maintainer sign-off.
- Build/CI governance changes require Main Maintainer sign-off.
- PM provides planning approval only; PM does not grant coding or release approvals.

## Out-of-Scope Protection
- Reject feature creep that is not in the active milestone goals.
- Defer non-critical ideas to backlog with owner and rationale.

## Merge and Veto Authority
- Standard merge readiness is coordinated by PM and QA.
- Governance/build veto authority is held by Main Maintainer.
- In PM tie-breaker vs governance/build veto conflicts, Main Maintainer veto prevails.
- PM tie-breaker remains final for non-governance/non-build cross-role conflicts.

## Emergency Overrides
Only PM + Main Maintainer can authorize temporary scope overrides for:
- build breakage
- data-loss risk
- release-blocking regressions.

Document every override with date, owner, and follow-up task.
