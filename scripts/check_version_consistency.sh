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
#
# Verify that all version-bearing files reference the same version as VERSION.
# Intended for CI — exits non-zero on any mismatch.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

VERSION=$(tr -d '[:space:]' < "${PROJECT_ROOT}/VERSION")
ERRORS=0

check() {
    local label="$1" actual="$2"
    if [ "$actual" != "$VERSION" ]; then
        echo -e "${RED}MISMATCH${NC} $label: got '$actual', expected '$VERSION'"
        ERRORS=$((ERRORS + 1))
    else
        echo -e "${GREEN}OK${NC}       $label: $actual"
    fi
}

# packaging/opensomeip.spec — Version: field
SPEC_FILE="${PROJECT_ROOT}/packaging/opensomeip.spec"
if [ -f "$SPEC_FILE" ]; then
    SPEC_VER=$(awk '/^Version:/ { print $2 }' "$SPEC_FILE")
    check "packaging/opensomeip.spec" "$SPEC_VER"
else
    echo "SKIP     packaging/opensomeip.spec (file not found)"
fi

# README.md — **Current Version**: X.Y.Z
README_FILE="${PROJECT_ROOT}/README.md"
if [ -f "$README_FILE" ]; then
    README_VER=$(sed -n 's/.*\*\*Current Version\*\*: \([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/p' "$README_FILE")
    check "README.md" "$README_VER"
else
    echo "SKIP     README.md (file not found)"
fi

# CHANGELOG.md — must have a section header for this version
CHANGELOG_FILE="${PROJECT_ROOT}/CHANGELOG.md"
if [ -f "$CHANGELOG_FILE" ]; then
    if grep -q "## \[${VERSION}\]" "$CHANGELOG_FILE"; then
        echo -e "${GREEN}OK${NC}       CHANGELOG.md: section [${VERSION}] found"
    else
        echo -e "${RED}MISMATCH${NC} CHANGELOG.md: no section for [${VERSION}]"
        ERRORS=$((ERRORS + 1))
    fi
else
    echo "SKIP     CHANGELOG.md (file not found)"
fi

echo ""
if [ "$ERRORS" -gt 0 ]; then
    echo -e "${RED}$ERRORS version mismatch(es) detected.${NC}"
    echo "Run ./scripts/bump_version.sh to synchronise all files."
    exit 1
else
    echo -e "${GREEN}All version references are consistent ($VERSION).${NC}"
fi
