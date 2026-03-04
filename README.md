# CGalleryOrganizer

CGalleryOrganizer is a local-first media organizer written in C/C++ with two
frontends over the same backend API:
- CLI: `gallery_organizer`
- GUI: `gallery_organizer_gui`

It supports scan/cache, organize preview/execute, duplicate analysis,
pattern-based rename workflows, and optional local ML similarity enrichment.

## Quick Start

Prerequisites and platform notes live in
[docs/dependencies.md](docs/dependencies.md).

Build:
```bash
make
```

Run tests:
```bash
make test
```

Install models (needed for ML/similarity workflows):
```bash
make models
```

Run a first scan:
```bash
./build/bin/gallery_organizer /path/to/source /path/to/env
```

Build and run GUI (optional):
```bash
make gui
make run-gui
```

## Core Workflows

- Scan and cache metadata
- Preview and execute organization plans
- Generate duplicate reports and move duplicates explicitly
- Preview/apply rename plans with history, rollback, and redo support
- Generate similarity reports (optional ML path)

Detailed step-by-step guides are in
[docs/user/workflows.md](docs/user/workflows.md).

## Documentation

Start with [docs/README.md](docs/README.md) for audience-based navigation.

### End User Docs

- [docs/user/README.md](docs/user/README.md): start page for user-facing docs
- [docs/user/quickstart.md](docs/user/quickstart.md): minimal setup and first successful run
- [docs/user/cli.md](docs/user/cli.md): CLI usage and option families
- [docs/user/gui.md](docs/user/gui.md): GUI usage and operational behaviors
- [docs/user/workflows.md](docs/user/workflows.md): common real-world flows
- [tools/cache_viewer/README.md](tools/cache_viewer/README.md): cache viewer usage
- [tools/similarity_viewer/README.md](tools/similarity_viewer/README.md): similarity viewer usage

### Developer Docs

- [docs/dev/README.md](docs/dev/README.md): start page for developer docs
- [docs/app_api.md](docs/app_api.md): frontend/backend API contract
- [docs/frontends.md](docs/frontends.md): frontend architecture and ownership boundaries
- [docs/tests.md](docs/tests.md): test strategy, commands, and smoke gates
- [docs/CODE_STYLE.md](docs/CODE_STYLE.md): code style and modularity policy
- [docs/dependencies.md](docs/dependencies.md): dependency policy and setup
- [docs/git_policy.md](docs/git_policy.md): git and release workflow policy
- [docs/scope.md](docs/scope.md): scope history, release notes, and roadmap
- [docs/model_assets.md](docs/model_assets.md): model asset registry and attribution
- [docs/test_assets.md](docs/test_assets.md): test fixture registry and attribution

## Project Layout

- `src/`: implementation
- `include/`: public headers
- `tests/`: unit/integration/benchmark coverage
- `models/`: model manifest metadata
- `tools/`: static viewer frontends
- `docs/`: user and developer documentation

## Release Scope

For version-by-version scope and roadmap history, see
[docs/scope.md](docs/scope.md).
