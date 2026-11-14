#!/bin/bash
################################################################################
# Copyright (c) 2026 Vinicius Tadeu Zein
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
#
# Compare a release tag with the version strings that move together.
# A tag v0.2.2 requires version 0.2.2 in all of:
#   MODULE.bazel module() version
#   VERSION
#   README.md (**Current Version**)
#   packaging/opensomeip.spec (Version:)
#   CHANGELOG.md section ## [0.2.2]
#
# Usage: ./scripts/check_release_versions.sh v0.2.2

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

usage() {
    echo "Usage: $0 vX.Y.Z" >&2
    echo "Example: $0 v0.2.2" >&2
    exit 1
}

if [ $# -ne 1 ]; then
    usage
fi

tag="$1"
if [[ ! "$tag" =~ ^v([0-9]+\.[0-9]+\.[0-9]+)$ ]]; then
    echo -e "${RED}Error:${NC} tag must look like v0.2.2 (got '${tag}')" >&2
    exit 1
fi
expected="${BASH_REMATCH[1]}"

ERRORS=0

ok() {
    echo -e "${GREEN}OK${NC}       $1"
}

mismatch() {
    echo -e "${RED}MISMATCH${NC} $1"
    ERRORS=$((ERRORS + 1))
}

check_equal() {
    local label="$1"
    local actual="$2"
    if [ "$actual" = "$expected" ]; then
        ok "${label}: ${actual}"
    else
        mismatch "${label}: got '${actual}', expected '${expected}' (tag ${tag})"
    fi
}

version_file="${PROJECT_ROOT}/VERSION"
if [ ! -f "$version_file" ]; then
    mismatch "VERSION: file not found"
else
    file_ver="$(tr -d '[:space:]' < "$version_file")"
    check_equal "VERSION" "$file_ver"
fi

# module() version only. bazel_dep version attributes are not the release.
module_file="${PROJECT_ROOT}/MODULE.bazel"
if [ ! -f "$module_file" ]; then
    mismatch "MODULE.bazel: file not found"
else
    module_ver="$(awk '
        /^module[[:space:]]*\(/ { in_mod = 1 }
        in_mod && /^[[:space:]]*version[[:space:]]*=/ {
            if (match($0, /"[^"]*"/)) {
                print substr($0, RSTART + 1, RLENGTH - 2)
                exit
            }
        }
        in_mod && /^\)/ { exit }
    ' "$module_file")"
    if [ -z "${module_ver}" ]; then
        mismatch "MODULE.bazel: module() version not found"
    else
        check_equal "MODULE.bazel" "$module_ver"
    fi
fi

readme_file="${PROJECT_ROOT}/README.md"
if [ ! -f "$readme_file" ]; then
    mismatch "README.md: file not found"
else
    readme_ver="$(awk '
        {
            if (match($0, /\*\*Current Version\*\*: [^[:space:]]+/)) {
                s = substr($0, RSTART, RLENGTH)
                sub(/.*: /, "", s)
                print s
                exit
            }
        }
    ' "$readme_file")"
    if [ -z "${readme_ver}" ]; then
        mismatch "README.md: **Current Version** line not found"
    else
        check_equal "README.md" "$readme_ver"
    fi
fi

spec_file="${PROJECT_ROOT}/packaging/opensomeip.spec"
if [ ! -f "$spec_file" ]; then
    mismatch "packaging/opensomeip.spec: file not found"
else
    spec_ver="$(awk '/^Version:/ { print $2; exit }' "$spec_file")"
    if [ -z "${spec_ver}" ]; then
        mismatch "packaging/opensomeip.spec: Version field not found"
    else
        check_equal "packaging/opensomeip.spec" "$spec_ver"
    fi
fi

changelog_file="${PROJECT_ROOT}/CHANGELOG.md"
if [ ! -f "$changelog_file" ]; then
    mismatch "CHANGELOG.md: file not found"
else
    if awk -v ver="$expected" '
        BEGIN { found = 0 }
        /^## \[/ {
            line = $0
            sub(/^## \[/, "", line)
            sub(/\].*/, "", line)
            if (line == ver) { found = 1; exit }
        }
        END { exit !found }
    ' "$changelog_file"; then
        ok "CHANGELOG.md: section [${expected}]"
    else
        mismatch "CHANGELOG.md: no section [${expected}]"
    fi
fi

echo ""
if [ "$ERRORS" -gt 0 ]; then
    echo -e "${RED}${ERRORS} release version mismatch(es) for tag ${tag}.${NC}"
    exit 1
fi

echo -e "${GREEN}Release version strings match ${expected} (tag ${tag}).${NC}"
