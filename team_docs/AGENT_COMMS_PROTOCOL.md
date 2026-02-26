# AgentComms Protocol (Project-Specific)

This document defines how CGalleryOrganizer agents communicate when using AgentComms.
If there is any conflict with generic tool guidance, this project protocol wins.

## Scope
- Applies to all project roles: PM, Front Developer, API Maintainer, Back Developer, Main Maintainer, QA.
- Applies to all task coordination messages, dependency handoffs, blockers, and completion reports.

## Ownership
- PM owns workflow requirements and message contract evolution.
- Main Maintainer owns AgentComms server runtime/configuration/security governance.
- QA owns AgentComms health and smoke validation.
- HITL owns infrastructure credentials, secrets, and escalation approvals.

## 1. Role Boundaries
- Agents act only inside their assigned role playbook.
- Cross-role work requires PM handoff.
- Main Maintainer governance/build/release authority cannot be overridden.

## 2. Startup Handshake
1. Register once per session: `./agentcomms register <Organization> <Role>`.
2. Fetch inbox: `./agentcomms fetch`.
3. Send PM a ready message:
   - `READY | role=<Role> | scope=<short>`

## 3. Message Contract
All operational messages must use:

`TYPE | task_id | from | to | status | dependency | action | due | summary`

Allowed `TYPE` values:
- `TASK`
- `BLOCKER`
- `DECISION_REQUEST`
- `HANDOFF`
- `DONE`

## 4. Polling Cadence
- Fetch before any major action.
- Fetch periodically while blocked or waiting.
- Fetch before declaring task completion.

## 5. Ack Discipline
- Message processing is not complete until acknowledged.
- After processing, run: `./agentcomms ack <id...>`.
- If not ready to act, do not ack; reply with `BLOCKER` or `DECISION_REQUEST`.

## 6. Escalation Routing
- Scope/dependency conflicts -> PM
- API contract compatibility conflicts -> API Maintainer (PM informed)
- Governance/build/CI/release conflicts -> Main Maintainer
- Human approval/risk acceptance -> HITL

## 7. Completion Protocol
- Send `DONE` to PM including:
  - files changed
  - tests run
  - risks
  - required handoff
- Wait for PM confirmation before closing the task context.

## 8. Reliability Rules
- Never assume delivery without inbox confirmation.
- If no response after timeout window, resend with `BLOCKER` and original `task_id`.
- Keep messages concise and actionable.

## 9. Prohibited Behavior
- No silent scope expansion.
- No ownership reassignment between non-PM agents.
- No bypass of QA validation gates.
