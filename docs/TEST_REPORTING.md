<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# Test Reporting

## Overview

opensomeip uses a three-layer CI reporting architecture that applies across all
five test stacks (Host, FreeRTOS, ThreadX, Zephyr, Python). Reports are
generated automatically on every push and PR to `main`.

| Layer | Tool | Where it appears |
|-------|------|------------------|
| **Checks** | EnricoMi/publish-unit-test-result-action | PR sidebar / Checks tab |
| **Job Summaries** | ctrf-io/github-test-reporter, CodeCoverageSummary | Actions → job → Summary |
| **Allure Report** | Allure CLI + GitHub Pages | [vtz.github.io/opensomeip/allure/](https://vtz.github.io/opensomeip/allure/) |

## Layer 1: Check Runs (PR Sidebar)

Each test stack publishes a named check run via
[EnricoMi/publish-unit-test-result-action](https://github.com/EnricoMi/publish-unit-test-result-action)
(v2). These appear in the PR sidebar with pass/fail counts and individual test
annotations.

| Stack | Check name | Input files |
|-------|-----------|-------------|
| Host | Host Test Results | GTest XML (`gtest_results/**/*.xml`) |
| FreeRTOS | FreeRTOS Test Results | JUnit XML from ctest + Renode |
| ThreadX | ThreadX Test Results | JUnit XML from ctest + Renode |
| Zephyr | Zephyr Test Results | GTest XML + native\_sim + Renode JUnit |
| Python | Python Test Results | pytest JUnit XML |

PR comments are disabled (`comment_mode: "off"`) — results are visible only in
the Checks tab and as inline annotations.

**Python extra:** an additional check run (**Python Detailed Test Report**) is
created via `gh api repos/.../check-runs` with a markdown summary table and
up to 50 failed test details.

## Layer 2: Job Summaries

Richer reports are written to `$GITHUB_STEP_SUMMARY` and appear in
Actions → select workflow run → job → Summary tab.

### CTRF Reporter (Python)

After test execution, `junit-to-ctrf` converts JUnit XML to CTRF JSON, then
[ctrf-io/github-test-reporter](https://github.com/ctrf-io/github-test-reporter)
generates multiple summary views:

- **summary-report**: aggregate pass/fail/skip counts
- **failed-report**: details of failed tests
- **suite-list-report**: per-suite pass/fail/duration table
- **test-report**: per-test status, flaky flag, and duration

### Coverage Summary (Host)

[irongut/CodeCoverageSummary](https://github.com/irongut/CodeCoverageSummary)
produces a markdown badge + table for line/branch coverage from the Cobertura
XML, appended to the job summary.

### Requirements Validation

`requirements-validation.yml` writes a custom markdown report to the job summary
with traceability check results.

## Layer 3: Allure Report (GitHub Pages)

The `allure-report.yml` workflow runs after the `CI` workflow completes **on
`main`** (via `workflow_run` trigger). It does not run on PRs.

**Process:**

1. Downloads all `test-results-*`, `gtest-results-*`, and `allure-results-*`
   artifacts from the completed CI run
2. Restores trend history from the `gh-pages` branch
3. Generates the report with `allure generate`
4. Publishes to GitHub Pages under `/allure/`

**Features:**

- Historical trend tracking across builds
- Combined view of all test stacks in a single report
- Native Allure results from Python (via `allure-pytest`)
- JUnit XML from C++ stacks converted to Allure format

## Test Result Artifacts

Each workflow uploads standardized artifacts for downstream consumption:

| Artifact pattern | Contents | Consumed by |
|-----------------|----------|-------------|
| `test-results-host-*` | ctest JUnit XML | Allure |
| `gtest-results-host-*` | GTest XML with suite detail | EnricoMi, Allure |
| `test-results-freertos*` | JUnit XML (POSIX + Renode) | EnricoMi, Allure |
| `test-results-threadx*` | JUnit XML (POSIX + Renode) | EnricoMi, Allure |
| `test-results-zephyr-*` | JUnit XML (host, native\_sim, Renode) | EnricoMi, Allure |
| `gtest-results-zephyr-*` | GTest XML | EnricoMi, Allure |
| `test-results-python` | pytest JUnit XML | EnricoMi, CTRF, Allure |
| `allure-results-python` | Native Allure JSON | Allure |

## Local Development

### Generating JUnit XML

```bash
# CTest (C++ tests)
cd build
ctest --output-on-failure --output-junit junit_results.xml

# GTest detailed XML
export GTEST_OUTPUT="xml:gtest_results/"
ctest --output-on-failure
```

### Coverage Reports

```bash
# Build with coverage
cmake -B build -DCOVERAGE=ON
cmake --build build
cd build && ctest --output-on-failure

# Generate HTML coverage (requires gcovr)
gcovr --root .. --exclude ../tests/ --exclude ../examples/ \
      --html --html-details -o coverage/index.html

# Or use lcov
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' -o coverage.info
genhtml coverage.info -o coverage-html/
```

### Python Test Reports

```bash
cd tests/python
pytest --junitxml=junit_results.xml \
       --cov=someip --cov-report=html:htmlcov \
       --alluredir=allure-results
```

## Report Locations (Local)

```
build/
├── junit_results.xml          # CTest aggregate results
├── gtest_results/             # Per-suite GTest XML
├── coverage/
│   └── index.html             # C++ coverage (gcovr)
└── test_report.json           # Combined JSON report

tests/python/
├── junit_results.xml          # Python test results
├── htmlcov/
│   └── index.html             # Python coverage
├── coverage.xml               # Cobertura XML
└── allure-results/            # Allure native results
```

## External CI Integration

### Jenkins

```groovy
pipeline {
    stages {
        stage('Test') {
            steps {
                sh './scripts/run_tests.py --rebuild --coverage'
            }
            post {
                always {
                    junit 'build/junit_results.xml'
                    publishHTML(target: [
                        reportDir: 'build/coverage',
                        reportFiles: 'index.html',
                        reportName: 'Coverage Report'
                    ])
                }
            }
        }
    }
}
```

### GitLab CI

```yaml
test:
  stage: test
  script:
    - ./scripts/run_tests.py --rebuild --coverage
  artifacts:
    reports:
      junit: build/junit_results.xml
      coverage_report:
        coverage_format: cobertura
        path: tests/python/coverage.xml
    paths:
      - build/coverage/
    expire_in: 1 week
```
