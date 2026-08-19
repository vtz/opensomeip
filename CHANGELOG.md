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

### Breaking Changes (Wire Format)

- **String serialization now includes UTF-8 BOM and NUL terminator.**
  Dynamic UTF-8 strings are now serialized as
  `[length u32][BOM EF BB BF][utf8 data][0x00]` per the Open SOME/IP
  Specification (`feat_req_someip_662`, `800`, `687`).  Length =
  BOM(3) + data + NUL(1).  The unconditional 4-byte alignment after
  strings has been removed; alignment is now caller-controlled.
  ([#274](https://github.com/vtz/opensomeip/issues/274))

- **TP segments now carry a full 16-byte SOME/IP header.**  Every
  SOME/IP-TP segment (not just the first) includes a full SOME/IP
  header with TP-Flag set, followed by a 4-byte TP header, then
  payload.  Non-last segment payloads are now always a multiple of
  16 bytes, and all More-Segments=1 segments have uniform size
  (`feat_req_someiptp_765`, `772`, `778`).
  ([#275](https://github.com/vtz/opensomeip/issues/275))

- **TP segment payload maximized at `max_segment_size` (default 1392).**
  `max_segment_size` now means segment *payload* bytes, not total
  wire size.  Non-last segments produce payloads of exactly
  `(max_segment_size / 16) * 16` bytes (1392 with default config).
  Previously 20 bytes were reserved for headers, capping payload at
  1360.

- **Folklore 1000-byte TP-Flag path removed.**  Messages that fit in
  a single non-TP SOME/IP message are sent without TP-Flag and without
  a TP header.  The vsomeip-style `> 1000` threshold that set TP-Flag
  without appending a TP header has been deleted.

- **TP reassembly buffer keyed by spec-mandated composite key.**
  Reassembly buffers are now keyed by Message ID + Protocol Version +
  Interface Version + Message Type (wire byte 14, TP-flag masked off) +
  Request ID (Client ID + Session ID), per `feat_req_someiptp_781`.
  Session ID change detection discards stale buffers
  (`feat_req_someiptp_795`).
  ([#276](https://github.com/vtz/opensomeip/issues/276))

- **Undersized / zero-payload TP segments are rejected.**  Segments
  shorter than the required header overhead (20 bytes for TP) are
  rejected.  Zero-payload segments no longer vacuously complete a
  reassembly buffer.

- **TCP Magic Cookie field layout corrected.**  Session ID is now
  `0xBEEF` (was misplaced as `0x0001`), MessageType is `0x01`/`0x02`
  (was `0xBE`), and ReturnCode is `0x00` (was `0xEF`), matching
  `feat_req_someip_609`.
  ([#277](https://github.com/vtz/opensomeip/issues/277))

### Other Breaking Changes

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

- **Serialization**: string wire format now matches Open SOME/IP spec
  with UTF-8 BOM, NUL terminator, and correct length semantics (#274).
- **SOME/IP-TP**: every segment now carries a full SOME/IP header;
  non-last segment payloads are 16-byte aligned and uniformly sized (#275).
- **SOME/IP-TP**: reassembly buffer uses spec-mandated composite key
  (incl. wire Message Type and full Request ID) for interoperability;
  stale sessions are discarded on Session ID change (#276).
- **SOME/IP-TP**: undersized and zero-payload segments are now rejected
  instead of vacuously completing a reassembly buffer.
- **SOME/IP-TP**: `max_segment_size` now correctly represents payload
  capacity; non-last segments produce maximum-size payloads (1392 default).
- **SOME/IP-TP**: removed folklore 1000-byte threshold that set TP-Flag
  without appending a TP header.
- **TCP**: Magic Cookie byte layout corrected — Session ID, MessageType,
  and ReturnCode now match `feat_req_someip_609` (#277).
- UDP/TCP: listener-only mode no longer retains `MessagePtr` in the
  internal queue, preventing memory leaks and pool exhaustion (#269).
- TCP: `on_message_received()` is now invoked outside `connection_mutex_`,
  eliminating a potential deadlock when the callback calls
  `disconnect()`.

### Interop Notes

The following intentional extensions remain vs. the Open SOME/IP spec:

- **nPDU batching**: not yet implemented — each UDP datagram / TCP write
  carries exactly one SOME/IP message.  Multiple-message-per-datagram
  demux is a P1 follow-up.
- **TCP Magic Cookie insertion**: cookies are sent on a 10-second timer
  rather than at the start of each TCP segment.  Per-segment insertion
  is a follow-up.
- **UTF-16 strings**: only UTF-8 strings are supported; UTF-16 BOM
  detection and endianness validation is a P1 follow-up.
- **Configurable length-field widths**: strings and arrays use 32-bit
  length fields only; 8/16/0-bit variants are a P1 follow-up.
- **TP traffic shaping**: no inter-segment delay is applied.
- **TP + E2E protection**: the TP segmenter calls `serialize()` and
  truncates the result to 16 bytes to extract the SOME/IP header,
  which discards any E2E protection suffix.  E2E-protected messages
  **must not** be passed to the TP segmenter until a combined
  TP+E2E path is implemented.  This is a known limitation tracked
  for a future release.

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
