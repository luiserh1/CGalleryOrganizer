# Implementation Plan Post Audit

## Objective
Apply audit recommendations with safety-first sequencing and role accountability.

## Phase Plan

### Phase 1: Safety Contracts (Completed in this cycle)
- Owners: API Maintainer, Back Developer
- Work:
  - remove implicit duplicate move from default scan flow
  - add explicit CLI duplicate actions
  - align CLI version banner
- Acceptance:
  - plain scan non-mutating for duplicate moves
  - explicit duplicate flags validated by tests

### Phase 2: Drift Remediation (Completed in this cycle)
- Owner: Main Maintainer
- Work:
  - align cache viewer defaults/docs with real generated artifacts
  - sync behavior docs
- Acceptance:
  - no stale references to non-existent targets/paths

### Phase 3: Modularity Debt Reduction (Completed in this cycle)
- Owners: Front Developer, Back Developer
- Work:
  - decompose oversized GUI state/worker modules
  - keep CLI scan pipeline under style size target
- Acceptance:
  - style size targets satisfied
  - no behavior regressions

### Phase 4: Coverage Expansion (Completed in this cycle)
- Owners: QA, API Maintainer
- Work:
  - add app-layer duplicate and organize workflow tests
- Acceptance:
  - test suite remains green
  - critical workflow regressions covered

### Phase 5: Fragility Reduction (Open)
- Owners: QA, Back Developer
- Work:
  - reduce shell-heavy test surfaces where feasible
  - keep process-level tests only where required
- Acceptance:
  - reduced environment-dependent failures

### Phase 6: Team Operating System (Completed in this cycle)
- Owners: PM, Main Maintainer
- Work:
  - publish role playbooks and governance docs
  - publish human-in-the-loop setup runbook
- Acceptance:
  - junior developer can onboard and execute workflow from docs only
