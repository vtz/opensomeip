# Version Management

This document describes the semantic versioning system used by the SOME/IP stack implementation.

## Current Version

The current version is stored in the `VERSION` file at the project root.

## Semantic Versioning

This project follows [Semantic Versioning](https://semver.org/) with the format `MAJOR.MINOR.PATCH`:

- **MAJOR**: Breaking changes that are not backward compatible
- **MINOR**: New features that are backward compatible
- **PATCH**: Bug fixes that are backward compatible

## Single Source of Truth

The `VERSION` file at the project root is the single source of truth. All other
version references are derived from it—either dynamically at build time or
updated by `bump_version.sh`.

| File | How it gets the version |
|------|------------------------|
| `VERSION` | **Source of truth** — plain text, e.g. `0.1.0` |
| `CMakeLists.txt` | `file(READ ... VERSION)` — reads at configure time |
| `.packit.yaml` | `get-current-version: cat VERSION` — read by Packit |
| `packaging/opensomeip.spec` | Updated by `bump_version.sh` |
| `README.md` | Updated by `bump_version.sh` |
| `CHANGELOG.md` | Scaffolded by `bump_version.sh` |

## Version Management Scripts

### bump_version.sh

Bump the project version following semantic versioning rules. This script
updates **all** version-bearing files in a single command:

```bash
# Bump patch version (0.1.0 -> 0.1.1)
./scripts/bump_version.sh patch

# Bump minor version (0.1.0 -> 0.2.0)
./scripts/bump_version.sh minor

# Bump major version (0.1.0 -> 1.0.0)
./scripts/bump_version.sh major

# Set specific version
./scripts/bump_version.sh 2.1.3
```

Files updated automatically:
- `VERSION`
- `packaging/opensomeip.spec` (Version + Release reset)
- `README.md` (**Current Version** line)
- `CHANGELOG.md` (scaffolded section header + comparison link)

### check_version_consistency.sh

CI guard that verifies all version references match `VERSION`:

```bash
./scripts/check_version_consistency.sh
```

Exits non-zero if any file is out of sync. Run this in CI to catch drift before
it reaches `main`.

### bump_submodule.sh

Update git submodules to latest commits or specific targets:

```bash
# Update all submodules to latest main branch
./scripts/bump_submodule.sh

# Update specific submodule
./scripts/bump_submodule.sh open-someip-spec

# Update to specific tag/commit
./scripts/bump_submodule.sh open-someip-spec v1.2.3
```

## Release Process

1. **Development**: Work on features/fixes in feature branches
2. **Version Bump**: Use `bump_version.sh` to update version appropriately
3. **Submodule Updates**: Use `bump_submodule.sh` to update dependencies if needed
4. **Testing**: Ensure all tests pass
5. **Commit**: Commit version changes with descriptive message
6. **Tag**: Create git tag (e.g., `v1.2.3`)
7. **Release**: Push tag to trigger release process

## Version History

See git tags for version history: `git tag -l "v*"`

## Submodules

The project uses git submodules for external dependencies:

- **open-someip-spec**: SOME/IP protocol specification documents

Submodule versions are tracked in the main repository and updated using the `bump_submodule.sh` script.
