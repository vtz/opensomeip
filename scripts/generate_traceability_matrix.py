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
from typing import Dict, List, Set, Tuple


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


def extract_requirements_from_rst(rst_dir: Path) -> Tuple[Dict[str, Requirement], Dict[str, List[str]]]:
    """Extract requirements and satisfies relationships from RST files."""
    requirements = {}
    satisfies_map = {}

    for rst_file in rst_dir.rglob("*.rst"):
        content = rst_file.read_text(encoding='utf-8', errors='ignore')

        # Pattern to match requirement directives - first find the requirement block
        req_pattern = re.compile(
            r'\.\.\s+requirement::\s*(.+?)\n((?:\s+:[a-z_]+:.*?\n)+)',
            re.DOTALL | re.IGNORECASE
        )

        for match in req_pattern.finditer(content):
            title = match.group(1).strip()
            attrs_block = match.group(2)

            # Extract :id: field
            id_match = re.search(r':id:\s*(REQ_[A-Za-z0-9_]+)', attrs_block, re.IGNORECASE)
            if not id_match:
                continue
            req_id = id_match.group(1).upper()

            # Extract :satisfies: field
            satisfies_match = re.search(r':satisfies:\s*([^\n]+)', attrs_block, re.IGNORECASE)
            satisfies_str = satisfies_match.group(1) if satisfies_match else ""

            satisfies = [s.strip() for s in satisfies_str.split(",") if s.strip()]

            requirements[req_id] = Requirement(
                id=req_id,
                title=title,
                type="requirement",
                satisfies=satisfies
            )

            if satisfies:
                satisfies_map[req_id] = satisfies

    return requirements, satisfies_map


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


def classify_requirement(req_id: str) -> str:
    """Classify a requirement by its category."""
    if "_E0" in req_id or "_E1" in req_id:
        return "error_handling"
    if req_id.startswith("REQ_ARCH_"):
        return "architectural"
    if req_id.startswith("REQ_E2E_PLUGIN_"):
        return "plugin"
    if req_id.startswith("REQ_TRANSPORT_"):
        return "transport"
    if req_id.startswith("REQ_MSG_"):
        return "message"
    if req_id.startswith("REQ_SER_"):
        return "serialization"
    if req_id.startswith("REQ_SD_"):
        return "service_discovery"
    if req_id.startswith("REQ_TP_"):
        return "transport_protocol"
    return "other"


def get_priority(req_id: str, category: str) -> str:
    """
    Get priority classification for a requirement.

    Priority levels:
    - critical: Core protocol functionality (message header, basic serialization)
    - high: Important features (transport, SD basics)
    - medium: Extended features (complex types, advanced SD)
    - low: Error handling, optional features
    """
    # Error handling is generally lower priority
    if category == "error_handling":
        return "low"

    # Architectural requirements are high priority
    if category == "architectural":
        return "high"

    # Core message header requirements are critical
    if category == "message":
        # Basic header fields are critical
        num_part = req_id.replace("REQ_MSG_", "").split("_")[0]
        try:
            num = int(num_part)
            if num <= 20:  # Basic header structure
                return "critical"
            elif num <= 50:  # Extended fields
                return "high"
            else:  # Advanced features
                return "medium"
        except ValueError:
            return "medium"

    # Basic serialization is critical
    if category == "serialization":
        num_part = req_id.replace("REQ_SER_", "").split("_")[0]
        try:
            num = int(num_part)
            if num <= 20:  # Basic types
                return "critical"
            elif num <= 40:  # Arrays and strings
                return "high"
            else:  # Complex types
                return "medium"
        except ValueError:
            return "medium"

    # Transport protocol has high priority
    if category == "transport_protocol":
        num_part = req_id.replace("REQ_TP_", "").split("_")[0]
        try:
            num = int(num_part)
            if num <= 20:  # Segmentation
                return "high"
            elif num <= 50:  # Reassembly
                return "medium"
            else:  # Statistics, monitoring
                return "low"
        except ValueError:
            return "medium"

    # Service Discovery has medium priority
    if category == "service_discovery":
        return "medium"

    # Plugin mechanisms have high priority
    if category == "plugin":
        return "high"

    return "medium"


def classify_test_type(test_path: str) -> str:
    """Classify test type based on file path."""
    if "/integration/" in test_path:
        return "integration"
    if "/system/" in test_path:
        return "system"
    if test_path.endswith(".py"):
        # Python tests are often integration/system tests
        if "integration" in test_path.lower():
            return "integration"
        if "system" in test_path.lower():
            return "system"
        return "integration"
    # C++ tests are typically unit tests
    return "unit"


def generate_gap_analysis(
    requirements: Dict[str, Requirement],
    satisfies_map: Dict[str, List[str]],
    output_path: Path,
    test_cases: Dict[str, TestCase] = None
):
    """Generate gap analysis report highlighting traceability gaps."""
    from collections import defaultdict

    gaps = {
        "no_implementation": [],
        "no_tests": [],
        "missing_spec_links": [],
        "missing_spec_links_required": [],  # Only reqs that should have spec links
        "fully_traced": []
    }

    # Categories by requirement type
    by_category = defaultdict(lambda: {"total": 0, "implemented": 0, "tested": 0, "spec_linked": 0})

    # Priority tracking
    by_priority = defaultdict(lambda: {"total": 0, "implemented": 0, "tested": 0, "missing": []})

    # Test type tracking
    test_type_counts = {"unit": 0, "integration": 0, "system": 0}
    if test_cases:
        for tc_id, tc in test_cases.items():
            test_type = classify_test_type(tc.location)
            test_type_counts[test_type] += 1

    # Analyze each requirement
    for req_id, req in requirements.items():
        has_code = len(req.implemented_by) > 0
        has_tests = len(req.tested_by) > 0
        has_spec_links = len(satisfies_map.get(req_id, [])) > 0
        category = classify_requirement(req_id)
        priority = get_priority(req_id, category)

        # Update category stats
        by_category[category]["total"] += 1
        if has_code:
            by_category[category]["implemented"] += 1
        if has_tests:
            by_category[category]["tested"] += 1
        if has_spec_links:
            by_category[category]["spec_linked"] += 1

        # Update priority stats
        by_priority[priority]["total"] += 1
        if has_code:
            by_priority[priority]["implemented"] += 1
        if has_tests:
            by_priority[priority]["tested"] += 1
        if not has_code or not has_tests:
            by_priority[priority]["missing"].append(req_id)

        if has_code and has_tests:
            gaps["fully_traced"].append(req_id)
        else:
            if not has_code:
                gaps["no_implementation"].append(req_id)
            if not has_tests:
                gaps["no_tests"].append(req_id)

        # Check spec links for implementation requirements
        # Error handling, architectural, and plugin requirements may not need spec links
        if req_id.startswith("REQ_") and not has_spec_links:
            gaps["missing_spec_links"].append(req_id)
            if category not in ("error_handling", "architectural", "plugin"):
                gaps["missing_spec_links_required"].append(req_id)

    # Generate category summary table
    category_names = {
        "error_handling": "Error Handling (derived)",
        "architectural": "Architectural (derived)",
        "plugin": "Plugin (derived)",
        "transport": "Transport Layer",
        "message": "Message Header",
        "serialization": "Serialization",
        "service_discovery": "Service Discovery",
        "transport_protocol": "Transport Protocol",
        "other": "Other"
    }

    category_table = "| Category | Total | Implemented | Tested | Spec Linked |\n"
    category_table += "|----------|-------|-------------|--------|-------------|\n"
    for cat in sorted(by_category.keys()):
        stats = by_category[cat]
        name = category_names.get(cat, cat)
        impl_pct = stats['implemented'] / stats['total'] * 100 if stats['total'] > 0 else 0
        test_pct = stats['tested'] / stats['total'] * 100 if stats['total'] > 0 else 0
        spec_pct = stats['spec_linked'] / stats['total'] * 100 if stats['total'] > 0 else 0
        category_table += f"| {name} | {stats['total']} | {stats['implemented']} ({impl_pct:.0f}%) | {stats['tested']} ({test_pct:.0f}%) | {stats['spec_linked']} ({spec_pct:.0f}%) |\n"

    # Calculate derived vs spec-linked requirements
    derived_categories = {"error_handling", "architectural", "plugin"}
    derived_count = sum(by_category[cat]["total"] for cat in derived_categories if cat in by_category)
    spec_derived_count = len(requirements) - derived_count

    # Generate report
    report = f"""# ASPICE Traceability Gap Analysis Report

Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

## Summary

- **Total Requirements**: {len(requirements)}
- **Fully Traced (impl + tests)**: {len(gaps['fully_traced'])} ({len(gaps['fully_traced'])/len(requirements)*100:.1f}%)
- **Missing Implementation**: {len(gaps['no_implementation'])}
- **Missing Tests**: {len(gaps['no_tests'])}
- **Missing Spec Links (all)**: {len(gaps['missing_spec_links'])}
- **Missing Spec Links (required only)**: {len(gaps['missing_spec_links_required'])}

### Requirement Categories

{category_table}

**Note**: Error handling, architectural, and plugin requirements are implementation-derived and
may not require direct spec links.

- **Spec-Derived Requirements**: {spec_derived_count}
- **Implementation-Derived Requirements**: {derived_count}

### Priority Breakdown

| Priority | Total | Implemented | Tested | Coverage |
|----------|-------|-------------|--------|----------|
| Critical | {by_priority['critical']['total']} | {by_priority['critical']['implemented']} | {by_priority['critical']['tested']} | {by_priority['critical']['implemented']/by_priority['critical']['total']*100 if by_priority['critical']['total'] > 0 else 0:.0f}% |
| High | {by_priority['high']['total']} | {by_priority['high']['implemented']} | {by_priority['high']['tested']} | {by_priority['high']['implemented']/by_priority['high']['total']*100 if by_priority['high']['total'] > 0 else 0:.0f}% |
| Medium | {by_priority['medium']['total']} | {by_priority['medium']['implemented']} | {by_priority['medium']['tested']} | {by_priority['medium']['implemented']/by_priority['medium']['total']*100 if by_priority['medium']['total'] > 0 else 0:.0f}% |
| Low | {by_priority['low']['total']} | {by_priority['low']['implemented']} | {by_priority['low']['tested']} | {by_priority['low']['implemented']/by_priority['low']['total']*100 if by_priority['low']['total'] > 0 else 0:.0f}% |

### Test Coverage Breakdown

| Test Type | Count |
|-----------|-------|
| Unit Tests | {test_type_counts.get('unit', 0)} |
| Integration Tests | {test_type_counts.get('integration', 0)} |
| System Tests | {test_type_counts.get('system', 0)} |

## Gaps Requiring Attention

### Requirements Without Implementation
{chr(10).join(f"- {req_id}" for req_id in gaps["no_implementation"]) or "None - All requirements have implementation"}

### Requirements Without Test Coverage
{chr(10).join(f"- {req_id}" for req_id in gaps["no_tests"]) or "None - All requirements have test coverage"}

### Implementation Requirements Without Spec Links (Required)
These requirements should have spec links but don't:

{chr(10).join(f"- {req_id}" for req_id in gaps["missing_spec_links_required"]) or "None - All spec-derived requirements have spec links"}

### Implementation-Derived Requirements Without Spec Links (Expected)
These are derived requirements (error handling, architectural, plugin) that don't need spec links:

{chr(10).join(f"- {req_id}" for req_id in gaps["missing_spec_links"] if classify_requirement(req_id) in derived_categories) or "None"}

## ASPICE Compliance Assessment

### SWE.1 (Software Requirements Analysis)
- **Status**: {'✅ PASS' if len(gaps['missing_spec_links_required']) == 0 else '⚠️ PARTIAL - Some spec-derived requirements missing links'}
- **Details**: Spec-derived requirements must satisfy at least one specification requirement
- **Derived Requirements**: {derived_count} implementation-derived requirements do not require spec links

### SWE.3 (Software Architectural Design)
- **Status**: {'✅ PASS' if len(gaps['no_implementation']) == 0 else '❌ FAIL - Missing implementations'}
- **Details**: All requirements must have corresponding code implementation

### SWE.6 (Software Unit Verification)
- **Status**: {'✅ PASS' if len(gaps['no_tests']) == 0 else '❌ FAIL - Missing test coverage'}
- **Details**: All requirements must have corresponding test coverage

### Overall Compliance Level
- **Current Level**: {'CL2' if len(gaps['fully_traced']) == len(requirements) else 'CL1' if len(gaps['fully_traced']) >= len(requirements) * 0.8 else 'CL0'}
- **Target for Production**: CL2 (100% traceability)
- **Gap to Target**: {len(requirements) - len(gaps['fully_traced'])} requirements

## Recommendations

1. **Immediate Actions**:
   - Address requirements without implementation or test coverage
   - Add missing `:satisfies:` links to implementation requirements

2. **Process Improvements**:
   - Integrate `validate_requirements.py --strict` into CI pipeline
   - Require traceability annotations in code review checklist

3. **Documentation Updates**:
   - Update requirement status fields based on actual implementation state
   - Generate this report automatically in CI/CD pipeline
"""

    output_path.write_text(report)


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
    requirements, satisfies_map = extract_requirements_from_rst(args.requirements_dir)

    if args.verbose:
        print(f"Loading code references from: {args.code_refs}")
    code_refs, test_cases = load_code_references(args.code_refs)

    # Build traceability
    requirements = build_traceability(requirements, code_refs, test_cases)

    # Generate outputs
    html_path = args.output_dir / "matrix.html"
    json_path = args.output_dir / "matrix.json"
    csv_path = args.output_dir / "matrix.csv"
    gap_report_path = args.output_dir / "gap_analysis.md"

    if args.verbose:
        print(f"Generating HTML: {html_path}")
    generate_html(requirements, code_refs, test_cases, html_path)

    if args.verbose:
        print(f"Generating JSON: {json_path}")
    generate_json(requirements, code_refs, test_cases, json_path)

    if args.verbose:
        print(f"Generating CSV: {csv_path}")
    generate_csv(requirements, code_refs, test_cases, csv_path)

    if args.verbose:
        print(f"Generating gap analysis: {gap_report_path}")
    generate_gap_analysis(requirements, satisfies_map, gap_report_path, test_cases)

    print(f"Traceability matrix generated:")
    print(f"  HTML: {html_path}")
    print(f"  JSON: {json_path}")
    print(f"  CSV: {csv_path}")
    print(f"  Gap Analysis: {gap_report_path}")
    print(f"\nSummary:")
    print(f"  Requirements: {len(requirements)}")
    print(f"  Code References: {len(code_refs)}")
    print(f"  Test Cases: {len(test_cases)}")

    # Quick gap summary
    fully_traced = sum(1 for req in requirements.values() if req.implemented_by and req.tested_by)
    print(f"  Fully Traced: {fully_traced}/{len(requirements)} ({fully_traced/len(requirements)*100:.1f}%)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
