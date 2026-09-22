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

### CI / Infrastructure

- **Coverity Scan**: Re-enable the weekly Monday 04:00 UTC schedule and
  `push` to `main` now that scan.coverity.com is serving the project again
  ([#265](https://github.com/vtz/opensomeip/issues/265)). `workflow_dispatch`
  remains for a manual verification run.
- Fork PRs no longer fail the RPM workflow template on empty Docker Hub
  secrets; Fedora images are pulled anonymously
  ([#330](https://github.com/vtz/opensomeip/issues/330)). The Python
  detailed check-run step already skips forks (landed in #325).

### Documentation

- **CMake**: `find_package(opensomeip)` / `opensomeip::opensomeip` documented
  as the installed package; the old `SomeIP::someip-common` snippet was wrong
  ([#271](https://github.com/vtz/opensomeip/issues/271)). Host CI installs the
  package and builds `tests/cmake_package` against it. The exported target now
  includes the selected PAL backend include dirs and
  `opensomeipConfig.cmake` calls `find_dependency(Threads)`.

### Documentation

- **Traceability**: Regenerated `docs/specification/spec-mapping-report.md` so
  `feat_req_someipsd_818` maps to `REQ_SD_818` (unicast Subscribe family) instead
  of shutdown `REQ_SD_310`. CAPI, PAL, and `REQ_TP_081_ATOM` are classified as
  implementation-derived and no longer listed as missing Open SOME/IP links
  ([#309](https://github.com/vtz/opensomeip/issues/309)).

### Added

- **RPC / Events**: Add constructor overloads accepting a caller-owned,
  exclusive `ITransport`, with component-managed lifecycle and
  `get_transport_result()` diagnostics. Existing default UDP construction and
  C API entry points are unchanged. This is an initial increment of
  [#341](https://github.com/vtz/opensomeip/issues/341); SD injection and shared
  transport dispatch remain separate work.
- **Message**: `try_deserialize()` returns a structured `someip::Result` for
  each semantic rejection class. Existing `deserialize()` overloads remain
  source-compatible bool wrappers
  ([#316](https://github.com/vtz/opensomeip/issues/316)).
- **Transport**: Defaulted `ITransportListener::on_message_rejected` plus
  `set_message_rejection_handler` on RPC and event APIs so complete malformed
  UDP/TCP frames are visible to applications instead of being dropped silently
  ([#315](https://github.com/vtz/opensomeip/issues/315)). Structured
  `Message::try_deserialize` reasons (#316) are not required; failures currently
  report `Result::MALFORMED_MESSAGE`.

### Bug Fixes

- **Events**: Reject distinct instance/eventgroup subscriptions for the same service
  on one subscriber, rather than choosing an arbitrary callback for notifications
  lacking that identity. Exact-key renewal and different services remain supported.
  The C API follows the same restriction. Document that filter metadata is not
  enforced and selective sending remains pending.
- **RPC server**: Serialize initialize/shutdown through complete cleanup, gate method
  registration before stopping dispatch, and release the gate on exceptional exits.
  Registration before initialization remains supported.
- **FreeRTOS**: Split oversized sleeps into full-width finite tick chunks instead
  of narrowing away the requested delay. Batch arithmetic before multiplication
  and round the final remainder upward.
- **Tests**: Bound callback-entry waits and release parked callbacks on assertion
  failure before joining async workers.
- **FreeRTOS**: Round positive sleep requests upward when converting milliseconds
  to ticks, including fractional ticks such as 11 ms at 100 Hz.
- **Static-allocation tests**: Scope the host heap trap to the thread that arms it,
  so live transport threads cannot trigger another thread's trap.
- **RPC**: Release fire-and-forget sessions and guard session ownership while
  constructing a request or inserting its pending registration. Skip both zero
  and still-live call handles when the counter wraps.
- **Tests**: Release retained request captures between sequential RPCs so the
  static-allocation regression does not exhaust the byte-buffer pool itself.
- **RPC / Events**: Re-register receive listeners on reinitialization, detach
  them after a failed start, and drain transport callbacks before clearing
  handler/subscription state during shutdown. Clear pending field-request
  callbacks on subscriber shutdown, and release subscriber callback captures
  one entry at a time so teardown does not hold a whole fixed-capacity map on
  the stack. Pending RPC completion callbacks now run after the transport is
  stopped, rather than before.
- **Transport injection**: Keep private helper includes usable in the Zephyr
  module without exporting a source include root. Stop before listener detachment
  and discard queued receives at the session boundary. Retain cleanup ownership
  for retry when a quiesced backend still reports running after a stop failure.
- **Callbacks**: Invoke event subscriber notification/field callbacks outside
  storage mutexes; discard failed-send field callbacks. Release RPC-server
  handler storage one entry at a time after the transport has drained. Remove a
  completed field request before invocation so a reentrant request survives for
  the next response. Capture copy/move/destruction still must not re-enter a
  facade while its storage mutex is held. SD lifecycle changes remain deferred.
- **RPC**: Represent synchronous calls as revocable wait registrations rather
  than application callbacks. Response, cancellation and exception cleanup share
  the pending-call mutex, avoiding both expired stack access and an unbounded
  callback lifetime drain. Complete synchronous calls before asynchronous
  shutdown callbacks and attempt every callback. `shutdown()` is no-throw: a
  throwing application callback no longer escapes the `void` API, which could
  terminate the process when `shutdown()` is called from a destructor.
- **RPC**: Never issue call handle `0`. It doubles as the "not registered"
  sentinel, so once `next_call_handle_` wrapped past 2^32 a pending entry was
  inserted and then abandoned holding a pointer to the caller's expired stack
  frame (ASan: `stack-use-after-return`). Registrations now track an explicit
  armed flag instead of overloading the handle value.
- **RPC**: Start the response-timeout budget after the transport send returns,
  so a slow blocking send can no longer consume the whole budget and report
  `TIMEOUT` for a request the client never waited on. Report
  `SERVICE_NOT_AVAILABLE` rather than `INTERNAL_ERROR` when submission is
  refused because the client is stopped or the pending-call table is full.
- **RPC**: Release session-manager entries when a call completes, is cancelled,
  or is swept by shutdown, and reject submission when no session id is
  available. Sessions previously leaked for the client's lifetime; once the
  table filled, every request reused session id 0 and concurrent calls to the
  same method could receive each other's responses.
- **Events**: Detect unsubscribe reentrancy from the dispatching thread's
  identity rather than a `thread_local`. `thread_local` has no per-task backing
  on the FreeRTOS and ThreadX ARM ports, where all tasks shared one block, so
  an unrelated thread was treated as reentrant and skipped the barrier.
- **Events**: Scope the unsubscribe barrier to the subscription being removed
  and publish the matched key while the subscription mutex is still held, so a
  dispatch that has already snapshotted its callback cannot be missed. Always
  run the barrier on the external path, including when the entry was already
  removed by a reentrant call — that case is exactly when an application
  concludes teardown is safe.
- **Events**: Remove a subscription even when its service endpoint no longer
  resolves; the unsubscribe send is best-effort and the entry was previously
  left permanently unremovable.
- **Events**: Drain in-flight notification dispatches during `shutdown()` and
  hand off with any external unsubscriber before tearing down the dispatch
  mutex and condition variable they may still be parked on.
- **Transport**: Make `TransportSession::active_` atomic and claim stop
  ownership with a single exchange, so two concurrent `shutdown()` calls (or a
  destructor racing an explicit one) cannot both close the socket and join the
  receive thread.
- **RPC / Events**: Give `~RpcServerImpl`, `~EventPublisherImpl` and
  `~EventSubscriberImpl` the exception firewall `~RpcClientImpl` already had;
  each reaches a caller-supplied `ITransport::stop()`.
- **Events**: External unsubscription waits for previously admitted notification
  dispatches; callback-initiated unsubscription remains reentrant. Reject duplicate
  pending field requests without replacing the original callback. Bound callback
  storage teardown and reject method registration while server shutdown runs.
- **Transport lifecycle**: Preserve lender-owned queued receives after failed
  initialization; retain the start error when cleanup succeeds.
- **FreeRTOS**: Ensure a positive millisecond sleep blocks for at least one tick
  instead of becoming a zero-tick yield below a 1 kHz tick rate.
- **Transport**: TCP no longer busy-loops on an invalid length field; resync
  is only at a Magic Cookie. Declared frames larger than `max_receive_buffer`
  report `BUFFER_OVERFLOW` once. UDP rejection IDs are taken from the wire
  prefix, and malformed SOME/IP-TP datagrams report `TP_REASSEMBLY`
  ([#315](https://github.com/vtz/opensomeip/issues/315)).
- **SOME/IP-SD**: SubscribeEventgroup with both a UDP and a TCP
  IPv4EndpointOption is accepted; only true duplicates (two UDP or two
  TCP) are NACKed
  ([#321](https://github.com/vtz/opensomeip/issues/321)).
- **SOME/IP-SD**: SubscribeEventgroupAck/Nack are sent to the SD datagram
  sender, not the event IPv4EndpointOption address
  ([#322](https://github.com/vtz/opensomeip/issues/322)).
- **SOME/IP-SD**: IPv6 Endpoint (Type 0x06) and IPv6 Multicast (Type 0x16)
  options are parsed instead of being skipped as unknown. IPv6 SD Endpoint
  (0x26) and ``AF_INET6`` transport remain out of scope
  ([#320](https://github.com/vtz/opensomeip/issues/320)).

- **SOME/IP-SD**: SubscribeEventgroup family is unicast-only. Clients send
  Subscribe/StopSubscribe to the Offer datagram source (not the SD multicast
  group); servers ignore Subscribe received on a multicast destination
  ([#294](https://github.com/vtz/opensomeip/issues/294)).
- **SOME/IP-SD**: Clients process SubscribeEventgroupAck/Nack and expose
  `SdClient::get_eventgroup_subscription_state()`
  ([#295](https://github.com/vtz/opensomeip/issues/295)).
- **SOME/IP-SD**: SubscribeEventgroupAck IPv4MulticastOption uses the offered
  eventgroup endpoint; unicast eventgroups omit the option
  ([#298](https://github.com/vtz/opensomeip/issues/298)).
- **SOME/IP-SD**: OfferService never uses TTL 0 (default TTL applied or offer
  rejected); StopOfferService still uses TTL 0
  ([#299](https://github.com/vtz/opensomeip/issues/299)).
- **SOME/IP-SD**: Default SD multicast endpoint unified to
  `239.255.255.251:30490` (port is specified; group is a deployment default)
  ([#300](https://github.com/vtz/opensomeip/issues/300)).
- **SOME/IP-SD**: Unknown 16-byte entry types are skipped instead of dropping
  the whole SD message
  ([#302](https://github.com/vtz/opensomeip/issues/302)).
- **SOME/IP-SD**: Unicast Flag is set on all SD TX; Reboot Flag stays set until
  session ID wraps `0xFFFF` → `0x0001`
  ([#303](https://github.com/vtz/opensomeip/issues/303)).
- **SOME/IP-SD**: SubscribeEventgroupAck/Nack copy the Subscribe counter and
  reserved 12-bit field
  ([#304](https://github.com/vtz/opensomeip/issues/304)).
- **SOME/IP-SD**: Initial Wait Phase picks a random delay between
  `initial_delay_min` and `initial_delay_max` (deterministic override for tests)
  ([#306](https://github.com/vtz/opensomeip/issues/306)).
- **Events**: New subscribers receive current field values after Subscribe Ack;
  TTL refresh of an existing client does not repeat the initial burst
  ([#307](https://github.com/vtz/opensomeip/issues/307)).

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
- `RpcClient::send_request_no_return()` — fire-and-forget `REQUEST_NO_RETURN`
  (message type 0x01) with no pending-call wait (#308).
- `RpcServer::register_method(..., MethodSemantics)` — request/response vs
  fire-and-forget method semantics (#308).

### Bug Fixes

- **SOME/IP-TP**: Message Type bit 5 (`0x20`) is the TP flag for all
  types, including Response (`0xA0`) and Error (`0xA1`). Unknown types
  are decided after masking bit 5 (`#296`).
- **UDP**: `UdpTransport` segments and reassembles large messages via
  `TpManager` when `enable_tp` is true (`#305`).
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
- RPC: Interface Version is the service major. The message header no longer
  rejects values other than 0x01; `RpcServer` returns
  `E_WRONG_INTERFACE_VERSION` (0x08) on mismatch (#297).
- RPC: `RpcClient` sends to a configured offered-service endpoint instead of
  defaulting to `127.0.0.1:30490` (the SD port). `RpcServer` binds
  `127.0.0.1:30501` by default (#301).

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
- **TP + E2E protection**: the TP segmenter now **rejects**
  E2E-protected messages (`SEGMENTATION_FAILED`) instead of silently
  truncating the E2E suffix via `serialize()`+`resize(16)`.  A
  combined TP+E2E path is tracked for a future release.

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
