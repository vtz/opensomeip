#!/usr/bin/env python3
"""Check that committed traceability docs match current validation output.

Runs the requirement validation pipeline and compares key metrics against
the values claimed in TRACEABILITY_SUMMARY.md. Fails if the committed doc
is stale (metrics diverged from actual validation output).

Exit codes:
  0 - docs are up to date
  1 - docs are stale or could not be validated
"""

import re
import subprocess
import sys
from pathlib import Path


def extract_metric(text: str, label: str) -> str | None:
    """Extract a metric value from a markdown table row."""
    pattern = re.compile(rf"\|\s*{re.escape(label)}\s*\|\s*([^\|]+?)\s*\|", re.IGNORECASE)
    m = pattern.search(text)
    if m:
        return m.group(1).strip()
    return None


def extract_validation_metrics(output: str) -> dict[str, str]:
    """Parse key metrics from validate_requirements.py stdout."""
    metrics: dict[str, str] = {}

    total_m = re.search(r"Total requirements:\s*(\d+)", output)
    if total_m:
        metrics["total"] = total_m.group(1)

    # validate_requirements.py prints "616/697 (88.4%)", not "616 (88.4%)".
    traced_m = re.search(r"Fully traced.*?:\s*(\d+)(?:/\d+)?\s*\((\d+\.\d+)%\)", output)
    if traced_m:
        metrics["traced_count"] = traced_m.group(1)
        metrics["traced_pct"] = traced_m.group(2)

    orphan_m = re.search(r"Orphaned.*?:\s*(\d+)", output)
    if orphan_m:
        metrics["orphaned"] = orphan_m.group(1)

    return metrics


def main() -> int:
    project_root = Path(__file__).resolve().parent.parent
    summary_path = project_root / "TRACEABILITY_SUMMARY.md"

    if not summary_path.exists():
        print("TRACEABILITY_SUMMARY.md not found — nothing to check.")
        return 0

    code_refs_path = project_root / "build" / "code_references.json"
    if not code_refs_path.exists():
        print("build/code_references.json not found. Run extract_code_requirements.py first.")
        print("Generating code references...")
        result = subprocess.run(
            [
                sys.executable,
                str(project_root / "scripts" / "extract_code_requirements.py"),
                "--project-root",
                str(project_root),
                "--output",
                str(code_refs_path),
                "--src-dirs",
                "src",
                "include",
                "--test-dirs",
                "tests",
            ],
            capture_output=True,
            text=True,
            cwd=str(project_root),
        )
        if result.returncode != 0:
            print(f"extract_code_requirements.py failed:\n{result.stderr}")
            return 1

    result = subprocess.run(
        [
            sys.executable,
            str(project_root / "scripts" / "validate_requirements.py"),
            "--project-root",
            str(project_root),
            "--code-refs",
            str(code_refs_path),
        ],
        capture_output=True,
        text=True,
        cwd=str(project_root),
    )
    validation_output = result.stdout + result.stderr

    actual = extract_validation_metrics(validation_output)
    if not actual.get("total"):
        print("Could not extract metrics from validate_requirements.py output.")
        print(f"Output:\n{validation_output[:2000]}")
        return 1

    summary_text = summary_path.read_text(encoding="utf-8")

    # Labels are literal table text. extract_metric() escapes them; passing
    # a pre-escaped regex made every lookup miss and the check always pass.
    committed_total = extract_metric(summary_text, "Total requirements (RST)")
    committed_traced = extract_metric(summary_text, "Fully traced (code + tests)")
    committed_orphaned = extract_metric(summary_text, "Orphaned (no code annotation)")

    def first_int(value: str | None) -> str | None:
        if not value:
            return None
        match = re.search(r"\d+", value)
        return match.group(0) if match else None

    errors: list[str] = []

    if first_int(committed_total) != actual.get("total"):
        errors.append(f"Total requirements: committed={committed_total}, actual={actual['total']}")

    if first_int(committed_traced) != actual.get("traced_count"):
        errors.append(
            f"Fully traced: committed={committed_traced}, "
            f"actual={actual.get('traced_count', '?')} ({actual.get('traced_pct', '?')}%)"
        )

    if first_int(committed_orphaned) != actual.get("orphaned"):
        errors.append(f"Orphaned: committed={committed_orphaned}, actual={actual['orphaned']}")

    if errors:
        print("TRACEABILITY_SUMMARY.md is stale — committed metrics don't match validation:\n")
        for e in errors:
            print(f"  - {e}")
        print(
            "\nRun 'cmake --build build --target requirements_check' and commit "
            "the updated TRACEABILITY_SUMMARY.md."
        )
        return 1

    print("TRACEABILITY_SUMMARY.md is up to date with validation output.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
