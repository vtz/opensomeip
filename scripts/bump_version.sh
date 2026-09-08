#!/bin/bash
################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
################################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print usage
usage() {
    echo "Usage: $0 <patch|minor|major> [version]"
    echo ""
    echo "Bump the project version following semantic versioning."
    echo ""
    echo "Arguments:"
    echo "  patch    Bump patch version (e.g., 1.2.3 -> 1.2.4)"
    echo "  minor    Bump minor version (e.g., 1.2.3 -> 1.3.0)"
    echo "  major    Bump major version (e.g., 1.2.3 -> 2.0.0)"
    echo ""
    echo "  version  Set to a specific version (e.g., 2.1.0)"
    echo ""
    echo "Examples:"
    echo "  $0 patch         # Bump patch version"
    echo "  $0 minor         # Bump minor version"
    echo "  $0 major         # Bump major version"
    echo "  $0 1.2.3         # Set specific version"
    exit 1
}

# Function to validate semantic version
validate_version() {
    local version="$1"
    if ! [[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo -e "${RED}Error: Invalid version format '$version'. Expected semantic version (e.g., 1.2.3)${NC}" >&2
        exit 1
    fi
}

# Function to bump version
bump_version() {
    local current_version="$1"
    local bump_type="$2"

    IFS='.' read -ra VERSION_PARTS <<< "$current_version"
    local major=${VERSION_PARTS[0]}
    local minor=${VERSION_PARTS[1]}
    local patch=${VERSION_PARTS[2]}

    case $bump_type in
        patch)
            patch=$((patch + 1))
            ;;
        minor)
            minor=$((minor + 1))
            patch=0
            ;;
        major)
            major=$((major + 1))
            minor=0
            patch=0
            ;;
        *)
            echo -e "${RED}Error: Invalid bump type '$bump_type'${NC}" >&2
            usage
            ;;
    esac

    echo "$major.$minor.$patch"
}

# Main script
main() {
    if [ $# -lt 1 ]; then
        usage
    fi

    local version_file="${PROJECT_ROOT}/VERSION"

    # Read current version
    if [ ! -f "$version_file" ]; then
        echo -e "${RED}Error: VERSION file not found at $version_file${NC}" >&2
        exit 1
    fi

    local current_version
    current_version=$(<"$version_file")
    current_version=$(echo "$current_version" | tr -d '[:space:]')

    echo -e "${BLUE}Current version:${NC} $current_version"

    local new_version

    if [ $# -eq 1 ]; then
        case $1 in
            patch|minor|major)
                new_version=$(bump_version "$current_version" "$1")
                echo -e "${BLUE}Bumping $1 version:${NC} $current_version -> $new_version"
                ;;
            *)
                # Treat as specific version
                validate_version "$1"
                new_version="$1"
                echo -e "${BLUE}Setting specific version:${NC} $current_version -> $new_version"
                ;;
        esac
    else
        # Specific version provided
        validate_version "$1"
        new_version="$1"
        echo -e "${BLUE}Setting specific version:${NC} $current_version -> $new_version"
    fi

    # Validate new version format
    validate_version "$new_version"

    # Write new version to VERSION file (single source of truth)
    echo "$new_version" > "$version_file"
    echo -e "${GREEN}Updated:${NC} VERSION"

    # Update packaging/opensomeip.spec
    local spec_file="${PROJECT_ROOT}/packaging/opensomeip.spec"
    if [ -f "$spec_file" ]; then
        sed -i.bak "s/^Version:.*$/Version:        ${new_version}/" "$spec_file"
        sed -i.bak "s/^Release:.*$/Release:        1%{?dist}/" "$spec_file"
        rm -f "${spec_file}.bak"
        echo -e "${GREEN}Updated:${NC} packaging/opensomeip.spec"
    fi

    # Update README.md
    local readme_file="${PROJECT_ROOT}/README.md"
    if [ -f "$readme_file" ]; then
        sed -i.bak "s/\*\*Current Version\*\*: [0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*/\*\*Current Version\*\*: ${new_version}/" "$readme_file"
        rm -f "${readme_file}.bak"
        echo -e "${GREEN}Updated:${NC} README.md"
    fi

    # Scaffold CHANGELOG.md entry (only if section doesn't already exist)
    local changelog_file="${PROJECT_ROOT}/CHANGELOG.md"
    if [ -f "$changelog_file" ]; then
        if ! grep -q "## \[${new_version}\]" "$changelog_file"; then
            local today
            today=$(date +%Y-%m-%d)
            # Insert new section after the "adheres to" preamble line
            sed -i.bak "/^## \[/i\\
## [${new_version}] - ${today}\\
\\
### Added\\
\\
### Fixed\\
\\
### Changed\\
" "$changelog_file"
            rm -f "${changelog_file}.bak"

            # Add comparison link if not present
            if ! grep -q "^\[${new_version}\]:" "$changelog_file"; then
                sed -i.bak "s|^\[${current_version}\]:|\[${new_version}\]: https://github.com/vtz/opensomeip/compare/v${current_version}...v${new_version}\n\[${current_version}\]:|" "$changelog_file"
                rm -f "${changelog_file}.bak"
            fi
            echo -e "${GREEN}Updated:${NC} CHANGELOG.md (scaffolded new section)"
        else
            echo -e "${YELLOW}Skipped:${NC} CHANGELOG.md (section for ${new_version} already exists)"
        fi
    fi

    echo ""
    echo -e "${GREEN}Version updated successfully!${NC}"
    echo -e "${BLUE}New version:${NC} $new_version"
    echo ""
    echo -e "${BLUE}Files modified:${NC}"
    echo "  VERSION"
    echo "  packaging/opensomeip.spec"
    echo "  README.md"
    echo "  CHANGELOG.md"

    # Check if git is available and suggest commit
    if command -v git >/dev/null 2>&1 && [ -d "${PROJECT_ROOT}/.git" ]; then
        echo ""
        echo -e "${YELLOW}Consider committing this version change:${NC}"
        echo "  git add VERSION packaging/opensomeip.spec README.md CHANGELOG.md"
        echo "  git commit -m \"release: bump version to $new_version\""
        echo "  git tag v$new_version"
    fi
}

main "$@"
