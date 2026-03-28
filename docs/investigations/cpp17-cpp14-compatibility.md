# C++17 dependencies vs. C++14 compatibility (opensomeip core)

**Scope:** `include/` and `src/` only (not `tests/` or `examples/`).  
**Related:** GitHub issue [#171](https://github.com/vtz/opensomeip/issues/171).

## Executive summary

**Can we support C++14?** Yes, in principle. The codebase does not use the heaviest C++17 facilities (no `std::filesystem`, `std::variant`, `std::string_view`, coroutines, or nested namespace definitions). The work clusters into a few mechanical themes: replacing **`std::optional`** (and its use in public headers), rewriting **`if constexpr` / `std::is_same_v`** dispatch in the serializer templates, replacing **`std::apply`** in three RTOS thread implementations, fixing one **structured binding**, one **`inline static` data member**, and optionally downgrading or guarding **`[[nodiscard]]` / `[[maybe_unused]]`**.

**Effort (rough):** on the order of **several days** for a careful port plus **CI matrix** work (a C++14 build, and optionally Zephyr’s `-std=c++17` flag review). The largest risk is **public API**: `someip::Message::get_e2e_header()` and `e2e::E2EProtection::extract_header()` expose `std::optional` in installed headers. Any replacement type (`boost::optional`, `absl::optional`, or a small in-tree optional) **changes the public surface** unless you standardize on a C++14-compatible optional in headers for all supported standards.

**Recommendation:** Prefer **dual-standard support** via a small abstraction (e.g. `someip::Optional<T>` typedef to `std::optional` when `__cplusplus >= 201703L`, else a bundled or third-party optional) **or** a **`#ifdef` split** only if the project accepts maintaining two code paths. A **full C++14-only** tree is feasible without redesigning architecture; it is mostly substitution and template refactors.

---

## Standard / build system note

| Location | Finding |
|----------|---------|
| `CMakeLists.txt` (root) | `CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON` |
| `zephyr/CMakeLists.txt` | `-std=c++17` |

Dropping the language requirement to C++14 (or making it configurable) is a prerequisite for validating the port.

---

## C++17 standard library headers checked

| Header | Present in `include/` / `src/`? |
|--------|-----------------------------------|
| `<optional>` | **Yes** — `include/someip/message.h`, `include/serialization/serializer.h`, `include/e2e/e2e_protection.h`, `include/e2e/e2e_header.h` |
| `<string_view>` | **No** |
| `<variant>` | **No** |
| `<any>` | **No** |
| `<filesystem>` | **No** |
| `<shared_mutex>` | **No** |

**Note:** `include/e2e/e2e_header.h` includes `<optional>` but the declarations in that header do not use `std::optional`; the include appears **redundant** and could be removed as cleanup.

---

## Detailed findings

### 1. `std::optional`

| File | Line(s) | Role in context | C++14 alternative | Straightforward? | Public API impact |
|------|---------|-----------------|-------------------|------------------|-------------------|
| `include/someip/message.h` | 22 (`#include`), 133, 147–149, 169 | Optional E2E header on `Message`; `get_e2e_header()` returns `std::optional<e2e::E2EHeader>` | Use `boost::optional`, `absl::optional`, or a minimal `someip::optional` in headers; keep `has_value()`-style API or match chosen type’s API | **Moderate** (touches serialization and all call sites) | **Yes** — return type and member type are public |
| `include/serialization/serializer.h` | 21, 80–88, 92, 227–234 | `DeserializationResult` stores value in `std::optional<T>`; `read_be_*` return `std::optional<…>` | Same optional abstraction; `get_value()` / `move_value()` can use `*opt` or `.get()` after `has_value()` checks instead of `.value()` | **Straightforward** with a shared optional type | **Yes** — template class in public header |
| `include/e2e/e2e_protection.h` | 22, 73 | `extract_header()` returns `std::optional<E2EHeader>` | Same as above | **Moderate** (callers in `src/`) | **Yes** |
| `include/e2e/e2e_header.h` | 20 | Include only (no use in this file) | Remove include, or keep only if switching to a custom optional used here | **Trivial** | **No** |
| `src/serialization/serializer.cpp` | 436–447, 458, 478–486, 494, 502, 512 | Implementations of `read_be_*` returning `std::optional` | Return same replacement optional type; construct with `nullopt` equivalent | **Straightforward** | **No** (implementation) |
| `src/e2e/e2e_protection.cpp` | 86 | Returns optional from parsing | Same optional type | **Straightforward** | **No** |
| `src/e2e/e2e_profiles/standard_profile.cpp` | 135–140 | Local `header_opt`, `has_value()`, `.value()` | Use replacement optional; prefer explicit check + `*` after guard instead of throwing `.value()` | **Straightforward** | **No** |
| `src/someip/message.cpp` | 84, 118, 169–170, 269, 467, 532 | Copy ctor, length, serialize paths using `has_value()` and `operator->` | Equivalent methods on replacement optional | **Straightforward** | **No** |

**Example (replace throwing `.value()` after a guard):**

```cpp
// C++17 (current pattern in core)
if (!header_opt.has_value()) { return false; }
const E2EHeader& header = header_opt.value();

// C++14-friendly (works with boost::optional, absl::optional, or * after assert)
if (!header_opt) { return false; }
const E2EHeader& header = *header_opt;
```

---

### 2. `if constexpr` and `std::is_same_v`

| File | Line(s) | Role in context | C++14 alternative | Straightforward? | Public API impact |
|------|---------|-----------------|-------------------|------------------|-------------------|
| `include/serialization/serializer.h` | 246–273 (`serialize_array`), 285–312 (`deserialize_array`) | Dispatch serialization/deserialization per element type `T` without runtime overhead | **Tag dispatch** with overloads, or **`std::enable_if`** / SFINAE on helper templates, or a macro-generated `switch` on a type enum | **Moderate** (verbose but mechanical) | **No** (behavior unchanged) |

**Example sketch (tag dispatch):**

```cpp
// C++14: overload helpers instead of if constexpr
template<typename T> void serialize_elem(const T&) {
    static_assert(sizeof(T) == 0, "unsupported");
}
inline void serialize_elem(bool e) { serialize_bool(e); }
inline void serialize_elem(uint8_t e) { serialize_uint8(e); }
// ... one inline overload per supported type ...

template<typename T>
void Serializer::serialize_array(const std::vector<T>& array) {
    serialize_uint32(static_cast<uint32_t>(array.size()));
    for (const auto& element : array) {
        serialize_elem(element);
    }
}
```

Use `std::is_same<T, U>::value` instead of `std::is_same_v<T, U>` if any `_v` traits remain elsewhere.

---

### 3. Structured bindings

| File | Line(s) | Role in context | C++14 alternative | Straightforward? | Public API impact |
|------|---------|-----------------|-------------------|------------------|-------------------|
| `src/transport/tcp_transport.cpp` | 90 | Pops `std::pair<MessagePtr, Endpoint>` from `message_queue_` | Use `.first` / `.second` on `front()` | **Trivial** | **No** |

**Example:**

```cpp
// C++17
auto [message, sender] = message_queue_.front();

// C++14
const auto& queued = message_queue_.front();
MessagePtr message = queued.first;
const Endpoint& sender = queued.second;  // if needed
```

---

### 4. `std::apply`

| File | Line(s) | Role in context | C++14 alternative | Straightforward? | Public API impact |
|------|---------|-----------------|-------------------|------------------|-------------------|
| `include/platform/freertos/thread_impl.h` | 155 | Invoke callable with tuple of thread args inside lambda | Implement `someip::detail::apply(Fn&&, Tuple&&)` with `std::index_sequence` + `std::get<I>` | **Straightforward** (small header-only helper, shared by three files) | **No** |
| `include/platform/threadx/thread_impl.h` | 141 | Same pattern | Same helper | **Straightforward** | **No** |
| `include/platform/zephyr/thread_impl.h` | 92 | Same pattern | Same helper | **Straightforward** | **No** |

**Example C++14 helper (single definition, include from all three):**

```cpp
namespace someip { namespace detail {
template <class F, class Tuple, std::size_t... I>
decltype(auto) apply_impl(F&& f, Tuple&& t, std::index_sequence<I...>) {
    return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
}
template <class F, class Tuple>
decltype(auto) apply(F&& f, Tuple&& t) {
    using Raw = typename std::decay<Tuple>::type;
    constexpr std::size_t N = std::tuple_size<Raw>::value;
    return apply_impl(std::forward<F>(f), std::forward<Tuple>(t),
                      std::make_index_sequence<N>{});
}
}}  // namespace someip::detail
```

---

### 5. `[[nodiscard]]`

| File | Line(s) | Role in context | C++14 alternative | Straightforward? | Public API impact |
|------|---------|-----------------|-------------------|------------------|-------------------|
| `include/transport/transport.h` | 78, 91, 97, 121, 127 | Encourage checking `Result` from transport operations | Remove attributes; or use `__attribute__((warn_unused_result))` / `[[gnu::warn_unused_result]]` under compiler guards | **Trivial** | **No** (attribute only) |
| `include/transport/tcp_transport.h` | 101, 109 | Same | Same | **Trivial** | **No** |
| `include/transport/udp_transport.h` | 72 | Same | Same | **Trivial** | **No** |

---

### 6. `[[maybe_unused]]`

| File | Line(s) | Role in context | C++14 alternative | Straightforward? | Public API impact |
|------|---------|-----------------|-------------------|------------------|-------------------|
| `src/someip/message.cpp` | 300, 356, 369 | Silence unused locals | `(void)service_id;` or omit variable if truly unused | **Trivial** | **No** |
| `src/sd/sd_server.cpp` | 482 | Silence unused `client_protocol` | `(void)client_protocol;` or comment why kept | **Trivial** | **No** |

---

### 7. `inline static` data member (C++17 “inline variable” for static members)

| File | Line(s) | Role in context | C++14 alternative | Straightforward? | Public API impact |
|------|---------|-----------------|-------------------|------------------|-------------------|
| `include/platform/threadx/thread_impl.h` | 226 | `s_registry` array of `std::atomic<Thread*>` | Declare in class: `static std::atomic<Thread*> s_registry[kMaxSlots];` and **define once** in `src/platform/threadx/thread.cpp` (or a single `.cpp` in ThreadX build): `std::atomic<Thread*> Thread::s_registry[kMaxSlots] = {};` | **Moderate** (must ensure one definition in exactly one TU per link) | **No** |

---

## Features explicitly searched and not found in `include/` / `src/`

- `std::byte`, `std::invoke` (beyond `apply`), `std::void_t`, `std::conjunction` / `std::disjunction`, fold expressions, class template argument deduction guides, `if (init; cond)` / `switch (init; val)`, `__has_include`, nested `namespace a::b { }`, `[[fallthrough]]`, `std::filesystem`, `std::variant`, `std::any`, `std::string_view`, `std::shared_mutex`.

*(Lambda init captures like `[x = expr]` in thread lambdas are **C++14**.)*

---

## Recommended approach

1. **Dual-standard (recommended):** Introduce a documented optional type for headers (`std::optional` when C++17+, otherwise bundled or external optional), refactor serializer dispatch without `if constexpr`, add `detail::apply`, fix structured binding and `inline static`, adjust CMake/Zephyr flags for a **C++14 configuration**, add at least one **CI job** compiling core with C++14.
2. **C++14-only:** Same code changes, but drop `std::optional` entirely from public headers in favor of one chosen optional type for all builds.
3. **Not feasible:** Only if the project refuses any third-party or bundled optional and cannot accept API churn—in that case staying on C++17 is simpler.

---

## Proposed follow-up tickets

1. **Optional abstraction:** Add `someip/optional.h` (or policy for Boost/absl) and migrate `Message`, `DeserializationResult`, E2E APIs, and `Deserializer::read_be_*`.
2. **Serializer:** Replace `if constexpr` / `std::is_same_v` in `serializer.h` with C++14-friendly dispatch (tag dispatch or overload set).
3. **RTOS threads:** Add `someip::detail::apply` and replace `std::apply` in FreeRTOS, ThreadX, and Zephyr `thread_impl.h`.
4. **ThreadX:** Move `inline static s_registry` definition to a single translation unit for C++14.
5. **TCP transport:** Replace structured binding in `tcp_transport.cpp` with explicit `pair` access.
6. **Attributes / cleanup:** Replace `[[nodiscard]]` / `[[maybe_unused]]` with C++14-compatible patterns or compiler-specific attributes; remove unused `<optional>` from `e2e_header.h` if still unused.
7. **Build / CI:** Make `CMAKE_CXX_STANDARD` (and Zephyr `-std`) configurable; add a C++14 build to the pipeline.

---

## Audit method

Ripgrep-style searches across `include/**/*.h` and `src/**/*.{h,cpp}` for: `std::optional`, `<optional>`, `if constexpr`, `std::is_same_v`, `[[nodiscard]]`, `[[maybe_unused]]`, `[[fallthrough]]`, `std::apply`, structured-binding-like `auto [`, nested namespaces, and the standard headers listed above; manual review of hits and of `CMakeLists.txt` / Zephyr build flags.
