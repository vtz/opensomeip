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
# Verify that every release version string matches VERSION, including
# MODULE.bazel. The tag form is v plus that version, which is what a release
# of the current tree would use.
#
# The comparison itself is check_release_versions.sh so this entry point
# cannot ignore MODULE.bazel or the changelog section.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

version="$(tr -d '[:space:]' < "${PROJECT_ROOT}/VERSION")"
exec "${SCRIPT_DIR}/check_release_versions.sh" "v${version}"
