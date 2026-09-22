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

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Unreleased

## [0.2.0] - 2026-10-05

This minor release packages new public APIs (C ABI, static-allocation PAL,
fire-and-forget RPC, event-driven transports, multi-client TCP) and
**breaking** wire-format / behavioral changes that landed on `main` since
v0.1.0. Per this repo's 0.x policy, incompatible API and wire changes bump
the minor version; 1.0.0 remains reserved until the public API is declared
stable.

### Breaking Changes

- **E2E**: `E2EConfig::offset` is renamed to `offset_bits` (no alias). The
  field is bits from the start of the Length-covered region / Request ID
  (Open SOME/IP Offset, feat_req_someip_102). The default is
  `E2EConfig::DEFAULT_OFFSET_BITS` (64), not the previous unused `8`.
  Callers who set `offset = 8` must switch to `offset_bits` and drop that
  assignment. `protect`/`validate` return `NOT_IMPLEMENTED` when
  `offset_bits` is not the default or the plugin `get_header_size()` is
  not 12 (`NOT_INITIALIZED` still wins if no profile is registered).
  `INVALID_ARGUMENT` remains CRC/Data ID/replay and true caller errors.
  The C ABI is unchanged (Offset is not a C field)
  ([#318](https://github.com/vtz/opensomeip/issues/318),
  leftover [#339](https://github.com/vtz/opensomeip/issues/339)).
- **Transport receive model: listener and polling are now mutually
  exclusive.**  When `set_listener()` is installed, incoming messages are
  dispatched only via `ITransportListener::on_message_received()` and are
  **no longer enqueued** into the internal receive queue.
  `receive_message()` returns `nullptr` in listener mode.  Previously,
  messages were both enqueued and dispatched, causing unbounded queue
  growth — memory leaks on POSIX or fixed-pool exhaustion on FreeRTOS
  ([#269](https://github.com/vtz/opensomeip/issues/269),
  [#270](https://github.com/vtz/opensomeip/issues/270)).
  Code that relied on draining `receive_message()` while a listener was
  set must be updated to consume messages exclusively through one path.
- **RPC defaults and Interface Version**: Interface Version is the service
  major (not a hardcoded `0x01`); `RpcServer` returns
  `E_WRONG_INTERFACE_VERSION` (0x08) on mismatch. `E_WRONG_MESSAGE_TYPE`
  (0x0a) is returned before that Interface Version error. A response whose
  Interface Version does not match the client is
  `RpcResult::WRONG_INTERFACE_VERSION`, not `INTERNAL_ERROR`. SD messages
  (`0xFFFF` / `0x8100`) are still checked at the header against interface
  version `0x01`; other messages pass that field through
  ([#297](https://github.com/vtz/opensomeip/issues/297),
  [#312](https://github.com/vtz/opensomeip/pull/312)).
  `RpcClient` no longer defaults traffic to `127.0.0.1:30490`;
  `RpcServer` binds `127.0.0.1:30501` by default
  ([#301](https://github.com/vtz/opensomeip/issues/301),
  [#312](https://github.com/vtz/opensomeip/pull/312)).
- **Static allocation backend types**: When `SOMEIP_USE_STATIC_ALLOC=ON`,
  public types change underlying representation
  ([#268](https://github.com/vtz/opensomeip/issues/268)):

  | Type | Dynamic (default) | Static (`SOMEIP_USE_STATIC_ALLOC=ON`) |
  |---|---|---|
  | `platform::ByteBuffer` | `std::vector<uint8_t>` | Slab-backed buffer (pool-allocated, fixed capacity per tier) |
  | `platform::String<N>` | `std::string` | `etl::string<N>` (fixed capacity `N`, default 64) |
  | `platform::Vector<T, N>` | `std::vector<T>` | `etl::vector<T, N>` (fixed capacity `N`) |
  | `platform::UnorderedMap<K, V, N>` | `std::unordered_map<K, V>` | `etl::unordered_map<K, V, N>` (fixed capacity `N`) |
  | `MessagePtr` | `std::shared_ptr<Message>` | `IntrusivePtr<Message>` (pool-allocated, refcounted) |

  Under the default dynamic backend these remain aliases of the STL types.
  With static-alloc, RPC payload types and `MethodHandler`
  (`platform::Function<…>`) are distinct; code that relies on unlimited
  `ByteBuffer` growth, `shared_ptr`-specific APIs, or unbounded
  `platform::String` capacity must migrate.
- **TCP server model**: a listening transport serves multiple clients
  concurrently. It previously kept a single connection and closed every
  surplus accepted socket. `send_message()` routes by its endpoint
  argument, and socket I/O is serialised per connection
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- **TP statistics API**: `TpManager::get_statistics()` is removed. Call
  `get_sender_statistics()` or `get_receiver_statistics()` for a
  point-in-time snapshot. Sender and receiver counters are separate
  ([#326](https://github.com/vtz/opensomeip/issues/326),
  [#327](https://github.com/vtz/opensomeip/issues/327),
  [#328](https://github.com/vtz/opensomeip/pull/328)).

### Breaking Changes (Wire Format)

Peers running v0.1.0 will not interoperate with this release on these paths:

- **String serialization now includes UTF-8 BOM and NUL terminator.**
  Dynamic UTF-8 strings are now serialized as
  `[length u32][BOM EF BB BF][utf8 data][0x00]` per the Open SOME/IP
  Specification (`feat_req_someip_662`, `800`, `687`).  Length =
  BOM(3) + data + NUL(1).  The unconditional 4-byte alignment after
  strings has been removed; alignment is now caller-controlled.
  ([#274](https://github.com/vtz/opensomeip/issues/274),
  [#278](https://github.com/vtz/opensomeip/pull/278))
- **TP segments now carry a full 16-byte SOME/IP header.**  Every
  SOME/IP-TP segment (not just the first) includes a full SOME/IP
  header with TP-Flag set, followed by a 4-byte TP header, then
  payload.  Non-last segment payloads are now always a multiple of
  16 bytes, and all More-Segments=1 segments have uniform size
  (`feat_req_someiptp_765`, `772`, `778`).
  ([#275](https://github.com/vtz/opensomeip/issues/275),
  [#278](https://github.com/vtz/opensomeip/pull/278))
- **TP segment payload maximized at `max_segment_size` (default 1392).**
  `max_segment_size` now means segment *payload* bytes, not total
  wire size.  Non-last segments produce payloads of exactly
  `(max_segment_size / 16) * 16` bytes (1392 with default config).
  Previously 20 bytes were reserved for headers, capping payload at
  1360 ([#278](https://github.com/vtz/opensomeip/pull/278)).
- **Folklore 1000-byte TP-Flag path removed.**  Messages that fit in
  a single non-TP SOME/IP message are sent without TP-Flag and without
  a TP header.  The previous `> 1000` threshold that set TP-Flag
  without appending a TP header has been deleted
  ([#278](https://github.com/vtz/opensomeip/pull/278)).
- **TP reassembly buffer keyed by spec-mandated composite key.**
  Reassembly buffers are now keyed by Message ID + Protocol Version +
  Interface Version + Message Type (wire byte 14, TP-flag masked off) +
  Request ID (Client ID + Session ID) + UDP sender IPv4 and port, per
  `feat_req_someiptp_781`. Two peers that reuse the same request id do
  not share a buffer. Session ID change detection discards stale buffers
  (`feat_req_someiptp_795`).
  ([#276](https://github.com/vtz/opensomeip/issues/276),
  [#278](https://github.com/vtz/opensomeip/pull/278),
  [#310](https://github.com/vtz/opensomeip/pull/310))
- **Undersized / zero-payload TP segments are rejected.**  Segments
  shorter than the required header overhead (20 bytes for TP) are
  rejected.  Zero-payload segments no longer vacuously complete a
  reassembly buffer ([#278](https://github.com/vtz/opensomeip/pull/278)).
- **TCP Magic Cookie field layout corrected.**  Session ID is now
  `0xBEEF` (was misplaced as `0x0001`), MessageType is `0x01`/`0x02`
  (was `0xBE`), and ReturnCode is `0x00` (was `0xEF`), matching
  `feat_req_someip_609`. Detection also requires method and type to
  match: client method `0x0000` with type `0x01`, or server method
  `0x8000` with type `0x02`. The crossed pairs are not cookies
  ([#277](https://github.com/vtz/opensomeip/issues/277),
  [#278](https://github.com/vtz/opensomeip/pull/278),
  [#279](https://github.com/vtz/opensomeip/pull/279)).
- **SOME/IP-SD**: Subscribe family is unicast-only; Offer TTL 0 only for
  StopOffer; default multicast `239.255.255.251:30490`; Unicast/Reboot
  flags; Ack counter/reserved copy
  ([#294](https://github.com/vtz/opensomeip/issues/294)–[#307](https://github.com/vtz/opensomeip/issues/307),
  [#311](https://github.com/vtz/opensomeip/pull/311)).

### Added

- **C ABI / FFI qualification boundary**: `include/capi/opensomeip.h`,
  exception firewall on every entry point, `BUILD_CAPI` / `find_package`
  export, RPM packaging, unit + integration tests, and FFI FMEA /
  C API user docs
  ([#313](https://github.com/vtz/opensomeip/pull/313)).
- **No-heap static allocation PAL**: ETL + slab pools, intrusive
  `MessagePtr`, FreeRTOS/ThreadX Renode CI, malloc-trap verification
  ([#268](https://github.com/vtz/opensomeip/issues/268)).
- **Transport**: Event-driven `IUdpSocketAdapter` / `ITcpSocketAdapter` plus
  `EventDrivenUdpTransport` and `EventDrivenTcpTransport` so integrators can
  drive `ITransport` from an existing reactor without BSD sockets
  ([#172](https://github.com/vtz/opensomeip/issues/172)).
  `IMulticastTransport` is the shared join/leave surface for
  `UdpTransport` and `EventDrivenUdpTransport`. Event-driven UDP rejects a
  send larger than `max_message_size`. Event-driven TCP consumes Magic
  Cookies instead of delivering them as messages, and its receive path parses
  and delivers one SOME/IP frame at a time (no bounded staging list); only an
  incomplete trailing fragment is retained. Reassembly-buffer exhaustion
  discards the stream buffer and reports `BUFFER_OVERFLOW` via
  `on_message_rejected`. Message-pool exhaustion consumes the current complete
  frame and reports `OUT_OF_MEMORY` via `on_error` so the stream is not stalled
  with complete unparsed PDUs
  ([#178](https://github.com/vtz/opensomeip/pull/178)).
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
- **SOME/IP-TP**: `TpReassembler::is_reassembling`,
  `get_reassembly_progress`, and `cancel_reassembly` accept a
  `TpReassemblyKey`. The `message_id` overloads still match every transfer
  with that id
  ([#279](https://github.com/vtz/opensomeip/pull/279)).
- **Transport**: Defaulted `ITransportListener::on_message_rejected` plus
  `set_message_rejection_handler` on RPC and event APIs so complete malformed
  UDP/TCP frames are visible to applications instead of being dropped silently
  ([#315](https://github.com/vtz/opensomeip/issues/315)). Structured
  `Message::try_deserialize` reasons (#316) are not required; failures currently
  report `Result::MALFORMED_MESSAGE`.
- `UdpTransport::receive_message_with_sender(Endpoint& sender)` — polling
  mode variant that also returns the sender's endpoint for reply
  addressing without requiring a listener.
- `TcpTransport::receive_message_with_sender(Endpoint& sender)` — the same
  polling helper on TCP, so a multi-peer server can reply without a
  listener
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- `TcpTransport::connection_count()`, `max_connections()`,
  `is_peer_connected(const Endpoint&)` and
  `disconnect_peer(const Endpoint&)` — inspect and manage individual
  peers. `disconnect_peer()` closes one connection and reports
  `on_connection_lost()` for it; `disconnect()` still closes every
  connection the transport holds
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- `SOMEIP_MAX_TCP_CONNECTIONS` (default 10, matching the long-standing
  `TcpTransportConfig::max_connections` default) sizes the TCP connection
  table at compile time. It is defined in `static_config.h` and forwarded
  from CMake like the other static-alloc knobs, so
  `-DSOMEIP_MAX_TCP_CONNECTIONS` reaches the compiler. The configured
  limit is clamped to it and reported by `max_connections()`. Raising it
  on a static-allocation build usually means raising the
  `SOMEIP_BYTE_POOL_*` counts too, since each served connection may hold
  a pooled receive buffer
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- `RpcClient::send_request_no_return()` — fire-and-forget `REQUEST_NO_RETURN`
  (message type 0x01) with no pending-call wait
  ([#308](https://github.com/vtz/opensomeip/issues/308)).
- `RpcServer::register_method(..., MethodSemantics)` — request/response vs
  fire-and-forget method semantics
  ([#308](https://github.com/vtz/opensomeip/issues/308)).
- `SdClient::get_eventgroup_subscription_state()`
  ([#295](https://github.com/vtz/opensomeip/issues/295)).
- **`PayloadView`** — non-owning, span-like view over contiguous payload
  bytes across dynamic and static backends
  ([#268](https://github.com/vtz/opensomeip/issues/268)).

### Fixed

- **Transport**: An unresolved remote application destination fails before
  send. `EventSubscriber` subscription and field requests no longer fall
  back to `127.0.0.1:30490`. Generic application defaults are unresolved
  (empty address, port 0). Port 0 with an explicit address remains an
  ephemeral local bind, and `SOMEIP_SD_MULTICAST_ENDPOINT` stays on the
  SD port (30490)
  ([#343](https://github.com/vtz/opensomeip/issues/343),
  [#350](https://github.com/vtz/opensomeip/pull/350)).
- **Transport / SD**: Multicast join failures no longer report success.
  `UdpTransport::join_multicast_group` / `leave_multicast_group` return
  `Result::MULTICAST_ERROR` with group/interface diagnostics via
  `last_multicast_error()`. `SdServer` starts in an explicit degraded
  multicast state and re-attempts membership on the offer timer within
  `SdConfig::multicast_rejoin_max_attempts`; `SdClient` keeps fail-closed
  SD init, tracks eventgroup membership per owned subscription (bounded),
  and retries from the maintenance loop
  ([#340](https://github.com/vtz/opensomeip/issues/340),
  [#348](https://github.com/vtz/opensomeip/pull/348)).
- **E2E**: profile resolution stays sequential (ID, then name, then default).
  A non-zero unregistered `profile_id` no longer skips `profile_name` and
  silently protect/validate with the default 12-byte profile
  ([#318](https://github.com/vtz/opensomeip/issues/318)).
- **Transport**: TCP no longer busy-loops on an invalid length field; resync
  is only at a Magic Cookie. Declared frames larger than `max_receive_buffer`
  report `BUFFER_OVERFLOW` once. UDP rejection IDs are taken from the wire
  prefix, and malformed SOME/IP-TP datagrams report `TP_REASSEMBLY`
  ([#315](https://github.com/vtz/opensomeip/issues/315)).
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
- **SOME/IP-SD**: SubscribeEventgroup with both a UDP and a TCP
  IPv4EndpointOption is accepted; only true duplicates (two UDP or two
  TCP) are NACKed
  ([#321](https://github.com/vtz/opensomeip/issues/321)).
- **SOME/IP-SD**: SubscribeEventgroupAck/Nack are sent to the SD datagram
  sender, not the event IPv4EndpointOption address
  ([#322](https://github.com/vtz/opensomeip/issues/322)).
- **SOME/IP-SD**: IPv6 Endpoint (Type 0x06) and IPv6 Multicast (Type 0x16)
  options are parsed instead of being skipped as unknown. IPv6 SD Endpoint
  (0x26) and `AF_INET6` transport remain out of scope
  ([#320](https://github.com/vtz/opensomeip/issues/320)).
- **SOME/IP-SD**: SubscribeEventgroup family is unicast-only. Clients send
  Subscribe/StopSubscribe to the Offer datagram source (not the SD multicast
  group); servers ignore Subscribe received on a multicast destination
  ([#294](https://github.com/vtz/opensomeip/issues/294)).
- **SOME/IP-SD**: Clients process SubscribeEventgroupAck/Nack
  ([#295](https://github.com/vtz/opensomeip/issues/295)).
- **SOME/IP-SD**: SubscribeEventgroupAck IPv4MulticastOption uses the offered
  eventgroup endpoint; unicast eventgroups omit the option
  ([#298](https://github.com/vtz/opensomeip/issues/298)).
- **SOME/IP-SD**: OfferService never uses TTL 0 (default TTL applied or offer
  rejected); StopOfferService still uses TTL 0
  ([#299](https://github.com/vtz/opensomeip/issues/299)).
- **SOME/IP-SD**: Default SD multicast endpoint unified to
  `239.255.255.251:30490`
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
- Event subscription TTL expiry
  ([#267](https://github.com/vtz/opensomeip/issues/267)).
- **SOME/IP-TP**: Message Type bit 5 (`0x20`) is the TP flag for all
  types, including Response (`0xA0`) and Error (`0xA1`). Unknown types
  are decided after masking bit 5
  ([#296](https://github.com/vtz/opensomeip/issues/296)).
- **UDP**: `UdpTransport` segments and reassembles large messages via
  `TpManager` when `enable_tp` is true
  ([#305](https://github.com/vtz/opensomeip/issues/305)).
- **UDP**: A datagram whose SOME/IP Length field does not match the datagram
  size is rejected. Segmentation also runs when the serialized message
  exceeds `max_message_size` even if the payload alone fits
  `max_segment_size`
  ([#310](https://github.com/vtz/opensomeip/pull/310)).
- **SOME/IP-TP**: A follow-up segment whose total length differs from the
  open buffer is rejected and the buffer is kept. An offset past that
  total no longer erases the buffer. A non-final segment whose payload is
  not a multiple of 16 bytes is rejected
  ([#279](https://github.com/vtz/opensomeip/pull/279),
  [#310](https://github.com/vtz/opensomeip/pull/310)).
- **Serialization**: `deserialize_string` rejects a length that does not
  fit in the bytes still remaining, including a `uint32_t` wrap of
  `position + length` on 32-bit targets
  ([#279](https://github.com/vtz/opensomeip/pull/279)).
- **SOME/IP-SD**: Offers use an exponential repetition phase
  (`repetition_base * multiplier^index`, capped, then cyclic). The offer
  thread sleeps until the next due offer instead of polling every 10 ms,
  and idles for 500 ms when nothing is offered. A lowered legacy
  `initial_delay` is honored. A subscription renewed after TTL expiry is
  a new subscriber and receives the current field values;
  `EventPublisher::shutdown()` drops cached field values
  ([#311](https://github.com/vtz/opensomeip/pull/311),
  [#323](https://github.com/vtz/opensomeip/issues/323)).
- TCP: `on_message_received()` is now invoked outside `connection_mutex_`,
  eliminating a potential deadlock when the callback calls `disconnect()`.
- Static-alloc capacity-aware SD/event/serializer bounds checks and
  no-heap cleanup (reserve-before-send, peer-table abort, filter capacity,
  `snprintf` instead of `std::to_string`, etc.)
  ([#268](https://github.com/vtz/opensomeip/issues/268)).
- **TCP**: connections beyond the limit are accepted and closed
  immediately, so the client observes a refusal, instead of completing a
  handshake into the backlog and waiting on a server that will never
  serve it (`REQ_TRANSPORT_003_E01`)
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- **TCP**: `send_data()` no longer retries `EAGAIN` forever. It gives up
  after `TcpTransportConfig::send_timeout`, returning `TIMEOUT` when
  nothing was written and `CONNECTION_LOST` when a partial write left the
  peer's stream unframeable, in which case that peer is closed. The
  unbounded retry could hold a connection's I/O lock indefinitely against
  a peer that stopped reading, which in turn made `disconnect_peer()`,
  `stop()` and the destructor block forever
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- **TCP**: `get_connection_state()` reports `CONNECTING` while an outbound
  `connect()` handshake is in progress, then `CONNECTED` or
  `DISCONNECTED` (`REQ_TRANSPORT_003a`). `DISCONNECTING` is still
  reported while a peer is being torn down
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- **TCP**: a peer that reconnects from the same source port replaces its
  own stale connection. The accept path retires a still-ACTIVE entry for
  that endpoint before allocating, so the table no longer holds two
  entries for one peer and no longer refuses the replacement when the
  stale entry occupied the last free slot
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- **TCP**: `connect()` restores blocking mode on the socket it hands to
  the connection table. It was left non-blocking from the bounded
  handshake, so `SO_SNDTIMEO` did not apply and `send_data()` spun on
  `EAGAIN` for a full `send_timeout`
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- **TCP**: `SOMEIP_MAX_TCP_CONNECTIONS` of 0 is rejected at compile time
  instead of building a server that refuses every connection
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- **TCP**: `connect()` on a server-mode transport always returns
  `INVALID_STATE`. The mode guard now runs before the already-connected
  short-circuit, which any ACTIVE slot — including an accepted peer — had
  been satisfying
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- **TCP**: client `connect()` short-circuits only when already connected to
  that peer. A second `connect(B)` after `connect(A)` used to return
  `SUCCESS` without opening B
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- **TCP**: default `max_receive_buffer` is 65543 (8 + `MAX_MESSAGE_SIZE`) so
  a legal max-length frame can be received complete. The previous 65536
  default filled the buffer one byte short of the parser's limit and then
  closed the peer with `BUFFER_OVERFLOW`
  ([#319](https://github.com/vtz/opensomeip/issues/319),
  [#324](https://github.com/vtz/opensomeip/pull/324)).
- **SOME/IP-TP**: overlapping segments use first-wins semantics
  (`feat_req_someiptp_797`): only bytes not already received are written,
  so a retransmission with different data cannot mix payloads. A completed
  reassembly publishes the SOME/IP header and payload together
  ([#326](https://github.com/vtz/opensomeip/issues/326),
  [#327](https://github.com/vtz/opensomeip/issues/327),
  [#328](https://github.com/vtz/opensomeip/pull/328)).

### Changed

- **Coverity Scan**: Re-enable the weekly Monday 04:00 UTC schedule and
  `push` to `main` now that scan.coverity.com is serving the project again
  ([#265](https://github.com/vtz/opensomeip/issues/265),
  [#336](https://github.com/vtz/opensomeip/pull/336)). The workflow had been
  limited to `workflow_dispatch` while the service was offline
  ([#264](https://github.com/vtz/opensomeip/pull/264)); `workflow_dispatch`
  remains for a manual verification run.
- Fork PRs no longer fail the RPM workflow template on empty Docker Hub
  secrets; Fedora images are pulled anonymously
  ([#330](https://github.com/vtz/opensomeip/issues/330),
  [#331](https://github.com/vtz/opensomeip/pull/331)). The Python
  detailed check-run step already skips forks (landed in #325).
- Dev containers install `pre-commit` from `requirements-dev.txt` so
  `scripts/run_pre_pr_tests.sh` does not fall back to a floating pip
  install
  ([#325](https://github.com/vtz/opensomeip/pull/325)).
- **CMake docs**: `find_package(opensomeip)` / `opensomeip::opensomeip`
  documented as the installed package; the old `SomeIP::someip-common`
  snippet was wrong
  ([#271](https://github.com/vtz/opensomeip/issues/271),
  [#335](https://github.com/vtz/opensomeip/pull/335)). Host CI installs the
  package and builds `tests/cmake_package` against it. The exported target
  includes the selected PAL backend include dirs and
  `opensomeipConfig.cmake` calls `find_dependency(Threads)`. When an etl,
  FreeRTOS, or ThreadX target blocks the export set, headers and libraries
  still install and `find_package(opensomeip)` is not generated.
- **Traceability**: Regenerated `docs/specification/spec-mapping-report.md`
  so `feat_req_someipsd_818` maps to `REQ_SD_818` (unicast Subscribe family)
  instead of shutdown `REQ_SD_310`. CAPI, PAL, and `REQ_TP_081_ATOM` are
  classified as implementation-derived and no longer listed as missing
  Open SOME/IP links
  ([#309](https://github.com/vtz/opensomeip/issues/309),
  [#338](https://github.com/vtz/opensomeip/pull/338)).

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

### Known Limitations (Static Allocation)

- **E2E `make_unique` heap allocation** — `std::make_unique<BasicE2EProfile>()`
  in `e2e_profiles/standard_profile.cpp` still allocates on the heap.
  E2E profile registration is a one-time startup cost performed before the
  malloc trap is armed.
- **Debug `to_string()` heap allocation** — diagnostic-only functions not
  called on the data path still use `std::string` / `std::stringstream`.
- **Examples disabled under static-alloc** — `BUILD_EXAMPLES=OFF` in all
  static-alloc CMake presets.
- **FreeRTOS zero-heap test uses size delta** — balanced
  `pvPortMalloc`/`vPortFree` within the window would go undetected.

## [0.1.0] - 2026-05-20

This is the first minor release and includes **breaking changes** to wire formats,
public API types, and default behaviors to bring the stack into compliance with the
SOME/IP and SOME/IP-SD specifications.

### Breaking Changes

- **SD minor version widened to 32-bit**: `ServiceEntry::minor_version_` and `ServiceInstance::minor_version` changed from `uint8_t` to `uint32_t`; `get_minor_version()` / `set_minor_version()` signatures updated accordingly (#245, #251)
- **SD ConfigurationOption length field corrected**: Wire-format length now includes the Reserved byte per spec; messages serialized by v0.0.x are **not** wire-compatible with v0.1.0 (#246, #251)
- **SD unknown-option skip corrected**: Skip logic uses `3 + len` instead of `4 + len`, fixing option-array parsing for messages containing unknown options (#247, #251)
- **Serialization `serialize_array` writes byte count**: Length prefix now encodes total byte length instead of element count per SOME/IP spec; existing serialized payloads are **not** backward-compatible (#249, #251)
- **Serialization `uint64` endianness made portable**: 64-bit ser/des uses explicit MSB-first byte layout instead of unconditional byte-swap; changes wire bytes on big-endian targets (#250, #251)
- **E2E default profile name**: `E2EConfig::profile_name` default changed from `"standard"` to `"basic"` to match the registered profile name; code relying on the old default will silently fail to find a profile (#248, #251)
- **`E2ECRC::calculate_crc` return type**: Changed from `uint32_t` to `std::optional<uint32_t>` to signal invalid `crc_type` instead of returning 0 (#251)
- **TP segment offset widened to 32-bit**: `TpSegmentHeader::segment_offset` changed from `uint16_t` to `uint32_t`; `TpReassemblyBuffer` segment methods updated to match (#251)
- **Constructors made `explicit`**: `ServiceEntry`, `EventGroupEntry`, `SdEntry`, `SdOption`, `ServiceInstance`, `EventSubscription`, `EventNotification`, `EventGroupSubscription`, and `EventGroup` constructors are now `explicit` — implicit conversions from integers will no longer compile (#251)
- **Copy/move deleted on core types**: `SdEntry`, `SdOption`, `E2EProfile`, `E2EProtection`, `E2EProfileRegistry`, `ITransport`, `ITransportListener`, `UdpTransport`, and `SessionManager` are now non-copyable and non-movable (#251)

### Added

- **Shared Library Packaging**: Build shared `libopensomeip.so` and split RPM into base, devel, and static sub-packages (#216)
- **Version Single Source of Truth**: Make the `VERSION` file the authoritative version reference for all build and packaging artifacts (#213)
- **Path-Based CI Filtering**: Implement path-based workflow filtering to skip irrelevant CI jobs on documentation-only or scoped changes (#202)
- **MISRA-Aligned Quality Gate**: Add MISRA-aligned clang-tidy quality gate to CI for automotive-grade static analysis (#223)
- **RTOS Test Coverage**: Increase RTOS test coverage with shared E2E and TP suites across FreeRTOS, ThreadX, and Zephyr (#218)
- **CI Caching**: Cache CMake FetchContent, Zephyr SDK/workspace, ARM toolchain, pip packages, Renode binary, and ccache across CI jobs (#200, #201, #203, #204, #205, #206)
- **SD Session ID Counter**: `SdSessionIdCounter` class and `SdMessage::get/set_session_id()` for per-message session tracking per SOME/IP-SD spec (#251)
- **SD IPv4 Endpoint protocol field**: `IPv4EndpointOption::get/set_protocol()` to specify L4 protocol (default UDP) (#251)
- **SD Server event-group selection**: `SdServer::offer_service()` accepts explicit `eventgroup_ids` parameter (#251)
- **Event publisher/subscriber endpoint APIs**: `EventPublisher::set_default_client_endpoint()`, `EventSubscriber::set_default_endpoint()`, and `EventSubscriber::set_endpoint_resolver()` (#251)
- **TCP Magic Cookie support**: Periodic magic-cookie keep-alives with `TcpTransportConfig::magic_cookie_enabled/interval`; `TcpTransport::is_magic_cookie()`, `make_magic_cookie_client()`, `make_magic_cookie_server()` (#251)
- **TCP stream parsing made public**: `TcpTransport::parse_message_from_buffer()` exposed for external use (#251)
- **15 new regression tests**: Dedicated tests for each spec-compliance fix across SD, serialization, and E2E (#251)

### Fixed

- **Spec Compliance** (6 bugs): SD minor version truncation, ConfigurationOption length off-by-one, unknown option skip off-by-one, E2E profile name mismatch, `serialize_array` element-vs-byte count, `uint64` endianness — all with regression tests (#245, #246, #247, #248, #249, #250, #251)
- **Coverity CI**: Repair Coverity Scan CI download URL and upgrade to Node.js 24 actions (#240, #241)
- **SD Interop**: Correct IPv4 option wire format for SOME/IP-SD interoperability (#239)
- **Stale Code Removal**: Remove stale vsomeip interop stubs and inflated conformance claims (#237)
- **Transport Safety**: Eliminate TOCTOU race on transport `listener_` pointer; use `std::atomic<ITransportListener*>` (#214)
- **Code Quality**: Resolve all remaining clang-tidy violations — threshold reduced to 0 across 9 remediation batches (#222, #225, #227, #228, #229, #230, #231, #232, #234, #236)
- **CI Compatibility**: Support Fedora container jobs on fork PRs (#183)

### Changed

- **CI Architecture**: Trim CI matrix — Fedora, Renode skip, reporting cleanup (#212); extract Python 3.12 installation to a reusable composite action (#215)
- **Documentation**: Convert PlantUML diagrams to Mermaid and add PAL architecture diagram (#220); rewrite TEST_REPORTING.md to reflect current CI reporting architecture (#217)

## [0.0.5] - 2026-03-31

This release supersedes v0.0.4 as the first published Fedora Copr package.

### Added

- **RPM Packaging**: Automated RPM builds via Packit for Fedora Copr on PRs and releases (#135)
- **Gateway Documentation**: Comprehensive gateway docs with platform/architecture coverage (#179)
- **Pre-PR Test Runner**: Cross-platform test runner scripts for local validation before submitting PRs (#148)
- **Allure Reporting**: Integrated Allure for rich test reports with historical trends (#141, #155)
- **Renode Hardware Simulation**: Enabled Renode testing for Zephyr, FreeRTOS, and ThreadX targets (#107)
- **MkDocs Project Site**: Documentation site with SEO configuration for GitHub Pages discoverability (#62, #103, #108)
- **Lightweight Integration**: `SOMEIP_DEV_TOOLS` CMake option to skip dev-tool discovery (#113)
- **MC/DC Test Coverage**: Increased code coverage with meaningful tests and MC/DC analysis (#117)

### Fixed

- **RPM Packaging**: Correct Copr project owner in Packit configuration (#186)
- **Transport Robustness**: Retry `EINTR` in UDP and TCP `send_data()`/`receive_data()` (#83, #86); check `someip_getsockopt()` return before trusting `SO_ERROR` (#85); correct TCP keepalive socket option setup (#84)
- **Windows/MSVC Portability**: Portable `someip_socket_t` handles (#88), `NOGDI` compile definition (#67), `in_addr_t` typedef (#80), MSVC socket macros (#70), E2E false-positive deserialization (#65)
- **Test Reliability**: Resolve use-after-destroy race in `UdpTransportTest` (#167); replace fixed sleeps with readiness synchronization (#89); add null guards (#124); fix events example API (#150)
- **RPM Packaging**: Use `git archive` instead of `tar` to prevent build failures; remove unnecessary `srpm_build_deps` (#157)
- **CI Stability**: Pin Zephyr to v4.3.0 for SDK 0.17.0 compatibility (#98); resolve Python e2e/system test failures (#143); fix JUnit XML discovery (#140)

### Changed

- **Build Consolidation**: Merged per-layer libraries into single `libopensomeip` static target (#126)
- **CI Architecture**: Consolidated workflows into single parent workflow for unified artifacts (#125); added ASan/UBSan to host workflow (#128); added Windows (MSVC) and Fedora 42 to build matrix (#64, #90)
- **CI Reporting**: Moved reports from PR comments to Job Summaries and Checks tab (#111); adopted `ctrf-io/github-test-reporter` (#133)
- **Performance**: Avoid per-packet `memset` in `UdpTransport::receive_loop` (#149)
- **Cross-Compilation**: Refactored cross-compilation build presets and toolchain support (#100)

## [0.0.4] - 2026-03-30

_Superseded by v0.0.5 — Copr packages were not published for this release._

## [0.0.3] - 2026-03-09

### Added

- **Multi-Platform PAL Backends**: Refactored Platform Abstraction Layer and added FreeRTOS, ThreadX, and lwIP backends (#25)
- **Zephyr RTOS Port**: Ported SOME/IP stack to Zephyr with stabilized native_sim CI support
- **Requirements Traceability**: Complete requirements tracking, test coverage mapping, and PAL conformance verification (#49)
- **Coverity Static Analysis**: Integrated Coverity Scan for continuous static code analysis (#56)
- **CI Test & Coverage Reports**: Publish test and coverage reports on pull requests (#54)
- **ConditionVariable Wrapper**: Added platform-agnostic ConditionVariable for host platforms (#34)

### Fixed

- **Build System**: Derive backend flags before `add_subdirectory(src)` (#39); use `CMAKE_CURRENT_SOURCE_DIR` for VERSION file (#16)
- **Cross-Platform Compatibility**: Guard lwIP byte-order helpers for big-endian hosts (#41)
- **Docker Environment**: Pin pip package versions for reproducible dev image builds (#36); add `network_mode: host` for Zephyr native_sim networking (#35)
- **Test Reliability**: Check `tx_thread_create` return value and fail fast on error (#42)
- **CI Fixes**: Correct Coverity project name, download endpoint, and form encoding (#57, #59)

### Changed

- **CI Architecture**: Split monolithic workflow into per-platform files (host, FreeRTOS, ThreadX, Zephyr) (#31)
- **CI Security**: Scope write permissions to PR-comment job only (#43)
- **CI Reliability**: Add `--no-tests=error` to ctest for FreeRTOS and ThreadX to prevent false-green builds (#38, #44); skip build and tests on documentation-only changes (#18)
- **Documentation**: Add Zephyr, FreeRTOS, and ThreadX references to README (#37); add `SOMEIP_FREERTOS_LINUX_TESTS` to build options table (#40); improve README SEO with OpenSOME/IP branding (#15)

## [0.0.2] - 2026-01-25

### Added

- **End-to-End (E2E) Protection**: Complete implementation of E2E protection for SOME/IP messages (#9)
  - CRC calculation and verification
  - E2E header handling
  - Profile registry for managing protection profiles
  - Standard profile implementation

- **Sphinx-Needs Requirements Management**: Integrated requirements management system (#10, #11)
  - Requirements traceability documentation
  - Implementation tracking for architecture, transport, serialization, and more
  - Specification references linking implementation to open-someip-spec

- **Pre-commit Hooks**: Added automated code quality checks (#8)
  - Clang-format enforcement
  - Clang-tidy static analysis
  - Automated linting before commits

- **Docker Testing Environment**: Added containerized testing support
  - Dockerfile.test for consistent test environments
  - Cross-platform testing capabilities

- **Cross-Platform Demo**: Added example demonstrating macOS client ↔ Linux Docker server communication

- **SD (Service Discovery) Enhancements**:
  - Multicast support for service discovery
  - IPv4 options handling
  - Protocol testing tools (multicast_listener.py, multicast_sender.py)
  - Comprehensive SD tests for serialization and client/server integration

- **Configurable UDP Transport**: Added blocking/non-blocking modes with configurable socket buffer sizes

- **PlantUML Diagram Generation**: CI job for validating and rendering architecture diagrams

- **Semantic Versioning System**: Version management scripts (bump_version.sh, bump_submodule.sh)

### Fixed

- **Documentation Fixes**:
  - Resolved sphinx-needs link type and extra option conflict (#13)
  - Removed 'status' from needs_extra_options (#12)
  - Fixed PlantUML note syntax (#2)
  - Fixed README code fence rendering
  - Fixed example build documentation and removed CMake target conflicts

- **Cross-Platform Compatibility**:
  - Made socket buffer size settings non-critical for CI compatibility
  - Made message socket includes portable
  - Added missing headers in example programs (mutex, cstring, string, functional)
  - Added socket headers for cross-platform htons/htonl support

- **Thread Safety**:
  - Guarded TP reassembler config with mutex
  - Added mutex headers for TP reassembler locks

- **CI/CD Improvements**:
  - Fixed CI hang issues
  - Dropped Windows job (out of scope)

### Changed

- Removed vsomeip-specific references from infra_test README
- Updated documentation for better clarity and accuracy

## [0.0.1] - Initial Release

### Added

- Initial SOME/IP stack implementation based on open-someip-spec
- Core message handling and types
- RPC client and server implementation
- Event publisher and subscriber
- Transport layer (TCP and UDP)
- TP (Transport Protocol) segmentation and reassembly
- Serialization framework
- Service Discovery (SD) client and server
- Session management
- Comprehensive test suite
- Example applications (basic and advanced)
- Architecture documentation and diagrams

---

[0.2.0]: https://github.com/vtz/opensomeip/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/vtz/opensomeip/compare/v0.0.5...v0.1.0
[0.0.5]: https://github.com/vtz/opensomeip/compare/v0.0.4...v0.0.5
[0.0.4]: https://github.com/vtz/opensomeip/compare/v0.0.3...v0.0.4
[0.0.3]: https://github.com/vtz/opensomeip/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/vtz/opensomeip/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/vtz/opensomeip/releases/tag/v0.0.1
