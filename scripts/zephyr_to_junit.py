#!/usr/bin/env python3
"""Parse Zephyr test output and generate JUnit XML.

Parses lines like:
    === Results: 5 passed, 0 failed ===
    PASS - test_name
    FAIL - test_name
"""

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def parse_zephyr_log(log_text: str, suite_name: str) -> ET.Element:
    testsuite = ET.Element("testsuite", name=suite_name)

    tests = 0
    failures = 0

    for line in log_text.splitlines():
        pass_match = re.match(r"\s*PASS\s*-\s*(.+)", line)
        fail_match = re.match(r"\s*FAIL\s*-\s*(.+)", line)

        if pass_match:
            tests += 1
            ET.SubElement(
                testsuite, "testcase", name=pass_match.group(1).strip(), classname=suite_name
            )
        elif fail_match:
            tests += 1
            failures += 1
            tc = ET.SubElement(
                testsuite, "testcase", name=fail_match.group(1).strip(), classname=suite_name
            )
            ET.SubElement(tc, "failure", message="Test failed")

    if tests == 0:
        results_match = re.search(
            r"=== Results:\s*(\d+)\s*passed,\s*(\d+)\s*failed\s*===", log_text
        )
        if results_match:
            passed = int(results_match.group(1))
            failed = int(results_match.group(2))
            tests = passed + failed
            for i in range(passed):
                ET.SubElement(testsuite, "testcase", name=f"test_{i + 1}", classname=suite_name)
            for i in range(failed):
                failures += 1
                tc = ET.SubElement(
                    testsuite, "testcase", name=f"failed_test_{i + 1}", classname=suite_name
                )
                ET.SubElement(tc, "failure", message="Test failed")

    testsuite.set("tests", str(tests))
    testsuite.set("failures", str(failures))
    testsuite.set("errors", "0")

    return testsuite


def main():
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <log_file> <suite_name> <output_xml>", file=sys.stderr)
        sys.exit(1)

    log_file = Path(sys.argv[1])
    suite_name = sys.argv[2]
    output_xml = Path(sys.argv[3])

    if not log_file.exists():
        print(f"Error: log file not found: {log_file}", file=sys.stderr)
        sys.exit(1)

    log_text = log_file.read_text()
    testsuite = parse_zephyr_log(log_text, suite_name)

    testsuites = ET.Element("testsuites")
    testsuites.append(testsuite)
    tree = ET.ElementTree(testsuites)
    ET.indent(tree, space="  ")
    tree.write(str(output_xml), xml_declaration=True, encoding="utf-8")

    tests = int(testsuite.get("tests", "0"))
    failures = int(testsuite.get("failures", "0"))
    print(f"Generated {output_xml}: {tests} tests, {failures} failures")

    sys.exit(1 if failures > 0 else 0)


if __name__ == "__main__":
    main()
