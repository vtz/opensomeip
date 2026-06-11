<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  SPDX-License-Identifier: Apache-2.0
-->

# FMEA — Static Allocation in Lockstep Mode

| Field | Value |
|-------|-------|
| **Document ID** | FMEA-STATIC-ALLOC-001 |
| **Version** | 1.0 |
| **Status** | Draft |
| **Scope** | OpenSOME/IP static-allocation backend (`SOMEIP_USE_STATIC_ALLOC`) |
| **Related requirements** | REQ_ARCH_008, REQ_PLATFORM_STATIC_001–005, REQ_PAL_* |

## 1. Purpose

This document performs a **Failure Mode and Effects Analysis (FMEA)** for the
OpenSOME/IP **static allocation** feature operating in **lockstep mode**.

Lockstep mode is the no-heap runtime configuration enabled by
`SOMEIP_USE_STATIC_ALLOC`. In this mode, all protocol buffers, containers,
message objects, and pimpl storage are backed by compile-time-sized static
pools and inline buffers. No calls to `malloc`, `free`, `new`, or `delete`
occur during normal protocol operation.

The analysis identifies failure modes arising from bounded resource exhaustion,
capacity overflow, and concurrency hazards; documents detection and mitigation
strategies; and assesses residual risk for integrators deploying the stack in
safety-related contexts.

This FMEA supplements the project-wide software FMEA (FMEA-OPENSOMEIP-001) and
is traceable to platform requirements in
`docs/requirements/implementation/platform.rst` and architecture requirement
REQ_ARCH_008 in `docs/requirements/implementation/architecture.rst`.

## 2. ISO 26262 Applicability

| ISO 26262 Part | Relevance to this FMEA |
|----------------|------------------------|
| **Part 5 — Product development at the hardware level** | Static pool sizing is a hardware–software co-design activity. Integrators must map ECU RAM budgets to `SOMEIP_STATIC_MESSAGE_POOL_SIZE`, `SOMEIP_STATIC_BUFPOOL_TIER_*`, and container capacity CMake options. Undersized pools are a configuration hazard analogous to insufficient hardware memory (Part 5, clause 7). |
| **Part 6 — Product development at the software level** | Static allocation directly supports freedom-from-interference (clause 7.4.6) and WCET determinism (clause 7.4.11) by eliminating heap fragmentation and non-deterministic allocator latency. Error-path handling for pool exhaustion and container overflow must comply with software architectural design principles (clause 7.4.3) and defensive programming guidance (clause 9.4.3). |
| **Part 9 — Automotive Safety Integrity Level (ASIL)-oriented and safety-oriented analyses** | This FMEA follows the systematic failure-analysis methodology described in Part 9, clause 8, adapted for software resource-bounding failure modes. |

## 3. ETL Error Handler Strategy

The static-allocation backend uses the **Embedded Template Library (ETL)** for
bounded container implementations (`etl::vector`, `etl::string`, `etl::map`,
etc.) selected when `SOMEIP_USE_STATIC_ALLOC` is enabled.

### 3.1 Default ETL Behaviour (Unacceptable for Safety-Critical Builds)

By default, ETL invokes an internal error handler that **asserts** (or calls
`abort()`) when a container operation exceeds its compile-time capacity. This
behaviour is unacceptable in safety-critical operation because:

- An assert terminates the process/task, causing loss of communication and
  potential violation of safe state requirements.
- Assert paths are typically stripped in release builds, leading to undefined
  behaviour on overflow.

### 3.2 Safety-Critical Custom Error Handler

For safety-critical lockstep builds, a **custom ETL error handler** shall be
registered that:

1. **Logs** the error with structured fields: component name, ETL error code,
   source file, and line number.
2. **Returns** an error code to the calling ETL container operation (no
   exception, no process termination).
3. **Does not** call `abort()`, `assert()`, or any function that terminates
   the runtime.

The caller (PAL wrapper or protocol layer) is responsible for checking the
return value and propagating a `Result` error to the application.

### 3.3 Build Configuration

Configure ETL for non-terminating error propagation via compile definitions:

```cmake
target_compile_definitions(someip PRIVATE
    ETL_LOG_ERRORS=1
    ETL_THROW_EXCEPTIONS=0
)
```

| Macro | Value | Effect |
|-------|-------|--------|
| `ETL_LOG_ERRORS` | `1` | Routes overflow and contract violations through the custom error handler with diagnostic logging. |
| `ETL_THROW_EXCEPTIONS` | `0` | Disables C++ exception throw on ETL errors; errors are returned as status codes instead. |

### 3.4 Rationale

This strategy aligns with ISO 26262 Part 6 clause 9.4.3 (defensive programming):
detectable errors are reported to the caller, internal container state remains
consistent, and the system can transition to a defined safe degradation mode
(e.g., drop incoming message, reject RPC, emit diagnostic event) rather than
terminating unpredictably.

## 4. FMEA Table

Severity scale used in this analysis:

| Level | Label | Description |
|-------|-------|-------------|
| **S4** | Critical | Loss of safety function or data corruption affecting vehicle behaviour |
| **S3** | Major | Service disruption, message loss, or stale data with detectable downstream impact |
| **S2** | Moderate | Degraded performance or transient communication failure with recovery path |
| **S1** | Minor | Benign failure with no impact when handled correctly |

| Failure Mode | Affected Component | Effect | Detection | Mitigation | Severity |
|--------------|-------------------|--------|-----------|------------|----------|
| **Message pool exhaustion** — `allocate_message()` returns `nullptr` when all `SOMEIP_STATIC_MESSAGE_POOL_SIZE` slots are in use | `src/platform/static/memory.cpp`, `include/platform/static/memory_impl.h` | Incoming or outgoing SOME/IP messages cannot be allocated. Transport receive loops drop data; RPC responses may fail; SD entries may not be processed. | Unit test `TC_STATIC_MSG_POOL_ALLOC` / `TC_BUFPOOL_EXHAUST`; runtime `nullptr` check at every `allocate_message()` call site; optional diagnostic log on allocation failure (REQ_PAL_MEM_EXHAUST_E01). | Size pool via CMake for peak concurrent in-flight messages; callers check `nullptr` and return `Result::RESOURCE_EXHAUSTED`; release messages promptly via intrusive refcount; integrator monitors allocation-failure counters. | **S3** |
| **Buffer pool tier 0 exhaustion** — all small-buffer slots in use; `acquire_buffer(size)` returns `nullptr` for requests fitting tier 0 | `include/platform/static/buffer_pool_impl.h`, `src/platform/static/buffer_pool.cpp` | Small payload serialization (SD entries, short RPC arguments, header parsing scratch) fails. Affected operations abort or skip processing. | Unit test `TC_BUFPOOL_EXHAUST`; `nullptr` return from `acquire_buffer()`; tier-selection logging in debug builds. | Configure `SOMEIP_STATIC_BUFPOOL_TIER_0_COUNT` and `SOMEIP_STATIC_BUFPOOL_TIER_0_SIZE` for expected small-buffer concurrency; callers handle `nullptr`; release buffers in RAII/defer paths; avoid holding small buffers across async boundaries. | **S2** |
| **Buffer pool tier 1 exhaustion** — all medium/UDP-buffer slots in use | `include/platform/static/buffer_pool_impl.h`, `src/platform/static/buffer_pool.cpp` | UDP datagram assembly, medium RPC payloads, and event notifications cannot obtain working buffers. UDP receive path may drop datagrams. | Unit test `TC_BUFPOOL_TIER_SELECT` and `TC_BUFPOOL_EXHAUST`; `nullptr` return when requested size maps to tier 1; transport-layer error counters. | Size tier 1 for peak concurrent UDP sessions and event fan-out; transport checks `acquire_buffer()` result before copy; TP layer falls back to error response rather than partial write. | **S3** |
| **Buffer pool tier 2 exhaustion** — all large/TCP/TP-buffer slots in use | `include/platform/static/buffer_pool_impl.h`, `src/platform/static/buffer_pool.cpp` | TCP stream reassembly, transport-protocol (TP) segmentation, and large RPC payloads fail. Multi-frame messages cannot be buffered; reassembly state may stall. | Unit test `TC_BUFPOOL_TIERED_ALLOC`; `nullptr` on tier 2 acquire; TP reassembler allocation-failure path (REQ in `transport_protocol.rst`: cancel oldest reassembly when pool full). | Configure `SOMEIP_STATIC_BUFPOOL_TIER_2_COUNT` for max concurrent large sessions; enforce TP reassembly slot limits; cancel oldest incomplete reassembly on exhaustion; callers propagate `Result::RESOURCE_EXHAUSTED`. | **S3** |
| **Container capacity overflow** — ETL-backed `StaticVector`, `StaticQueue`, `StaticUnorderedMap` insertion exceeds compile-time capacity | `include/platform/static/container_vector.h`, `container_queue.h`, `container_map.h` | Session table, subscription registry, or work queue cannot accept new entries. New subscriptions silently fail or existing state becomes inconsistent if error is ignored. | Unit test `TC_CONTAINER_CAPACITY_EXHAUST`; ETL custom error handler logs component + error code + file + line; PAL `push_back()` / `insert()` returns `false` or error `Result`. | Custom ETL error handler (§3) with `ETL_LOG_ERRORS=1`, `ETL_THROW_EXCEPTIONS=0`; size containers via `static_config.h` for peak entries; callers check return values; reject new sessions rather than overwrite. | **S3** |
| **String capacity overflow** — `StaticString<Capacity>` append exceeds fixed size; truncation or error | `include/platform/static/container_string.h` | Service names, endpoint identifiers, or diagnostic strings are truncated. Mismatched service lookup, incorrect endpoint routing, or incomplete log messages. | Unit test `TC_CONTAINER_STRING_APPEND`; `append()` returns `false` or reports truncated length; ETL error handler log on overflow attempt. | Set `Capacity` to maximum wire-format string length plus terminator; validate string length at API boundary before append; reject oversized identifiers with `Result::INVALID_ARGUMENT`. | **S2** |
| **Callback capture overflow** — callable stored in `StaticFunction<Signature, Capacity>` exceeds inline buffer | `include/platform/static/container_function.h` | **Compile-time failure** — callback registration cannot be expressed if lambda/functor object size exceeds `Capacity`. | `static_assert(sizeof(Callable) <= Capacity)` at template instantiation; build failure with clear diagnostic during integrator callback wiring. | Choose `Capacity` to accommodate largest registered callback (including capture size); prefer stateless function pointers or thin functors; document maximum capture size in integration guide. | **S1** (prevented at build time) |
| **Pimpl storage undersized** — `StaticPimpl<Impl, Size>` buffer smaller than `sizeof(Impl)` | `include/platform/static/pimpl.h`, public API headers (Transport, SD, SessionManager) | **Compile-time failure** — implementation object does not fit inline storage; build breaks rather than silent heap fallback. | `static_assert(sizeof(Impl) <= Size)` in `StaticPimpl`; unit test `TC_PIMPL_NO_HEAP` verifies construction without heap under interception. | Size `StaticPimpl` buffer with margin for Impl growth; run `test_pimpl_static.cpp` in CI on every release; review `sizeof(Impl)` when adding fields to pimpl classes. | **S1** (prevented at build time) |
| **Concurrent pool access race** — unsynchronized acquire/release corrupts pool bitmap or slot metadata | `src/platform/static/memory.cpp`, `src/platform/static/buffer_pool.cpp` | Double-allocation of same slot, use-after-free, pool metadata corruption, intermittent crashes or data corruption under multi-threaded load. | Unit tests `TC_BUFPOOL_CONCURRENT`, `TC_BUFPOOL_THREADSAFE`; ThreadSanitizer / stress tests on host; code review of mutex scope. | All pool acquire/release paths protected by PAL `Mutex` (REQ_PLATFORM_STATIC_004); mutex held for entire bitmap update and slot hand-off; no lock-free partial updates; single lock ordering (pool mutex only, no nested pool locks) prevents deadlock. **Safe because**: (1) every mutation of `block_used[]` / slot free-list occurs inside `Mutex::lock()` scope; (2) returned pointers are exclusive to the acquiring thread until `release_*()`; (3) `release_*()` validates pointer origin before returning slot to pool. | **S4** (if unmitigated); **S1** (with mutex — residual risk from priority inversion, see §5) |
| **IntrusivePtr ref count overflow** — `uint16_t` reference count reaches 65535 and wraps on increment | `include/platform/intrusive_ptr.h`, `include/common/message.h` | Wrapping causes premature return of `Message` to pool while references still exist (use-after-free), or permanent pool leak if count saturates without release. | Unit test `TC_INTRUSIVE_PTR_LIFETIME`; debug-build saturation assertion before increment; static analysis of `MessagePtr` copy sites. | Increment checks `refcount < UINT16_MAX` before `++`; on saturation, log error and refuse to create new reference (return error or no-op copy); document maximum concurrent references per message (65534); code review to avoid reference cycles and excessive copying. | **S4** (if wrap occurs); **S2** (with saturation check) |

## 5. Residual Risk Assessment

### 5.1 Mitigated to Acceptable Levels

| Area | Residual Risk | Justification |
|------|---------------|---------------|
| Compile-time failures (callback capture, pimpl sizing) | **Negligible** | `static_assert` prevents deployment of misconfigured builds. Failures occur during integration compile, not in the field. |
| Pool exhaustion (message and buffer tiers) | **Low–Moderate** | All exhaustion paths return `nullptr` or error codes without corruption. Risk remains that integrators under-size pools for their workload. Mitigated by CMake configurability, unit tests, and documented sizing guidance. Integrator responsibility per Assumptions of Use. |
| Container/string overflow | **Low–Moderate** | Custom ETL error handler prevents abort. Risk remains if callers ignore error return values. Mitigated by PAL wrappers returning `bool`/`Result` and CI tests. |
| Concurrent pool access | **Low** | Mutex serialization verified by concurrent unit tests. Residual risk: priority inversion if a low-priority task holds the pool mutex while a high-priority task waits. Integrators should assign SOME/IP worker thread priority per system design (ISO 26262 Part 6, clause 7.4.12). |
| Ref count overflow | **Low** | Saturation guard limits exposure. Residual risk exists if an application creates >65534 concurrent `MessagePtr` copies to a single message — impractical in normal SOME/IP workloads but must be documented. |

### 5.2 Remaining Integrator Responsibilities

1. **Pool sizing validation** — Static capacities must be validated against
   worst-case concurrent load on the target ECU (ISO 26262 Part 5 memory budget
   analysis). Undersized pools are the dominant residual hazard.

2. **Error-path testing** — Integrators shall verify that application-level
   handlers respond correctly to `Result::RESOURCE_EXHAUSTED` and allocation
   `nullptr` returns (ISO 26262 Part 6, clause 9.4.2 — error injection testing).

3. **ETL handler registration** — The custom error handler must be linked in
   safety builds. A build without the handler reverts to ETL assert behaviour
   (unacceptable for ASIL-rated deployment).

4. **Runtime monitoring** — Allocation-failure counters and ETL overflow logs
   should be connected to the ECU diagnostic framework so field undersizing is
   detectable before safety impact.

### 5.3 Overall Assessment

With the mitigations described in §3 and §4, the static-allocation lockstep
mode achieves **bounded, deterministic memory behaviour** suitable for
freedom-from-interference arguments under ISO 26262 Part 6.

The dominant residual risk is **integrator misconfiguration of pool and container
capacities**, which is an assumption-of-use item rather than a library defect.
All runtime exhaustion failure modes degrade gracefully when callers honour
return-value contracts.

**Recommended ASIL allocation**: Failure modes with handled `nullptr`/error
returns map to **QM–ASIL B** depending on the safety goal of the consuming
function. Compile-time prevented failures (callback capture, pimpl sizing) carry
no runtime residual risk.

---

*End of document FMEA-STATIC-ALLOC-001*
