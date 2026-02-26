# RACI Decision Tree

Use this page to decide ownership quickly before starting work.

## 1) What kind of change is it?
1. Scope, priority, sequencing, or cross-role dependency?  
   -> Owner: PM
2. Agent communication workflow/protocol/message contract?  
   -> Owner: PM
3. Frontend behavior/UI/CLI presentation layer?  
   -> Owner: Front Developer
4. API contract/shape/versioning/compatibility?  
   -> Owner: API Maintainer
5. Backend logic/data-processing internals?  
   -> Owner: Back Developer
6. Dependency policy, build system, CI policy, release tag/changelog, governance docs, AgentComms runtime/security?  
   -> Owner: Main Maintainer
7. Test strategy, regression design, smoke validation execution (including AgentComms health checks)?  
   -> Owner: QA
8. Human approval/escalation/risk acceptance, infrastructure credentials/secrets required?  
   -> Owner: HITL

## 2) Is this a governance/build conflict?
1. Yes -> Main Maintainer veto applies.
2. No -> PM tie-breaker applies for cross-role conflicts.

## 3) Is PM authoring a doc change?
1. If inside `team_docs/`: PM may author planning/governance content.
2. Final merge/commit must be done by Main Maintainer.
3. PM does not author code commits.

## 4) Is behavior changing?
1. Yes -> Require:
   - acceptance criteria (PM)
   - implementation owner (FD/API/BD)
   - tests updated (QA + implementation owner)
   - docs synchronized (MM)
2. No (structural refactor) -> Require:
   - no-regression proof
   - maintainability rationale
   - QA validation scope confirmed

## 5) Can this be merged now?
1. `make test` green (and `make gui` when applicable).
2. QA confirms coverage/regression checks.
3. PM confirms acceptance criteria/dependency order.
4. Main Maintainer confirms governance/build/CI compliance.
5. No Main Maintainer veto active.

If all 5 are true -> merge allowed.

## 6) Can this be released now?
1. QA reports validation status.
2. PM confirms milestone scope completeness.
3. Main Maintainer makes final go/no-go and executes:
   - release tag
   - changelog publication

## 7) Fast Escalation Path
1. Delivery/scope dispute -> PM
2. API compatibility dispute -> API Maintainer (PM informed)
3. Build/CI/dependency/release dispute -> Main Maintainer
4. Safety/risk acceptance beyond agent authority -> HITL
5. AgentComms outage/security incident -> Main Maintainer + HITL
