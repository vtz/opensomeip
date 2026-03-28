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

This section defines requirements for the platform abstraction layer (PAL),
which enables OpenSOMEIP to run on multiple operating systems and RTOSes.

Overview
========

The platform abstraction layer provides portable interfaces for:

1. Threading (Thread, Mutex, ConditionVariable, ScopedLock, sleep_for)
2. Memory management (Message pool allocation)
3. Networking (socket API abstraction)
4. Byte-order conversion

Each platform backend implements these interfaces in a ``*_impl.h``
header selected at build time via CMake include-path configuration.

PAL Architecture
================

.. requirement:: Platform Abstraction Layer Architecture
   :id: REQ_PLATFORM_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Verify no ``#ifdef`` in public platform headers (thread.h, memory.h, net.h, byteorder.h). Verify build system selects backend via include path. Verify host and Zephyr builds compile unchanged.

   The platform abstraction shall use a build-system-selected include-path
   mechanism:

   * Public headers (``thread.h``, ``memory.h``, ``net.h``, ``byteorder.h``)
     contain only common code and ``#include "*_impl.h"``
   * No ``#if``/``#elif``/``#ifdef`` for platform detection in public headers
   * Each backend lives in its own directory under ``include/platform/``
   * CMake sets the include path to select the correct backend
   * Adding a new platform = adding a directory + a CMake ``if()`` block

   **Rationale**: Eliminates unmaintainable ``#ifdef`` chains and makes
   the platform layer extensible for future backends.

   **Code Location**: ``include/platform/``, ``CMakeLists.txt``,
   ``src/CMakeLists.txt``

PAL Interface Contracts
=======================

Mutex Interface
---------------

.. requirement:: PAL Mutex Lock
   :id: REQ_PAL_MUTEX_LOCK
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Thread A calls lock(), Thread B calls try_lock() and verifies it returns false while A holds the lock. Thread A calls unlock(), Thread B retries try_lock() and verifies it returns true.

   ``Mutex::lock()`` shall acquire exclusive ownership of the mutex. If
   another thread holds the mutex, the caller shall block until ownership
   is available.

   **Rationale**: Exclusive locking is the fundamental synchronization
   primitive used by SD, TP, and transport layers.

   **Code Location**: ``include/platform/posix/thread_impl.h``,
   ``include/platform/freertos/thread_impl.h``,
   ``include/platform/threadx/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: PAL Mutex Unlock
   :id: REQ_PAL_MUTEX_UNLOCK
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Thread A calls lock() then unlock(). Thread B immediately calls try_lock() and verifies it returns true (mutex is released).

   ``Mutex::unlock()`` shall release exclusive ownership of the mutex,
   allowing other threads blocked on ``lock()`` to proceed.

   **Rationale**: Timely release prevents deadlock and ensures progress.

   **Code Location**: ``include/platform/posix/thread_impl.h``,
   ``include/platform/freertos/thread_impl.h``,
   ``include/platform/threadx/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: PAL Mutex Try Lock
   :id: REQ_PAL_MUTEX_TRYLOCK
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Call try_lock() on an unheld mutex — returns true. Call try_lock() again from a second thread while the first holds it — returns false. After unlock, the second thread's try_lock() succeeds.

   ``Mutex::try_lock()`` shall attempt to acquire exclusive ownership
   without blocking. It shall return ``true`` if ownership was acquired
   and ``false`` otherwise.

   **Rationale**: Non-blocking acquisition is needed for lock-free polling
   patterns and diagnostic probes.

   **Code Location**: ``include/platform/posix/thread_impl.h``,
   ``include/platform/freertos/thread_impl.h``,
   ``include/platform/threadx/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: PAL Mutex Non-Copyable
   :id: REQ_PAL_MUTEX_NONCOPY
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Static analysis: Verify Mutex copy constructor, copy-assignment operator, move constructor (``Mutex::Mutex(Mutex&&)``), and move-assignment operator (``Mutex::operator=(Mutex&&)``) are all ``= delete``. Verify code that attempts to copy or move a Mutex does not compile.

   ``Mutex`` shall be non-copyable and non-movable. Copy constructor,
   copy-assignment, move constructor, and move-assignment shall be deleted.

   **Rationale**: Mutex identity is tied to its OS resource handle; copying
   would create dangling or aliased handles.

   **Code Location**: ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: Error - Mutex Double Unlock
   :id: REQ_PAL_MUTEX_UNLOCK_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Construct Mutex, call lock() then unlock() twice in sequence. Verify no crash or deadlock (behavior is platform-defined but must not corrupt state).

   The Mutex implementation shall not corrupt internal state if ``unlock()``
   is called without a matching ``lock()``.

   **Rationale**: Double-unlock is a common programming error; the PAL must
   not crash or corrupt state.

   **Error Handling**: Platform-defined (POSIX: undefined; FreeRTOS:
   xSemaphoreGive returns pdFAIL; Zephyr: k_mutex_unlock returns -EPERM).

   **Code Location**: ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

ConditionVariable Interface
---------------------------

.. requirement:: PAL ConditionVariable Wait
   :id: REQ_PAL_CV_WAIT
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Thread A holds Mutex and calls wait(Mutex). Thread B calls notify_one(). Verify A unblocks and still holds the Mutex after wait() returns.

   ``ConditionVariable::wait(Mutex& m)`` shall atomically release ``m``
   and suspend the calling thread until a notification is received. Before
   returning, it shall re-acquire ``m``.

   **Rationale**: Atomic release-and-wait is the fundamental building block
   for producer/consumer patterns in SD and event delivery.

   **Code Location**: ``include/platform/host/host_condition_variable.h``,
   ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: PAL ConditionVariable Wait with Predicate
   :id: REQ_PAL_CV_WAIT_PRED
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Thread A calls wait(Mutex, [&]{ return flag; }). Thread B sets flag=true under Mutex and calls notify_one(). Verify A wakes up only when flag is true. Verify spurious wakeups (if any) do not cause early return.

   ``ConditionVariable::wait(Mutex& m, Pred pred)`` shall loop internally,
   re-checking ``pred()`` on each wakeup, and return only when ``pred()``
   returns ``true``. The Mutex shall be held on return.

   **Rationale**: Predicate-based wait eliminates spurious-wakeup bugs at
   the API level.

   **Code Location**: ``include/platform/host/host_condition_variable.h``,
   ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: PAL ConditionVariable Notify One
   :id: REQ_PAL_CV_NOTIFY_ONE
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Spawn 4 waiters. Call notify_one(). Verify exactly 1 waiter wakes (use a permit counter guarded by Mutex to prevent spurious-wakeup false positives).

   ``ConditionVariable::notify_one()`` shall wake exactly one thread
   that is blocked in ``wait()``. If no threads are waiting, the
   notification shall be a no-op.

   **Rationale**: Single-wake avoids thundering-herd effects on SD
   message dispatch.

   **Code Location**: ``include/platform/host/host_condition_variable.h``,
   ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: PAL ConditionVariable Notify All
   :id: REQ_PAL_CV_NOTIFY_ALL
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Spawn 4 waiters. Call notify_all(). Verify all 4 waiters wake and proceed.

   ``ConditionVariable::notify_all()`` shall wake all threads that are
   blocked in ``wait()``.

   **Rationale**: Broadcast wake is needed for shutdown sequences where
   all worker threads must unblock.

   **Code Location**: ``include/platform/host/host_condition_variable.h``,
   ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: PAL ConditionVariable Mutex Ownership Guarantee
   :id: REQ_PAL_CV_OWNERSHIP
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: After wait() returns (with or without predicate), probe the mutex from another thread via try_lock(). Verify try_lock() returns false (caller still holds the mutex).

   On all exit paths from ``wait()`` — normal return, predicate
   satisfaction, or exception propagation — the caller shall still hold
   the Mutex that was passed to ``wait()``.

   **Rationale**: Mutex ownership loss after wait leads to data races that
   are extremely hard to diagnose.

   **Code Location**: ``include/platform/host/host_condition_variable.h``

.. requirement:: Error - ConditionVariable Exception Safety
   :id: REQ_PAL_CV_EXCEPT_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Call wait(Mutex, pred) where pred throws an exception after being signaled. Verify the caller still owns the Mutex after the exception propagates.

   If the predicate throws during ``wait(Mutex&, Pred)``, the caller
   shall still own the Mutex when the exception propagates.

   **Rationale**: Exception-unsafe CV wrappers cause mutex ownership loss,
   leading to data races.

   **Error Handling**: RAII guard calls ``lk.release()`` on all exit paths.

   **Code Location**: ``include/platform/host/host_condition_variable.h``

Thread Interface
----------------

.. requirement:: PAL Thread Creation
   :id: REQ_PAL_THREAD_CREATE
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct Thread with a lambda that sets an atomic flag. Verify the flag is set after join(). Construct Thread with a function and two int arguments, verify they are forwarded correctly.

   ``Thread`` shall be constructible from a callable (function, lambda,
   or functor) with optional arguments. Execution shall begin immediately
   upon construction.

   **Rationale**: Immediate-start semantics match ``std::thread`` and
   simplify worker-thread creation in SD and transport layers.

   **Code Location**: ``include/platform/posix/thread_impl.h``,
   ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: PAL Thread Joinable Query
   :id: REQ_PAL_THREAD_JOINABLE
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: After construction, verify joinable() returns true. After join(), verify joinable() returns false. For a default-constructed Thread, verify joinable() returns false.

   ``Thread::joinable()`` shall return ``true`` if the thread has been
   started and has not yet been joined, and ``false`` otherwise.

   **Rationale**: Callers must check joinability before calling join() to
   avoid double-join errors.

   **Code Location**: ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: PAL Thread Join
   :id: REQ_PAL_THREAD_JOIN
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Start a Thread that sleeps for 50ms. Call join(). Verify the caller blocked for at least 40ms. Call join() a second time — verify it is a no-op and does not crash.

   ``Thread::join()`` shall block the calling thread until the target
   thread completes execution. The call shall be idempotent: calling
   ``join()`` on an already-joined Thread shall be a safe no-op.

   **Rationale**: Join is the primary mechanism for safe thread shutdown
   in transport and SD layers.

   **Code Location**: ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: PAL Thread Non-Copyable
   :id: REQ_PAL_THREAD_NONCOPY
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Static analysis: Verify Thread copy/move constructors and assignment operators are ``= delete``.

   ``Thread`` shall be non-copyable and non-movable.

   **Rationale**: Thread identity is tied to an OS thread handle; copying
   would create invalid aliases.

   **Code Location**: ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: Error - Thread Creation Failure
   :id: REQ_PAL_THREAD_CREATE_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test (RTOS only): Exhaust thread/stack resources, attempt to create a Thread. Verify joinable() returns false and join() is a no-op (no crash).

   If the underlying OS fails to create a thread (e.g., stack exhaustion
   on FreeRTOS, thread limit on Zephyr), the Thread object shall be in a
   non-joinable state and ``join()`` shall be a safe no-op.

   **Rationale**: Embedded systems have limited thread resources; failure
   must be handled gracefully.

   **Error Handling**: ``started_`` remains ``false``; resources are freed
   in the constructor error path.

   **Code Location**: ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: Error - Thread Destructor Without Join
   :id: REQ_PAL_THREAD_DTOR_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Construct Thread, let it go out of scope without calling join(). Verify no resource leak and no crash (destructor aborts the task).

   The Thread destructor shall safely abort a running thread if
   ``joinable()`` is ``true`` at destruction time, preventing resource leaks.

   **Rationale**: Forgetting to join is a common error; the destructor must
   clean up without undefined behavior.

   **Error Handling**: Call platform-specific abort (vTaskDelete, k_thread_abort)
   and free context.

   **Code Location**: ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

ScopedLock Interface
--------------------

.. requirement:: PAL ScopedLock Acquisition on Construction
   :id: REQ_PAL_LOCK_ACQUIRE
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct ScopedLock with a Mutex. From another thread, call try_lock() — verify it returns false (mutex is held by ScopedLock).

   The ``ScopedLock`` constructor shall call ``Mutex::lock()`` to acquire
   exclusive ownership of the associated mutex.

   **Rationale**: Automatic acquisition on construction ensures the
   critical section begins at the point of declaration.

   **Code Location**: ``include/platform/thread.h``

.. requirement:: PAL ScopedLock Release on Destruction
   :id: REQ_PAL_LOCK_RELEASE
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Construct ScopedLock in a nested scope. After the scope exits, call try_lock() from another thread — verify it returns true (mutex is released).

   The ``ScopedLock`` destructor shall call ``Mutex::unlock()`` to release
   exclusive ownership of the associated mutex.

   **Rationale**: Automatic release on scope exit prevents mutex leaks on
   early returns and exceptions.

   **Code Location**: ``include/platform/thread.h``

.. requirement:: PAL ScopedLock Non-Copyable
   :id: REQ_PAL_LOCK_NONCOPY
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Static analysis: Verify ScopedLock copy constructor and copy-assignment operator are ``= delete``.

   ``ScopedLock`` shall be non-copyable and non-assignable.

   **Rationale**: Copying a lock guard would create double-unlock on
   destruction.

   **Code Location**: ``include/platform/thread.h``

sleep_for Interface
-------------------

.. requirement:: PAL sleep_for Minimum Duration
   :id: REQ_PAL_SLEEP_DURATION
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Record time, call sleep_for(50ms), record time. Verify elapsed >= 40ms (lower bound accounting for timer resolution). Verify elapsed <= 500ms (sanity upper bound).

   ``someip::platform::this_thread::sleep_for(duration)`` shall block the
   calling thread for **at least** the specified duration. The actual
   sleep time may be longer due to scheduling jitter.

   **Rationale**: SD timers and TP reassembly timeout rely on sleep_for
   providing a minimum delay guarantee.

   **Code Location**: ``include/platform/posix/thread_impl.h``,
   ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

.. requirement:: PAL sleep_for Zero Duration
   :id: REQ_PAL_SLEEP_ZERO
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Call sleep_for(0ms). Verify it returns within 50ms (effectively immediate).

   ``sleep_for`` with a zero or negative duration shall return immediately
   without blocking.

   **Rationale**: Prevents accidental infinite blocks when computed
   durations are zero or negative.

   **Code Location**: ``include/platform/freertos/thread_impl.h``,
   ``include/platform/zephyr/thread_impl.h``

Memory Interface
----------------

.. requirement:: PAL Memory Allocation
   :id: REQ_PAL_MEM_ALLOC
   :satisfies: REQ_ARCH_001, REQ_ARCH_003
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Call allocate_message(), verify result is non-null. Set service_id and payload, verify both roundtrip correctly.

   ``someip::platform::allocate_message()`` shall return a ``MessagePtr``
   (``std::shared_ptr<Message>``) owning a fully constructed, usable
   ``Message`` object.

   **Rationale**: Decouples message lifetime management from the platform
   memory strategy (heap vs. pool).

   **Code Location**: ``include/platform/memory.h``,
   ``include/platform/posix/memory_impl.h``,
   ``include/platform/freertos/memory_impl.h``,
   ``include/platform/threadx/memory_impl.h``,
   ``include/platform/zephyr/memory_impl.h``

.. requirement:: PAL Memory Independence
   :id: REQ_PAL_MEM_INDEPENDENT
   :satisfies: REQ_ARCH_001, REQ_ARCH_003
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Call allocate_message() twice. Set different service_ids on each. Verify each retains its own value and the pointers are distinct.

   Multiple calls to ``allocate_message()`` shall return independent
   ``Message`` objects. Modifying one shall not affect the other.

   **Rationale**: Protocol layers allocate messages concurrently; aliased
   objects would cause data corruption.

   **Code Location**: ``include/platform/posix/memory_impl.h``,
   ``include/platform/freertos/memory_impl.h``

.. requirement:: Error - Memory Pool Exhaustion
   :id: REQ_PAL_MEM_EXHAUST_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test (RTOS): Allocate messages until allocate_message() returns nullptr. Release one message. Allocate again — verify non-null. Verify no memory corruption.

   When the platform's message pool is exhausted,
   ``allocate_message()`` shall return ``nullptr`` without
   crashing or corrupting the pool.

   **Rationale**: Embedded pools have fixed capacity; callers must handle
   exhaustion gracefully.

   **Error Handling**: Return ``nullptr``; pool internals remain consistent.

   **Code Location**: ``include/platform/freertos/memory_impl.h``,
   ``include/platform/threadx/memory_impl.h``,
   ``src/platform/zephyr/memory.cpp``

.. requirement:: Error - Memory Pool Thread Safety
   :id: REQ_PAL_MEM_THREADSAFE_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test (RTOS): Spawn 4 threads, each allocating and releasing 100 messages concurrently. Verify no corruption, no deadlock, and all messages freed.

   Pool-based ``allocate_message()`` implementations shall be thread-safe:
   concurrent allocations and releases shall not corrupt the pool or
   deadlock.

   **Rationale**: Transport and SD layers allocate messages from different
   threads.

   **Error Handling**: Mutex-protected allocation path.

   **Code Location**: ``include/platform/freertos/memory_impl.h``,
   ``include/platform/threadx/memory_impl.h``,
   ``src/platform/zephyr/memory.cpp``

Networking Interface
--------------------

.. requirement:: PAL Socket Close
   :id: REQ_PAL_NET_CLOSE
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Create a UDP socket, call someip_close_socket(fd). Verify subsequent operations on fd fail (EBADF).

   ``someip_close_socket(fd)`` shall close the socket descriptor and
   release the associated OS resources.

   **Rationale**: Portable socket close is needed because POSIX uses
   ``close()`` while Win32 uses ``closesocket()``.

   **Code Location**: ``include/platform/posix/net_impl.h``,
   ``include/platform/win32/net_impl.h``,
   ``include/platform/lwip/net_impl.h``,
   ``include/platform/zephyr/net_impl.h``

.. requirement:: PAL Socket Shutdown
   :id: REQ_PAL_NET_SHUTDOWN
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Create a TCP socket, call someip_shutdown_socket(fd). Verify the socket is no longer readable or writable.

   ``someip_shutdown_socket(fd)`` shall shutdown both read and write
   directions on the socket (equivalent to ``SHUT_RDWR`` / ``SD_BOTH``).

   **Rationale**: Graceful TCP shutdown requires separate shutdown before
   close.

   **Code Location**: ``include/platform/posix/net_impl.h``,
   ``include/platform/win32/net_impl.h``,
   ``include/platform/lwip/net_impl.h``,
   ``include/platform/zephyr/net_impl.h``

.. requirement:: PAL Socket Set Non-Blocking
   :id: REQ_PAL_NET_NONBLOCK
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Create socket, call someip_set_nonblocking(fd), verify return is 0 (success). Call recv() on empty socket, verify it returns immediately with EAGAIN/EWOULDBLOCK instead of blocking.

   ``someip_set_nonblocking(fd)`` shall set the socket to non-blocking
   mode and return 0 on success.

   **Rationale**: Non-blocking I/O is required for event-driven transport
   receive loops.

   **Code Location**: ``include/platform/posix/net_impl.h``,
   ``include/platform/win32/net_impl.h``,
   ``include/platform/zephyr/net_impl.h``

.. requirement:: PAL Socket Set Blocking
   :id: REQ_PAL_NET_BLOCK
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Create socket, call someip_set_nonblocking(fd), then someip_set_blocking(fd). Verify return is 0 (success). Verify the socket is back in blocking mode.

   ``someip_set_blocking(fd)`` shall set the socket back to blocking mode
   and return 0 on success.

   **Rationale**: Some operations (e.g., TCP connect) benefit from blocking
   mode with timeout.

   **Code Location**: ``include/platform/posix/net_impl.h``,
   ``include/platform/win32/net_impl.h``,
   ``include/platform/zephyr/net_impl.h``

.. requirement:: Error - Socket Mode Change Failure
   :id: REQ_PAL_NET_MODE_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Call someip_set_nonblocking(-1) and someip_set_blocking(-1). Verify both return -1 (failure on invalid fd).

   ``someip_set_nonblocking()`` and ``someip_set_blocking()`` shall return
   ``-1`` when the underlying system call fails (e.g., invalid fd).

   **Rationale**: Transport layers check the return value to detect
   invalid socket states.

   **Error Handling**: Return ``-1``; do not modify socket state.

   **Code Location**: ``include/platform/posix/net_impl.h``,
   ``include/platform/win32/net_impl.h``,
   ``include/platform/zephyr/net_impl.h``

Byte-Order Interface
--------------------

.. requirement:: PAL Host-to-Network 16-bit Conversion
   :id: REQ_PAL_BYTE_HTONS
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Call someip_htons(0x0102). Verify the first byte of the result is 0x01 and the second is 0x02 (big-endian wire format).

   ``someip_htons(x)`` shall convert a 16-bit value from host byte order
   to network byte order (big-endian).

   **Rationale**: SOME/IP Service ID, Method ID, and Length fields are
   16-bit big-endian on the wire.

   **Code Location**: ``include/platform/posix/byteorder_impl.h``,
   ``include/platform/win32/byteorder_impl.h``,
   ``include/platform/lwip/byteorder_impl.h``,
   ``include/platform/zephyr/byteorder_impl.h``

.. requirement:: PAL Network-to-Host 16-bit Conversion
   :id: REQ_PAL_BYTE_NTOHS
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Call someip_ntohs(someip_htons(0x1234)). Verify the result equals 0x1234 (roundtrip identity).

   ``someip_ntohs(x)`` shall convert a 16-bit value from network byte
   order (big-endian) to host byte order.

   **Rationale**: Parsing incoming SOME/IP headers requires converting
   wire-format 16-bit fields to host order.

   **Code Location**: ``include/platform/posix/byteorder_impl.h``,
   ``include/platform/win32/byteorder_impl.h``,
   ``include/platform/lwip/byteorder_impl.h``,
   ``include/platform/zephyr/byteorder_impl.h``

.. requirement:: PAL Host-to-Network 32-bit Conversion
   :id: REQ_PAL_BYTE_HTONL
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Call someip_htonl(0x01020304). Verify bytes are [0x01, 0x02, 0x03, 0x04] in memory (big-endian wire format).

   ``someip_htonl(x)`` shall convert a 32-bit value from host byte order
   to network byte order (big-endian).

   **Rationale**: SOME/IP Message ID and Length are serialized as 32-bit
   big-endian values.

   **Code Location**: ``include/platform/posix/byteorder_impl.h``,
   ``include/platform/win32/byteorder_impl.h``,
   ``include/platform/lwip/byteorder_impl.h``,
   ``include/platform/zephyr/byteorder_impl.h``

.. requirement:: PAL Network-to-Host 32-bit Conversion
   :id: REQ_PAL_BYTE_NTOHL
   :satisfies: REQ_ARCH_001
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Call someip_ntohl(someip_htonl(0x12345678)). Verify the result equals 0x12345678 (roundtrip identity).

   ``someip_ntohl(x)`` shall convert a 32-bit value from network byte
   order (big-endian) to host byte order.

   **Rationale**: Parsing incoming SOME/IP headers requires converting
   wire-format 32-bit fields to host order.

   **Code Location**: ``include/platform/posix/byteorder_impl.h``,
   ``include/platform/win32/byteorder_impl.h``,
   ``include/platform/lwip/byteorder_impl.h``,
   ``include/platform/zephyr/byteorder_impl.h``

Platform Backends
=================

POSIX/Host Backend
------------------

.. requirement:: POSIX/Host Threading Backend
   :id: REQ_PLATFORM_POSIX_001
   :satisfies: REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK, REQ_PAL_CV_WAIT, REQ_PAL_CV_WAIT_PRED, REQ_PAL_CV_NOTIFY_ONE, REQ_PAL_CV_NOTIFY_ALL, REQ_PAL_CV_OWNERSHIP, REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN, REQ_PAL_LOCK_ACQUIRE, REQ_PAL_LOCK_RELEASE, REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test (host build): Mutex lock/try_lock/unlock, ConditionVariable wait with predicate + notify_one/notify_all, Thread join, ScopedLock RAII, sleep_for timing. All tests in test_platform_threading.cpp pass.

   The POSIX/host backend shall implement the PAL threading primitives
   using C++ standard library types:

   * ``Mutex`` → ``std::mutex``
   * ``Thread`` → ``std::thread``
   * ``ConditionVariable`` → custom wrapper over ``std::condition_variable``
     that accepts ``Mutex&`` (matching the RTOS API)
   * ``sleep_for`` → ``std::this_thread::sleep_for``

   **Rationale**: POSIX/host is the primary development and CI platform;
   standard library types provide zero-overhead abstractions.

   **Code Location**: ``include/platform/posix/thread_impl.h``,
   ``include/platform/host/host_condition_variable.h``

.. requirement:: POSIX/Host Memory Backend
   :id: REQ_PLATFORM_POSIX_002
   :satisfies: REQ_PAL_MEM_ALLOC, REQ_PAL_MEM_INDEPENDENT
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Call allocate_message(), verify non-null. Set and verify payload. Call twice, verify independent objects.

   The POSIX/host backend shall implement ``allocate_message()`` using
   ``std::make_shared<Message>()``.

   **Rationale**: On host/desktop, heap allocation is the natural choice;
   no pool is needed.

   **Code Location**: ``include/platform/posix/memory_impl.h``

.. requirement:: POSIX/Host Networking Backend
   :id: REQ_PLATFORM_POSIX_003
   :satisfies: REQ_PAL_NET_CLOSE, REQ_PAL_NET_SHUTDOWN, REQ_PAL_NET_NONBLOCK, REQ_PAL_NET_BLOCK
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Verify transport compiles on Linux/macOS without modification. Verify someip_close_socket maps to close(), someip_set_nonblocking uses fcntl(F_SETFL, O_NONBLOCK).

   The POSIX/host backend shall provide networking through standard BSD
   socket headers (``sys/socket.h``, ``netinet/in.h``, ``arpa/inet.h``)
   and portable helpers:

   * ``someip_close_socket(fd)`` → ``close(fd)``
   * ``someip_shutdown_socket(fd)`` → ``shutdown(fd, SHUT_RDWR)``
   * ``someip_set_nonblocking(fd)`` → ``fcntl`` with ``O_NONBLOCK``
   * ``someip_set_blocking(fd)`` → ``fcntl`` clearing ``O_NONBLOCK``

   **Rationale**: POSIX sockets are the reference networking API.

   **Code Location**: ``include/platform/posix/net_impl.h``

.. requirement:: POSIX/Host Byte-Order Backend
   :id: REQ_PLATFORM_POSIX_004
   :satisfies: REQ_PAL_BYTE_HTONS, REQ_PAL_BYTE_NTOHS, REQ_PAL_BYTE_HTONL, REQ_PAL_BYTE_NTOHL
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Verify someip_htons/ntohs/htonl/ntohl resolve to arpa/inet.h functions. Verify roundtrip conversion of 0x1234 (16-bit) and 0x12345678 (32-bit).

   The POSIX/host backend shall map byte-order macros to ``arpa/inet.h``
   functions:

   * ``someip_htons`` → ``htons``
   * ``someip_ntohs`` → ``ntohs``
   * ``someip_htonl`` → ``htonl``
   * ``someip_ntohl`` → ``ntohl``

   **Rationale**: POSIX provides standard network byte-order functions.

   **Code Location**: ``include/platform/posix/byteorder_impl.h``

FreeRTOS Backend
----------------

.. requirement:: FreeRTOS Threading Backend
   :id: REQ_PLATFORM_FREERTOS_001
   :satisfies: REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK, REQ_PAL_CV_WAIT, REQ_PAL_CV_WAIT_PRED, REQ_PAL_CV_NOTIFY_ONE, REQ_PAL_CV_NOTIFY_ALL, REQ_PAL_CV_OWNERSHIP, REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN, REQ_PAL_LOCK_ACQUIRE, REQ_PAL_LOCK_RELEASE, REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Compilation with SOMEIP_USE_FREERTOS=ON; runtime tests on FreeRTOS POSIX port for Thread join, Mutex lock/unlock/try_lock, ConditionVariable notify/wait, and sleep_for timing bounds (40ms–150ms for 50ms sleep).

   The FreeRTOS backend shall implement the PAL threading primitives:

   * ``Mutex`` wrapping ``xSemaphoreCreateMutex()``
   * ``ConditionVariable`` using a counting semaphore
   * ``Thread`` wrapping ``xTaskCreate()`` with binary-semaphore join support
   * ``sleep_for`` via ``vTaskDelay()``

   **Rationale**: FreeRTOS is one of the most widely used RTOSes in
   automotive embedded systems.

   **Code Location**: ``include/platform/freertos/thread_impl.h``

.. requirement:: FreeRTOS Memory Pool Backend
   :id: REQ_PLATFORM_FREERTOS_002
   :satisfies: REQ_PAL_MEM_ALLOC, REQ_PAL_MEM_INDEPENDENT
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Pool allocation until exhaustion returns nullptr; release and re-allocate succeeds; payload roundtrip after allocation; no memory corruption under concurrent access.

   The FreeRTOS backend shall provide a static pool allocator for Message
   objects:

   * Fixed-size pool (configurable via ``SOMEIP_FREERTOS_MESSAGE_POOL_SIZE``)
   * ``alignas(Message)`` alignment on pool buffer
   * Mutex-protected alloc/release
   * Returns ``nullptr`` on pool exhaustion

   **Rationale**: Embedded targets require deterministic memory allocation
   without heap fragmentation.

   **Code Location**: ``include/platform/freertos/memory_impl.h``,
   ``src/platform/freertos/memory.cpp``

ThreadX Backend
---------------

.. requirement:: ThreadX Threading Backend
   :id: REQ_PLATFORM_THREADX_001
   :satisfies: REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK, REQ_PAL_CV_WAIT, REQ_PAL_CV_WAIT_PRED, REQ_PAL_CV_NOTIFY_ONE, REQ_PAL_CV_NOTIFY_ALL, REQ_PAL_CV_OWNERSHIP, REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN, REQ_PAL_LOCK_ACQUIRE, REQ_PAL_LOCK_RELEASE, REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Compilation with SOMEIP_USE_THREADX=ON; runtime tests on ThreadX linux port for Mutex lock/unlock/try_lock, ConditionVariable notify, and sleep_for timing.

   The ThreadX backend shall implement the PAL threading primitives:

   * ``Mutex`` wrapping ``TX_MUTEX`` with ``TX_INHERIT`` for priority inheritance
   * ``ConditionVariable`` using ``TX_EVENT_FLAGS_GROUP`` (flag bit 0)
   * ``Thread`` wrapping ``tx_thread_create()`` with static stack and join support
   * ``sleep_for`` via ``tx_thread_sleep()``

   **Rationale**: ThreadX (Azure RTOS) is widely used in automotive and
   industrial embedded systems.

   **Code Location**: ``include/platform/threadx/thread_impl.h``

.. requirement:: ThreadX Memory Pool Backend
   :id: REQ_PLATFORM_THREADX_002
   :satisfies: REQ_PAL_MEM_ALLOC, REQ_PAL_MEM_INDEPENDENT
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Pool allocation until exhaustion returns nullptr; release and re-allocate succeeds; payload roundtrip; no corruption under concurrent access.

   The ThreadX backend shall provide a static pool allocator for Message
   objects:

   * ``TX_BLOCK_POOL`` backed by a static buffer
   * Fixed-size pool (configurable via ``SOMEIP_THREADX_MESSAGE_POOL_SIZE``)
   * Lazy initialization guarded by ``TX_MUTEX``
   * Returns ``nullptr`` on pool exhaustion

   **Rationale**: Embedded targets require deterministic memory allocation
   without heap fragmentation.

   **Code Location**: ``include/platform/threadx/memory_impl.h``,
   ``src/platform/threadx/memory.cpp``

lwIP Backend
------------

.. requirement:: lwIP Networking Backend
   :id: REQ_PLATFORM_LWIP_001
   :satisfies: REQ_PAL_NET_CLOSE, REQ_PAL_NET_SHUTDOWN, REQ_PAL_NET_NONBLOCK, REQ_PAL_NET_BLOCK
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Transport compiles with SOMEIP_USE_LWIP=ON; socket calls resolve to lwIP API; TCP/UDP loopback test on hardware or QEMU.

   The lwIP backend shall provide networking through lwIP's socket layer:

   * Socket creation, binding, listen, accept, connect
   * UDP sendto/recvfrom and TCP send/recv
   * Non-blocking I/O via ``lwip_fcntl()``
   * ``someip_close_socket`` → ``lwip_close``
   * ``someip_set_nonblocking`` / ``someip_set_blocking`` via ``lwip_fcntl``
   * Conditional macro mapping when ``LWIP_COMPAT_SOCKETS`` is disabled

   **Rationale**: lwIP is the standard TCP/IP stack for FreeRTOS and
   ThreadX on microcontrollers.

   **Code Location**: ``include/platform/lwip/net_impl.h``

.. requirement:: lwIP Byte-Order Backend
   :id: REQ_PLATFORM_LWIP_002
   :satisfies: REQ_PAL_BYTE_HTONS, REQ_PAL_BYTE_NTOHS, REQ_PAL_BYTE_HTONL, REQ_PAL_BYTE_NTOHL
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Verify someip_htons/ntohs/htonl/ntohl resolve to lwip/def.h functions. Verify roundtrip conversion.

   The lwIP backend shall map byte-order macros to lwIP definitions from
   ``lwip/def.h``.

   **Rationale**: lwIP provides its own byte-order functions optimized for
   the target architecture.

   **Code Location**: ``include/platform/lwip/byteorder_impl.h``

Zephyr Backend
--------------

.. requirement:: Zephyr Threading Backend
   :id: REQ_PLATFORM_ZEPHYR_001
   :satisfies: REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK, REQ_PAL_CV_WAIT, REQ_PAL_CV_WAIT_PRED, REQ_PAL_CV_NOTIFY_ONE, REQ_PAL_CV_NOTIFY_ALL, REQ_PAL_CV_OWNERSHIP, REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN, REQ_PAL_LOCK_ACQUIRE, REQ_PAL_LOCK_RELEASE, REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Compilation with SOMEIP_USE_ZEPHYR=ON; runtime tests for Mutex (k_mutex), ConditionVariable (k_condvar), Thread (k_thread_create/join), and sleep_for (k_msleep).

   The Zephyr backend shall implement the PAL threading primitives:

   * ``Mutex`` wrapping ``k_mutex`` with ``k_mutex_init``/``k_mutex_lock``/
     ``k_mutex_unlock``
   * ``ConditionVariable`` wrapping ``k_condvar`` with ``k_condvar_signal``/
     ``k_condvar_broadcast``/``k_condvar_wait``
   * ``Thread`` wrapping ``k_thread_create`` with ``K_KERNEL_STACK_MEMBER``
     and ``k_thread_join`` support
   * ``sleep_for`` via ``k_msleep`` with chunked conversion for durations
     exceeding ``INT32_MAX``

   **Rationale**: Zephyr is the RTOS with native SOME/IP SD multicast
   support used in OpenSOMEIP CI.

   **Code Location**: ``include/platform/zephyr/thread_impl.h``,
   ``src/platform/zephyr/thread.cpp``

.. requirement:: Zephyr Memory Pool Backend
   :id: REQ_PLATFORM_ZEPHYR_002
   :satisfies: REQ_PAL_MEM_ALLOC, REQ_PAL_MEM_INDEPENDENT
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Pool allocation and release on Zephyr target; exhaustion returns nullptr; concurrent access from multiple threads.

   The Zephyr backend shall provide ``allocate_message()`` and
   ``release_message()`` using Zephyr kernel memory primitives.

   **Rationale**: Zephyr provides kernel-managed memory pools suitable for
   deterministic embedded allocation.

   **Code Location**: ``include/platform/zephyr/memory_impl.h``,
   ``src/platform/zephyr/memory.cpp``

.. requirement:: Zephyr Networking Backend
   :id: REQ_PLATFORM_ZEPHYR_003
   :satisfies: REQ_PAL_NET_CLOSE, REQ_PAL_NET_SHUTDOWN, REQ_PAL_NET_NONBLOCK, REQ_PAL_NET_BLOCK
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Transport compiles with SOMEIP_USE_ZEPHYR=ON; verify zsock_ macro mappings. UDP/TCP loopback test on Zephyr native_sim or QEMU.

   The Zephyr backend shall provide networking through Zephyr's ``zsock_``
   socket API with macro mappings for standard BSD socket names:

   * ``socket`` → ``zsock_socket``, ``close`` → ``zsock_close``, etc.
   * ``someip_close_socket`` → ``zsock_close``
   * ``someip_set_nonblocking`` / ``someip_set_blocking`` via ``zsock_fcntl``
   * ``inet_addr`` emulated via ``zsock_inet_pton``
   * NOTE: ``connect`` is not macro-mapped to avoid clashing with
     ``ITransport::connect()``

   **Rationale**: Zephyr removed ``CONFIG_NET_SOCKETS_POSIX_NAMES`` in 4.3;
   explicit macros restore portability.

   **Code Location**: ``include/platform/zephyr/net_impl.h``

.. requirement:: Zephyr Byte-Order Backend
   :id: REQ_PLATFORM_ZEPHYR_004
   :satisfies: REQ_PAL_BYTE_HTONS, REQ_PAL_BYTE_NTOHS, REQ_PAL_BYTE_HTONL, REQ_PAL_BYTE_NTOHL
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Verify someip_htons/ntohs/htonl/ntohl resolve to zephyr/sys/byteorder.h functions. Verify roundtrip conversion.

   The Zephyr backend shall map byte-order macros to
   ``zephyr/sys/byteorder.h`` functions.

   **Rationale**: Zephyr provides endian-aware byte-order functions.

   **Code Location**: ``include/platform/zephyr/byteorder_impl.h``

Win32 Backend
-------------

.. requirement:: Win32 Threading Backend
   :id: REQ_PLATFORM_WIN32_001
   :satisfies: REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK, REQ_PAL_CV_WAIT, REQ_PAL_CV_WAIT_PRED, REQ_PAL_CV_NOTIFY_ONE, REQ_PAL_CV_NOTIFY_ALL, REQ_PAL_CV_OWNERSHIP, REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN, REQ_PAL_LOCK_ACQUIRE, REQ_PAL_LOCK_RELEASE, REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Compilation with MSVC on Windows; Mutex/Thread/ConditionVariable tests pass using std::mutex/std::thread and host_condition_variable.h wrapper.

   The Win32 backend shall implement the PAL threading primitives using
   C++ standard library types (identical to POSIX/host):

   * ``Mutex`` → ``std::mutex``
   * ``Thread`` → ``std::thread``
   * ``ConditionVariable`` → host wrapper over ``std::condition_variable``
   * ``sleep_for`` → ``std::this_thread::sleep_for``

   **Rationale**: Windows support enables development on Windows workstations.

   **Code Location**: ``include/platform/win32/thread_impl.h``,
   ``include/platform/host/host_condition_variable.h``

.. requirement:: Win32 Memory Backend
   :id: REQ_PLATFORM_WIN32_002
   :satisfies: REQ_PAL_MEM_ALLOC, REQ_PAL_MEM_INDEPENDENT
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Verify allocate_message() returns non-null on Windows; payload roundtrip succeeds.

   The Win32 backend shall implement ``allocate_message()`` using
   ``std::make_shared<Message>()``.

   **Rationale**: Desktop Windows uses standard heap allocation.

   **Code Location**: ``include/platform/win32/memory_impl.h``

.. requirement:: Win32 Networking Backend
   :id: REQ_PLATFORM_WIN32_003
   :satisfies: REQ_PAL_NET_CLOSE, REQ_PAL_NET_SHUTDOWN, REQ_PAL_NET_NONBLOCK, REQ_PAL_NET_BLOCK
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Transport compiles on MSVC; someip_close_socket maps to closesocket(); someip_set_nonblocking uses ioctlsocket(FIONBIO).

   The Win32 backend shall provide networking through Winsock2:

   * ``someip_close_socket(fd)`` → ``closesocket(fd)``
   * ``someip_shutdown_socket(fd)`` → ``shutdown(fd, SD_BOTH)``
   * ``someip_set_nonblocking(fd)`` → ``ioctlsocket(fd, FIONBIO, &mode)``
   * Winsock2 headers and ``ws2_32.lib`` linkage

   **Rationale**: Winsock2 is the standard networking API on Windows.

   **Code Location**: ``include/platform/win32/net_impl.h``

.. requirement:: Win32 Byte-Order Backend
   :id: REQ_PLATFORM_WIN32_004
   :satisfies: REQ_PAL_BYTE_HTONS, REQ_PAL_BYTE_NTOHS, REQ_PAL_BYTE_HTONL, REQ_PAL_BYTE_NTOHL
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Verify someip_htons/ntohs/htonl/ntohl resolve to winsock2.h functions. Verify roundtrip conversion.

   The Win32 backend shall map byte-order macros to Winsock2 functions.

   **Rationale**: Winsock2 provides standard byte-order functions on Windows.

   **Code Location**: ``include/platform/win32/byteorder_impl.h``
