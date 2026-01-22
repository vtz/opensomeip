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
Verify implementation status by code pattern matching.

This script goes beyond annotation-based traceability to check if functionality
actually exists in the codebase, even without @implements annotations.

Key checks:
- Search for function names matching requirement descriptions
- Check for constants/values matching requirement specs (e.g., 1392 bytes for REQ_TP_002)
- Verify class methods exist for requirement functionality
- Match requirement descriptions to code comments/function names

Output:
- Requirements with code but missing annotations (needs annotation fixes)
- Requirements truly missing implementation (needs code)
- Requirements with partial implementation (needs completion)

Exit codes:
- 0: All checks passed or only warnings
- 1: Errors found (if --strict mode)
"""

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


@dataclass
class RequirementPattern:
    """Pattern to match requirement implementation in code."""
    id: str
    title: str
    description: str
    patterns: List[str] = field(default_factory=list)  # Regex patterns to search
    values: List[str] = field(default_factory=list)    # Specific values to search
    files: List[str] = field(default_factory=list)     # Expected implementation files


@dataclass
class ImplementationMatch:
    """Match found in code for a requirement."""
    file_path: str
    line_number: int
    match_type: str  # 'annotation', 'pattern', 'value', 'function'
    matched_text: str
    confidence: str  # 'high', 'medium', 'low'


# Requirement patterns for code matching
# These map requirement IDs to expected code patterns
REQUIREMENT_PATTERNS: Dict[str, RequirementPattern] = {
    # Transport Protocol Requirements
    "REQ_TP_002": RequirementPattern(
        id="REQ_TP_002",
        title="Maximum Segment Payload Size",
        description="Maximum segment payload is 1392 bytes",
        patterns=[r"max_segment.*1392", r"1392.*bytes", r"MAX_SEGMENT.*1392"],
        values=["1392", "87 * 16"],
        files=["tp_segmenter.cpp", "tp_types.h"]
    ),
    "REQ_TP_003": RequirementPattern(
        id="REQ_TP_003",
        title="Segment Alignment",
        description="Segments are multiples of 16 bytes",
        patterns=[r"align.*16", r"% 16", r"multiple.*16"],
        values=["16"],
        files=["tp_segmenter.cpp"]
    ),
    "REQ_TP_010": RequirementPattern(
        id="REQ_TP_010",
        title="TP Header Position",
        description="TP header at byte 16",
        patterns=[r"tp.*header.*16", r"header.*position.*16", r"byte.*16"],
        values=["16"],
        files=["tp_segmenter.cpp", "tp_reassembler.cpp"]
    ),
    # Message Header Requirements
    "REQ_MSG_001": RequirementPattern(
        id="REQ_MSG_001",
        title="Message Structure",
        description="SOME/IP message structure",
        patterns=[r"struct\s+Message", r"class\s+Message", r"Message\s*\{"],
        files=["message.h", "message.cpp"]
    ),
    "REQ_MSG_011": RequirementPattern(
        id="REQ_MSG_011",
        title="Message ID",
        description="Message ID (Service ID + Method ID)",
        patterns=[r"message_id", r"service_id.*method_id", r"get_message_id"],
        files=["message.h", "message.cpp"]
    ),
    # Serialization Requirements
    "REQ_SER_001": RequirementPattern(
        id="REQ_SER_001",
        title="Bool Serialization",
        description="Serialize bool as uint8",
        patterns=[r"serialize.*bool", r"bool.*uint8", r"write_bool"],
        files=["serializer.cpp"]
    ),
    "REQ_SER_005": RequirementPattern(
        id="REQ_SER_005",
        title="Big Endian Conversion",
        description="Multi-byte values in big endian",
        patterns=[r"big.*endian", r"hton", r"ntoh", r"byte.*swap"],
        files=["serializer.cpp"]
    ),
    # Service Discovery Requirements
    "REQ_SD_001": RequirementPattern(
        id="REQ_SD_001",
        title="SD Message Format",
        description="SD message format",
        patterns=[r"sd.*message", r"ServiceDiscovery", r"SDMessage"],
        files=["sd_message.cpp", "sd_message.h"]
    ),
}


def extract_requirements_from_rst(rst_dir: Path) -> Dict[str, dict]:
    """Extract requirements from RST files."""
    requirements = {}
    
    for rst_file in rst_dir.rglob("*.rst"):
        content = rst_file.read_text(encoding='utf-8', errors='ignore')
        
        # Pattern to match requirement directives
        pattern = re.compile(
            r'\.\.\s+requirement::\s*(.+?)\n((?:\s+:[a-z_]+:.*?\n)+)',
            re.DOTALL | re.IGNORECASE
        )
        
        for match in pattern.finditer(content):
            title = match.group(1).strip()
            attrs_block = match.group(2)
            
            # Extract ID
            id_match = re.search(r':id:\s*(REQ_[A-Za-z0-9_]+)', attrs_block, re.IGNORECASE)
            if not id_match:
                continue
            
            req_id = id_match.group(1).upper()
            
            # Extract description (content after the attribute block)
            desc_start = match.end()
            desc_end = content.find("\n.. ", desc_start)
            if desc_end == -1:
                desc_end = len(content)
            description = content[desc_start:desc_end].strip()[:500]
            
            # Extract code location hint
            code_loc_match = re.search(r'\*\*Code Location\*\*:\s*``([^`]+)``', description)
            code_location = code_loc_match.group(1) if code_loc_match else ""
            
            requirements[req_id] = {
                "id": req_id,
                "title": title,
                "description": description,
                "code_location": code_location,
                "file": str(rst_file)
            }
    
    return requirements


def load_code_references(json_path: Path) -> Set[str]:
    """Load implemented requirements from code references JSON."""
    implemented = set()
    
    if not json_path.exists():
        return implemented
    
    with open(json_path, 'r') as f:
        data = json.load(f)
    
    needs = data.get("versions", {}).get("current", {}).get("needs", {})
    
    for need_id, need in needs.items():
        if need.get("type") == "code_ref":
            implements = need.get("implements", "")
            for req in implements.split(","):
                req = req.strip().upper()
                if req.startswith("REQ_"):
                    implemented.add(req)
    
    return implemented


def search_file_for_patterns(
    file_path: Path,
    patterns: List[str],
    values: List[str]
) -> List[ImplementationMatch]:
    """Search a file for requirement implementation patterns."""
    matches = []
    
    try:
        content = file_path.read_text(encoding='utf-8', errors='ignore')
        lines = content.split('\n')
    except Exception:
        return matches
    
    # Search for regex patterns
    for pattern in patterns:
        try:
            regex = re.compile(pattern, re.IGNORECASE)
            for i, line in enumerate(lines, 1):
                if regex.search(line):
                    matches.append(ImplementationMatch(
                        file_path=str(file_path),
                        line_number=i,
                        match_type="pattern",
                        matched_text=line.strip()[:100],
                        confidence="medium"
                    ))
        except re.error:
            continue
    
    # Search for specific values
    for value in values:
        for i, line in enumerate(lines, 1):
            if value in line:
                matches.append(ImplementationMatch(
                    file_path=str(file_path),
                    line_number=i,
                    match_type="value",
                    matched_text=line.strip()[:100],
                    confidence="low"
                ))
    
    return matches


def search_for_implementation(
    req_id: str,
    requirement: dict,
    src_dirs: List[Path]
) -> List[ImplementationMatch]:
    """Search for implementation of a requirement."""
    matches = []
    
    # Get pattern if available
    pattern = REQUIREMENT_PATTERNS.get(req_id)
    
    # Build search patterns from requirement info
    search_patterns = []
    search_values = []
    target_files = []
    
    if pattern:
        search_patterns = pattern.patterns
        search_values = pattern.values
        target_files = pattern.files
    
    # Add patterns from requirement title/description
    title = requirement.get("title", "")
    if title:
        # Convert title to potential function/variable names
        snake_case = re.sub(r'[^a-zA-Z0-9]', '_', title.lower())
        camel_case = title.replace(' ', '')
        search_patterns.extend([
            re.escape(snake_case),
            re.escape(camel_case),
        ])
    
    # Add code location hint
    code_location = requirement.get("code_location", "")
    if code_location:
        target_files.append(Path(code_location).name)
    
    # Search in source directories
    for src_dir in src_dirs:
        if not src_dir.exists():
            continue
        
        # If we have target files, search only those
        if target_files:
            for target in target_files:
                for file_path in src_dir.rglob(f"*{target}*"):
                    if file_path.suffix in ['.cpp', '.h', '.hpp', '.c']:
                        file_matches = search_file_for_patterns(
                            file_path, search_patterns, search_values
                        )
                        matches.extend(file_matches)
        else:
            # Search all source files
            for file_path in src_dir.rglob("*.cpp"):
                file_matches = search_file_for_patterns(
                    file_path, search_patterns, search_values
                )
                matches.extend(file_matches)
    
    # Remove duplicates
    seen = set()
    unique_matches = []
    for m in matches:
        key = (m.file_path, m.line_number)
        if key not in seen:
            seen.add(key)
            unique_matches.append(m)
    
    return unique_matches


def analyze_requirements(
    requirements: Dict[str, dict],
    annotated: Set[str],
    src_dirs: List[Path],
    verbose: bool = False
) -> Tuple[Dict[str, List[ImplementationMatch]], List[str], List[str]]:
    """
    Analyze all requirements for implementation status.
    
    Returns:
        - Dict mapping req_id to found matches
        - List of requirements with code but missing annotations
        - List of requirements truly missing implementation
    """
    matches_by_req = {}
    missing_annotations = []
    truly_missing = []
    
    for req_id, req in requirements.items():
        if req_id in annotated:
            # Already annotated, skip pattern matching
            if verbose:
                print(f"  {req_id}: Already annotated ✓")
            continue
        
        # Search for implementation
        matches = search_for_implementation(req_id, req, src_dirs)
        
        if matches:
            # Found potential implementation
            high_confidence = any(m.confidence == "high" for m in matches)
            medium_confidence = any(m.confidence == "medium" for m in matches)
            
            if high_confidence or medium_confidence:
                missing_annotations.append(req_id)
                matches_by_req[req_id] = matches
                if verbose:
                    print(f"  {req_id}: Found code, missing annotation ⚠️")
                    for m in matches[:3]:
                        print(f"    → {m.file_path}:{m.line_number}: {m.matched_text[:50]}...")
            else:
                # Low confidence matches, likely false positives
                truly_missing.append(req_id)
                if verbose:
                    print(f"  {req_id}: Low confidence matches, likely missing ❌")
        else:
            truly_missing.append(req_id)
            if verbose:
                print(f"  {req_id}: No implementation found ❌")
    
    return matches_by_req, missing_annotations, truly_missing


def generate_report(
    requirements: Dict[str, dict],
    annotated: Set[str],
    matches_by_req: Dict[str, List[ImplementationMatch]],
    missing_annotations: List[str],
    truly_missing: List[str],
    output_path: Optional[Path]
) -> str:
    """Generate verification report."""
    
    lines = []
    lines.append("# Implementation Verification Report\n")
    lines.append("## Summary\n")
    lines.append(f"- **Total Requirements**: {len(requirements)}")
    lines.append(f"- **Annotated (in code)**: {len(annotated)}")
    lines.append(f"- **Missing Annotations (code exists)**: {len(missing_annotations)}")
    lines.append(f"- **Truly Missing (no code found)**: {len(truly_missing)}")
    lines.append("")
    
    # Calculate effective implementation rate
    implemented = len(annotated) + len(missing_annotations)
    rate = implemented / len(requirements) * 100 if requirements else 0
    lines.append(f"- **Effective Implementation Rate**: {implemented}/{len(requirements)} ({rate:.1f}%)")
    lines.append("")
    
    # Missing annotations - need annotation fixes
    if missing_annotations:
        lines.append("## Requirements with Code but Missing Annotations\n")
        lines.append("These requirements appear to be implemented but lack `@implements` annotations.\n")
        for req_id in sorted(missing_annotations):
            req = requirements.get(req_id, {})
            lines.append(f"### {req_id}: {req.get('title', 'Unknown')}\n")
            
            matches = matches_by_req.get(req_id, [])
            if matches:
                lines.append("**Potential implementation locations:**\n")
                for m in matches[:5]:
                    lines.append(f"- `{m.file_path}:{m.line_number}` ({m.match_type})")
                    lines.append(f"  - `{m.matched_text[:80]}...`")
            lines.append("")
    
    # Truly missing - need implementation
    if truly_missing:
        lines.append("## Requirements Truly Missing Implementation\n")
        lines.append("These requirements have no detected implementation.\n")
        
        # Group by category
        by_category = {}
        for req_id in truly_missing:
            if req_id.startswith("REQ_TP_"):
                cat = "Transport Protocol"
            elif req_id.startswith("REQ_SER_"):
                cat = "Serialization"
            elif req_id.startswith("REQ_MSG_"):
                cat = "Message Header"
            elif req_id.startswith("REQ_SD_"):
                cat = "Service Discovery"
            elif req_id.startswith("REQ_ARCH_"):
                cat = "Architecture"
            elif "_E0" in req_id or "_E1" in req_id:
                cat = "Error Handling"
            else:
                cat = "Other"
            
            if cat not in by_category:
                by_category[cat] = []
            by_category[cat].append(req_id)
        
        for cat, reqs in sorted(by_category.items()):
            lines.append(f"### {cat} ({len(reqs)} missing)\n")
            for req_id in sorted(reqs):
                req = requirements.get(req_id, {})
                code_loc = req.get("code_location", "")
                if code_loc:
                    lines.append(f"- **{req_id}**: {req.get('title', 'Unknown')} → `{code_loc}`")
                else:
                    lines.append(f"- **{req_id}**: {req.get('title', 'Unknown')}")
            lines.append("")
    
    # Recommendations
    lines.append("## Recommendations\n")
    lines.append("### Immediate Actions (Quick Wins)\n")
    if missing_annotations:
        lines.append(f"1. Add `@implements` annotations to {len(missing_annotations)} requirements")
        lines.append("   - These appear to be implemented but need traceability annotations")
    
    lines.append("\n### Implementation Required\n")
    if truly_missing:
        lines.append(f"2. Implement {len(truly_missing)} missing requirements")
        for cat, reqs in sorted(by_category.items()):
            lines.append(f"   - {cat}: {len(reqs)} requirements")
    
    report = "\n".join(lines)
    
    if output_path:
        output_path.write_text(report)
    
    return report


def main():
    parser = argparse.ArgumentParser(
        description="Verify implementation status by code pattern matching"
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path.cwd(),
        help="Project root directory"
    )
    parser.add_argument(
        "--requirements-dir",
        type=Path,
        default=None,
        help="Requirements RST directory"
    )
    parser.add_argument(
        "--code-refs",
        type=Path,
        default=None,
        help="Code references JSON file"
    )
    parser.add_argument(
        "--src-dirs",
        nargs="+",
        default=["src", "include"],
        help="Source directories to search"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output report file"
    )
    parser.add_argument(
        "--min-coverage",
        type=float,
        default=0,
        help="Minimum coverage percentage required"
    )
    parser.add_argument(
        "--fail-on-critical-gaps",
        action="store_true",
        help="Fail if critical requirements are missing"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Verbose output"
    )
    
    args = parser.parse_args()
    
    # Set default paths
    if args.requirements_dir is None:
        args.requirements_dir = args.project_root / "docs" / "requirements"
    
    if args.code_refs is None:
        args.code_refs = args.project_root / "build" / "code_references.json"
    
    # Convert src dirs to absolute paths
    src_dirs = [args.project_root / d for d in args.src_dirs]
    
    print("Implementation Verification")
    print("=" * 40)
    
    # Extract requirements
    print(f"Loading requirements from: {args.requirements_dir}")
    requirements = extract_requirements_from_rst(args.requirements_dir)
    print(f"  Found {len(requirements)} requirements")
    
    # Load annotated requirements
    print(f"Loading code references from: {args.code_refs}")
    annotated = load_code_references(args.code_refs)
    print(f"  Found {len(annotated)} annotated requirements")
    
    # Analyze requirements
    print("\nAnalyzing requirements...")
    matches, missing_annotations, truly_missing = analyze_requirements(
        requirements, annotated, src_dirs, args.verbose
    )
    
    # Generate report
    report = generate_report(
        requirements, annotated, matches,
        missing_annotations, truly_missing,
        args.output
    )
    
    # Print summary
    print("\n" + "=" * 40)
    print("Summary:")
    print(f"  Total requirements: {len(requirements)}")
    print(f"  Annotated: {len(annotated)}")
    print(f"  Missing annotations: {len(missing_annotations)}")
    print(f"  Truly missing: {len(truly_missing)}")
    
    # Calculate coverage
    implemented = len(annotated) + len(missing_annotations)
    coverage = implemented / len(requirements) * 100 if requirements else 0
    print(f"  Effective coverage: {coverage:.1f}%")
    
    if args.output:
        print(f"\nReport written to: {args.output}")
    
    # Check thresholds
    exit_code = 0
    
    if args.min_coverage > 0 and coverage < args.min_coverage:
        print(f"\n❌ Coverage {coverage:.1f}% is below minimum {args.min_coverage}%")
        exit_code = 1
    
    if args.fail_on_critical_gaps:
        # Check for critical missing requirements (non-error handling, non-derived)
        critical_missing = [
            r for r in truly_missing
            if not ("_E0" in r or "_E1" in r or r.startswith("REQ_ARCH_"))
        ]
        if critical_missing:
            print(f"\n❌ {len(critical_missing)} critical requirements are missing")
            exit_code = 1
    
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
