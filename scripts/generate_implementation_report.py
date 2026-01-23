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
Generate comprehensive implementation status report.

This script complements generate_requirement_diff.py by providing:
- Full implementation status report (not just diffs)
- Requirements implemented
- Tests added
- Coverage improvements
- Remaining gaps with priorities
- Next steps recommendations
- Category-wise breakdown
- Trend analysis (if historical data available)
"""

import argparse
import json
import re
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


def load_gap_analysis(gap_file: Path) -> dict:
    """Parse gap analysis markdown to extract key metrics."""
    if not gap_file.exists():
        return {}

    content = gap_file.read_text()
    metrics = {}

    # Extract summary metrics
    patterns = {
        "total_requirements": r"\*\*Total Requirements\*\*:\s*(\d+)",
        "fully_traced": r"\*\*Fully Traced.*?\*\*:\s*(\d+)",
        "missing_implementation": r"\*\*Missing Implementation\*\*:\s*(\d+)",
        "missing_tests": r"\*\*Missing Tests\*\*:\s*(\d+)",
        "missing_spec_links_all": r"\*\*Missing Spec Links \(all\)\*\*:\s*(\d+)",
        "missing_spec_links_required": r"\*\*Missing Spec Links \(required only\)\*\*:\s*(\d+)",
    }

    for key, pattern in patterns.items():
        match = re.search(pattern, content)
        if match:
            metrics[key] = int(match.group(1))

    # Extract priority breakdown
    priority_pattern = r"\|\s*(Critical|High|Medium|Low)\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(\d+)%\s*\|"
    priorities = {}
    for match in re.finditer(priority_pattern, content):
        priorities[match.group(1).lower()] = {
            "total": int(match.group(2)),
            "implemented": int(match.group(3)),
            "tested": int(match.group(4)),
            "coverage": int(match.group(5))
        }
    metrics["priorities"] = priorities

    # Extract test type counts
    test_pattern = r"\|\s*(Unit|Integration|System)\s*Tests?\s*\|\s*(\d+)\s*\|"
    test_types = {}
    for match in re.finditer(test_pattern, content):
        test_types[match.group(1).lower()] = int(match.group(2))
    metrics["test_types"] = test_types

    return metrics


def load_code_references(json_path: Path) -> Tuple[Set[str], Set[str]]:
    """Load implemented requirements and test cases from code references."""
    implemented = set()
    test_cases = set()

    if not json_path.exists():
        return implemented, test_cases

    with open(json_path, 'r') as f:
        data = json.load(f)

    needs = data.get("versions", {}).get("current", {}).get("needs", {})

    for need_id, need in needs.items():
        need_type = need.get("type", "")

        if need_type == "code_ref":
            implements = need.get("implements", "")
            for req in implements.split(","):
                req = req.strip().upper()
                if req.startswith("REQ_"):
                    implemented.add(req)

        elif need_type == "test_case":
            test_cases.add(need_id)

    return implemented, test_cases


def classify_requirement(req_id: str) -> Tuple[str, str]:
    """Classify requirement by category and priority."""
    # Category
    if "_E0" in req_id or "_E1" in req_id:
        category = "error_handling"
    elif req_id.startswith("REQ_ARCH_"):
        category = "architectural"
    elif req_id.startswith("REQ_E2E_PLUGIN_"):
        category = "plugin"
    elif req_id.startswith("REQ_TRANSPORT_"):
        category = "transport"
    elif req_id.startswith("REQ_MSG_"):
        category = "message"
    elif req_id.startswith("REQ_SER_"):
        category = "serialization"
    elif req_id.startswith("REQ_SD_"):
        category = "service_discovery"
    elif req_id.startswith("REQ_TP_"):
        category = "transport_protocol"
    else:
        category = "other"

    # Priority
    if category == "error_handling":
        priority = "low"
    elif category in ("architectural", "plugin"):
        priority = "high"
    elif category == "message":
        priority = "critical"
    elif category == "serialization":
        priority = "critical"
    else:
        priority = "medium"

    return category, priority


def generate_report(
    metrics: dict,
    implemented: Set[str],
    test_cases: Set[str],
    output_path: Optional[Path] = None
) -> str:
    """Generate comprehensive implementation status report."""

    now = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    lines = []
    lines.append("# OpenSOMEIP Implementation Status Report\n")
    lines.append(f"Generated: {now}\n")

    # Executive Summary
    lines.append("## Executive Summary\n")

    total = metrics.get("total_requirements", 0)
    fully_traced = metrics.get("fully_traced", 0)
    coverage_pct = fully_traced / total * 100 if total > 0 else 0

    lines.append("### Key Metrics\n")
    lines.append(f"- **Total Requirements**: {total}")
    lines.append(f"- **Fully Implemented & Tested**: {fully_traced} ({coverage_pct:.1f}%)")
    lines.append(f"- **Code References**: {len(implemented)}")
    lines.append(f"- **Test Cases**: {len(test_cases)}")
    lines.append("")

    # Status indicators
    if coverage_pct >= 90:
        status = "✅ READY FOR PRODUCTION"
    elif coverage_pct >= 70:
        status = "⚠️ READY FOR BETA"
    elif coverage_pct >= 50:
        status = "🔶 ALPHA STAGE"
    else:
        status = "🔴 DEVELOPMENT STAGE"

    lines.append(f"### Project Status: {status}\n")

    # Priority Analysis
    priorities = metrics.get("priorities", {})
    if priorities:
        lines.append("## Priority Analysis\n")
        lines.append("| Priority | Total | Implemented | Tested | Coverage | Status |")
        lines.append("|----------|-------|-------------|--------|----------|--------|")

        for prio in ["critical", "high", "medium", "low"]:
            data = priorities.get(prio, {})
            total_p = data.get("total", 0)
            impl = data.get("implemented", 0)
            tested = data.get("tested", 0)
            cov = data.get("coverage", 0)

            if cov >= 80:
                status = "✅"
            elif cov >= 50:
                status = "⚠️"
            else:
                status = "❌"

            lines.append(f"| {prio.capitalize()} | {total_p} | {impl} | {tested} | {cov}% | {status} |")
        lines.append("")

    # Test Coverage
    test_types = metrics.get("test_types", {})
    if test_types:
        lines.append("## Test Coverage\n")
        lines.append("| Test Level | Count | Description |")
        lines.append("|------------|-------|-------------|")
        lines.append(f"| Unit | {test_types.get('unit', 0)} | Component-level tests |")
        lines.append(f"| Integration | {test_types.get('integration', 0)} | Module interaction tests |")
        lines.append(f"| System | {test_types.get('system', 0)} | End-to-end tests |")
        lines.append("")

        total_tests = sum(test_types.values())
        lines.append(f"**Total Test Cases**: {total_tests}\n")

    # Gap Analysis Summary
    lines.append("## Gap Analysis\n")
    lines.append("### Implementation Gaps\n")

    missing_impl = metrics.get("missing_implementation", 0)
    missing_tests = metrics.get("missing_tests", 0)
    missing_spec = metrics.get("missing_spec_links_required", 0)

    lines.append(f"- Requirements without implementation: **{missing_impl}**")
    lines.append(f"- Requirements without test coverage: **{missing_tests}**")
    lines.append(f"- Requirements without spec links: **{missing_spec}**")
    lines.append("")

    # Recommendations
    lines.append("## Recommendations\n")

    lines.append("### Immediate Actions (P0)\n")
    if priorities.get("critical", {}).get("coverage", 100) < 80:
        lines.append("1. **Complete Critical Requirements**: Focus on completing the remaining critical priority requirements")
    if missing_spec > 0:
        lines.append("2. **Add Spec Links**: Add `:satisfies:` links to requirements missing spec references")
    if test_types.get("integration", 0) < 10:
        lines.append("3. **Add Integration Tests**: Increase integration test coverage")

    lines.append("\n### Short-term Actions (P1)\n")
    if priorities.get("high", {}).get("coverage", 100) < 70:
        lines.append("1. Implement remaining high-priority requirements")
    if test_types.get("system", 0) < 5:
        lines.append("2. Add more system-level tests for end-to-end validation")

    lines.append("\n### Long-term Actions (P2)\n")
    lines.append("1. Complete medium and low priority requirements")
    lines.append("2. Add comprehensive error handling tests")
    lines.append("3. Performance and stress testing")

    # CI/CD Status
    lines.append("\n## CI/CD Integration\n")
    lines.append("### Automated Checks\n")
    lines.append("- [x] Code requirements extraction (`extract_code_requirements.py`)")
    lines.append("- [x] Requirements validation (`validate_requirements.py`)")
    lines.append("- [x] Traceability matrix generation (`generate_traceability_matrix.py`)")
    lines.append("- [x] Gap analysis (`generate_traceability_matrix.py`)")
    lines.append("- [x] Spec mapping verification (`check_spec_requirements.py`)")
    lines.append("- [x] Implementation verification (`verify_implementation_status.py`)")
    lines.append("")

    # Next Steps
    lines.append("## Next Steps\n")
    lines.append(f"1. **Current Focus**: Complete critical requirements (currently at {priorities.get('critical', {}).get('coverage', 0)}% coverage)")
    lines.append("2. **Target**: Achieve 80% overall traceability coverage")
    lines.append("3. **Timeline**: Review progress weekly using this report")

    report = "\n".join(lines)

    if output_path:
        output_path.write_text(report)

    return report


def main():
    parser = argparse.ArgumentParser(
        description="Generate comprehensive implementation status report"
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path.cwd(),
        help="Project root directory"
    )
    parser.add_argument(
        "--gap-analysis",
        type=Path,
        default=None,
        help="Gap analysis markdown file"
    )
    parser.add_argument(
        "--code-refs",
        type=Path,
        default=None,
        help="Code references JSON file"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output report file"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Verbose output"
    )

    args = parser.parse_args()

    # Set defaults
    if args.gap_analysis is None:
        args.gap_analysis = args.project_root / "build" / "docs" / "traceability" / "gap_analysis.md"

    if args.code_refs is None:
        args.code_refs = args.project_root / "build" / "code_references.json"

    if args.output is None:
        args.output = args.project_root / "build" / "docs" / "implementation_report.md"

    print("Implementation Status Report Generator")
    print("=" * 40)

    # Load data
    print(f"\nLoading gap analysis from: {args.gap_analysis}")
    metrics = load_gap_analysis(args.gap_analysis)

    print(f"Loading code references from: {args.code_refs}")
    implemented, test_cases = load_code_references(args.code_refs)

    # Generate report
    print("\nGenerating report...")
    report = generate_report(metrics, implemented, test_cases, args.output)

    print(f"\nReport written to: {args.output}")

    # Print summary
    print("\n" + "=" * 40)
    print("Quick Summary:")
    print(f"  Total Requirements: {metrics.get('total_requirements', 'N/A')}")
    print(f"  Fully Traced: {metrics.get('fully_traced', 'N/A')}")
    print(f"  Code References: {len(implemented)}")
    print(f"  Test Cases: {len(test_cases)}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
