# Audit Test Strategy

## Current Health
- Baseline suite passes: `make test`.
- Coverage includes core scan/cache/ML/similarity plus GUI layout/rules and app profile/model/runtime behavior.

## Key Improvements Added
- App workflow tests now include:
  - duplicate discover + move (`AppFindDuplicates`, `AppMoveDuplicates`)
  - organize preview + execute + rollback (`AppPreviewOrganize`, `AppExecuteOrganize`, `AppRollback`)
- CLI integration tests now enforce duplicate safety contract:
  - no implicit duplicate move on plain scan
  - `--duplicates-report` non-mutating
  - `--duplicates-move` requires `env_dir` and performs explicit move

## Remaining Risks
- Shell-dependent tests are still numerous and can be sensitive to environment/path behavior.
- GUI runtime build is not part of the default `make test` path.

## Strategy Going Forward
1. Keep a two-layer suite:
   - deterministic unit/app tests (fast, low fragility)
   - process-level CLI integration tests (targeted, fewer, explicit purpose)
2. Add CI jobs:
   - `make test`
   - `make gui`
3. Add smoke profile for release candidates:
   - scan/profile reuse
   - explicit duplicate report/move
   - organize/rollback
   - similarity generation
4. Track flaky tests and maintain a monthly cleanup backlog.

## Acceptance Criteria for Future Changes
- New public behavior requires tests at:
  - parser/validation level
  - app operation level
  - CLI/user-facing output level (if behavior is CLI-facing)
- No merge for behavior changes without docs sync (`README.md`, `docs/tests.md`, and feature docs).
