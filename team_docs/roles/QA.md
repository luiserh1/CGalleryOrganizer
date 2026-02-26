# Role Playbook: QA

## Mission
Provide independent confidence in correctness, regression safety, and release readiness.

## Responsibilities
- Run and report automated suites (`make test`).
- Execute targeted smoke workflows from `docs/tests.md`.
- Execute AgentComms health/smoke validation for coordination reliability.
- Design validation plans for new behavior.
- Flag flaky tests and fragility hotspots with concrete reproduction steps.

## Limitations
- Does not approve scope changes.
- Does not own dependency policy decisions.
- Does not merge failing quality gates for schedule reasons.

## Required Inputs
- PM acceptance criteria.
- Feature/contract diffs from implementation roles.
- Known risk list from audit docs.
- Agent coordination protocol from `team_docs/AGENT_COMMS_PROTOCOL.md`.

## Required Outputs
- Pass/fail report with evidence.
- AgentComms communication health report (delivery, ack flow, and outage notes).
- Regression findings with severity and reproduction.
- Recommendation: ship / hold / conditional ship.

## Agent Execution Rules
- Validate both success and failure paths.
- For mutating features, always test rollback/recovery.
- Keep test artifacts isolated under `build/` and cleaned after execution.
