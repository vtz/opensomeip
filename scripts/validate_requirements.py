#!/usr/bin/env python3
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
Validate requirements completeness and traceability.

This script checks:
- All requirements have at least one code reference
- All requirements have at least one test case
- Implementation requirements (REQ_*) have spec requirement links (:satisfies:)
- No orphaned code references (referencing non-existent requirements)
- No orphaned test cases (testing non-existent requirements)

Exit codes:
- 0: All validations passed
- 1: Errors found or strict mode violations
- 2: No requirements found (configuration issue)

Usage in CI/pre-commit:
  python scripts/validate_requirements.py --strict
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple


def load_json(file_path: Path) -> dict:
    """Load a JSON file."""
    if not file_path.exists():
        return {}
    with open(file_path, 'r') as f:
        return json.load(f)


def extract_requirements_from_rst(rst_dir: Path) -> Tuple[Set[str], Dict[str, List[str]]]:
    """Extract requirement IDs and their satisfies relationships from RST files."""
    requirements = set()
    satisfies_map = {}  # req_id -> list of satisfied spec requirements

    import re

    # Pattern to find requirement blocks - captures the entire directive block
    req_block_pattern = re.compile(
        r'\.\.\s+requirement::.*?\n((?:\s+:[^\n]+\n)+)',
        re.IGNORECASE
    )

    # Patterns for individual fields
    id_pattern = re.compile(r':id:\s*(REQ_[A-Za-z0-9_]+)', re.IGNORECASE)
    satisfies_pattern = re.compile(r':satisfies:\s*([^\n]+)', re.IGNORECASE)

    for rst_file in rst_dir.rglob("*.rst"):
        content = rst_file.read_text(encoding='utf-8', errors='ignore')

        for block_match in req_block_pattern.finditer(content):
            block_content = block_match.group(1)

            # Extract ID
            id_match = id_pattern.search(block_content)
            if not id_match:
                continue

            req_id = id_match.group(1).upper()
            requirements.add(req_id)

            # Extract satisfies (if present)
            satisfies_match = satisfies_pattern.search(block_content)
            if satisfies_match:
                satisfies_str = satisfies_match.group(1)
                satisfies = [s.strip() for s in satisfies_str.split(",") if s.strip()]
                satisfies_map[req_id] = satisfies

    return requirements, satisfies_map


def validate_requirements(
    requirements: Set[str],
    satisfies_map: Dict[str, List[str]],
    code_refs: dict,
    verbose: bool = False
) -> Tuple[List[str], List[str], List[str]]:
    """
    Validate requirements traceability.

    Returns:
        Tuple of (errors, warnings, info)
    """
    errors = []
    warnings = []
    info = []

    # Get code references and test cases from JSON
    needs = code_refs.get("versions", {}).get("current", {}).get("needs", {})

    code_implements = set()
    code_satisfies = set()
    test_tests = set()

    for need_id, need in needs.items():
        need_type = need.get("type", "")

        if need_type == "code_ref":
            implements = need.get("implements", "")
            satisfies = need.get("satisfies", "")

            for req in implements.split(","):
                req = req.strip().upper()
                if req:
                    code_implements.add(req)

            for req in satisfies.split(","):
                req = req.strip().lower()
                if req:
                    code_satisfies.add(req)

        elif need_type == "test_case":
            tests = need.get("tests", "")
            for req in tests.split(","):
                req = req.strip()
                if req.startswith("REQ_"):
                    test_tests.add(req.upper())
                elif req.startswith("feat_req_"):
                    test_tests.add(req.lower())

    # Check each requirement for implementation and testing
    orphaned_requirements = []
    for req_id in requirements:
        has_code = req_id in code_implements
        has_test = req_id in test_tests

        if not has_code:
            orphaned_requirements.append(req_id)
            warnings.append(f"Requirement {req_id} has no code implementation")

        if not has_test:
            warnings.append(f"Requirement {req_id} has no test coverage")

        if has_code and has_test:
            info.append(f"Requirement {req_id}: OK (implemented and tested)")

    # Check for missing spec links (implementation requirements must satisfy at least one spec requirement)
    # Skip validation for implementation-derived requirements (error handling, architectural, plugin)
    missing_spec_links = []
    for req_id in requirements:
        if req_id.startswith("REQ_"):  # Implementation requirement
            # Skip spec link validation for implementation-derived requirements
            is_implementation_derived = (
                '_E0' in req_id or  # Error handling (_E01, _E02, _E03)
                '_ARCH_' in req_id or  # Architectural
                'PLUGIN' in req_id or  # Plugin requirements
                'MY_' in req_id  # Custom requirements
            )

            if not is_implementation_derived:
                satisfies = satisfies_map.get(req_id, [])
                if not satisfies:
                    missing_spec_links.append(req_id)
                    errors.append(f"Implementation requirement {req_id} has no spec requirement links (missing :satisfies: field)")

    # Check for orphaned code references
    for ref in code_implements:
        if ref not in requirements:
            warnings.append(f"Code reference to non-existent requirement: {ref}")

    # Check for orphaned test references
    for ref in test_tests:
        if ref.startswith("REQ_") and ref not in requirements:
            warnings.append(f"Test reference to non-existent requirement: {ref}")

    # Summary with gap analysis
    # Fully traced means requirements that have BOTH code implementation AND test coverage
    fully_traced = len(requirements & code_implements & test_tests)
    spec_linked = len(requirements) - len(missing_spec_links)

    # Gap Analysis Summary
    info.append(f"\nGap Analysis Summary:")
    info.append(f"  Total requirements: {len(requirements)}")
    info.append(f"  Fully traced (code + tests): {fully_traced}/{len(requirements)} ({fully_traced/len(requirements)*100:.1f}%)")
    info.append(f"  Spec-linked implementation reqs: {spec_linked}/{len([r for r in requirements if r.startswith('REQ_')])}")
    info.append(f"  Requirements with code: {len(requirements & code_implements)}")
    info.append(f"  Requirements with tests: {len(requirements & test_tests)}")
    info.append(f"  Orphaned requirements (no code): {len(orphaned_requirements)}")
    info.append(f"  Missing spec links: {len(missing_spec_links)}")
    info.append(f"  Code references: {len(code_implements)}")
    info.append(f"  Test references: {len(test_tests)}")

    return errors, warnings, info


def main():
    parser = argparse.ArgumentParser(
        description="Validate requirements completeness"
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path.cwd(),
        help="Project root directory"
    )
    parser.add_argument(
        "--code-refs",
        type=Path,
        default=None,
        help="Code references JSON file"
    )
    parser.add_argument(
        "--requirements-dir",
        type=Path,
        default=None,
        help="Requirements RST directory"
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Verbose output"
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Treat warnings as errors"
    )
    parser.add_argument(
        "--ci-mode",
        action="store_true",
        help="CI mode: exit with code 2 if no requirements found"
    )

    args = parser.parse_args()

    # Default paths
    if args.code_refs is None:
        args.code_refs = args.project_root / "build" / "code_references.json"

    if args.requirements_dir is None:
        args.requirements_dir = args.project_root / "docs" / "requirements"

    # Load data
    code_refs = load_json(args.code_refs)
    requirements, satisfies_map = extract_requirements_from_rst(args.requirements_dir)

    if not requirements:
        if args.ci_mode:
            print("Error: No requirements found in RST files")
            return 2
        else:
            print("Warning: No requirements found in RST files")

    # Validate
    errors, warnings, info = validate_requirements(
        requirements, satisfies_map, code_refs, args.verbose
    )

    # Print results
    for msg in info:
        print(msg)

    if warnings:
        print("\nWarnings:")
        for msg in warnings:
            print(f"  - {msg}")

    if errors:
        print("\nErrors:")
        for msg in errors:
            print(f"  - {msg}")

    # Exit code
    if errors:
        return 1
    if args.strict and warnings:
        return 1

    print("\nValidation passed!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
