<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# Changelog

## Unreleased

### Breaking Changes

- **Transport receive model: listener and polling are now mutually
  exclusive.**  When `set_listener()` is installed, incoming messages are
  dispatched only via `ITransportListener::on_message_received()` and are
  **no longer enqueued** into the internal receive queue.
  `receive_message()` returns `nullptr` in listener mode.  Previously,
  messages were both enqueued and dispatched, causing unbounded queue
  growth — memory leaks on POSIX or fixed-pool exhaustion on FreeRTOS
  ([#269](https://github.com/vtz/opensomeip/issues/269)).
  Code that relied on draining `receive_message()` while a listener was
  set must be updated to consume messages exclusively through one path.

### New Features

- `UdpTransport::receive_message_with_sender(Endpoint& sender)` — polling
  mode variant that also returns the sender's endpoint for reply
  addressing without requiring a listener.

### Bug Fixes

- UDP/TCP: listener-only mode no longer retains `MessagePtr` in the
  internal queue, preventing memory leaks and pool exhaustion (#269).
- TCP: listener callback is now invoked outside `connection_mutex_`,
  eliminating a potential deadlock when the listener calls
  `disconnect()`.

## Unreleased — Static Allocation Backend (`feature/no-heap-static-alloc`)

### Breaking Changes

This release introduces a compile-time static allocation backend.  When
`SOMEIP_USE_STATIC_ALLOC=ON`, the following public types change their
underlying representation:

| Type | Dynamic (default) | Static (`SOMEIP_USE_STATIC_ALLOC=ON`) |
|---|---|---|
| `platform::ByteBuffer` | `std::vector<uint8_t>` | Slab-backed buffer (pool-allocated, fixed capacity per tier) |
| `platform::String<N>` | `std::string` | `etl::string<N>` (fixed capacity `N`, default 64) |
| `platform::Vector<T, N>` | `std::vector<T>` | `etl::vector<T, N>` (fixed capacity `N`) |
| `platform::UnorderedMap<K, V, N>` | `std::unordered_map<K, V>` | `etl::unordered_map<K, V, N>` (fixed capacity `N`) |
| `MessagePtr` | `std::shared_ptr<Message>` | `IntrusivePtr<Message>` (pool-allocated, refcounted) |

**Consumer impact:**

- Code that stores `ByteBuffer` by value and relies on unlimited growth
  must account for pool-tier capacity limits.  `push_back()`, `resize()`,
  and `insert()` now return early / leave the buffer unchanged when the
  pool cannot satisfy the request.
- `platform::String<N>` is capacity-bounded.  Assigning a string longer
  than `N` truncates under ETL's default error policy.  Override `N` via
  template parameter or the `SOMEIP_DEFAULT_STRING_CAPACITY` CMake
  variable.
- `MessagePtr` is no longer `shared_ptr` under static-alloc.  Code that
  calls `shared_ptr`-specific APIs (e.g. `use_count()`, `weak_ptr`)
  will not compile.  Use `MessagePtr` opaquely.

### New Features

- **`PayloadView`** — non-owning, span-like view over contiguous payload
  bytes.  Works identically across dynamic and static backends.
  `operator[]` includes a debug-mode assertion; production builds match
  `std::span` semantics (no bounds check).
- **Static slab allocators** for `Message` and `ByteBuffer` with
  configurable pool sizes via `static_config.h` or CMake `-D` overrides.
- **`MallocTrapGuard`** (RAII) and `malloc_trap` link-time interposition
  for verifying zero-heap behavior in tests.
- **FreeRTOS and ThreadX Renode CI** — cross-compiled static-alloc tests
  run on Cortex-M4 under Renode simulation.
- **SD capacity-aware contracts** — `add_entry()` / `add_option()` return
  `bool`; `deserialize()` rejects messages that exceed container capacity.

### Bug Fixes

- SD client: reserve local tracking map slot **before** sending network
  traffic in `find_service()` and `subscribe_eventgroup()`.  Previously,
  a successful send with a full map would discard the callback.
- SD server: callers of `next_unicast_session_id()` now abort the
  response when the peer table is full (returns session ID 0), instead
  of sending an invalid SOME/IP message.
- Event publisher: `handle_subscription_locked()` now rejects filter
  lists that exceed the bounded container's capacity instead of silently
  truncating them and returning success.
- `PayloadView::operator[]` now asserts `i < size_` in debug builds.
- Overflow-safe bounds check in `e2e_header.cpp` (`offset + header_size`
  wraparound).
- `e2e_crc.cpp` returns `nullopt` when temporary slice allocation fails
  under static pool pressure.
- `sd_message.cpp` rejects oversized configuration strings before
  `assign()` to prevent ETL assertion / truncation.
- `sd_server.cpp` / `event_subscriber.cpp` replaced `std::to_string()`
  with stack-local `snprintf()` to eliminate heap allocation.
- `event_subscriber.cpp` field-response correlation key normalized to
  `instance_id=0` on both store and lookup paths.
- `serializer.h` `deserialize_array` rejects wire-controlled lengths
  exceeding static vector capacity (`MALFORMED_MESSAGE`).

### Known Limitations (Intentional)

- **E2E `make_unique` heap allocation** — `std::make_unique<BasicE2EProfile>()`
  in `e2e_profiles/standard_profile.cpp` (line 324) still allocates on the
  heap.  E2E profile registration is a one-time startup cost and is
  performed before the malloc trap is armed.  Tracked for static-pool
  migration if E2E is used on bare-metal targets.
- **Debug `to_string()` heap allocation** — `Message::to_string()`,
  `Endpoint::to_string()`, `to_string(Result)`, `to_string(MessageType)`,
  and `to_string(ReturnCode)` use `std::string` / `std::stringstream`.
  These are diagnostic-only functions not called on the data path.
- **Examples disabled under static-alloc** — `BUILD_EXAMPLES=OFF` in all
  static-alloc CMake presets.  Examples use `std::vector`, `std::string`,
  and `std::make_shared` pervasively.  Migrating them is possible but
  not prioritized; they serve as dynamic-backend usage documentation.
- **FreeRTOS zero-heap test uses size delta** — `test_freertos_static_zero_heap()`
  compares `xPortGetFreeHeapSize()` before and after.  A balanced
  `pvPortMalloc`/`vPortFree` pair within the window would go undetected.
  An allocation-counter approach (wrapping `pvPortMalloc`) would catch
  transient allocations.  Acceptable as-is; stronger instrumentation
  tracked as a follow-up.
