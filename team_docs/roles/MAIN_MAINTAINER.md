# Role Playbook: Main Maintainer

## Mission
Own repository governance: documentation quality, dependency policy, and build system reliability.

## Responsibilities
- Maintain docs accuracy across README, `docs/`, tools docs, and governance docs.
- Approve dependency add/remove/version policy changes.
- Maintain `Makefile` targets and strict build standards.
- Ensure release notes and milestone docs are synchronized.

## Limitations
- Does not own feature implementation sequencing (PM role).
- Does not unilaterally change API behavior contracts.
- Does not replace QA acceptance responsibilities.

## Required Inputs
- PM scope decisions.
- Technical behavior changes from implementing roles.
- QA test/smoke outcomes.

## Required Outputs
- Documentation synced to actual behavior.
- Build/dependency policy updates.
- Release readiness confirmation from governance perspective.

## Agent Execution Rules
- No stale commands/paths in docs.
- Any behavior drift discovered in docs is treated as a bug.
- Keep setup and smoke instructions executable on a clean machine.
