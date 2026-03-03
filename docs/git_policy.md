# Git and GitHub Policy

## Purpose

Define a consistent Git workflow for CGalleryOrganizer so branch history remains
auditable without accumulating stale branches.

## Canonical Branch

- `master` is the canonical long-lived branch.
- GitHub default branch must be `master`.
- Direct pushes to `master` are discouraged; use pull requests.

## Branch Naming

- Feature/milestone branches: `codex/<version-or-scope>`
- Maintenance branches: `codex/<scope>`
- Release/hotfix branches only when needed: `release/<version>`

## Development Flow

1. Create a `codex/<scope>` branch from `master`.
2. Implement changes with scoped commits.
3. Open a PR targeting `master`.
4. Ensure CI is green before merge.
5. Merge via PR (prefer merge commits to preserve milestone context).
6. Delete the head branch after merge (local + remote).

## Pull Request Requirements

- CI required checks must pass.
- Scope/docs changes required by `docs/scope.md` governance must be included.
- Behavioral changes must include tests or justified test updates.
- Release notes/docs sync must be part of the PR when user-facing behavior changes.

## Tags and Releases

- Release tags use `vX.Y.Z` and must point to a commit in `master`.
- Create tags only after merge to `master`.
- Annotated tags are preferred.

## Cleanup Policy

- Enable GitHub setting: auto-delete head branches after merge.
- Periodically delete merged local branches:
  - `git branch --merged master`
- Periodically prune remote-tracking refs:
  - `git fetch --prune`
- Keep only active branches open remotely.

## History Safety

- Do not rewrite public/shared branch history.
- Avoid force-push on shared branches unless explicitly coordinated.
- Merge commits + tags + PRs are the source of historical traceability; stale
  branches are not required for auditability.

## Backport and Hotfix

- If a backport is needed, create `release/<version>` from the target base.
- Merge/cherry-pick required commits, validate CI, tag the resulting release,
  and merge forward to `master` when applicable.
