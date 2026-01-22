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
Generate requirement diff report for pull requests.

Compares current requirements against a baseline and reports:
- New requirements added
- Requirements removed
- Requirements modified
- Changes in traceability
"""

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Dict, Set


def extract_requirements_from_rst(rst_dir: Path) -> Dict[str, dict]:
    """Extract requirements from RST files."""
    requirements = {}
    
    if not rst_dir.exists():
        return requirements
    
    for rst_file in rst_dir.rglob("*.rst"):
        content = rst_file.read_text(encoding='utf-8', errors='ignore')
        
        # Pattern to match requirement directives
        pattern = re.compile(
            r'\.\.\s+requirement::\s*(.+?)\n'
            r'(.*?)'
            r'(?=\.\.\s+\w+::|$)',
            re.DOTALL
        )
        
        for match in pattern.finditer(content):
            title = match.group(1).strip()
            body = match.group(2)
            
            # Extract ID
            id_match = re.search(r':id:\s*(REQ_[A-Za-z0-9_]+)', body, re.IGNORECASE)
            if not id_match:
                continue
            
            req_id = id_match.group(1).upper()
            
            # Extract satisfies
            satisfies_match = re.search(r':satisfies:\s*([^\n]+)', body)
            satisfies = satisfies_match.group(1).strip() if satisfies_match else ""
            
            # Extract status
            status_match = re.search(r':status:\s*([^\n]+)', body)
            status = status_match.group(1).strip() if status_match else ""
            
            requirements[req_id] = {
                "id": req_id,
                "title": title,
                "satisfies": satisfies,
                "status": status,
                "file": str(rst_file.relative_to(rst_dir))
            }
    
    return requirements


def generate_diff_report(
    current: Dict[str, dict],
    baseline: Dict[str, dict]
) -> str:
    """Generate markdown diff report."""
    
    current_ids = set(current.keys())
    baseline_ids = set(baseline.keys())
    
    added = current_ids - baseline_ids
    removed = baseline_ids - current_ids
    common = current_ids & baseline_ids
    
    modified = set()
    for req_id in common:
        if current[req_id] != baseline[req_id]:
            modified.add(req_id)
    
    unchanged = common - modified
    
    lines = []
    
    # Summary
    lines.append("### Requirements Change Summary\n")
    lines.append(f"- **Added**: {len(added)}")
    lines.append(f"- **Removed**: {len(removed)}")
    lines.append(f"- **Modified**: {len(modified)}")
    lines.append(f"- **Unchanged**: {len(unchanged)}")
    lines.append(f"- **Total**: {len(current_ids)}")
    lines.append("")
    
    # Added requirements
    if added:
        lines.append("### Added Requirements\n")
        for req_id in sorted(added):
            req = current[req_id]
            lines.append(f"- **{req_id}**: {req['title']}")
            if req.get('satisfies'):
                lines.append(f"  - Satisfies: {req['satisfies']}")
        lines.append("")
    
    # Removed requirements
    if removed:
        lines.append("### Removed Requirements\n")
        for req_id in sorted(removed):
            req = baseline[req_id]
            lines.append(f"- **{req_id}**: {req['title']}")
        lines.append("")
    
    # Modified requirements
    if modified:
        lines.append("### Modified Requirements\n")
        for req_id in sorted(modified):
            old = baseline[req_id]
            new = current[req_id]
            lines.append(f"- **{req_id}**:")
            
            if old.get('title') != new.get('title'):
                lines.append(f"  - Title: `{old.get('title')}` → `{new.get('title')}`")
            
            if old.get('satisfies') != new.get('satisfies'):
                lines.append(f"  - Satisfies: `{old.get('satisfies')}` → `{new.get('satisfies')}`")
            
            if old.get('status') != new.get('status'):
                lines.append(f"  - Status: `{old.get('status')}` → `{new.get('status')}`")
        lines.append("")
    
    # If no changes
    if not added and not removed and not modified:
        lines.append("*No requirement changes detected.*")
    
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Generate requirement diff report"
    )
    parser.add_argument(
        "--current",
        type=Path,
        default=None,
        help="Current requirements directory"
    )
    parser.add_argument(
        "--baseline",
        type=Path,
        default=None,
        help="Baseline requirements JSON or directory"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output file (default: stdout)"
    )
    
    args = parser.parse_args()
    
    # Default current path
    if args.current is None:
        args.current = Path.cwd() / "docs" / "requirements"
    
    # Load current requirements
    current = extract_requirements_from_rst(args.current)
    
    # Load baseline (empty if not provided)
    baseline = {}
    if args.baseline and args.baseline.exists():
        if args.baseline.suffix == '.json':
            with open(args.baseline, 'r') as f:
                baseline = json.load(f)
        else:
            baseline = extract_requirements_from_rst(args.baseline)
    
    # Generate report
    report = generate_diff_report(current, baseline)
    
    # Output
    if args.output:
        args.output.write_text(report)
    else:
        print(report)
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
