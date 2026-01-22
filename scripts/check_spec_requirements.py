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
Verify spec requirement mappings from open-someip-spec.

This script:
- Parses open-someip-spec RST files to extract all feat_req_someip_* requirements
- Cross-checks which OpenSOMEIP requirements (REQ_*) map to spec requirements
- Identifies requirements that should have spec links but don't
- Generates mapping report: spec requirement → OpenSOMEIP requirement → implementation status

Exit codes:
- 0: All mappings valid
- 1: Missing or invalid mappings found
"""

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Set, Tuple


def extract_spec_requirements(spec_dir: Path) -> Dict[str, dict]:
    """Extract feat_req_* requirements from open-someip-spec RST files."""
    spec_reqs = {}
    
    if not spec_dir.exists():
        print(f"Warning: Spec directory not found: {spec_dir}")
        return spec_reqs
    
    # Pattern to match feat_req directives in open-someip-spec
    patterns = [
        # Standard feat_req directive
        re.compile(
            r'\.\.\s+feat_req::\s*[^\n]*\n\s+:id:\s*(feat_req_\w+)',
            re.IGNORECASE
        ),
        # Heading with id
        re.compile(
            r'\.\.\s+heading::\s*([^\n]+)\n\s+:id:\s*(feat_req_\w+)',
            re.IGNORECASE
        ),
    ]
    
    for rst_file in spec_dir.rglob("*.rst"):
        try:
            content = rst_file.read_text(encoding='utf-8', errors='ignore')
        except Exception:
            continue
        
        # Extract with first pattern
        for match in patterns[0].finditer(content):
            req_id = match.group(1).lower()
            spec_reqs[req_id] = {
                "id": req_id,
                "file": str(rst_file.relative_to(spec_dir)),
                "type": "requirement"
            }
        
        # Extract with heading pattern
        for match in patterns[1].finditer(content):
            title = match.group(1).strip()
            req_id = match.group(2).lower()
            spec_reqs[req_id] = {
                "id": req_id,
                "title": title,
                "file": str(rst_file.relative_to(spec_dir)),
                "type": "heading"
            }
    
    return spec_reqs


def extract_impl_requirements(req_dir: Path) -> Tuple[Dict[str, dict], Dict[str, List[str]]]:
    """
    Extract OpenSOMEIP requirements and their :satisfies: mappings.
    
    Returns:
        - Dict of requirement IDs to requirement info
        - Dict mapping REQ_* to list of feat_req_* they satisfy
    """
    requirements = {}
    satisfies_map = {}
    
    if not req_dir.exists():
        return requirements, satisfies_map
    
    pattern = re.compile(
        r'\.\.\s+requirement::\s*(.+?)\n((?:\s+:[a-z_]+:.*?\n)+)',
        re.DOTALL | re.IGNORECASE
    )
    
    for rst_file in req_dir.rglob("*.rst"):
        try:
            content = rst_file.read_text(encoding='utf-8', errors='ignore')
        except Exception:
            continue
        
        for match in pattern.finditer(content):
            title = match.group(1).strip()
            attrs_block = match.group(2)
            
            # Extract ID
            id_match = re.search(r':id:\s*(REQ_[A-Za-z0-9_]+)', attrs_block, re.IGNORECASE)
            if not id_match:
                continue
            
            req_id = id_match.group(1).upper()
            
            # Extract satisfies
            satisfies_match = re.search(r':satisfies:\s*([^\n]+)', attrs_block, re.IGNORECASE)
            satisfies = []
            if satisfies_match:
                satisfies_str = satisfies_match.group(1)
                satisfies = [s.strip().lower() for s in satisfies_str.split(",") if s.strip()]
            
            requirements[req_id] = {
                "id": req_id,
                "title": title,
                "satisfies": satisfies,
                "file": str(rst_file)
            }
            
            if satisfies:
                satisfies_map[req_id] = satisfies
    
    return requirements, satisfies_map


def classify_requirement(req_id: str) -> str:
    """Classify requirement as spec-derived or implementation-derived."""
    if "_E0" in req_id or "_E1" in req_id:
        return "error_handling"
    if req_id.startswith("REQ_ARCH_"):
        return "architectural"
    if req_id.startswith("REQ_E2E_PLUGIN_"):
        return "plugin"
    return "spec_derived"


def analyze_mappings(
    spec_reqs: Dict[str, dict],
    impl_reqs: Dict[str, dict],
    satisfies_map: Dict[str, List[str]]
) -> Dict[str, any]:
    """Analyze spec to implementation mappings."""
    
    results = {
        "spec_req_count": len(spec_reqs),
        "impl_req_count": len(impl_reqs),
        "spec_derived_count": 0,
        "impl_derived_count": 0,
        "mapped_spec_reqs": set(),
        "unmapped_spec_reqs": set(),
        "impl_missing_spec_link": [],
        "invalid_spec_links": [],
        "spec_to_impl": defaultdict(list),
        "impl_to_spec": {},
    }
    
    # Build reverse mapping and check validity
    for impl_id, spec_ids in satisfies_map.items():
        results["impl_to_spec"][impl_id] = spec_ids
        for spec_id in spec_ids:
            results["spec_to_impl"][spec_id].append(impl_id)
            results["mapped_spec_reqs"].add(spec_id)
            
            # Check if spec_id is valid
            if spec_id not in spec_reqs:
                results["invalid_spec_links"].append({
                    "impl_id": impl_id,
                    "invalid_spec_id": spec_id
                })
    
    # Find unmapped spec requirements
    results["unmapped_spec_reqs"] = set(spec_reqs.keys()) - results["mapped_spec_reqs"]
    
    # Find implementation requirements missing spec links
    for impl_id, impl_info in impl_reqs.items():
        category = classify_requirement(impl_id)
        
        if category == "spec_derived":
            results["spec_derived_count"] += 1
            if impl_id not in satisfies_map or not satisfies_map[impl_id]:
                results["impl_missing_spec_link"].append(impl_id)
        else:
            results["impl_derived_count"] += 1
    
    return results


def generate_report(
    spec_reqs: Dict[str, dict],
    impl_reqs: Dict[str, dict],
    analysis: Dict[str, any],
    output_path: Path = None
) -> str:
    """Generate spec mapping report."""
    
    lines = []
    lines.append("# Spec Requirements Mapping Report\n")
    
    # Summary
    lines.append("## Summary\n")
    lines.append(f"- **Spec Requirements (open-someip-spec)**: {analysis['spec_req_count']}")
    lines.append(f"- **Implementation Requirements (OpenSOMEIP)**: {analysis['impl_req_count']}")
    lines.append(f"  - Spec-derived: {analysis['spec_derived_count']}")
    lines.append(f"  - Implementation-derived: {analysis['impl_derived_count']}")
    lines.append(f"- **Mapped Spec Requirements**: {len(analysis['mapped_spec_reqs'])}")
    lines.append(f"- **Unmapped Spec Requirements**: {len(analysis['unmapped_spec_reqs'])}")
    lines.append(f"- **Implementation Reqs Missing Spec Links**: {len(analysis['impl_missing_spec_link'])}")
    lines.append(f"- **Invalid Spec Links**: {len(analysis['invalid_spec_links'])}")
    lines.append("")
    
    # Coverage
    if analysis['spec_req_count'] > 0:
        coverage = len(analysis['mapped_spec_reqs']) / analysis['spec_req_count'] * 100
        lines.append(f"**Spec Coverage**: {coverage:.1f}%\n")
    
    # Invalid spec links
    if analysis['invalid_spec_links']:
        lines.append("## Invalid Spec Links\n")
        lines.append("These implementation requirements reference non-existent spec requirements:\n")
        for item in analysis['invalid_spec_links']:
            lines.append(f"- **{item['impl_id']}** → `{item['invalid_spec_id']}` (not found in spec)")
        lines.append("")
    
    # Implementation requirements missing spec links
    if analysis['impl_missing_spec_link']:
        lines.append("## Implementation Requirements Missing Spec Links\n")
        lines.append("These spec-derived requirements should have `:satisfies:` links:\n")
        
        # Group by prefix
        by_prefix = defaultdict(list)
        for impl_id in analysis['impl_missing_spec_link']:
            prefix = impl_id.split("_")[1] if "_" in impl_id else "OTHER"
            by_prefix[prefix].append(impl_id)
        
        for prefix, reqs in sorted(by_prefix.items()):
            lines.append(f"\n### {prefix} ({len(reqs)})")
            for req_id in sorted(reqs):
                info = impl_reqs.get(req_id, {})
                lines.append(f"- **{req_id}**: {info.get('title', 'Unknown')}")
        lines.append("")
    
    # Spec requirements coverage by category
    lines.append("## Spec Requirements Coverage by Category\n")
    
    # Group spec reqs by file
    by_file = defaultdict(list)
    for spec_id, info in spec_reqs.items():
        by_file[info.get('file', 'unknown')].append(spec_id)
    
    for file_path, spec_ids in sorted(by_file.items()):
        mapped = [s for s in spec_ids if s in analysis['mapped_spec_reqs']]
        unmapped = [s for s in spec_ids if s not in analysis['mapped_spec_reqs']]
        coverage = len(mapped) / len(spec_ids) * 100 if spec_ids else 0
        
        lines.append(f"### {file_path}")
        lines.append(f"- Total: {len(spec_ids)}, Mapped: {len(mapped)}, Coverage: {coverage:.0f}%")
        
        if unmapped and len(unmapped) <= 10:
            lines.append("- Unmapped: " + ", ".join(unmapped[:10]))
        elif unmapped:
            lines.append(f"- Unmapped: {len(unmapped)} requirements (see details below)")
        lines.append("")
    
    # Detailed mapping table
    lines.append("## Detailed Mapping (Sample)\n")
    lines.append("| Spec Requirement | Implementation Requirements |")
    lines.append("|------------------|----------------------------|")
    
    count = 0
    for spec_id in sorted(analysis['spec_to_impl'].keys()):
        if count >= 20:
            break
        impl_ids = analysis['spec_to_impl'][spec_id]
        lines.append(f"| {spec_id} | {', '.join(impl_ids[:3])}{'...' if len(impl_ids) > 3 else ''} |")
        count += 1
    
    if len(analysis['spec_to_impl']) > 20:
        lines.append(f"| ... | ({len(analysis['spec_to_impl']) - 20} more mappings) |")
    
    lines.append("")
    
    report = "\n".join(lines)
    
    if output_path:
        output_path.write_text(report)
    
    return report


def main():
    parser = argparse.ArgumentParser(
        description="Verify spec requirement mappings"
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path.cwd(),
        help="Project root directory"
    )
    parser.add_argument(
        "--spec-dir",
        type=Path,
        default=None,
        help="open-someip-spec directory"
    )
    parser.add_argument(
        "--requirements-dir",
        type=Path,
        default=None,
        help="OpenSOMEIP requirements directory"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output report file"
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Fail on missing mappings"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Verbose output"
    )
    
    args = parser.parse_args()
    
    # Set defaults
    if args.spec_dir is None:
        args.spec_dir = args.project_root / "open-someip-spec" / "src"
    
    if args.requirements_dir is None:
        args.requirements_dir = args.project_root / "docs" / "requirements"
    
    print("Spec Requirements Mapping Verification")
    print("=" * 40)
    
    # Load spec requirements
    print(f"\nLoading spec requirements from: {args.spec_dir}")
    spec_reqs = extract_spec_requirements(args.spec_dir)
    print(f"  Found {len(spec_reqs)} spec requirements")
    
    # Load implementation requirements
    print(f"\nLoading implementation requirements from: {args.requirements_dir}")
    impl_reqs, satisfies_map = extract_impl_requirements(args.requirements_dir)
    print(f"  Found {len(impl_reqs)} implementation requirements")
    print(f"  Found {len(satisfies_map)} requirements with :satisfies: links")
    
    # Analyze mappings
    print("\nAnalyzing mappings...")
    analysis = analyze_mappings(spec_reqs, impl_reqs, satisfies_map)
    
    # Generate report
    report = generate_report(spec_reqs, impl_reqs, analysis, args.output)
    
    # Print summary
    print("\n" + "=" * 40)
    print("Summary:")
    print(f"  Spec requirements: {analysis['spec_req_count']}")
    print(f"  Implementation requirements: {analysis['impl_req_count']}")
    print(f"  Mapped spec requirements: {len(analysis['mapped_spec_reqs'])}")
    print(f"  Unmapped spec requirements: {len(analysis['unmapped_spec_reqs'])}")
    print(f"  Missing spec links: {len(analysis['impl_missing_spec_link'])}")
    print(f"  Invalid spec links: {len(analysis['invalid_spec_links'])}")
    
    if analysis['spec_req_count'] > 0:
        coverage = len(analysis['mapped_spec_reqs']) / analysis['spec_req_count'] * 100
        print(f"  Spec coverage: {coverage:.1f}%")
    
    if args.output:
        print(f"\nReport written to: {args.output}")
    
    # Determine exit code
    exit_code = 0
    if args.strict:
        if analysis['invalid_spec_links']:
            print(f"\n❌ Found {len(analysis['invalid_spec_links'])} invalid spec links")
            exit_code = 1
        if analysis['impl_missing_spec_link']:
            print(f"\n❌ Found {len(analysis['impl_missing_spec_link'])} implementation requirements missing spec links")
            exit_code = 1
    
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
