..
   Copyright (c) 2025 Vinicius Tadeu Zein

   See the NOTICE file(s) distributed with this work for additional
   information regarding copyright ownership.

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0

======================================
Platform Abstraction Requirements
======================================

This section defines requirements for the platform abstraction layer,
which enables OpenSOMEIP to run on multiple operating systems and RTOSes.

Overview
========

The platform abstraction layer provides portable interfaces for:

1. Threading (Thread, Mutex, ConditionVariable, sleep_for)
2. Memory management (Message pool allocation)
3. Networking (socket API)
4. Byte-order conversion

Each platform backend implements these interfaces in a ``*_impl.h``
header selected at build time via CMake include-path configuration.

Requirements
============

FreeRTOS Threading
------------------

.. requirement:: FreeRTOS Threading Backend
   :id: REQ_PLATFORM_FREERTOS_001
   :status: implemented
   :priority: high
   :verification: Compilation with SOMEIP_USE_FREERTOS=ON; runtime tests for Thread join, Mutex lock/unlock, ConditionVariable notify/wait, and sleep_for timing.

   The implementation shall provide threading primitives for FreeRTOS:

   * ``Mutex`` wrapping ``xSemaphoreCreateMutex()``
   * ``ConditionVariable`` using a counting semaphore
   * ``Thread`` wrapping ``xTaskCreate()`` with join support
   * ``ScopedLock`` (RAII, platform-independent)
   * ``this_thread::sleep_for`` via ``vTaskDelay()``

   **Rationale**: FreeRTOS is one of the most widely used RTOSes in
   automotive embedded systems.

   **Code Location**: ``include/platform/freertos/thread_impl.h``

FreeRTOS Memory Pool
--------------------

.. requirement:: FreeRTOS Memory Pool Backend
   :id: REQ_PLATFORM_FREERTOS_002
   :status: implemented
   :priority: high
   :verification: Pool allocation until exhaustion returns nullptr; release and re-allocate succeeds; no memory corruption under concurrent access.

   The implementation shall provide a static pool allocator for Message
   objects on FreeRTOS:

   * Fixed-size pool (configurable via ``SOMEIP_FREERTOS_MESSAGE_POOL_SIZE``)
   * ``alignas(Message)`` alignment on pool buffer
   * Mutex-protected alloc/release
   * Returns ``nullptr`` on pool exhaustion

   **Rationale**: Embedded targets require deterministic memory allocation
   without heap fragmentation.

   **Code Location**: ``include/platform/freertos/memory_impl.h``,
   ``src/platform/freertos/memory.cpp``

lwIP Networking
---------------

.. requirement:: lwIP Networking Backend
   :id: REQ_PLATFORM_LWIP_001
   :status: implemented
   :priority: high
   :verification: Transport compiles with SOMEIP_USE_LWIP=ON; socket calls resolve to lwIP; TCP/UDP loopback test on hardware or QEMU.

   The implementation shall provide a networking backend using lwIP's
   socket API:

   * Socket creation, binding, listen, accept, connect
   * UDP sendto/recvfrom and TCP send/recv
   * Non-blocking I/O via ``lwip_fcntl()``
   * Byte-order conversion via ``lwip/def.h``
   * Conditional macro mapping when ``LWIP_COMPAT_SOCKETS`` is disabled

   **Rationale**: lwIP is the standard TCP/IP stack for FreeRTOS and
   ThreadX on microcontrollers.

   **Code Location**: ``include/platform/lwip/net_impl.h``,
   ``include/platform/lwip/byteorder_impl.h``

ThreadX Threading
------------------

.. requirement:: ThreadX Threading Backend
   :id: REQ_PLATFORM_THREADX_001
   :status: implemented
   :priority: high
   :verification: Compilation with SOMEIP_USE_THREADX=ON; runtime tests for Thread join, Mutex lock/unlock, ConditionVariable notify/wait, and sleep_for timing.

   The implementation shall provide threading primitives for ThreadX:

   * ``Mutex`` wrapping ``TX_MUTEX`` with ``TX_INHERIT`` for priority inheritance
   * ``ConditionVariable`` using ``TX_EVENT_FLAGS_GROUP`` (flag bit 0)
   * ``Thread`` wrapping ``tx_thread_create()`` with static stack and join support
   * ``ScopedLock`` (RAII, platform-independent)
   * ``this_thread::sleep_for`` via ``tx_thread_sleep()``

   **Rationale**: ThreadX (Azure RTOS) is widely used in automotive and
   industrial embedded systems, particularly on NXP and STM32 platforms.

   **Code Location**: ``include/platform/threadx/thread_impl.h``

ThreadX Memory Pool
--------------------

.. requirement:: ThreadX Memory Pool Backend
   :id: REQ_PLATFORM_THREADX_002
   :status: implemented
   :priority: high
   :verification: Pool allocation until exhaustion returns nullptr; release and re-allocate succeeds; no memory corruption under concurrent access.

   The implementation shall provide a static pool allocator for Message
   objects on ThreadX:

   * ``TX_BLOCK_POOL`` backed by a static buffer
   * Fixed-size pool (configurable via ``SOMEIP_THREADX_MESSAGE_POOL_SIZE``)
   * Lazy initialization guarded by ``TX_MUTEX``
   * Returns ``nullptr`` on pool exhaustion

   **Rationale**: Embedded targets require deterministic memory allocation
   without heap fragmentation.

   **Code Location**: ``include/platform/threadx/memory_impl.h``,
   ``src/platform/threadx/memory.cpp``

Platform Abstraction Architecture
----------------------------------

.. requirement:: Platform Abstraction Layer Architecture
   :id: REQ_PLATFORM_ARCH_001
   :status: implemented
   :priority: high
   :verification: No #ifdef in public platform headers; build system selects backend via include path; host and Zephyr builds pass unchanged.

   The platform abstraction shall use a build-system-selected include-path
   mechanism:

   * Public headers (``thread.h``, ``memory.h``, ``net.h``, ``byteorder.h``)
     contain only common code and ``#include "*_impl.h"``
   * No ``#if``/``#elif``/``#ifdef`` for platform detection in public headers
   * Each backend lives in its own directory under ``include/platform/``
   * CMake sets the include path to select the correct backend
   * Adding a new platform = adding a directory + a CMake ``if()`` block

   **Rationale**: Eliminates unmaintainable ``#ifdef`` chains and makes
   the platform layer extensible for future backends (ThreadX, OpenBSW).

   **Code Location**: ``include/platform/``, ``CMakeLists.txt``,
   ``src/CMakeLists.txt``
