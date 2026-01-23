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
Extract requirement traceability annotations from source code and test files.

This script parses source files for Doxygen-style requirement tags:
- @implements REQ_XXX  (code implements OpenSOMEIP requirement)
- @satisfies feat_req_someip_XXX  (code satisfies spec requirement)
- @test_case TC_XXX  (test case identifier)
- @tests REQ_XXX  (test verifies requirement)

Output is a JSON file compatible with sphinx-needs import.
"""

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set


@dataclass
class CodeReference:
    """Represents a code location that references requirements."""
    id: str
    file_path: str
    line_number: int
    function_name: Optional[str]
    implements: List[str] = field(default_factory=list)
    satisfies: List[str] = field(default_factory=list)
    description: str = ""


@dataclass
class TestCase:
    """Represents a test case that tests requirements."""
    id: str
    file_path: str
    line_number: int
    test_name: str
    tests: List[str] = field(default_factory=list)
    description: str = ""


# Regex patterns for extracting annotations
IMPLEMENTS_PATTERN = re.compile(r'@implements\s+(REQ_[A-Za-z0-9_]+)', re.IGNORECASE)
SATISFIES_PATTERN = re.compile(r'@satisfies\s+(feat_req_[a-z0-9_]+)', re.IGNORECASE)
TEST_CASE_PATTERN = re.compile(r'@test_case\s+(TC_[A-Za-z0-9_]+)', re.IGNORECASE)
TESTS_PATTERN = re.compile(r'@tests\s+([A-Za-z0-9_]+)', re.IGNORECASE)
BRIEF_PATTERN = re.compile(r'@brief\s+(.+?)(?:\n|\*\/|$)', re.IGNORECASE)

# Patterns for function/class detection
CPP_FUNCTION_PATTERN = re.compile(
    r'^(?:virtual\s+)?(?:static\s+)?(?:inline\s+)?'
    r'(?:\w+(?:<[^>]*>)?(?:::)?)+\s+'
    r'(\w+)\s*\([^)]*\)',
    re.MULTILINE
)
CPP_CLASS_PATTERN = re.compile(r'^(?:class|struct)\s+(\w+)', re.MULTILINE)
GTEST_PATTERN = re.compile(r'TEST(?:_F)?\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)')
PYTEST_PATTERN = re.compile(r'^def\s+(test_\w+)\s*\(', re.MULTILINE)


def find_source_files(root_dir: Path, extensions: Set[str]) -> List[Path]:
    """Find all source files with given extensions."""
    files = []
    for ext in extensions:
        files.extend(root_dir.rglob(f"*{ext}"))
    return sorted(files)


def extract_comment_blocks(content: str) -> List[tuple]:
    """Extract Doxygen comment blocks with their positions."""
    blocks = []

    # Multi-line /** ... */ comments
    pattern = re.compile(r'/\*\*(.+?)\*/', re.DOTALL)
    for match in pattern.finditer(content):
        blocks.append((match.start(), match.end(), match.group(1)))

    # Single-line /// comments
    pattern = re.compile(r'///(.+?)$', re.MULTILINE)
    for match in pattern.finditer(content):
        blocks.append((match.start(), match.end(), match.group(1)))

    return sorted(blocks, key=lambda x: x[0])


def extract_python_docstrings(content: str) -> List[tuple]:
    """Extract Python docstrings with their positions."""
    blocks = []

    # Triple-quoted docstrings
    pattern = re.compile(r'"""(.+?)"""', re.DOTALL)
    for match in pattern.finditer(content):
        blocks.append((match.start(), match.end(), match.group(1)))

    pattern = re.compile(r"'''(.+?)'''", re.DOTALL)
    for match in pattern.finditer(content):
        blocks.append((match.start(), match.end(), match.group(1)))

    return sorted(blocks, key=lambda x: x[0])


def get_line_number(content: str, position: int) -> int:
    """Get line number for a position in content."""
    return content[:position].count('\n') + 1


def find_following_function(content: str, comment_end: int) -> Optional[str]:
    """Find the function name following a comment block."""
    # Look at the next 500 characters after the comment
    following = content[comment_end:comment_end + 500]

    # Try to find a C++ function
    match = CPP_FUNCTION_PATTERN.search(following)
    if match:
        return match.group(1)

    # Try to find a class
    match = CPP_CLASS_PATTERN.search(following)
    if match:
        return match.group(1)

    # Try to find a Google Test
    match = GTEST_PATTERN.search(following)
    if match:
        return f"{match.group(1)}.{match.group(2)}"

    return None


def find_following_pytest(content: str, comment_end: int) -> Optional[str]:
    """Find the pytest function name following a docstring."""
    # Look at the previous 200 characters (docstring is inside function)
    preceding = content[max(0, comment_end - 500):comment_end]

    match = PYTEST_PATTERN.search(preceding)
    if match:
        return match.group(1)

    return None


def extract_from_cpp_file(file_path: Path) -> tuple:
    """Extract code references and test cases from C++ file."""
    code_refs = []
    test_cases = []

    content = file_path.read_text(encoding='utf-8', errors='ignore')
    comment_blocks = extract_comment_blocks(content)

    is_test_file = 'test' in file_path.name.lower()

    for start, end, comment_text in comment_blocks:
        implements = IMPLEMENTS_PATTERN.findall(comment_text)
        satisfies = SATISFIES_PATTERN.findall(comment_text)
        test_case_ids = TEST_CASE_PATTERN.findall(comment_text)
        tests = TESTS_PATTERN.findall(comment_text)
        brief_match = BRIEF_PATTERN.search(comment_text)

        if not (implements or satisfies or test_case_ids or tests):
            continue

        line_number = get_line_number(content, start)
        function_name = find_following_function(content, end)
        description = brief_match.group(1).strip() if brief_match else ""

        if test_case_ids or tests:
            # This is a test case
            for tc_id in test_case_ids:
                test_case = TestCase(
                    id=tc_id,
                    file_path=str(file_path),
                    line_number=line_number,
                    test_name=function_name or "unknown",
                    tests=tests + implements + satisfies,
                    description=description
                )
                test_cases.append(test_case)

        if implements or satisfies:
            # This is a code reference
            ref_id = f"CODE_{file_path.stem}_{line_number}"
            code_ref = CodeReference(
                id=ref_id,
                file_path=str(file_path),
                line_number=line_number,
                function_name=function_name,
                implements=implements,
                satisfies=satisfies,
                description=description
            )
            code_refs.append(code_ref)

    return code_refs, test_cases


def extract_from_python_file(file_path: Path) -> tuple:
    """Extract test cases from Python test file."""
    code_refs = []
    test_cases = []

    content = file_path.read_text(encoding='utf-8', errors='ignore')
    docstrings = extract_python_docstrings(content)

    for start, end, docstring_text in docstrings:
        test_case_ids = TEST_CASE_PATTERN.findall(docstring_text)
        tests = TESTS_PATTERN.findall(docstring_text)
        implements = IMPLEMENTS_PATTERN.findall(docstring_text)
        satisfies = SATISFIES_PATTERN.findall(docstring_text)

        if not (test_case_ids or tests or implements or satisfies):
            continue

        line_number = get_line_number(content, start)
        function_name = find_following_pytest(content, end)

        # Extract first line as description
        first_line = docstring_text.strip().split('\n')[0].strip()
        if first_line.startswith('@'):
            first_line = ""

        if test_case_ids or tests:
            for tc_id in test_case_ids or [f"TC_{file_path.stem}_{line_number}"]:
                test_case = TestCase(
                    id=tc_id,
                    file_path=str(file_path),
                    line_number=line_number,
                    test_name=function_name or "unknown",
                    tests=tests + implements + satisfies,
                    description=first_line
                )
                test_cases.append(test_case)

    return code_refs, test_cases


def generate_needs_json(code_refs: List[CodeReference],
                        test_cases: List[TestCase],
                        project_root: Path) -> dict:
    """Generate sphinx-needs compatible JSON."""
    needs = {
        "created": "extract_code_requirements.py",
        "project": "OpenSOMEIP",
        "versions": {
            "current": {
                "needs": {}
            }
        }
    }

    current_needs = needs["versions"]["current"]["needs"]

    # Add code references
    for ref in code_refs:
        rel_path = Path(ref.file_path).relative_to(project_root)
        current_needs[ref.id] = {
            "id": ref.id,
            "type": "code_ref",
            "title": f"{ref.function_name or 'Code'} ({rel_path}:{ref.line_number})",
            "description": ref.description,
            "code_location": f"{rel_path}:{ref.line_number}",
            "implements": ", ".join(ref.implements),
            "satisfies": ", ".join(ref.satisfies),
        }

    # Add test cases
    for tc in test_cases:
        rel_path = Path(tc.file_path).relative_to(project_root)
        current_needs[tc.id] = {
            "id": tc.id,
            "type": "test_case",
            "title": f"{tc.test_name} ({rel_path}:{tc.line_number})",
            "description": tc.description,
            "code_location": f"{rel_path}:{tc.line_number}",
            "tests": ", ".join(tc.tests),
        }

    return needs


def main():
    parser = argparse.ArgumentParser(
        description="Extract requirement traceability from source code"
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path.cwd(),
        help="Project root directory"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("code_references.json"),
        help="Output JSON file"
    )
    parser.add_argument(
        "--src-dirs",
        nargs="+",
        default=["src", "include"],
        help="Source directories to scan"
    )
    parser.add_argument(
        "--test-dirs",
        nargs="+",
        default=["tests"],
        help="Test directories to scan"
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Verbose output"
    )

    args = parser.parse_args()

    all_code_refs = []
    all_test_cases = []

    # Scan source directories
    for src_dir in args.src_dirs:
        src_path = args.project_root / src_dir
        if not src_path.exists():
            if args.verbose:
                print(f"Skipping non-existent directory: {src_path}")
            continue

        cpp_files = find_source_files(src_path, {".cpp", ".h", ".hpp"})
        for file_path in cpp_files:
            if args.verbose:
                print(f"Processing: {file_path}")
            code_refs, test_cases = extract_from_cpp_file(file_path)
            all_code_refs.extend(code_refs)
            all_test_cases.extend(test_cases)

    # Scan test directories
    for test_dir in args.test_dirs:
        test_path = args.project_root / test_dir
        if not test_path.exists():
            if args.verbose:
                print(f"Skipping non-existent directory: {test_path}")
            continue

        # C++ tests
        cpp_files = find_source_files(test_path, {".cpp", ".h", ".hpp"})
        for file_path in cpp_files:
            if args.verbose:
                print(f"Processing: {file_path}")
            code_refs, test_cases = extract_from_cpp_file(file_path)
            all_code_refs.extend(code_refs)
            all_test_cases.extend(test_cases)

        # Python tests
        py_files = find_source_files(test_path, {".py"})
        for file_path in py_files:
            if args.verbose:
                print(f"Processing: {file_path}")
            code_refs, test_cases = extract_from_python_file(file_path)
            all_code_refs.extend(code_refs)
            all_test_cases.extend(test_cases)

    # Generate output
    needs_json = generate_needs_json(all_code_refs, all_test_cases, args.project_root)

    # Write output
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, 'w') as f:
        json.dump(needs_json, f, indent=2)

    print(f"Extracted {len(all_code_refs)} code references")
    print(f"Extracted {len(all_test_cases)} test cases")
    print(f"Output written to: {args.output}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
