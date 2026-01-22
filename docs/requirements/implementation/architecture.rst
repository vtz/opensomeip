..
   Copyright (c) 2025 Vinicius Tadeu Zein

   See the NOTICE file(s) distributed with this work for additional
   information regarding copyright ownership.

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0

==============================
Architecture Requirements
==============================

This section defines architectural requirements for the OpenSOMEIP
implementation, covering modularity, thread safety, and memory management.

Overview
========

The OpenSOMEIP architecture is designed with:

1. Modular layered architecture
2. Thread-safe operations
3. Safe memory management
4. Clear separation of concerns

Requirements
============

Modularity
----------

.. requirement:: Modular Architecture
   :id: REQ_ARCH_001
   :status: implemented
   :priority: high

   The implementation shall follow a modular layered architecture:

   * **Core Layer** (``someip-common``): Message structures, serialization
   * **Transport Layer** (``someip-transport``): UDP/TCP transports
   * **Protocol Layer**: SD, TP, RPC, Events
   * **Safety Layer** (``someip-e2e``): E2E protection

   Each layer shall:

   * Have clearly defined interfaces
   * Depend only on lower layers
   * Be independently testable
   * Be replaceable without affecting other layers

   **Rationale**: Modularity enables maintainability, testability,
   and selective deployment.

   **Code Location**: ``CMakeLists.txt``, ``src/CMakeLists.txt``

Thread Safety
-------------

.. requirement:: Thread Safety
   :id: REQ_ARCH_002
   :status: implemented
   :priority: high

   The implementation shall be thread-safe:

   * Shared data structures protected by appropriate synchronization
   * Session manager thread-safe for concurrent requests
   * Transport operations safe for multi-threaded use
   * No data races in normal operation
   * Document thread-safety guarantees in API documentation

   **Rationale**: Automotive applications require concurrent
   operation without data corruption.

   **Code Location**: All source files

Memory Management
-----------------

.. requirement:: Safe Memory Management
   :id: REQ_ARCH_003
   :status: implemented
   :priority: high

   The implementation shall use safe memory management:

   * RAII (Resource Acquisition Is Initialization) patterns
   * Smart pointers (``std::unique_ptr``, ``std::shared_ptr``)
   * No raw pointer ownership
   * Bounds checking on all array/vector access
   * No memory leaks in normal operation

   **Rationale**: Memory safety is critical for reliability and
   aligns with safety-oriented design goals.

   **Code Location**: All source files

Error Handling
--------------

.. requirement:: Consistent Error Handling
   :id: REQ_ARCH_004
   :status: implemented
   :priority: high

   The implementation shall use consistent error handling:

   * Return ``Result`` enum for operation status
   * No exceptions in core protocol code
   * Validate all external inputs
   * Implement safe failure modes
   * Log errors with sufficient context

   **Rationale**: Consistent error handling improves reliability
   and debuggability.

   **Code Location**: ``include/common/result.h``

Coding Standards
----------------

.. requirement:: Coding Standards Compliance
   :id: REQ_ARCH_005
   :status: implemented
   :priority: medium

   The implementation shall follow coding standards:

   * C++17 standard
   * Naming conventions per ``docs/CODING_GUIDELINES.md``
   * Clang-format for consistent formatting
   * Clang-tidy for static analysis
   * Doxygen comments for public APIs

   **Rationale**: Consistent coding standards improve readability
   and maintainability.

   **Code Location**: ``.clang-format``, ``.clang-tidy``

Build System
------------

.. requirement:: CMake Build System
   :id: REQ_ARCH_006
   :status: implemented
   :priority: medium

   The implementation shall use CMake for build configuration:

   * CMake 3.20+ required
   * Support for multiple compilers (GCC, Clang, MSVC)
   * Optional features via CMake options
   * Export compile commands for IDE integration
   * Install targets for system installation

   **Rationale**: CMake provides cross-platform build configuration.

   **Code Location**: ``CMakeLists.txt``

Testing Infrastructure
----------------------

.. requirement:: Comprehensive Testing
   :id: REQ_ARCH_007
   :status: implemented
   :priority: high

   The implementation shall include comprehensive testing:

   * Unit tests using Google Test
   * Integration tests using pytest
   * Code coverage reporting
   * Continuous integration via GitHub Actions
   * Test traceability to requirements

   **Rationale**: Testing ensures correctness and prevents regressions.

   **Code Location**: ``tests/``

Traceability
============

Implementation Files
--------------------

* ``CMakeLists.txt`` - Main build configuration
* ``src/CMakeLists.txt`` - Source build configuration
* ``include/common/result.h`` - Error handling
* ``.clang-format`` - Formatting configuration
* ``.clang-tidy`` - Static analysis configuration
* ``docs/CODING_GUIDELINES.md`` - Coding standards

Test Files
----------

* ``tests/CMakeLists.txt`` - Test configuration
* ``tests/*.cpp`` - Unit tests
* ``tests/integration/*.py`` - Integration tests
* ``tests/system/*.py`` - System tests
