<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  SPDX-License-Identifier: Apache-2.0
-->

# FMEA — C API FFI Boundary

| Field | Value |
|-------|-------|
| **Document ID** | FMEA-FFI-CAPI-001 |
| **Version** | 0.1 (stub — expanded in OSI-CAPI-11) |
| **Status** | Draft |
| **Scope** | OpenSOME/IP C ABI (`include/capi/opensomeip.h`, `src/capi/`) |
| **Related requirements** | REQ_CAPI_001–REQ_CAPI_013 |

## 1. Purpose

This document will perform a **Failure Mode and Effects Analysis (FMEA)** for
the OpenSOME/IP **C API FFI boundary** — the `extern "C"` wrapper layer that
language bindings (Rust, Python, etc.) and pure-C consumers call.

The analysis will cover:

- Stack unwind across FFI (C++ exceptions escaping `extern "C"`)
- Use-after-destroy of opaque handles
- Buffer overflow in caller-provided buffers
- Callback re-entry and lifetime hazards
- Mixed-language threading and synchronisation hazards

## 2. Assumptions of Use

*(To be completed in OSI-CAPI-11 after implementation is stable.)*

## 3. FMEA Table

*(To be completed in OSI-CAPI-11.)*

## 4. Residual Risk Assessment

*(To be completed in OSI-CAPI-11.)*

---

*End of document FMEA-FFI-CAPI-001 (stub)*
