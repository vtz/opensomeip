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
- Parses open-someip-spec RST files to extract all feat_req_* requirements
  across all domain prefixes (someip, someipsd, someiptp, someipcompat, someipids)
- Cross-checks which OpenSOMEIP requirements (REQ_*) map to spec requirements
- Auto-corrects wrong-prefix references (e.g. feat_req_someip_100 → feat_req_someipsd_100)
- Identifies requirements that should have spec links but don't
- Generates mapping report: spec requirement → OpenSOMEIP requirement → status

Exit codes:
- 0: All mappings valid
- 1: Missing or invalid mappings found
"""

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path


def extract_spec_requirements(spec_dir: Path) -> dict[str, dict]:
    """Extract feat_req_* requirements from open-someip-spec RST files."""
    spec_reqs = {}

    if not spec_dir.exists():
        print(f"Warning: Spec directory not found: {spec_dir}")
        return spec_reqs

    patterns = [
        re.compile(r"\.\.\s+feat_req::\s*[^\n]*\n\s+:id:\s*(feat_req_\w+)", re.IGNORECASE),
        re.compile(r"\.\.\s+heading::\s*([^\n]+)\n\s+:id:\s*(feat_req_\w+)", re.IGNORECASE),
    ]

    for rst_file in spec_dir.rglob("*.rst"):
        try:
            content = rst_file.read_text(encoding="utf-8", errors="ignore")
        except Exception:
            continue

        rel_path = str(rst_file.relative_to(spec_dir))

        for match in patterns[0].finditer(content):
            req_id = match.group(1).lower()
            spec_reqs[req_id] = {"id": req_id, "file": rel_path, "type": "requirement"}

        for match in patterns[1].finditer(content):
            title = match.group(1).strip()
            req_id = match.group(2).lower()
            spec_reqs[req_id] = {"id": req_id, "title": title, "file": rel_path, "type": "heading"}

    return spec_reqs


def build_numeric_index(spec_reqs: dict[str, dict]) -> dict[str, list[str]]:
    """Build an index from numeric suffix to all spec IDs sharing that suffix.

    This enables fuzzy matching when a `:satisfies:` link uses the wrong
    domain prefix (e.g. feat_req_someip_100 instead of feat_req_someipsd_100).
    """
    num_pattern = re.compile(r"_(\d+)$")
    index: dict[str, list[str]] = defaultdict(list)
    for spec_id in spec_reqs:
        m = num_pattern.search(spec_id)
        if m:
            index[m.group(1)].append(spec_id)
    return index


def resolve_spec_id(
    raw_id: str,
    spec_reqs: dict[str, dict],
    numeric_index: dict[str, list[str]],
    impl_id: str,
) -> tuple[str | None, dict | None]:
    """Resolve a `:satisfies:` value to its actual spec requirement.

    Returns (resolved_id, correction_info) where correction_info is None
    if the ID matched directly, or a dict with details if fuzzy-matched.
    """
    if raw_id in spec_reqs:
        return raw_id, None

    num_match = re.search(r"_(\d+)$", raw_id)
    if not num_match:
        return None, None

    suffix = num_match.group(1)
    candidates = numeric_index.get(suffix, [])

    if len(candidates) == 1:
        correct_id = candidates[0]
        return correct_id, {
            "impl_id": impl_id,
            "written": raw_id,
            "resolved": correct_id,
            "confidence": "high",
            "file": spec_reqs[correct_id].get("file", "unknown"),
        }

    if len(candidates) > 1:
        raw_prefix = re.sub(r"_\d+$", "", raw_id)
        domain_hint = _domain_from_impl_prefix(impl_id)
        best = _pick_best_candidate(raw_prefix, domain_hint, candidates)
        if best:
            return best, {
                "impl_id": impl_id,
                "written": raw_id,
                "resolved": best,
                "confidence": "medium",
                "alternatives": candidates,
                "file": spec_reqs[best].get("file", "unknown"),
            }

    return None, None


def _domain_from_impl_prefix(impl_id: str) -> str | None:
    """Guess which spec domain an implementation requirement belongs to."""
    mapping = {
        "REQ_SD_": "someipsd",
        "REQ_TP_": "someiptp",
        "REQ_MSG_": "someip",
        "REQ_SER_": "someip",
        "REQ_TRANSPORT_": "someip",
        "REQ_PLATFORM_": None,
        "REQ_ARCH_": None,
        "REQ_E2E_": "someip",
        "REQ_MY_": "someip",
    }
    for prefix, domain in mapping.items():
        if impl_id.startswith(prefix):
            return domain
    return None


def _pick_best_candidate(
    raw_prefix: str,
    domain_hint: str | None,
    candidates: list[str],
) -> str | None:
    """When multiple spec IDs share a numeric suffix, pick the best match."""
    if domain_hint:
        domain_matches = [c for c in candidates if f"_{domain_hint}_" in c]
        if len(domain_matches) == 1:
            return domain_matches[0]

    prefix_matches = [c for c in candidates if c.startswith(raw_prefix)]
    if len(prefix_matches) == 1:
        return prefix_matches[0]

    return None


def extract_impl_requirements(req_dir: Path) -> tuple[dict[str, dict], dict[str, list[str]]]:
    """Extract OpenSOMEIP requirements and their :satisfies: mappings."""
    requirements = {}
    satisfies_map = {}

    if not req_dir.exists():
        return requirements, satisfies_map

    pattern = re.compile(
        r"\.\.\s+requirement::\s*(.+?)\n((?:\s+:[a-z_]+:.*?\n)+)", re.DOTALL | re.IGNORECASE
    )

    for rst_file in req_dir.rglob("*.rst"):
        try:
            content = rst_file.read_text(encoding="utf-8", errors="ignore")
        except Exception:
            continue

        for match in pattern.finditer(content):
            title = match.group(1).strip()
            attrs_block = match.group(2)

            id_match = re.search(r":id:\s*(REQ_[A-Za-z0-9_]+)", attrs_block, re.IGNORECASE)
            if not id_match:
                continue

            req_id = id_match.group(1).upper()

            satisfies_match = re.search(r":satisfies:\s*([^\n]+)", attrs_block, re.IGNORECASE)
            satisfies = []
            if satisfies_match:
                satisfies_str = satisfies_match.group(1)
                satisfies = [s.strip().lower() for s in satisfies_str.split(",") if s.strip()]

            requirements[req_id] = {
                "id": req_id,
                "title": title,
                "satisfies": satisfies,
                "file": str(rst_file),
            }

            if satisfies:
                satisfies_map[req_id] = satisfies

    return requirements, satisfies_map


_IMPL_DERIVED_PREFIXES = (
    "REQ_ARCH_",
    "REQ_PLATFORM_",
    "REQ_E2E_PLUGIN_",
)


def classify_requirement(req_id: str) -> str:
    """Classify requirement as spec-derived or implementation-derived."""
    if "_E0" in req_id or "_E1" in req_id:
        return "impl_derived"
    for prefix in _IMPL_DERIVED_PREFIXES:
        if req_id.startswith(prefix):
            return "impl_derived"
    return "spec_derived"


def analyze_mappings(
    spec_reqs: dict[str, dict],
    impl_reqs: dict[str, dict],
    satisfies_map: dict[str, list[str]],
) -> dict[str, any]:
    """Analyze spec to implementation mappings with fuzzy resolution."""
    numeric_index = build_numeric_index(spec_reqs)

    results = {
        "spec_req_count": len(spec_reqs),
        "impl_req_count": len(impl_reqs),
        "spec_derived_count": 0,
        "impl_derived_count": 0,
        "mapped_spec_reqs": set(),
        "unmapped_spec_reqs": set(),
        "impl_missing_spec_link": [],
        "invalid_spec_links": [],
        "auto_corrections": [],
        "unresolvable_links": [],
        "spec_to_impl": defaultdict(list),
        "impl_to_spec": {},
    }

    for impl_id, raw_spec_ids in satisfies_map.items():
        resolved_ids = []
        for raw_id in raw_spec_ids:
            # Skip internal REQ_* references (impl-to-impl links like platform -> arch)
            if raw_id.startswith("req_"):
                continue

            resolved_id, correction = resolve_spec_id(raw_id, spec_reqs, numeric_index, impl_id)

            if resolved_id:
                resolved_ids.append(resolved_id)
                results["spec_to_impl"][resolved_id].append(impl_id)
                results["mapped_spec_reqs"].add(resolved_id)
                if correction:
                    results["auto_corrections"].append(correction)
            else:
                results["unresolvable_links"].append(
                    {
                        "impl_id": impl_id,
                        "invalid_spec_id": raw_id,
                    }
                )

        if resolved_ids:
            results["impl_to_spec"][impl_id] = resolved_ids

    results["invalid_spec_links"] = results["unresolvable_links"]
    results["unmapped_spec_reqs"] = set(spec_reqs.keys()) - results["mapped_spec_reqs"]

    for impl_id in impl_reqs:
        category = classify_requirement(impl_id)
        if category == "spec_derived":
            results["spec_derived_count"] += 1
            if impl_id not in results["impl_to_spec"] or not results["impl_to_spec"][impl_id]:
                results["impl_missing_spec_link"].append(impl_id)
        else:
            results["impl_derived_count"] += 1

    return results


def generate_report(
    spec_reqs: dict[str, dict],
    impl_reqs: dict[str, dict],
    analysis: dict[str, any],
    output_path: Path | None = None,
) -> str:
    """Generate spec mapping report."""
    lines = []
    lines.append("# Spec Requirements Mapping Report\n")

    mapped_count = len(analysis["mapped_spec_reqs"])
    total_spec = analysis["spec_req_count"]
    coverage = mapped_count / total_spec * 100 if total_spec > 0 else 0

    # Summary
    lines.append("## Summary\n")
    lines.append(f"- **Spec Requirements (open-someip-spec)**: {total_spec}")
    lines.append(f"- **Implementation Requirements (OpenSOMEIP)**: {analysis['impl_req_count']}")
    lines.append(f"  - Spec-derived: {analysis['spec_derived_count']}")
    lines.append(f"  - Implementation-derived: {analysis['impl_derived_count']}")
    lines.append(f"- **Mapped Spec Requirements**: {mapped_count}")
    lines.append(f"- **Unmapped Spec Requirements**: {len(analysis['unmapped_spec_reqs'])}")
    lines.append(
        f"- **Implementation Reqs Missing Spec Links**: {len(analysis['impl_missing_spec_link'])}"
    )
    lines.append(f"- **Auto-Corrected Links**: {len(analysis['auto_corrections'])}")
    lines.append(f"- **Unresolvable Links**: {len(analysis['unresolvable_links'])}")
    lines.append("")
    lines.append(f"**Spec Coverage**: {coverage:.1f}%\n")

    # Auto-corrections
    if analysis["auto_corrections"]:
        lines.append("## Auto-Corrected Spec Links\n")
        lines.append(
            "These `:satisfies:` values had wrong prefixes but were resolved by numeric suffix:\n"
        )
        lines.append("| Implementation Req | Written | Resolved | Confidence | Spec File |")
        lines.append("|---|---|---|---|---|")
        for c in sorted(analysis["auto_corrections"], key=lambda x: x["impl_id"]):
            lines.append(
                f"| {c['impl_id']} | `{c['written']}` | `{c['resolved']}` "
                f"| {c['confidence']} | {c['file']} |"
            )
        lines.append("")

    # Unresolvable links
    if analysis["unresolvable_links"]:
        lines.append("## Unresolvable Spec Links\n")
        lines.append("These `:satisfies:` values could not be matched to any spec requirement:\n")
        for item in analysis["unresolvable_links"]:
            lines.append(f"- **{item['impl_id']}** → `{item['invalid_spec_id']}`")
        lines.append("")

    # Missing spec links
    if analysis["impl_missing_spec_link"]:
        lines.append("## Implementation Requirements Missing Spec Links\n")
        lines.append("These spec-derived requirements should have `:satisfies:` links:\n")

        by_prefix = defaultdict(list)
        for impl_id in analysis["impl_missing_spec_link"]:
            parts = impl_id.split("_")
            prefix = parts[1] if len(parts) > 1 else "OTHER"
            by_prefix[prefix].append(impl_id)

        for prefix, reqs in sorted(by_prefix.items()):
            lines.append(f"\n### {prefix} ({len(reqs)})")
            for req_id in sorted(reqs):
                info = impl_reqs.get(req_id, {})
                lines.append(f"- **{req_id}**: {info.get('title', 'Unknown')}")
        lines.append("")

    # Coverage by spec file
    lines.append("## Spec Requirements Coverage by Category\n")

    by_file = defaultdict(list)
    for spec_id, info in spec_reqs.items():
        by_file[info.get("file", "unknown")].append(spec_id)

    for file_path, spec_ids in sorted(by_file.items()):
        mapped = [s for s in spec_ids if s in analysis["mapped_spec_reqs"]]
        unmapped = [s for s in spec_ids if s not in analysis["mapped_spec_reqs"]]
        file_coverage = len(mapped) / len(spec_ids) * 100 if spec_ids else 0

        lines.append(f"### {file_path}")
        lines.append(
            f"- Total: {len(spec_ids)}, Mapped: {len(mapped)}, Coverage: {file_coverage:.0f}%"
        )

        if unmapped and len(unmapped) <= 10:
            lines.append("- Unmapped: " + ", ".join(sorted(unmapped)))
        elif unmapped:
            lines.append(f"- Unmapped: {len(unmapped)} requirements (see details below)")
        lines.append("")

    # Detailed mapping table (all mappings)
    lines.append("## Detailed Mapping\n")
    lines.append("| Spec Requirement | Implementation Requirements |")
    lines.append("|------------------|----------------------------|")

    for spec_id in sorted(analysis["spec_to_impl"].keys()):
        impl_ids = analysis["spec_to_impl"][spec_id]
        display = ", ".join(sorted(impl_ids)[:5])
        if len(impl_ids) > 5:
            display += f"... (+{len(impl_ids) - 5})"
        lines.append(f"| {spec_id} | {display} |")

    lines.append("")

    report = "\n".join(lines)

    if output_path:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(report)

    return report


def main():
    parser = argparse.ArgumentParser(description="Verify spec requirement mappings")
    parser.add_argument(
        "--project-root", type=Path, default=Path.cwd(), help="Project root directory"
    )
    parser.add_argument("--spec-dir", type=Path, default=None, help="open-someip-spec directory")
    parser.add_argument(
        "--requirements-dir", type=Path, default=None, help="OpenSOMEIP requirements directory"
    )
    parser.add_argument("--output", type=Path, default=None, help="Output report file")
    parser.add_argument("--strict", action="store_true", help="Fail on unresolvable links")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")

    args = parser.parse_args()

    if args.spec_dir is None:
        args.spec_dir = args.project_root / "open-someip-spec" / "src"
    if args.requirements_dir is None:
        args.requirements_dir = args.project_root / "docs" / "requirements"

    print("Spec Requirements Mapping Verification")
    print("=" * 40)

    print(f"\nLoading spec requirements from: {args.spec_dir}")
    spec_reqs = extract_spec_requirements(args.spec_dir)
    print(f"  Found {len(spec_reqs)} spec requirements")

    prefixes = defaultdict(int)
    for sid in spec_reqs:
        prefix = re.sub(r"_\d+$", "", sid)
        prefixes[prefix] += 1
    for p, c in sorted(prefixes.items()):
        print(f"    {p}_*: {c}")

    print(f"\nLoading implementation requirements from: {args.requirements_dir}")
    impl_reqs, satisfies_map = extract_impl_requirements(args.requirements_dir)
    print(f"  Found {len(impl_reqs)} implementation requirements")
    print(f"  Found {len(satisfies_map)} requirements with :satisfies: links")

    print("\nAnalyzing mappings...")
    analysis = analyze_mappings(spec_reqs, impl_reqs, satisfies_map)

    generate_report(spec_reqs, impl_reqs, analysis, args.output)

    print("\n" + "=" * 40)
    print("Summary:")
    print(f"  Spec requirements: {analysis['spec_req_count']}")
    print(f"  Implementation requirements: {analysis['impl_req_count']}")
    print(f"    Spec-derived: {analysis['spec_derived_count']}")
    print(f"    Implementation-derived: {analysis['impl_derived_count']}")
    print(f"  Mapped spec requirements: {len(analysis['mapped_spec_reqs'])}")
    print(f"  Unmapped spec requirements: {len(analysis['unmapped_spec_reqs'])}")
    print(f"  Auto-corrected links: {len(analysis['auto_corrections'])}")
    print(f"  Unresolvable links: {len(analysis['unresolvable_links'])}")
    print(f"  Missing spec links: {len(analysis['impl_missing_spec_link'])}")

    if analysis["spec_req_count"] > 0:
        coverage = len(analysis["mapped_spec_reqs"]) / analysis["spec_req_count"] * 100
        print(f"  Spec coverage: {coverage:.1f}%")

    if analysis["auto_corrections"]:
        print(f"\n  Auto-corrected {len(analysis['auto_corrections'])} wrong-prefix links:")
        for c in analysis["auto_corrections"][:10]:
            print(f"    {c['impl_id']}: {c['written']} → {c['resolved']}")
        if len(analysis["auto_corrections"]) > 10:
            print(f"    ... and {len(analysis['auto_corrections']) - 10} more")

    if args.output:
        print(f"\nReport written to: {args.output}")

    exit_code = 0
    if args.strict:
        if analysis["unresolvable_links"]:
            print(f"\nFAIL: {len(analysis['unresolvable_links'])} unresolvable spec links")
            exit_code = 1
        if analysis["impl_missing_spec_link"]:
            print(
                f"\nFAIL: {len(analysis['impl_missing_spec_link'])} implementation requirements missing spec links"
            )
            exit_code = 1

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
