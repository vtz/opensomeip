#!/usr/bin/env python3
"""Cross-check test annotations against defined requirements.

Scans all C++ test files for @tests and @implements annotations, then verifies
each referenced requirement ID actually exists in the RST requirement files.

Exit codes:
  0 - all annotations reference valid requirement IDs
  1 - one or more annotations reference undefined requirement IDs
"""

import re
import sys
from collections import defaultdict
from pathlib import Path


def collect_defined_ids(req_dir: Path) -> set[str]:
    """Collect all requirement IDs defined in RST files."""
    id_pattern = re.compile(r"^\s+:id:\s+(REQ_\S+)", re.MULTILINE)
    ids: set[str] = set()
    for rst_file in req_dir.rglob("*.rst"):
        for m in id_pattern.finditer(rst_file.read_text(encoding="utf-8", errors="ignore")):
            ids.add(m.group(1))
    return ids


def collect_annotations(src_dirs: list[Path]) -> dict[str, list[tuple[str, int]]]:
    """Collect REQ_* IDs from @tests and @implements annotations.

    Returns a mapping of requirement ID -> [(file, line), ...].
    """
    tag_pattern = re.compile(r"@(?:tests|implements)\s+([\w,\s]+)")
    req_id = re.compile(r"(REQ_\w+)")
    refs: dict[str, list[tuple[str, int]]] = defaultdict(list)

    for src_dir in src_dirs:
        for cpp_file in sorted(src_dir.rglob("*.cpp")):
            content = cpp_file.read_text(encoding="utf-8", errors="ignore")
            for line_no, line in enumerate(content.splitlines(), 1):
                for tag_match in tag_pattern.finditer(line):
                    for id_match in req_id.finditer(tag_match.group(1)):
                        refs[id_match.group(1)].append((str(cpp_file), line_no))
        for h_file in sorted(src_dir.rglob("*.h")):
            content = h_file.read_text(encoding="utf-8", errors="ignore")
            for line_no, line in enumerate(content.splitlines(), 1):
                for tag_match in tag_pattern.finditer(line):
                    for id_match in req_id.finditer(tag_match.group(1)):
                        refs[id_match.group(1)].append((str(h_file), line_no))

    return dict(refs)


def main() -> int:
    project_root = Path(__file__).resolve().parent.parent
    req_dir = project_root / "docs" / "requirements" / "implementation"
    src_dirs = [
        project_root / "src",
        project_root / "include",
        project_root / "tests",
    ]

    if not req_dir.exists():
        print(f"Requirements directory not found: {req_dir}")
        return 1

    defined = collect_defined_ids(req_dir)
    annotations = collect_annotations([d for d in src_dirs if d.exists()])

    orphans: list[tuple[str, list[tuple[str, int]]]] = []
    for rid, locations in sorted(annotations.items()):
        if rid not in defined:
            orphans.append((rid, locations))

    if orphans:
        print(f"Found {len(orphans)} annotation(s) referencing undefined requirements:\n")
        for rid, locations in orphans:
            print(f"  {rid}:")
            for filepath, line in locations:
                rel = Path(filepath).relative_to(project_root)
                print(f"    - {rel}:{line}")
        return 1

    print(
        f"All {len(annotations)} annotated requirement IDs are valid "
        f"(checked against {len(defined)} defined requirements)."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
