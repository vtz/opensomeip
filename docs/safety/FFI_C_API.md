<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  SPDX-License-Identifier: Apache-2.0
-->

# FMEA — C API FFI Boundary

| Field | Value |
|-------|-------|
| **Document ID** | FMEA-FFI-CAPI-001 |
| **Version** | 1.0 |
| **Status** | Draft |
| **Scope** | OpenSOME/IP C ABI (`include/capi/opensomeip.h`, `src/capi/`) |
| **Related requirements** | REQ_CAPI_001–REQ_CAPI_013 |

## 1. Purpose

This document performs a **Failure Mode and Effects Analysis (FMEA)** for the
OpenSOME/IP **C API FFI boundary** — the `extern "C"` wrapper layer that
language bindings (Rust, Python, etc.) and pure-C consumers call.

The C ABI is the ISO 26262 mixed-language qualification boundary between the
C++17 protocol stack and foreign-language consumers. All protocol operations
are accessed exclusively through opaque handles and POD parameter types.
No C++ types, templates, or exceptions appear in the public API surface.

This FMEA supplements the project-wide software FMEA and is traceable to
C API requirements in `docs/requirements/implementation/c_api.rst` and
architecture requirements in `docs/requirements/implementation/architecture.rst`.

## 2. ISO 26262 Applicability

| ISO 26262 Part | Relevance to this FMEA |
|----------------|------------------------|
| **Part 6 — Software** | The FFI wrapper layer is a software-architectural boundary (clause 7.4.3). Error propagation across the boundary must be defined (clause 9.4.3). Exception/unwind behaviour must be deterministic (no UB). |
| **Part 9 — ASIL analyses** | This FMEA follows the systematic failure-analysis methodology described in Part 9, clause 8, adapted for cross-language FFI failure modes. |

## 3. Exception Firewall Strategy

### 3.1 Problem: C++ Exceptions across `extern "C"`

Allowing a C++ exception to propagate through an `extern "C"` function is
**undefined behaviour** in the C++ standard (and always fatal when the caller
is Rust compiled with `panic=abort`). The C API must guarantee that no
exception escapes any wrapper function.

### 3.2 Mitigation: try/catch on Every Entry Point

Every C wrapper function follows this pattern:

```cpp
extern "C" opensomeip_result_t opensomeip_xxx(...) {
    if (!handle) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        // C++ operations
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) {
        return OPENSOMEIP_RESULT_INTERNAL_ERROR;
    }
}
```

The `catch (...)` clause ensures that **no exception type** — including
`std::bad_alloc`, `std::runtime_error`, or any user-defined exception —
can escape the wrapper. The catch converts the exception to
`OPENSOMEIP_RESULT_INTERNAL_ERROR`.

### 3.3 Rationale

This strategy aligns with REQ_CAPI_007 (Exception Firewall) and
REQ_ARCH_004 (Consistent Error Handling). The integer return code is the
only error-reporting mechanism visible to FFI consumers.

## 4. FMEA Table

Severity scale (same as FMEA-STATIC-ALLOC-001):

| Level | Label | Description |
|-------|-------|-------------|
| **S4** | Critical | Loss of safety function or data corruption |
| **S3** | Major | Service disruption or message loss with detectable impact |
| **S2** | Moderate | Degraded performance with recovery path |
| **S1** | Minor | Benign failure when handled correctly |

| Failure Mode | Affected Component | Effect | Detection | Mitigation | Severity |
|---|---|---|---|---|---|
| **C++ exception unwind across FFI** — exception thrown inside wrapper escapes `extern "C"` boundary | All `src/capi/*.cpp` wrappers | Undefined behaviour: Rust callers abort (`panic=abort`), C callers crash or corrupt stack. Complete loss of communication. | Code review: every wrapper has `try { … } catch (…)`; CI: unit test `TC_CAPI_MSG_NULL_001` verifies error codes, not exceptions. `grep -r "catch" src/capi/` confirms coverage. | Exception firewall (§3): `try/catch(...)` on every entry point. `OPENSOMEIP_RESULT_INTERNAL_ERROR` returned. No unwind crosses boundary. | **S4** (if unmitigated); **S1** (with firewall) |
| **Use-after-destroy** — caller uses a handle after calling `_destroy()` | All opaque handles (`opensomeip_message_t*`, etc.) | Dangling pointer dereference: crash (best case) or memory corruption / data leak (worst case). | ASan/UBSan in CI test suite; code review of caller lifetime discipline. | The library cannot prevent use-after-destroy in the caller. Documented as **Assumption of Use**: caller must not use a handle after `_destroy()`. `_destroy(nullptr)` returns `OPENSOMEIP_RESULT_INVALID_ARGUMENT` (no crash). Double-destroy of a valid pointer is UB — documented. | **S4** (caller defect); **S1** (with ASan in development) |
| **Buffer overflow in caller-provided buffer** — caller passes undersized `buf` / `buf_len` to get-payload, get-data, or serialize | `opensomeip_message_get_payload`, `opensomeip_serializer_get_data`, `opensomeip_message_serialize` | Out-of-bounds write: memory corruption, possible code execution. | Unit tests `TC_CAPI_MSG_PAYLOAD_OVERFLOW_001`, `TC_CAPI_SER_OVERFLOW_001` verify `OPENSOMEIP_RESULT_BUFFER_OVERFLOW` is returned without writing past the buffer. ASan in CI. | All copy-out functions check `*out_len < actual_size` **before** any `memcpy` and return `OPENSOMEIP_RESULT_BUFFER_OVERFLOW` with `*out_len` set to required size. No data is written when the buffer is too small. | **S4** (if unmitigated); **S1** (with bounds check) |
| **NULL handle passed to API** — caller passes `nullptr` where a valid handle is required | All API functions | Without check: null pointer dereference → crash. | Every wrapper checks `if (!handle) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;` as the first operation. Unit tests `TC_CAPI_MSG_NULL_001`, `TC_CAPI_SER_NULL_001`, etc. | NULL check on every entry point. Error code returned, no crash. | **S2** (if unmitigated); **S1** (with NULL check) |
| **Callback re-entry during destroy** — callback invocation triggers a destroy of the same handle | RPC async callback, SD callbacks, event callbacks | Deadlock or double-free if the callback calls `_destroy()` on the handle that is delivering the callback. | Code review; documented contract. | **Assumption of Use**: callbacks shall not call `_destroy()` on the handle delivering the callback. The library does not call callbacks after `_destroy()` returns. Documented in `opensomeip.h` (REQ_CAPI_004). | **S3** (caller defect); **S1** (with documented AoU) |
| **Callback panic/unwind in Rust consumer** — Rust callback panics, unwinding through C frames | Callback trampolines from Rust → C → C++ → C → Rust | Undefined behaviour: foreign unwind through C frames. Process may abort or corrupt state. | Rust consumers must use `catch_unwind` in callback trampolines or compile with `panic=abort`. | **Assumption of Use for Rust consumers**: callbacks must not panic. Rust `opensomeip` crate will use `catch_unwind` at the trampoline or `panic=abort` profile (REQ_CAPI_004, documented in `docs/FERROCENE_SUBSET.md`). | **S4** (if unmitigated in Rust); **S1** (with `panic=abort` or `catch_unwind`) |
| **Mixed-language thread safety** — concurrent calls from multiple language threads without synchronization | Transport send/receive, RPC async, SD callbacks | Data race: corrupted message state, lost callbacks, incorrect routing. | Thread-safety documentation per function (REQ_CAPI_006); C++ internals use mutexes (REQ_ARCH_002). | Each opaque handle is independent; concurrent operations on **different** handles are safe. Concurrent operations on the **same** handle require caller synchronization unless documented as thread-safe. Transport and RPC internals use internal mutexes. | **S3** (if caller ignores docs); **S2** (with internal mutexes) |
| **ABI version mismatch** — consumer compiled against header v0.1 links runtime v0.2 with changed struct layout | All API functions | Incorrect field offsets, memory corruption, silent wrong results. | `opensomeip_capi_version()` runtime query vs `OPENSOMEIP_CAPI_VERSION_*` macros. Consumer should assert at startup. | Semantic Versioning: MAJOR bump for ABI-breaking changes. Opaque handles prevent struct-layout coupling. Only POD endpoint struct is exposed — changes to it require a MAJOR bump. | **S3** (if mismatch occurs); **S1** (with version check + opaque handles) |

## 5. Assumptions of Use

Integrators deploying the C API must observe:

1. **Handle lifetime**: Do not use a handle after `_destroy()`. Do not call
   `_destroy()` twice on the same handle.

2. **Buffer sizing**: Always check the return code from copy-out functions.
   When `OPENSOMEIP_RESULT_BUFFER_OVERFLOW` is returned, `*out_len` contains
   the required size — reallocate and retry.

3. **Callback safety**: Callbacks must not call `_destroy()` on the handle
   delivering the callback. Callbacks must not throw C++ exceptions or Rust
   panics.

4. **Thread safety**: Concurrent use of the **same** handle from multiple
   threads requires external synchronization unless the function is documented
   as thread-safe.

5. **Version check**: At startup, verify that
   `opensomeip_capi_version() >> 16 == OPENSOMEIP_CAPI_VERSION_MAJOR`.

## 6. Residual Risk Assessment

### 6.1 Mitigated to Acceptable Levels

| Area | Residual Risk | Justification |
|------|---------------|---------------|
| Exception escape | **Negligible** | Every wrapper has `try/catch(...)`. CI grep confirms no uncovered entry points. |
| Buffer overflow | **Negligible** | Bounds check before every `memcpy`. ASan CI. |
| NULL handle | **Negligible** | NULL check on every entry point. |
| ABI mismatch | **Low** | Opaque handles isolate layout; version macros enable compile-time check. |

### 6.2 Remaining Integrator Responsibilities

| Area | Residual Risk | Justification |
|------|---------------|---------------|
| Use-after-destroy | **Moderate** | Library cannot prevent caller defect. ASan catches it in testing. |
| Callback re-entry | **Low–Moderate** | Documented AoU; library does not call callbacks post-destroy. |
| Rust panic in callback | **Low** | Mitigated by `panic=abort` or `catch_unwind` in Rust crate. |
| Thread safety | **Low–Moderate** | Internal mutexes for shared state; caller must not race on same handle. |

### 6.3 Overall Assessment

With the exception firewall (§3) and bounds-check patterns, the C API FFI
boundary provides **defined behaviour for all library-side failure modes**.

The dominant residual risks are **caller defects** (use-after-destroy,
callback re-entry, thread races on same handle) which are documented as
Assumptions of Use and detectable with ASan/TSan during development.

**Recommended ASIL allocation**: The wrapper layer itself adds no new
safety-critical failure modes beyond what the underlying C++ stack already
manages. FFI failure modes with library-side mitigation map to **QM–ASIL B**
depending on the safety goal of the consuming function.

---

*End of document FMEA-FFI-CAPI-001*
