# Human-in-the-Loop Agent Setup Guide

## Purpose
This guide is for the human supervisor (you) to set up, constrain, and validate agent-driven development safely.

## 1. Workstation Prerequisites

### macOS (primary path)
- Xcode command-line tools
- `make`
- `clang`, `clang++`
- `pkg-config`
- `exiv2`
- `zstd`
- `raylib` (GUI work)
- `onnxruntime` (if ML-enabled workflows are required)

Install example:
```bash
xcode-select --install
brew install make pkg-config exiv2 zstd raylib onnxruntime
```

### Linux (supported notes)
Install equivalent dev packages for:
- compiler toolchain
- `pkg-config`
- `libexiv2-dev`
- `libzstd-dev`
- `raylib`
- ONNX Runtime C headers/libs

## 2. Repository Bootstrap
```bash
git clone <repo-url>
cd CGalleryOrganizer
make
make test
```

If GUI changes are expected:
```bash
make gui
```

## 3. Model Setup
Install local model artifacts:
```bash
make models
```

Optional override:
```bash
export CGO_MODELS_ROOT=/absolute/path/to/models
```

## 4. Smoke Bootstrap Commands
```bash
mkdir -p build/smoke_source build/smoke_env
cp tests/assets/jpg/sample_exif.jpg build/smoke_source/test1.jpg
cp tests/assets/png/sample_no_exif.png build/smoke_source/test2.png
./build/bin/gallery_organizer build/smoke_source build/smoke_env
./build/bin/gallery_organizer build/smoke_source build/smoke_env --duplicates-report
./build/bin/gallery_organizer build/smoke_source build/smoke_env --similarity-report
```

## 5. Sandbox and Escalation Policy (Operator Checklist)
- Default to sandboxed execution.
- Approve escalation only when necessary (e.g., external writes, privileged commands, GUI launch blockers).
- Never approve broad destructive rules.
- Require explicit justification for escalated commands.

## 6. Branching and PR Workflow
1. Create a focused branch per change set.
2. Keep commits small and reviewable.
3. Before PR:
   - `make test`
   - `make gui` (if GUI touched)
   - docs sync check.
4. PR must include:
   - behavior summary
   - risk notes
   - validation evidence.

## 7. Prompt Contract Template for Agents
Use this structure for consistent execution:

```text
Goal:
Scope in:
Scope out:
Constraints:
Acceptance criteria:
Files expected to change:
Tests to run:
Do not:
```

Example:
```text
Goal: Add explicit duplicate action flags to CLI.
Scope in: src/main.c, src/cli/cli_args.c, docs, tests.
Scope out: app API type changes.
Constraints: no implicit mutating behavior; keep tests green.
Acceptance criteria: plain scan no longer moves duplicates; new flags documented and tested.
Files expected to change: listed explicitly.
Tests to run: make test.
Do not: modify dependency policy.
```

## 8. Execution Guardrails
- Require the agent to state assumptions before major edits.
- Require evidence for every behavior claim (tests or command output summary).
- Block merges if docs and behavior diverge.
- For file-moving logic, require explicit negative test coverage.

## 9. Rollback and Recovery Checklist
If a change causes regressions:
1. Stop new merges.
2. Reproduce with minimal command sequence.
3. Revert or patch in a dedicated hotfix branch.
4. Re-run full test gate.
5. Update audit backlog with root cause and prevention action.

If an agent run moves files unexpectedly:
1. Stop execution.
2. Capture `git status` and affected paths.
3. Use app rollback flow when applicable.
4. Restore test fixtures before continuing.

## 10. Daily Operating Rhythm
- Start: pull latest, run `make test`.
- During: require scoped prompts and explicit checkpoints.
- End: summarize outcomes, risks, and next actions in writing.
