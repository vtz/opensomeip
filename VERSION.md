# Version Management

This document describes the semantic versioning system used by the SOME/IP stack implementation.

## Current Version

The current version is stored in the `VERSION` file at the project root.

## Semantic Versioning

This project follows [Semantic Versioning](https://semver.org/) with the format `MAJOR.MINOR.PATCH`:

- **MAJOR**: Breaking changes that are not backward compatible
- **MINOR**: New features that are backward compatible
- **PATCH**: Bug fixes that are backward compatible

## Version File

The project version is maintained in a simple text file:

```
VERSION
```

This file contains only the version string (e.g., `0.0.1`).

## Version Management Scripts

### bump_version.sh

Bump the project version following semantic versioning rules:

```bash
# Bump patch version (0.0.1 -> 0.0.2)
./scripts/bump_version.sh patch

# Bump minor version (0.0.1 -> 0.1.0)
./scripts/bump_version.sh minor

# Bump major version (0.0.1 -> 1.0.0)
./scripts/bump_version.sh major

# Set specific version
./scripts/bump_version.sh 2.1.3
```

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
