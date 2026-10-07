<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# SOME/IP Traceability Analysis Summary

> **Important**: This document reflects the validated output of
> `scripts/validate_requirements.py` and `scripts/extract_code_requirements.py`.
> To regenerate, run: `cmake --build build --target requirements_check`

## Executive Summary

This analysis provides traceability from Open SOME/IP Specification requirements
to implementation and test coverage. No safety certification is claimed.

## Key Findings

### Current Coverage Snapshot (Validated)

| Metric | Value | Assessment |
|--------|-------|------------|
| Total requirements (RST) | 699 | - |
| Fully traced (code + tests) | 618 (88.4%) | Good |
| Requirements with code refs | 620 | Good |
| Requirements with test coverage | 686 | Good |
| Orphaned (no code annotation) | 78 | Needs improvement |
| Missing spec links | 2 | REQ_MSG_150, REQ_TRANSPORT_026 |
| Code references extracted | 625 | - |
| Test references extracted | 751 | - |

### Status

618 of 699 requirements are fully traced with both code implementation references
and test coverage annotations. 78 requirements remain without code annotations.
The extraction script properly parses comma-separated requirement IDs from
`@implements` and `@tests` annotations.

> **Regeneration**: Run `cmake --build build --target requirements_check` to
> update these metrics from `scripts/validate_requirements.py`.

## Coverage Breakdown

### Requirements Fully Traced (by module)

| Module | Total Reqs |
|--------|-----------|
| Service Discovery (REQ_SD_*) | 206 |
| Message Header (REQ_MSG_*) | 130 |
| Serialization (REQ_SER_*) | 115 |
| Transport Protocol (REQ_TP_*) | 83 |
| Platform (REQ_PLATFORM_*, REQ_PAL_*) | 73 |
| Transport (REQ_TRANSPORT_*) | 47 |
| Compatibility (REQ_COMPAT_*) | 17 |
| C ABI (REQ_CAPI_*) | 13 |
| Architecture (REQ_ARCH_*) | 8 |
| E2E (REQ_E2E_*) | 6 |
| Other (REQ_MY_*) | 1 |

> **Note**: Requirement counts reflect the full RST definitions.
> Per-module code ref and test coverage details are available in the
> `validate_requirements.py` output and `TEST_TRACEABILITY_MATRIX.md`.

### Test Execution Summary

Default host build (`SOMEIP_USE_STATIC_ALLOC=OFF`, C API on), 2026-10-04:
28 CTest binaries, 723 GTest cases, 722 passed, 0 failed. One case is
disabled (`TcpTransportTest.DISABLED_MessageRoundTrip`).

The static-allocation build (`SOMEIP_USE_STATIC_ALLOC=ON`) passed on
2026-10-01: 34 CTest binaries, 813 GTest cases, 0 failures, with that same TCP
case disabled. Suites that exist only in that build: buffer pool 14,
static message pool 16, platform containers 21, ETL error handler 4,
static-alloc integration 13, PAL static-alloc mock 24.

| Test Suite | Tests | Status |
|------------|-------|--------|
| SD Tests | 104 | All passing |
| Endpoint Tests | 76 | All passing |
| TP Tests | 62 | All passing |
| Serialization Tests | 60 | All passing |
| TCP Transport Tests | 51 | 50 passing, 1 disabled |
| E2E Tests | 43 | All passing |
| UDP Transport Tests | 38 | All passing |
| Message Tests | 36 | All passing |
| Events Tests | 28 | All passing |
| Event-driven TCP Tests | 25 | All passing |
| PAL FreeRTOS Mock | 25 | All passing |
| PAL ThreadX Mock | 25 | All passing |
| PAL Zephyr Mock | 25 | All passing |
| Session Manager Tests | 23 | All passing |
| Platform Threading Tests | 21 | All passing |
| RPC Tests | 16 | All passing |
| Event-driven UDP Tests | 12 | All passing |
| Multicast Membership Tests | 7 | All passing |
| C API Tests | 46 | All passing |

## Validation Status

> **Methodology**: Traceability counts are produced by `scripts/validate_requirements.py`
> using `@implements`, `@tests`, and `:satisfies:` annotations in source code and RST files.
> "Fully traced" means a requirement has both a code annotation (`@implements`) and a
> test annotation (`@tests`).  Run `cmake --build build --target requirements_check`
> to regenerate.  See `TEST_TRACEABILITY_MATRIX.md` Section 8 for the detailed breakdown.

`validate_requirements.py --strict` passes with zero critical errors.
Current validated traceability metrics should be read from the most recent
`requirements_check` output or `TEST_TRACEABILITY_MATRIX.md` Section 8.

## Recommendations

### Short-term

- Implement remaining serialization requirements (REQ_SER_090 through REQ_SER_107)
- Add `@implements` annotations for the 78 orphaned requirements
- Add test coverage for 12 requirements without `@tests` annotations
- Add performance, stress, and fault-injection tests

### Long-term

- Achieve >95% full traceability (currently 88.4%)
- Implement advanced SD features (load balancing, IPv6)
- Add cross-platform test coverage (FreeRTOS, ThreadX hardware)
- Implement Win32 and LwIP platform backends

## Files

1. `TRACEABILITY_MATRIX.md` - Requirements to implementation mapping
2. `TEST_TRACEABILITY_MATRIX.md` - Test case to requirements mapping
3. `TRACEABILITY_SUMMARY.md` - This executive summary

---

*Prepared for assessing the automotive SOME/IP implementation; does not constitute a safety certification.*
