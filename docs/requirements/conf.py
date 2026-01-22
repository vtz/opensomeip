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
needs_extra_options = [
    "satisfies",
    "implements",
    "tested_by",
    "code_location",
    "priority",
    "status",
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

# Import needs from open-someip-spec
# Path is relative to the build directory
needs_import_path = Path(__file__).parent.parent.parent / "open-someip-spec" / "build" / "needs.json"
if needs_import_path.exists():
    needs_import = [str(needs_import_path)]
else:
    needs_import = []

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
