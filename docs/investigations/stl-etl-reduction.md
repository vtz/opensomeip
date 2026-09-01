# Investigation: Reducing heap / STL usage for ETL compatibility

**Issue:** [#174](https://github.com/vtz/opensomeip/issues/174)
**Scope:** `include/` and `src/` only (excluding `tests/`, `examples/`, and in-tree `*.md` samples under `include/`).
**Inventory:** **81** translation units / headers under `include/` + `src/` (`.h`, `.hpp`, `.cpp`).
**Method:** `rg` scans for `std::vector`, `std::string`, `std::unordered_map`, `std::map`, smart pointers, `std::function`, `std::queue`, `new`/`delete`, plus targeted file reads (e.g. `Message`, `TpConfig`, memory backends).

## Executive summary

OpenSomeIP is built around **dynamically sized byte buffers** (`std::vector<uint8_t>`) for SOME/IP payloads, serialization, SD wire format, TP segments, and RPC/event plumbing. That pattern is **on the hot path** for every send/receive. Replacing it with **fixed-capacity** containers (`etl::vector`, span + external buffer) is **architecturally significant**: bounds must be **policy** (compile-time, CMake, or runtime config), not guessed.

**Quantitative snapshot (approximate, `include/` + `src/`):**

| Pattern | Matching lines (rg) | Files with ≥1 match |
|--------|---------------------|----------------------|
| `std::vector` | ~202 | **37** |
| `std::string` | ~98 | **24** |
| `std::unordered_map` | ~19 | **12** |
| `std::shared_ptr` | ~16 | **10** |
| `std::unique_ptr` | ~32 | **16** |
| `std::function` | ~17 | **8** |
| `std::queue` | **2** | **2** (UDP/TCP transport) |
| `std::map` | **0** | **0** |

**Smart pointer note:** `MessagePtr` is already abstracted; **FreeRTOS / ThreadX / Zephyr** backends use **placement `new` in pools** for `Message` (`src/platform/*/memory.cpp`), while **POSIX** uses `std::make_shared<Message>()` (`include/platform/posix/memory_impl.h`). The **payload inside `Message` remains `std::vector<uint8_t>`** on all backends—pooling the envelope does not remove payload heap usage.

**SOME/IP payload sizing (for bounded designs):**

- The header defines **practical caps** used in validation: `DEFAULT_MAX_PAYLOAD_SIZE = 1400`, `MAX_TCP_PAYLOAD_SIZE = 65535` (`include/someip/message.h`).
- The on-wire **Length** field is **32-bit**; in principle very large logical messages are representable, but **UDP datagrams** are bounded (~**65507** bytes payload for IPv4 UDP). Larger payloads rely on **TP** segmentation; this codebase sets **`TpConfig::max_message_size` default to 1_000_000 bytes** (~1 MiB) and **`max_segment_size` ~1392** (`include/tp/tp_types.h`).
- **Industry practice:** single-frame payloads often **MTU-sized (~1400)**; **64 KiB** is a common upper bound for single-segment TCP-ish stacks; **multi-megabyte** is possible via TP but should be an **explicit product requirement** if a fixed `N` is chosen.

**Recommendation:** Treat **ETL as an optional dependency** (CMake option), default **off** for host/POSIX builds. Prefer **type aliases + capacity policies** so embedded targets can swap `std::vector` for `etl::vector` (or a **span-based view**) without forking the whole API. Making ETL **required** would complicate consumers that already standardize on STL and do not need bounded memory.

---

## Detailed findings by category

### 1. `std::vector`

#### 1.1 Core message payload (data path)

| Location | Stores | Path | Bounded? | Realistic max |
|----------|--------|------|----------|----------------|
| `include/someip/message.h` L94–96, L112–113, L166 | `payload_`, serialize/deserialize | **Data** | Config + transport | 1400 default validation; TCP path up to **65535** per constants; TP up to **`max_message_size`** (default **1 MiB**); UDP single datagram **~64 KiB** |
| `src/someip/message.cpp` | serialize buffer, E2E slice | **Data** | same | same |

**ETL:** `etl::vector<uint8_t, N>` requires **choosing N** per product (e.g. `N=1536` for MTU-shaped, `N=65536` for “max single buffer”, or `N=max_message_size+overhead` for TP receiver). **Configurable** via template parameter + alias or runtime-checked `etl::vector` with `N = SOMEIP_MAX_PAYLOAD` macro.

**Verdict:** **Hard / policy-driven** for a *universal* library; **moderate** if the project documents **supported max payload** and rejects larger wire data.

#### 1.2 Serialization (`Serializer` / `Deserializer`)

| Location | Stores | Path | Bounded? |
|----------|--------|------|----------|
| `include/serialization/serializer.h` L135–147, L172–223, templates L240+ | `buffer_`, arrays | **Data** | Same as max message / schema |
| `src/serialization/serializer.cpp` | string deserialization builds `std::string` | **Data** | Length-prefixed from wire |

**ETL:** Buffer could be `etl::vector<uint8_t, N>` with `N` tied to same policy as `Message`. Template `deserialize_array` returns `std::vector<T>` today—would need `etl::vector<T, MaxN>` or caller-provided output iterator.

**Verdict:** **Moderate** (tied to message bounds API).

#### 1.3 Transport

| Location | Stores | Path | Bounded? | Max |
|----------|--------|------|----------|-----|
| `include/transport/udp_transport.h` L109–110 | send/receive buffers | **Data** | Yes—`receive_buffer_size` in config | Config-driven |
| `src/transport/udp_transport.cpp` L64, L354, L400, L427 | serialized message, recv buffer | **Data** | same | same |
| `include/transport/tcp_transport.h` L45, L213–215 | `receive_buffer`, I/O | **Data** | Grows with stream parsing | Practical: message length from header + **`MAX_TCP_PAYLOAD_SIZE`**-scale |
| `src/transport/tcp_transport.cpp` L73, L418, L468+, L517, L571 | buffers, message extraction | **Data** | Header-driven | Same |

**ETL:** Receive ring buffers are natural **`etl::vector<uint8_t, N>`** candidates once `N ≥` configured MTU/message cap.

**Verdict:** **Easy to moderate** (align `N` with existing config constants).

#### 1.4 TP (segmentation / reassembly)

| Location | Stores | Path | Bounded? | Max |
|----------|--------|------|----------|-----|
| `include/tp/tp_types.h` L83, L96–97, L136 | segment payload, reassembly, segment list | **Data** | `max_message_size`, `max_segment_size`, `max_concurrent_transfers` | Default **1 MiB** message, **1392** B segments |
| `src/tp/tp_segmenter.cpp`, `tp_reassembler.cpp`, `tp_manager.cpp` | temp buffers, segments | **Data** | same | same |

**ETL:** `TpSegment::payload` → `etl::vector<uint8_t, MAX_SEGMENT>`; reassembly buffer → `etl::vector<uint8_t, MAX_MESSAGE>`. **`std::vector<bool>`** for `received_segments` is a special case; prefer **`etl::bitset`** or **`etl::vector<uint8_t, ceil(max_segments/8)>`** (clearer + ETL-friendly).

**Verdict:** **Moderate** (already has numeric caps in `TpConfig`—good hooks for `N`).

#### 1.5 Service Discovery (SD)

| Location | Stores | Path | Bounded? |
|----------|--------|------|----------|
| `include/sd/sd_message.h` L47–48, entries/options L214–248 | wire bytes, polymorphic entry/option lists | **Data** | SD entries/options count bounded by **UDP datagram** (~1400 B typical) |
| `src/sd/sd_message.cpp` | serialize/deserialize | **Data** | same |
| `include/sd/sd_types.h` L83 | `event_ids` in `ServiceInstance` | **Config / SD** | Small list per offer |
| `include/sd/sd_server.h` / `sd_client.h` | `std::vector<ServiceInstance>` return types | **API / SD** | Number of services **deployment-specific** |

**ETL:** Wire buffers: `etl::vector<uint8_t, N>` with N ≈ **UDP SD message max** (often **~1400**, or **64 KiB** if stack allows). Entry/option vectors: `etl::vector<std::unique_ptr<...>>` is awkward; consider **fixed-capacity variant storage** or **SoA** layout.

**Verdict:** **Moderate** (wire side easier than public “list all services” API).

#### 1.6 RPC / events / E2E

| Location | Stores | Path |
|----------|--------|------|
| `include/rpc/rpc_types.h` L65, L81, L97 | parameters / return_values | **Data** |
| `include/rpc/rpc_server.h`, `rpc_client.h` | byte vectors in signatures | **Data** |
| `src/rpc/rpc_server.cpp`, `rpc_client.cpp` | handlers, pending calls | **Data** |
| `include/events/event_types.h` L100, L127 | `event_data`, `filter_data` | **Data** |
| `include/events/event_publisher.h`, `event_subscriber.h` | publish/subscribe APIs | **Data** |
| `src/events/event_*.cpp` | payloads, client lists | **Data** |
| `include/e2e/e2e_header.h`, `e2e_crc.h` | CRC input buffers | **Data** |
| `src/e2e/e2e_crc.cpp` L122 | **allocates slice** `std::vector<uint8_t>` for CRC window | **Data** |

**Verdict:** Same payload policy as **`Message`**. The **CRC slice** in `e2e_crc.cpp` is a small **easy win**: use **stack array** or **span into existing buffer** to avoid an allocation per call.

---

### 2. `std::string`

| Location | Role | Path | Bounded? | Typical max |
|----------|------|------|----------|-------------|
| `include/transport/endpoint.h` L50, L79–80, L106 | IPv4/IPv6 text + validation | **Config / setup** | Yes | IPv6 **~45** chars; hostnames if ever used larger |
| `include/transport/udp_transport.h` L38, L84–85, L113 | multicast interface / helpers | **Setup** | Yes | Small |
| `include/sd/sd_types.h` L67, L91, L93 | addresses in config | **Setup** | Yes | Same |
| `include/sd/sd_message.h` L153–154, L193–200 | IPv4 string, configuration option string | **Data** (wire) | SD option length field | **Option-length bounded** by SD message |
| `include/events/event_types.h` L119 | `event_name` | **Metadata** | App-defined | Often **< 64** if bounded |
| `include/e2e/e2e_profile.h` / registry | profile **name** | **Setup** | Tiny set | **< 32** typical |
| `include/someip/types.h`, `common/result.h` | `to_string` enums | **Diagnostic** | Fixed set | N/A |
| `src/someip/types.cpp`, `src/common/result.cpp` | static maps → string | **Diagnostic** | Fixed | N/A |
| `src/serialization/serializer.cpp` L389+ | deserialize string | **Data** | Wire length | Schema-limited |
| `src/sd/sd_server.cpp`, `endpoint.cpp`, `event_subscriber.cpp` | parsing, subscription keys | **Mix** | Keys: bounded if format fixed | e.g. `"s:i:g"` **~20** chars |

**ETL:** `etl::string<N>` with **N** per use case (`etl::string<64>` for keys, `etl::string<256>` for config strings).

**Verdict:** **Easy wins** for **diagnostic** paths; **moderate** for **API** (`Endpoint` currently exposes `std::string`).

---

### 3. `std::unordered_map` / `std::map`

**No `std::map` in `include/` + `src/`.** `std::unordered_map` sites:

| Location | Key → Value | Path | Bounded? | Suggested max entries |
|----------|-------------|------|----------|------------------------|
| `include/core/session_manager.h` L134 | `uint16_t` → `shared_ptr<Session>` | **Data** | **Yes**—65535 IDs, active subset small | **Active sessions** (product limit, e.g. **≤ 256**) |
| `include/tp/tp_manager.h` L174 | `uint32_t` → `TpTransfer` | **Data** | **`max_concurrent_transfers`** (default **10**) | **= config** |
| `include/tp/tp_reassembler.h` L107 | `uint32_t` → `unique_ptr<TpReassemblyBuffer>` | **Data** | Same | **= concurrent transfers** |
| `include/e2e/e2e_profile_registry.h` L88–89 | id → profile; name → pointer | **Setup** | **Yes**—handful of profiles | **< 16** |
| `src/rpc/rpc_server.cpp` L208 | `MethodId` → `MethodHandler` | **Data** | Service API size | **Tens–hundreds** typical |
| `src/rpc/rpc_client.cpp` L261 | `RpcCallHandle` → pending | **Data** | Outstanding calls | **≤ configured concurrency** |
| `src/sd/sd_client.cpp` L426, L432 | service subscriptions, pending finds | **SD** | Deployment | Bounded by **services of interest** |
| `src/events/event_publisher.cpp` L343, L346, L349 | events, subscriptions, rate limit | **Data** | Event IDs **uint16_t** | **≤ registered events** |
| `src/events/event_subscriber.cpp` L332, L335 | string key → subscription / callback | **Data** | Unique subscriptions | Product limit |
| `src/e2e/e2e_profiles/standard_profile.cpp` L294–295 | data ID → counters | **Data** | E2E-protected signals | **≤ configured IDs** |
| `src/someip/types.cpp`, `src/common/result.cpp` | enum → string | **Diagnostic** | Fixed | **Compile-time table**—candidate for **`constexpr` array** (no map) |

**ETL:** `etl::unordered_map<K,V, N, N>` with **N ≥** worst-case; for **string keys**, prefer **fixed key type** (`etl::string<32>`) or **numeric composite key** to avoid string hashing.

**Verdict:** Many maps are **already effectively bounded** by **16-bit IDs** or **config** → **moderate** migration. **Easiest:** replace **diagnostic** static maps with **constexpr** data (**easy win**).

---

### 4. `std::shared_ptr` / `std::unique_ptr`

| Use | Where | Path | ETL / embedded note |
|-----|-------|------|---------------------|
| `MessagePtr` | `message.h`, transport, queues | **Data** | Pool exists on RTOS; **shared ownership** may be replaced by **`unique_ptr` + explicit queue** or **intrusive list** if lifetime simplified |
| `ITransportPtr` | `transport.h` L137 | **Setup** | Rarely hot path |
| `Session` | `session_manager.h` | **Data** | `shared_ptr` convenient; **`unique_ptr`** or **stable pool index** suffices if single owner |
| SD/RPC/Event **pimpl** `unique_ptr<Impl>` | various `*_server.h` / `*_client.h` | **Setup** | Low priority |
| `TpSegmenter` / `TpReassembler` | `tp_manager.h` | **Setup** | Low priority |
| SD entries/options | `sd_message.h` | **Data** | `unique_ptr` for polymorphism—**bounded object pool** pattern fits embedded |

**Verdict:** **Moderate** (API + lifetime). **Pool already partially solves `Message`**.

---

### 5. `std::function`

| Location | Signature | Path | `etl::delegate` notes |
|----------|-----------|------|------------------------|
| `include/tp/tp_types.h` L154–156 | TP callbacks | **Data** | Fixed signature; **delegate** fits if **storage size** documented |
| `include/sd/sd_types.h` L107–109 | SD callbacks | **SD** | Same |
| `include/rpc/rpc_types.h` L90 | `RpcCallback` | **Data** | Same |
| `include/rpc/rpc_server.h` L35+ | `MethodHandler` | **Data** | Same |
| `include/events/event_types.h` L136–137 | event notifications | **Data** | Same |
| `include/platform/freertos/thread_impl.h` (and **ThreadX**, **Zephyr**) | `new std::function<void()>` for thread entry | **Platform** | **High-value target:** replace with **`etl::delegate`** or **function pointer + void\*** to remove **heap + std::function** |

**Verdict:** Public API uses **`std::function`** extensively → **moderate to hard** (ABI/API). **RTOS thread wrapper** is a **focused easy win** relative to user-visible API.

---

### 6. `std::queue`

| Location | Element | Path |
|----------|---------|------|
| `include/transport/udp_transport.h` L95 | `MessagePtr` | **Data**—receive path |
| `include/transport/tcp_transport.h` L197 | `pair<MessagePtr, Endpoint>` | **Data** |

Backed by `std::deque` (typically **heap**). **ETL:** `etl::queue<MessagePtr, N>` or **intrusive mailbox** with **fixed depth** (`N` = max queued messages).

**Verdict:** **Moderate**—needs **depth policy**.

---

### 7. `new` / `delete` (non-placement)

| Location | What | Notes |
|----------|------|-------|
| `include/platform/freertos/thread_impl.h`, `threadx/thread_impl.h`, `zephyr/thread_impl.h` | `new std::function<void()>(...)` | **Heap**; paired with `delete` in thread teardown |
| `src/platform/*/memory.cpp` | **`new (block) Message()`** | **Placement** into pool—**not general heap** |

---

### 8. Other heap-friendly patterns

| Pattern | Where | Notes |
|---------|-------|-------|
| `std::stringstream` | `src/someip/message.cpp` L547, `src/transport/endpoint.cpp` L87 | **Diagnostic / `to_string`**—low priority; could use **`etl::to_string`** or small stack buffer |
| `std::optional<E2EHeader>` | `message.h` | **Not heap** (value in `Message`); OK for embedded |

---

## Categorization summary

### Easy wins

- Replace **diagnostic** `std::unordered_map<Enum, std::string>` in `types.cpp` / `result.cpp` with **`constexpr` tables** or `std::array` + linear search (tiny cardinality).
- **`e2e_crc.cpp` CRC slice**: avoid allocating `std::vector<uint8_t>`; use **subrange view** into caller buffer.
- **RTOS/ThreadX/Zephyr thread context**: remove **`new`/`delete` of `std::function`** in favor of **`etl::delegate`** or **C-style context + fn pointer**.

### Moderate effort

- **Transport** receive buffers and **SD wire buffers**: align with **existing size config** → `etl::vector<uint8_t, N>`.
- **TP**: map `TpConfig` limits directly to **ETL container sizes**; replace **`vector<bool>`**.
- **Maps** keyed by integers with **small max population** → `etl::unordered_map` with explicit bucket size.
- **Endpoint / config strings** → `etl::string<N>` **or** keep `std::string` on host only behind alias.

### Hard / not feasible (without product policy)

- **Universal `Message::payload`** as a single fixed `etl::vector` unless the project **documents and enforces** a **maximum payload** smaller than what the wire can express (especially with **TP** and large `max_message_size`).
- **Replacing all public `std::function` callbacks** without an **API revision** (breaking change) or **dual API** (STL vs ETL).

### Not recommended to chase (for embedded gain)

- **Enum `to_string`**, **logging-oriented** code paths, **pimpl** indirections—**heap amortized** and **not on wire path**.

---

## Recommended approach

1. **Policy first:** Publish **Supported maximum payload**, **max SD message size**, **max TP reassembled size**, and **max queued messages** as **documented configuration**; enforce in validation (already partially present in `Message` / `TpConfig`).
2. **Abstraction layer (preferred for library ergonomics):** Introduce internal aliases, e.g. `someip::byte_buffer` / `someip::small_string`, defaulting to `std::vector` / `std::string` on POSIX; **opt-in** `SOMEIP_USE_ETL` switches to `etl::*` with **CMake-exposed capacity macros**.
3. **Direct ETL in core types** is faster long-term but **forces** every downstream user to depend on ETL and **fixed sizes**—acceptable only for **embedded-only** distributions.
4. **Keep `std::function` on host** until a **v2 API** can standardize on **`etl::delegate`** or **function_ref**-style non-owning callbacks.

---

## Prioritized follow-up tickets (suggested)

1. **Policy doc + enforcement:** Single source of truth for **max payload** (UDP vs TCP vs TP), align `Message` validation with `TpConfig::max_message_size` and transport buffer sizes.
2. **Alias layer:** `someip::byte_buffer` / `small_string` with STL default + optional ETL backend (`SOMEIP_USE_ETL`).
3. **Platform:** Remove heap `std::function` from **FreeRTOS/ThreadX/Zephyr** `thread_impl.h` (delegate or fn pointer).
4. **E2E:** Eliminate per-call **`vector` allocation** in `e2e_crc.cpp` slice path.
5. **Diagnostics:** Replace static **`unordered_map` → string** with **constexpr** tables.
6. **TP:** Replace **`std::vector<bool>`** segment bitmap with **bitset** or byte vector; bound segment list with **`max_message_size` / `max_segment_size`**.
7. **Transport:** Bounded **`etl::queue` or ring buffer** for `receive_queue_` / TCP `message_queue_` with explicit depth.
8. **API (major):** Callback type migration (`std::function` → **`etl::delegate`** or non-owning **`function_ref`**) behind versioned API.

---

## Should ETL be optional or required?

| Option | Pros | Cons |
|--------|------|------|
| **Optional (recommended)** | Host builds stay simple; POSIX users avoid extra dep; embedded opts in with known `N`. | Two configurations to test; conditional compilation discipline. |
| **Required** | Single container story; forces bounded thinking. | Alienates generic STL consumers; versioning pain; harder **Python bindings** / **host tools**. |

**Conclusion:** **Optional dependency** with a **clear, tested** `SOMEIP_USE_ETL` (or similar) configuration is the best fit for a **multi-platform** SOME/IP stack. Use **ETL-compatible patterns** (spans, fixed pools, no hidden allocations) even when STL types remain.

---

## File index (quick reference)

**`std::vector` (37 files):**
`include/e2e/e2e_crc.h`, `e2e_header.h`, `events/event_{publisher,subscriber}.h`, `event_types.h`, `rpc/rpc_{client,server}.h`, `rpc_types.h`, `sd/sd_{client,message,server}.h`, `sd_types.h`, `serialization/serializer.h`, `someip/message.h`, `tp/tp_{manager,reassembler,segmenter,types}.h`, `transport/tcp_transport.h`, `udp_transport.h`, and mirrored `src/**` implementations under `e2e/`, `events/`, `rpc/`, `sd/`, `serialization/`, `someip/`, `tp/`, `transport/`.

**`std::unordered_map` (12 files):**
`include/core/session_manager.h`, `e2e/e2e_profile_registry.h`, `tp/tp_{manager,reassembler}.h`, `src/sd/sd_client.cpp`, `rpc/rpc_{server,client}.cpp`, `events/event_{publisher,subscriber}.cpp`, `e2e/e2e_profiles/standard_profile.cpp`, `someip/types.cpp`, `common/result.cpp`.

---

*Generated as part of issue #174 investigation; numbers from repository scans at investigation time.*
