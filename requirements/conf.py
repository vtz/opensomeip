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

"""
Sphinx configuration for OpenSOMEIP Requirements Documentation.

This configuration uses sphinx-needs for requirements management and traceability.
"""

import os
import sys
from pathlib import Path

# Project information
project = "OpenSOMEIP Requirements"
copyright = "2025, Vinicius Tadeu Zein"
author = "Vinicius Tadeu Zein"
version = "0.0.1"
release = "0.0.1"

# Sphinx extensions
extensions = [
    "sphinx_needs",
    "sphinxcontrib.plantuml",
]

# Sphinx-needs configuration
needs_id_regex = r"^[A-Za-z0-9_]+"
needs_title_from_content = False

# Define needs types for OpenSOMEIP
needs_types = [
    {
        "directive": "requirement",
        "title": "Requirement",
        "prefix": "REQ_",
        "color": "#BFD8D2",
        "style": "node",
    },
    {
        "directive": "spec_req",
        "title": "Specification Requirement",
        "prefix": "feat_req_",
        "color": "#FEDCD2",
        "style": "node",
    },
    {
        "directive": "test_case",
        "title": "Test Case",
        "prefix": "TC_",
        "color": "#DCB239",
        "style": "node",
    },
    {
        "directive": "code_ref",
        "title": "Code Reference",
        "prefix": "CODE_",
        "color": "#DF744A",
        "style": "node",
    },
]

# Extra options for needs
# Note: 'status' is a core field in sphinx-needs and should not be listed here.
# Link types (satisfies, implements, tested_by) are defined in needs_extra_links
# and automatically create their own options, so they must not be duplicated here.
needs_extra_options = [
    "code_location",
    "priority",
]

# Extra links for traceability
needs_extra_links = [
    {
        "option": "satisfies",
        "incoming": "satisfied_by",
        "outgoing": "satisfies",
        "copy": False,
        "color": "#00AA00",
    },
    {
        "option": "implements",
        "incoming": "implemented_by",
        "outgoing": "implements",
        "copy": False,
        "color": "#0000AA",
    },
    {
        "option": "tested_by",
        "incoming": "tests",
        "outgoing": "tested_by",
        "copy": False,
        "color": "#AA0000",
    },
]

# Import needs from open-someip-spec submodule
# This provides automatic traceability to the SOME/IP specification requirements
spec_submodule_path = Path(__file__).parent.parent.parent / "open-someip-spec"
spec_needs_path = spec_submodule_path / "build" / "needs.json"

needs_import = []

# Check if spec submodule is available and has requirements
if spec_submodule_path.exists() and spec_submodule_path.is_dir():
    # Check if .git file exists (indicates submodule is initialized)
    git_file = spec_submodule_path / ".git"
    if git_file.exists():
        if spec_needs_path.exists():
            needs_import = [str(spec_needs_path)]
            print(f"Info: Importing spec requirements from {spec_needs_path}")
        else:
            print(f"Warning: Spec requirements not found at {spec_needs_path}")
            print("         Run 'cd open-someip-spec && make requirements' to generate them")
    else:
        print("Warning: open-someip-spec submodule not initialized")
        print("         Run 'git submodule update --init --recursive' to initialize")
else:
    print("Info: open-someip-spec submodule not present - spec traceability disabled")

# Remove default values from exported JSON
needs_json_remove_defaults = True

# HTML theme
html_theme = "alabaster"
html_static_path = ["_static"]
html_css_files = ["custom.css"]

# PlantUML configuration
plantuml_output_format = "svg"

# Source settings
source_suffix = ".rst"
master_doc = "index"

# Exclude patterns
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

# Output settings
html_title = "OpenSOMEIP Requirements"
html_short_title = "Requirements"
