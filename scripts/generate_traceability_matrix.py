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
Generate traceability matrix from requirements and code references.

Outputs:
- HTML: Interactive traceability matrix
- JSON: Machine-readable traceability data
- CSV: Spreadsheet-compatible format
"""

import argparse
import csv
import json
import re
import sys
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Set


@dataclass
class Requirement:
    """Represents a requirement with its traceability links."""
    id: str
    title: str = ""
    type: str = "requirement"
    satisfies: List[str] = field(default_factory=list)
    implemented_by: List[str] = field(default_factory=list)
    tested_by: List[str] = field(default_factory=list)


@dataclass
class CodeReference:
    """Represents a code location."""
    id: str
    location: str
    implements: List[str] = field(default_factory=list)
    satisfies: List[str] = field(default_factory=list)


@dataclass
class TestCase:
    """Represents a test case."""
    id: str
    name: str
    location: str
    tests: List[str] = field(default_factory=list)


def extract_requirements_from_rst(rst_dir: Path) -> Dict[str, Requirement]:
    """Extract requirements from RST files."""
    requirements = {}
    
    for rst_file in rst_dir.rglob("*.rst"):
        content = rst_file.read_text(encoding='utf-8', errors='ignore')
        
        # Pattern to match requirement directives
        pattern = re.compile(
            r'\.\.\s+requirement::\s*(.+?)\n'
            r'\s+:id:\s*(REQ_[A-Za-z0-9_]+).*?'
            r'(?::satisfies:\s*([^\n]+))?',
            re.DOTALL | re.IGNORECASE
        )
        
        for match in pattern.finditer(content):
            title = match.group(1).strip()
            req_id = match.group(2).upper()
            satisfies_str = match.group(3) or ""
            
            satisfies = [s.strip() for s in satisfies_str.split(",") if s.strip()]
            
            requirements[req_id] = Requirement(
                id=req_id,
                title=title,
                type="requirement",
                satisfies=satisfies
            )
    
    return requirements


def load_code_references(json_path: Path) -> tuple:
    """Load code references and test cases from JSON."""
    code_refs = {}
    test_cases = {}
    
    if not json_path.exists():
        return code_refs, test_cases
    
    with open(json_path, 'r') as f:
        data = json.load(f)
    
    needs = data.get("versions", {}).get("current", {}).get("needs", {})
    
    for need_id, need in needs.items():
        need_type = need.get("type", "")
        
        if need_type == "code_ref":
            implements = [s.strip() for s in need.get("implements", "").split(",") if s.strip()]
            satisfies = [s.strip() for s in need.get("satisfies", "").split(",") if s.strip()]
            
            code_refs[need_id] = CodeReference(
                id=need_id,
                location=need.get("code_location", ""),
                implements=[i.upper() for i in implements],
                satisfies=satisfies
            )
        
        elif need_type == "test_case":
            tests = [s.strip() for s in need.get("tests", "").split(",") if s.strip()]
            
            test_cases[need_id] = TestCase(
                id=need_id,
                name=need.get("title", ""),
                location=need.get("code_location", ""),
                tests=tests
            )
    
    return code_refs, test_cases


def build_traceability(
    requirements: Dict[str, Requirement],
    code_refs: Dict[str, CodeReference],
    test_cases: Dict[str, TestCase]
) -> Dict[str, Requirement]:
    """Build full traceability links."""
    
    # Link code references to requirements
    for ref_id, ref in code_refs.items():
        for req_id in ref.implements:
            if req_id in requirements:
                requirements[req_id].implemented_by.append(ref_id)
    
    # Link test cases to requirements
    for tc_id, tc in test_cases.items():
        for req_ref in tc.tests:
            req_id = req_ref.upper() if req_ref.startswith("REQ_") else req_ref
            if req_id in requirements:
                requirements[req_id].tested_by.append(tc_id)
    
    return requirements


def generate_html(
    requirements: Dict[str, Requirement],
    code_refs: Dict[str, CodeReference],
    test_cases: Dict[str, TestCase],
    output_path: Path
):
    """Generate HTML traceability matrix."""
    
    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OpenSOMEIP Traceability Matrix</title>
    <style>
        body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 20px; }}
        h1 {{ color: #333; }}
        table {{ border-collapse: collapse; width: 100%; margin-top: 20px; }}
        th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
        th {{ background-color: #4CAF50; color: white; }}
        tr:nth-child(even) {{ background-color: #f2f2f2; }}
        tr:hover {{ background-color: #ddd; }}
        .status-ok {{ color: #28a745; }}
        .status-warning {{ color: #ffc107; }}
        .status-error {{ color: #dc3545; }}
        .code-ref {{ font-family: monospace; font-size: 0.9em; }}
        .summary {{ margin-bottom: 20px; padding: 15px; background: #f8f9fa; border-radius: 5px; }}
    </style>
</head>
<body>
    <h1>OpenSOMEIP Traceability Matrix</h1>
    <p>Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
    
    <div class="summary">
        <strong>Summary:</strong>
        <ul>
            <li>Total Requirements: {len(requirements)}</li>
            <li>Code References: {len(code_refs)}</li>
            <li>Test Cases: {len(test_cases)}</li>
        </ul>
    </div>
    
    <h2>Requirements Traceability</h2>
    <table>
        <tr>
            <th>Requirement ID</th>
            <th>Title</th>
            <th>Satisfies</th>
            <th>Implemented By</th>
            <th>Tested By</th>
            <th>Status</th>
        </tr>
"""
    
    for req_id in sorted(requirements.keys()):
        req = requirements[req_id]
        
        satisfies = ", ".join(req.satisfies) if req.satisfies else "-"
        implemented = ", ".join(req.implemented_by) if req.implemented_by else "-"
        tested = ", ".join(req.tested_by) if req.tested_by else "-"
        
        has_impl = len(req.implemented_by) > 0
        has_tests = len(req.tested_by) > 0
        
        if has_impl and has_tests:
            status = '<span class="status-ok">✓ Complete</span>'
        elif has_impl or has_tests:
            status = '<span class="status-warning">⚠ Partial</span>'
        else:
            status = '<span class="status-error">✗ Missing</span>'
        
        html += f"""        <tr>
            <td>{req_id}</td>
            <td>{req.title}</td>
            <td>{satisfies}</td>
            <td class="code-ref">{implemented}</td>
            <td>{tested}</td>
            <td>{status}</td>
        </tr>
"""
    
    html += """    </table>
    
    <h2>Code References</h2>
    <table>
        <tr>
            <th>ID</th>
            <th>Location</th>
            <th>Implements</th>
            <th>Satisfies (Spec)</th>
        </tr>
"""
    
    for ref_id in sorted(code_refs.keys()):
        ref = code_refs[ref_id]
        implements = ", ".join(ref.implements) if ref.implements else "-"
        satisfies = ", ".join(ref.satisfies) if ref.satisfies else "-"
        
        html += f"""        <tr>
            <td>{ref_id}</td>
            <td class="code-ref">{ref.location}</td>
            <td>{implements}</td>
            <td>{satisfies}</td>
        </tr>
"""
    
    html += """    </table>
    
    <h2>Test Cases</h2>
    <table>
        <tr>
            <th>ID</th>
            <th>Name</th>
            <th>Location</th>
            <th>Tests</th>
        </tr>
"""
    
    for tc_id in sorted(test_cases.keys()):
        tc = test_cases[tc_id]
        tests = ", ".join(tc.tests) if tc.tests else "-"
        
        html += f"""        <tr>
            <td>{tc_id}</td>
            <td>{tc.name}</td>
            <td class="code-ref">{tc.location}</td>
            <td>{tests}</td>
        </tr>
"""
    
    html += """    </table>
</body>
</html>
"""
    
    output_path.write_text(html)


def generate_json(
    requirements: Dict[str, Requirement],
    code_refs: Dict[str, CodeReference],
    test_cases: Dict[str, TestCase],
    output_path: Path
):
    """Generate JSON traceability data."""
    
    data = {
        "generated": datetime.now().isoformat(),
        "requirements": {
            req_id: {
                "id": req.id,
                "title": req.title,
                "type": req.type,
                "satisfies": req.satisfies,
                "implemented_by": req.implemented_by,
                "tested_by": req.tested_by
            }
            for req_id, req in requirements.items()
        },
        "code_references": {
            ref_id: {
                "id": ref.id,
                "location": ref.location,
                "implements": ref.implements,
                "satisfies": ref.satisfies
            }
            for ref_id, ref in code_refs.items()
        },
        "test_cases": {
            tc_id: {
                "id": tc.id,
                "name": tc.name,
                "location": tc.location,
                "tests": tc.tests
            }
            for tc_id, tc in test_cases.items()
        }
    }
    
    with open(output_path, 'w') as f:
        json.dump(data, f, indent=2)


def generate_csv(
    requirements: Dict[str, Requirement],
    code_refs: Dict[str, CodeReference],
    test_cases: Dict[str, TestCase],
    output_path: Path
):
    """Generate CSV traceability matrix."""
    
    with open(output_path, 'w', newline='') as f:
        writer = csv.writer(f)
        
        # Header
        writer.writerow([
            "Requirement ID",
            "Title",
            "Type",
            "Satisfies",
            "Implemented By",
            "Tested By",
            "Has Implementation",
            "Has Tests"
        ])
        
        # Data
        for req_id in sorted(requirements.keys()):
            req = requirements[req_id]
            writer.writerow([
                req.id,
                req.title,
                req.type,
                "; ".join(req.satisfies),
                "; ".join(req.implemented_by),
                "; ".join(req.tested_by),
                "Yes" if req.implemented_by else "No",
                "Yes" if req.tested_by else "No"
            ])


def main():
    parser = argparse.ArgumentParser(
        description="Generate traceability matrix"
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
        "--output-dir",
        type=Path,
        default=None,
        help="Output directory for generated files"
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Verbose output"
    )
    
    args = parser.parse_args()
    
    # Default paths
    if args.code_refs is None:
        args.code_refs = args.project_root / "build" / "code_references.json"
    
    if args.requirements_dir is None:
        args.requirements_dir = args.project_root / "docs" / "requirements"
    
    if args.output_dir is None:
        args.output_dir = args.project_root / "build" / "docs" / "traceability"
    
    # Ensure output directory exists
    args.output_dir.mkdir(parents=True, exist_ok=True)
    
    # Load data
    if args.verbose:
        print(f"Loading requirements from: {args.requirements_dir}")
    requirements = extract_requirements_from_rst(args.requirements_dir)
    
    if args.verbose:
        print(f"Loading code references from: {args.code_refs}")
    code_refs, test_cases = load_code_references(args.code_refs)
    
    # Build traceability
    requirements = build_traceability(requirements, code_refs, test_cases)
    
    # Generate outputs
    html_path = args.output_dir / "matrix.html"
    json_path = args.output_dir / "matrix.json"
    csv_path = args.output_dir / "matrix.csv"
    
    if args.verbose:
        print(f"Generating HTML: {html_path}")
    generate_html(requirements, code_refs, test_cases, html_path)
    
    if args.verbose:
        print(f"Generating JSON: {json_path}")
    generate_json(requirements, code_refs, test_cases, json_path)
    
    if args.verbose:
        print(f"Generating CSV: {csv_path}")
    generate_csv(requirements, code_refs, test_cases, csv_path)
    
    print(f"Traceability matrix generated:")
    print(f"  HTML: {html_path}")
    print(f"  JSON: {json_path}")
    print(f"  CSV: {csv_path}")
    print(f"\nSummary:")
    print(f"  Requirements: {len(requirements)}")
    print(f"  Code References: {len(code_refs)}")
    print(f"  Test Cases: {len(test_cases)}")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
