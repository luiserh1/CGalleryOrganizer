# RACI Matrix

## Roles
- PM: Project Manager
- FD: Front Developer
- API: API Maintainer
- BD: Back Developer
- MM: Main Maintainer
- QA: Quality Assurance
- HITL: Human-in-the-loop (repo owner)

## Matrix
| Work Item | PM | FD | API | BD | MM | QA | HITL |
|---|---|---|---|---|---|---|---|
| Scope definition and milestone boundaries | A/R | C | C | C | C | C | I |
| Implementation plan (actor, order, dependencies) | A/R | C | C | C | C | C | I |
| Cross-role conflict resolution (non-governance/build) | A/R | C | C | C | C | C | I |
| Frontend implementation (CLI/GUI UX layer) | I | A/R | C | C | I | C | I |
| API contract design and compatibility | C | C | A/R | C | I | C | I |
| Backend implementation | I | C | C | A/R | I | C | I |
| Dependency add/remove/version policy | I | I | C | I | A/R | C | I |
| Build and CI policy | I | I | C | I | A/R | C | I |
| Governance/build merge veto | I | I | I | I | A/R | C | I |
| PM-authored `team_docs/` merge/commit | R | I | I | I | A | I | I |
| Test plan design and regression strategy | C | C | C | C | I | A/R | I |
| Test suite execution and smoke validation | I | I | C | C | I | A/R | I |
| Documentation sync for behavior changes | C | C | C | C | A/R | C | I |
| Release tagging and changelog publication | I | I | I | I | A/R | C | I |
| Final release go/no-go recommendation | C | I | C | C | A | R | I |
| Human approvals for escalations/risk acceptance | I | I | I | I | C | C | A/R |

## Governance Rules
1. If PM tie-breaker conflicts with governance/build veto, MM veto prevails.
2. PM is planning-only and does not author code commits.
3. PM-authored changes under `team_docs/` must be merged/committed by MM.
4. HITL provides final approvals when human authorization is required.
