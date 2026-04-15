#!/usr/bin/env python3
"""Convert a clang-tidy report into Allure-compatible JUnit XML.

Each clang-tidy check becomes a test suite, each unique warning becomes a
failing test case. The quality-gate result (pass/fail + counts) is emitted
as its own top-level test case.

Usage:
    python3 clang_tidy_to_allure.py <report.txt> <output-dir> [--threshold N]

The output directory will contain a single JUnit XML file that the Allure
report workflow can ingest.
"""

import argparse
import re
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path


def parse_report(report_path: str) -> tuple[list[tuple[str, int, str, str]], dict[str, int]]:
    warnings: list[tuple[str, int, str, str]] = []
    summary: dict[str, int] = {}

    with open(report_path) as f:
        for line in f:
            m = re.match(r"(.+?):(\d+):\d+: warning: (.+?) \[(.+?)\]", line.strip())
            if m:
                filepath, lineno, message, check = (
                    m.group(1),
                    int(m.group(2)),
                    m.group(3),
                    m.group(4),
                )
                warnings.append((filepath, lineno, message, check))
                continue

            for key in ("Total Warnings", "Total Errors", "Total Issues", "Files with issues"):
                if line.strip().startswith(f"{key}:"):
                    val = line.strip().split(":")[-1].strip()
                    if val.isdigit():
                        summary[key] = int(val)

    return warnings, summary


def build_xml(
    warnings: list[tuple[str, int, str, str]],
    summary: dict[str, int],
    threshold: int | None,
) -> ET.Element:
    testsuites = ET.Element("testsuites")

    by_check: dict[str, list[tuple[str, int, str]]] = defaultdict(list)
    for filepath, lineno, message, check in warnings:
        by_check[check].append((filepath, lineno, message))

    for check, items in sorted(by_check.items()):
        unique = sorted(set(items))
        suite = ET.SubElement(
            testsuites,
            "testsuite",
            name=f"clang-tidy/{check}",
            tests=str(len(unique)),
            failures=str(len(unique)),
        )
        for filepath, lineno, message in unique:
            tc = ET.SubElement(
                suite,
                "testcase",
                classname=f"clang-tidy.{check}",
                name=f"{filepath}:{lineno}",
            )
            failure = ET.SubElement(tc, "failure", message=message)
            failure.text = f"{filepath}:{lineno}: {message} [{check}]"

    total = summary.get("Total Issues", len(warnings))
    gate_suite = ET.SubElement(
        testsuites,
        "testsuite",
        name="clang-tidy/quality-gate",
        tests="1",
        failures="0" if (threshold is None or total <= threshold) else "1",
    )
    tc = ET.SubElement(
        gate_suite,
        "testcase",
        classname="clang-tidy.quality-gate",
        name=f"violations={total}" + (f" threshold={threshold}" if threshold else ""),
    )
    if threshold is not None and total > threshold:
        failure = ET.SubElement(
            tc, "failure", message=f"Violation count ({total}) exceeds threshold ({threshold})"
        )
        failure.text = f"FAILED: {total} > {threshold}"

    return testsuites


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", help="Path to clang-tidy-report.txt")
    parser.add_argument("output_dir", help="Output directory for JUnit XML")
    parser.add_argument("--threshold", type=int, default=None, help="Quality-gate threshold")
    args = parser.parse_args()

    warnings, summary = parse_report(args.report)
    xml_root = build_xml(warnings, summary, args.threshold)

    out = Path(args.output_dir)
    out.mkdir(parents=True, exist_ok=True)
    tree = ET.ElementTree(xml_root)
    ET.indent(tree, space="  ")
    tree.write(out / "clang-tidy-results.xml", encoding="unicode", xml_declaration=True)
    print(f"Wrote {len(warnings)} warnings to {out / 'clang-tidy-results.xml'}")


if __name__ == "__main__":
    main()
