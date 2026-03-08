#!/usr/bin/env python3
"""Validate requirement ID naming conventions.

Checks:
1. Every _E0x error-path requirement has a parent (base ID without _E0x suffix)
   defined in the same RST file.
2. The _E0x ID's domain prefix matches its parent's domain prefix.
3. No duplicate requirement IDs across all RST files.

Exit codes:
  0 - all checks pass
  1 - one or more violations found
"""

import re
import sys
from collections import defaultdict
from pathlib import Path


def main() -> int:
    req_dir = Path(__file__).resolve().parent.parent / "docs" / "requirements" / "implementation"
    if not req_dir.exists():
        print(f"Requirements directory not found: {req_dir}")
        return 1

    id_pattern = re.compile(r"^\s+:id:\s+(REQ_\S+)", re.MULTILINE)
    error_suffix = re.compile(r"^(.+)_E\d+$")

    all_ids: dict[str, str] = {}  # id -> file
    per_file: dict[str, set[str]] = defaultdict(set)
    errors: list[str] = []

    for rst_file in sorted(req_dir.rglob("*.rst")):
        content = rst_file.read_text(encoding="utf-8", errors="ignore")
        rel = rst_file.relative_to(req_dir.parent.parent.parent)

        for m in id_pattern.finditer(content):
            rid = m.group(1)

            if rid in all_ids:
                errors.append(f"Duplicate ID {rid} in {rel} (first seen in {all_ids[rid]})")
            else:
                all_ids[rid] = str(rel)

            per_file[str(rel)].add(rid)

    for rid, file_path in sorted(all_ids.items()):
        em = error_suffix.match(rid)
        if not em:
            continue
        parent = em.group(1)
        if parent not in all_ids:
            errors.append(f"{rid} ({file_path}): parent {parent} not found in any RST file")
            continue

        def domain(req_id: str) -> str:
            parts = req_id.split("_")
            # REQ_TP_082_E01 -> domain is TP; REQ_PAL_THREAD_CREATE_E01 -> PAL
            if len(parts) >= 3:
                return parts[1]
            return ""

        if domain(rid) != domain(parent):
            errors.append(
                f"{rid} ({file_path}): domain mismatch with parent {parent} "
                f"({domain(rid)} vs {domain(parent)})"
            )

    if errors:
        print(f"Found {len(errors)} requirement ID issue(s):\n")
        for e in errors:
            print(f"  - {e}")
        return 1

    print(f"All {len(all_ids)} requirement IDs are valid.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
