## Summary

Describe the intent and behavior changes introduced by this PR.

## Validation

- [ ] `make test` passes locally.
- [ ] Additional checks run (if applicable): describe below.

## Governance Checklist

- [ ] Scope/release docs updated when behavior changed (`docs/scope.md`, `README.md` roadmap/release notes).
- [ ] User-facing CLI/GUI version strings are aligned when release/version is advanced.
- [ ] Coverage baseline policy is respected:
  - [ ] baseline is non-zero and meaningful, or
  - [ ] justification included for baseline refresh/exception.
- [ ] Branch/policy consistency preserved (`master` target, `codex/*` branch workflow).
- [ ] CI config changes stay aligned with `docs/git_policy.md`.

## Risk and Rollback

- Risk level: low / medium / high
- Rollback plan:
  - [ ] Revert commit(s) if regression detected.
  - [ ] Any required data/config cleanup documented.

