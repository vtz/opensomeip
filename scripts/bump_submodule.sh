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
    echo "Usage: $0 [submodule] [target]"
    echo ""
    echo "Update git submodules to the latest commit or specific target."
    echo ""
    echo "Arguments:"
    echo "  submodule    Name of the submodule (default: all submodules)"
    echo "  target       Git reference to update to (default: origin/main)"
    echo "               Can be a branch, tag, or commit hash"
    echo ""
    echo "Examples:"
    echo "  $0                          # Update all submodules to latest main"
    echo "  $0 open-someip-spec         # Update open-someip-spec to latest main"
    echo "  $0 open-someip-spec v1.2.3  # Update to specific tag"
    echo "  $0 open-someip-spec abc123  # Update to specific commit"
    exit 1
}

# Function to check if we're in a git repository
check_git_repo() {
    if ! git rev-parse --git-dir >/dev/null 2>&1; then
        echo -e "${RED}Error: Not in a git repository${NC}" >&2
        exit 1
    fi
}

# Function to check if submodule exists
check_submodule() {
    local submodule="$1"
    if [ ! -d "$submodule" ]; then
        echo -e "${RED}Error: Submodule '$submodule' does not exist${NC}" >&2
        exit 1
    fi
    if ! git submodule status "$submodule" >/dev/null 2>&1; then
        echo -e "${RED}Error: '$submodule' is not a registered submodule${NC}" >&2
        exit 1
    fi
}

# Function to update submodule
update_submodule() {
    local submodule="$1"
    local target="${2:-origin/main}"

    echo -e "${BLUE}Updating submodule:${NC} $submodule"
    echo -e "${BLUE}Target:${NC} $target"

    # Get current commit
    local current_commit
    current_commit=$(git submodule status "$submodule" | awk '{print $1}' | sed 's/^+/-g/')
    echo -e "${BLUE}Current commit:${NC} ${current_commit:1:8}"

    # Update the submodule
    echo -e "${YELLOW}Fetching latest changes...${NC}"
    git submodule update --init --recursive "$submodule"

    # Change to the submodule directory and checkout target
    (
        cd "$submodule"
        echo -e "${YELLOW}Checking out $target...${NC}"
        git checkout "$target"
        git pull origin "$target" 2>/dev/null || true  # Ignore errors if target is a commit/tag
    )

    # Update the parent repo's submodule reference
    git add "$submodule"
    local new_commit
    new_commit=$(git submodule status "$submodule" | awk '{print $1}' | sed 's/^+/-g/')
    echo -e "${GREEN}Updated to commit:${NC} ${new_commit:1:8}"

    if [ "$current_commit" != "$new_commit" ]; then
        echo -e "${GREEN}Submodule updated successfully!${NC}"
        echo ""
        echo -e "${YELLOW}To commit this change:${NC}"
        echo "  git commit -m \"Update $submodule submodule to ${new_commit:1:8}\""
    else
        echo -e "${BLUE}Submodule was already up to date${NC}"
    fi
}

# Function to update all submodules
update_all_submodules() {
    local target="$1"
    local submodules=()
    while IFS= read -r line; do
        submodules+=("$line")
    done < <(git submodule status | awk '{print $2}')

    if [ ${#submodules[@]} -eq 0 ]; then
        echo -e "${YELLOW}No submodules found${NC}"
        return
    fi

    echo -e "${BLUE}Found ${#submodules[@]} submodule(s)${NC}"
    for submodule in "${submodules[@]}"; do
        echo ""
        update_submodule "$submodule" "$target"
    done
}

# Main script
main() {
    check_git_repo

    local submodule=""
    local target="origin/main"

    case $# in
        0)
            # Update all submodules to latest main
            update_all_submodules "$target"
            ;;
        1)
            # Update specific submodule to latest main
            submodule="$1"
            check_submodule "$submodule"
            update_submodule "$submodule" "$target"
            ;;
        2)
            # Update specific submodule to specific target
            submodule="$1"
            target="$2"
            check_submodule "$submodule"
            update_submodule "$submodule" "$target"
            ;;
        *)
            usage
            ;;
    esac
}

main "$@"
