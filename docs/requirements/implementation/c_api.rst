..
   Copyright (c) 2025 Vinicius Tadeu Zein

   See the NOTICE file(s) distributed with this work for additional
   information regarding copyright ownership.

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0

==============================
C API Requirements
==============================

This section defines requirements for the OpenSOMEIP C ABI, the stable
foreign-function-interface (FFI) boundary used by language bindings (Rust,
Python-future, etc.) and C-only consumers.

Overview
========

The C API provides:

1. Stable ``extern "C"`` ABI with no C++ types in the public surface
2. Opaque-handle ownership model with explicit create/destroy
3. Integer error codes matching ``someip::Result``
4. Exception firewall at every wrapper entry point
5. Callback support via function pointer + ``void* user_data``
6. Version macros and runtime version query

Requirements
============

ABI Stability
-------------

.. requirement:: Stable C ABI
   :id: REQ_CAPI_001
   :satisfies: feat_req_someip_2, feat_req_someip_3
   :status: pending
   :priority: high
   :verification: Compile a pure-C consumer (``.c`` file) against ``opensomeip.h``; link and run. No C++ symbols in the public header.

   The C API shall expose only ``extern "C"`` functions with POD parameter
   and return types (integers, pointers to opaque structs, caller-provided
   buffers). No C++ types (``std::string``, ``std::vector``, templates,
   classes) shall appear in the public header.

   **Rationale**: A stable C ABI is compiler- and language-agnostic, enabling
   FFI from Rust, Python, and other languages without C++ ABI coupling.

   **Code Location**: ``include/capi/opensomeip.h``

Error Model
-----------

.. requirement:: Integer Error Codes
   :id: REQ_CAPI_002
   :satisfies: REQ_ARCH_004
   :status: pending
   :priority: high
   :verification: Unit test that every ``someip::Result`` value round-trips through ``opensomeip_result_t``. Test that C++ exceptions thrown inside wrappers are caught and mapped to an error code.

   Every C API function shall return ``opensomeip_result_t`` (an integer enum)
   whose values match the ``someip::Result`` enum. No C++ exception shall
   propagate across the FFI boundary.

   **Rationale**: Integer return codes are the universal FFI error convention.
   Exceptions crossing FFI are undefined behaviour in mixed-language stacks.

   **Code Location**: ``include/capi/opensomeip.h``, ``src/capi/``

Ownership Model
---------------

.. requirement:: Explicit Create/Destroy Ownership
   :id: REQ_CAPI_003
   :satisfies: REQ_ARCH_003
   :status: pending
   :priority: high
   :verification: Unit test that every ``_create`` / ``_destroy`` pair does not leak (ASan). Test that double-destroy returns an error or is harmless.

   Resources exposed through the C API shall follow an explicit
   ``opensomeip_<type>_create`` / ``opensomeip_<type>_destroy`` lifecycle.
   The caller owns the handle and is responsible for calling destroy exactly
   once. Caller-provided buffers shall be documented as borrowed (not freed
   by the library).

   **Rationale**: Explicit ownership avoids hidden shared-state and
   double-free hazards across FFI.

   **Code Location**: ``include/capi/opensomeip.h``, ``src/capi/``

Callback Contract
-----------------

.. requirement:: Callback Safety
   :id: REQ_CAPI_004
   :status: pending
   :priority: high
   :verification: Unit test that a callback receiving ``user_data`` can access it. Test that a NULL callback is rejected gracefully. Integration test that callbacks do not re-enter destroy.

   Asynchronous notifications shall use a C function pointer plus a
   ``void* user_data`` context. Callbacks shall not unwind (no C++ exceptions
   or Rust panics). The library shall not call a callback after the
   corresponding handle has been destroyed.

   **Rationale**: Function pointer + userdata is the standard C callback
   idiom; it is FFI-safe and avoids closure captures.

   **Code Location**: ``include/capi/opensomeip.h``

Versioning
----------

.. requirement:: C API Versioning
   :id: REQ_CAPI_005
   :status: pending
   :priority: medium
   :verification: Unit test that ``opensomeip_capi_version()`` returns a value consistent with ``OPENSOMEIP_CAPI_VERSION_MAJOR/MINOR/PATCH`` macros.

   The C header shall define ``OPENSOMEIP_CAPI_VERSION_MAJOR``,
   ``OPENSOMEIP_CAPI_VERSION_MINOR``, ``OPENSOMEIP_CAPI_VERSION_PATCH``
   macros and a runtime ``opensomeip_capi_version()`` function. The version
   shall follow Semantic Versioning.

   **Rationale**: Consumers need compile-time and run-time version checks to
   detect ABI mismatches.

   **Code Location**: ``include/capi/opensomeip.h``

Thread Safety Documentation
---------------------------

.. requirement:: Thread Safety Documentation
   :id: REQ_CAPI_006
   :satisfies: REQ_ARCH_002
   :status: pending
   :priority: medium
   :verification: Inspection of header comments; each function documents its thread-safety guarantee.

   Every C API function shall document its thread-safety guarantee in the
   header comment (e.g., "thread-safe", "not thread-safe — caller must
   synchronize", "may be called from any thread").

   **Rationale**: Mixed-language consumers cannot infer C++ synchronization
   from the header alone.

   **Code Location**: ``include/capi/opensomeip.h``

Exception / Panic Firewall
--------------------------

.. requirement:: Exception Firewall
   :id: REQ_CAPI_007
   :satisfies: REQ_ARCH_004
   :status: pending
   :priority: high
   :verification: Unit test that a forced C++ exception inside a wrapped function does not propagate; the wrapper returns ``OPENSOMEIP_RESULT_INTERNAL_ERROR``.

   Every C wrapper function shall contain a ``try { … } catch (…)`` block
   (or equivalent) that converts any C++ exception to
   ``OPENSOMEIP_RESULT_INTERNAL_ERROR``. No exception or stack unwind shall
   cross the ``extern "C"`` boundary.

   **Rationale**: Unwinding across FFI is undefined behaviour (C++ → C, or
   C++ → Rust); the firewall guarantees defined behaviour.

   **Code Location**: ``src/capi/``

Per-Module C API Surface
------------------------

.. requirement:: Message and Serialization C API
   :id: REQ_CAPI_008
   :satisfies: REQ_ARCH_001
   :status: pending
   :priority: high
   :verification: Unit tests for message create/destroy, header get/set, payload copy, serializer round-trip.

   The C API shall expose message creation, header field access, payload
   get/set, and serializer/deserializer operations as ``extern "C"``
   functions operating on opaque handles.

   **Code Location**: ``include/capi/opensomeip.h``, ``src/capi/opensomeip_message.cpp``, ``src/capi/opensomeip_serializer.cpp``

.. requirement:: Transport C API
   :id: REQ_CAPI_009
   :satisfies: REQ_ARCH_001
   :status: pending
   :priority: high
   :verification: Unit tests for transport create/destroy, send/receive, endpoint configuration.

   The C API shall expose UDP and TCP transport handles with create, destroy,
   start, stop, send, and receive operations. The UDP constructor's potential
   exception shall be caught and returned as an error code (REQ_CAPI_007).

   **Code Location**: ``include/capi/opensomeip.h``, ``src/capi/opensomeip_transport.cpp``

.. requirement:: RPC C API
   :id: REQ_CAPI_010
   :satisfies: REQ_ARCH_001
   :status: pending
   :priority: high
   :verification: Unit tests for RPC client/server create/destroy, method registration, sync call, async callback.

   The C API shall expose RPC client and server handles. Synchronous calls
   shall block and return a result code. Asynchronous calls shall accept a
   C callback + userdata and invoke it on completion.

   **Code Location**: ``include/capi/opensomeip.h``, ``src/capi/opensomeip_rpc.cpp``

.. requirement:: Service Discovery C API
   :id: REQ_CAPI_011
   :satisfies: REQ_ARCH_001
   :status: pending
   :priority: high
   :verification: Unit tests for SD client/server create/destroy, offer/find, subscribe callbacks.

   The C API shall expose SD client and server handles with offer, find,
   subscribe, and unsubscribe operations mapped to C callbacks.

   **Code Location**: ``include/capi/opensomeip.h``, ``src/capi/opensomeip_sd.cpp``

.. requirement:: Events C API
   :id: REQ_CAPI_012
   :satisfies: REQ_ARCH_001
   :status: pending
   :priority: high
   :verification: Unit tests for event publisher/subscriber create/destroy, register/unregister, publish, subscribe callback.

   The C API shall expose event publisher and subscriber handles with
   register, unregister, publish, and subscribe operations.

   **Code Location**: ``include/capi/opensomeip.h``, ``src/capi/opensomeip_events.cpp``

.. requirement:: TP and E2E C API
   :id: REQ_CAPI_013
   :satisfies: REQ_ARCH_001
   :status: pending
   :priority: medium
   :verification: Unit tests for TP segment/reassemble and E2E protect/check on byte buffers.

   The C API shall expose TP segmentation/reassembly and E2E
   protect/check as functions operating on caller-provided byte buffers.

   **Code Location**: ``include/capi/opensomeip.h``, ``src/capi/opensomeip_tp.cpp``, ``src/capi/opensomeip_e2e.cpp``

Traceability
============

Implementation Files
--------------------

* ``include/capi/opensomeip.h`` — Public C API header
* ``src/capi/opensomeip_message.cpp`` — Message wrapper
* ``src/capi/opensomeip_serializer.cpp`` — Serializer wrapper
* ``src/capi/opensomeip_transport.cpp`` — Transport wrapper
* ``src/capi/opensomeip_rpc.cpp`` — RPC wrapper
* ``src/capi/opensomeip_sd.cpp`` — SD wrapper
* ``src/capi/opensomeip_events.cpp`` — Events wrapper
* ``src/capi/opensomeip_tp.cpp`` — TP wrapper
* ``src/capi/opensomeip_e2e.cpp`` — E2E wrapper

Test Files
----------

* ``tests/capi/unit/test_capi_message.cpp``
* ``tests/capi/unit/test_capi_serializer.cpp``
* ``tests/capi/unit/test_capi_transport.cpp``
* ``tests/capi/unit/test_capi_rpc.cpp``
* ``tests/capi/unit/test_capi_sd.cpp``
* ``tests/capi/unit/test_capi_events.cpp``
* ``tests/capi/unit/test_capi_tp.cpp``
* ``tests/capi/unit/test_capi_e2e.cpp``
* ``tests/capi/integration/test_capi_udp_hello.cpp``
* ``tests/capi/integration/test_capi_rpc_roundtrip.cpp``
