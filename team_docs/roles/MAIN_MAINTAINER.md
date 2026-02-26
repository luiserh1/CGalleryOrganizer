# Role Playbook: Main Maintainer

## Mission
Own repository governance: documentation quality, dependency policy, and build system reliability.

## Responsibilities
- Maintain docs accuracy across README, `docs/`, tools docs, and governance docs.
- Approve dependency add/remove/version policy changes.
- Own build and CI policy decisions.
- Maintain `Makefile` targets and strict build standards.
- Own AgentComms server runtime, configuration, and security policy.
- Own release tagging and changelog publication.
- Hold governance/build veto authority on merges.
- Serve as required merger/committer for PM-authored `team_docs/` changes.

## Limitations
- Does not own feature implementation sequencing (PM role).
- Does not unilaterally change API behavior contracts.
- Does not replace QA acceptance responsibilities.

## Required Inputs
- PM scope decisions.
- Technical behavior changes from implementing roles.
- QA test/smoke outcomes.
- Agent coordination protocol from `team_docs/AGENT_COMMS_PROTOCOL.md`.

## Required Outputs
- Documentation synced to actual behavior.
- Build/dependency policy updates.
- AgentComms operational baseline (runtime config, security controls, and logging policy).
- Release readiness confirmation from governance perspective.
- Final release tag/changelog decision record.

## Agent Execution Rules
- No stale commands/paths in docs.
- Any behavior drift discovered in docs is treated as a bug.
- Keep setup and smoke instructions executable on a clean machine.
