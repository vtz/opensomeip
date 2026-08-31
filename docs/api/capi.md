<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  SPDX-License-Identifier: Apache-2.0
-->

# C API User Guide

The OpenSOME/IP C API provides a stable `extern "C"` interface for integrating
the SOME/IP protocol stack into C projects, embedded toolchains, and
foreign-language bindings (Rust, Python, etc.).

The entire public surface is contained in a **single header**:

```c
#include <opensomeip.h>
```

## Table of Contents

- [Building with the C API](#building-with-the-c-api)
- [CMake Integration](#cmake-integration)
- [Error Handling](#error-handling)
- [Version Checking](#version-checking)
- [Messages](#messages)
- [Serialization and Deserialization](#serialization-and-deserialization)
- [UDP Transport](#udp-transport)
- [TCP Transport](#tcp-transport)
- [RPC (Remote Procedure Calls)](#rpc-remote-procedure-calls)
- [Service Discovery](#service-discovery)
- [Events (Publish/Subscribe)](#events-publishsubscribe)
- [TP (Transport Protocol / Large Messages)](#tp-transport-protocol--large-messages)
- [E2E Protection](#e2e-protection)
- [Thread Safety](#thread-safety)
- [Lifetime and Ownership Rules](#lifetime-and-ownership-rules)

---

## Building with the C API

Build OpenSOME/IP with the C API enabled:

```bash
mkdir build && cd build
cmake .. -DBUILD_CAPI=ON
cmake --build . -j$(nproc)
sudo cmake --install .
```

This installs:

- `include/capi/opensomeip.h` — the public C header
- `lib/libopensomeip.a` — core C++ library
- `lib/libopensomeip_capi.a` — C wrapper library
- `lib/cmake/opensomeip/` — CMake package files

## CMake Integration

### As a subdirectory

```cmake
set(BUILD_CAPI ON)
add_subdirectory(vendor/opensomeip)

add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE opensomeip_capi)
```

### With find_package (after install)

```cmake
cmake_minimum_required(VERSION 3.14)
project(my_someip_consumer C)

find_package(opensomeip REQUIRED)

add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE opensomeip::opensomeip_capi)
```

The `opensomeip::opensomeip_capi` target transitively links the C++ core
library, so you only need to link `opensomeip_capi`.

### pkg-config (alternative)

If you are not using CMake, link against both libraries:

```bash
cc -I/usr/local/include/capi -o my_app main.c \
   -L/usr/local/lib -lopensomeip_capi -lopensomeip -lstdc++
```

---

## Error Handling

Every C API function returns `opensomeip_result_t`. Check for
`OPENSOMEIP_RESULT_SUCCESS` (0):

```c
opensomeip_message_t* msg = NULL;
opensomeip_result_t rc = opensomeip_message_create(&msg);
if (rc != OPENSOMEIP_RESULT_SUCCESS) {
    fprintf(stderr, "Failed to create message: 0x%02X\n", rc);
    return 1;
}
```

Common result codes:

| Code | Meaning |
|------|---------|
| `OPENSOMEIP_RESULT_SUCCESS` (0x00) | Operation succeeded |
| `OPENSOMEIP_RESULT_INVALID_ARGUMENT` (0x61) | NULL pointer or bad parameter |
| `OPENSOMEIP_RESULT_BUFFER_OVERFLOW` (0x31) | Caller buffer too small; `*out_len` set to required size |
| `OPENSOMEIP_RESULT_OUT_OF_MEMORY` (0x30) | Allocation failed |
| `OPENSOMEIP_RESULT_INTERNAL_ERROR` (0x63) | Unhandled C++ exception caught by firewall |

The full enumeration is in `opensomeip.h`.

---

## Version Checking

Compile-time version macros:

```c
#if OPENSOMEIP_CAPI_VERSION_MAJOR != 0
#error "Incompatible opensomeip major version"
#endif
```

Runtime version query:

```c
uint32_t v = opensomeip_capi_version();
uint16_t major = (v >> 16) & 0xFFFF;
uint16_t minor = (v >> 8)  & 0xFF;
uint16_t patch =  v        & 0xFF;
printf("opensomeip C API %u.%u.%u\n", major, minor, patch);
```

---

## Messages

A `opensomeip_message_t` wraps a SOME/IP message with header fields and
an optional payload.

### Create and populate

```c
opensomeip_message_t* msg = NULL;
opensomeip_message_create(&msg);

opensomeip_message_set_service_id(msg, 0x1234);
opensomeip_message_set_method_id(msg, 0x0001);
opensomeip_message_set_client_id(msg, 0xCAFE);
opensomeip_message_set_session_id(msg, 0x0001);
opensomeip_message_set_protocol_version(msg, 0x01);
opensomeip_message_set_interface_version(msg, 0x01);
opensomeip_message_set_message_type(msg, OPENSOMEIP_MSG_REQUEST);
opensomeip_message_set_return_code(msg, OPENSOMEIP_RC_E_OK);

const uint8_t payload[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; /* "Hello" */
opensomeip_message_set_payload(msg, payload, sizeof(payload));
```

### Read header fields

```c
uint16_t svc = 0;
opensomeip_message_get_service_id(msg, &svc);
```

### Get payload

```c
size_t plen = 0;
opensomeip_message_get_payload_length(msg, &plen);

uint8_t* buf = malloc(plen);
size_t   cap = plen;
opensomeip_message_get_payload(msg, buf, &cap);
/* cap now holds actual bytes copied */
free(buf);
```

If the buffer is too small, `opensomeip_message_get_payload` returns
`OPENSOMEIP_RESULT_BUFFER_OVERFLOW` and sets `*out_len` to the required size.

### Serialize / deserialize

```c
/* Serialize to wire format */
size_t wire_len = 4096;
uint8_t wire[4096];
opensomeip_message_serialize(msg, wire, &wire_len);

/* Deserialize back */
opensomeip_message_t* msg2 = NULL;
opensomeip_message_create(&msg2);
opensomeip_message_deserialize(msg2, wire, wire_len);
```

### Cleanup

```c
opensomeip_message_destroy(msg);
opensomeip_message_destroy(msg2);
```

---

## Serialization and Deserialization

The serializer/deserializer API works with typed values (integers, byte
arrays) independent of message framing.

### Serializer

```c
opensomeip_serializer_t* ser = NULL;
opensomeip_serializer_create(&ser);

opensomeip_serializer_write_uint16(ser, 0x1234);
opensomeip_serializer_write_uint32(ser, 42);
opensomeip_serializer_write_bytes(ser, (const uint8_t*)"data", 4);

/* Extract serialized bytes */
size_t len = 0;
opensomeip_serializer_get_size(ser, &len);

uint8_t* buf = malloc(len);
size_t cap = len;
opensomeip_serializer_get_data(ser, buf, &cap);

/* Use buf[0..cap] as payload */
opensomeip_message_set_payload(msg, buf, cap);

free(buf);
opensomeip_serializer_destroy(ser);
```

### Deserializer

```c
opensomeip_deserializer_t* de = NULL;
opensomeip_deserializer_create(&de, payload_data, payload_len);

uint16_t val16 = 0;
uint32_t val32 = 0;
opensomeip_deserializer_read_uint16(de, &val16);
opensomeip_deserializer_read_uint32(de, &val32);

uint8_t raw[4];
size_t raw_len = sizeof(raw);
opensomeip_deserializer_read_bytes(de, raw, &raw_len);

opensomeip_deserializer_destroy(de);
```

---

## UDP Transport

### Loopback example (send and receive)

```c
/* Create message */
opensomeip_message_t* msg = NULL;
opensomeip_message_create(&msg);
opensomeip_message_set_service_id(msg, 0x1234);
opensomeip_message_set_method_id(msg, 0x0001);

/* Local endpoint */
opensomeip_endpoint_t local = {0};
strncpy(local.address, "127.0.0.1", sizeof(local.address) - 1);
local.port = 30490;
local.protocol = OPENSOMEIP_TRANSPORT_UDP;

/* Create and start transport */
opensomeip_udp_transport_t* udp = NULL;
opensomeip_result_t rc = opensomeip_udp_transport_create(&udp, &local);
if (rc != OPENSOMEIP_RESULT_SUCCESS) { /* handle error */ }

opensomeip_udp_transport_start(udp);

/* Send to self */
opensomeip_udp_transport_send(udp, msg, &local);

/* Receive */
opensomeip_message_t* rx = NULL;
opensomeip_endpoint_t sender = {0};
opensomeip_udp_transport_receive(udp, &rx, &sender);

/* Cleanup */
opensomeip_message_destroy(rx);
opensomeip_udp_transport_stop(udp);
opensomeip_udp_transport_destroy(udp);
opensomeip_message_destroy(msg);
```

---

## TCP Transport

```c
opensomeip_tcp_transport_t* tcp = NULL;
opensomeip_tcp_transport_create(&tcp);

opensomeip_endpoint_t local = {0};
strncpy(local.address, "127.0.0.1", sizeof(local.address) - 1);
local.port = 30491;
local.protocol = OPENSOMEIP_TRANSPORT_TCP;

opensomeip_tcp_transport_initialize(tcp, &local);
opensomeip_tcp_transport_start(tcp);

opensomeip_endpoint_t remote = {0};
strncpy(remote.address, "192.168.1.100", sizeof(remote.address) - 1);
remote.port = 30491;
remote.protocol = OPENSOMEIP_TRANSPORT_TCP;

opensomeip_tcp_transport_connect(tcp, &remote);

/* Send message */
opensomeip_tcp_transport_send(tcp, msg, &remote);

/* Cleanup */
opensomeip_tcp_transport_disconnect(tcp);
opensomeip_tcp_transport_stop(tcp);
opensomeip_tcp_transport_destroy(tcp);
```

---

## RPC (Remote Procedure Calls)

### Synchronous client call

```c
opensomeip_rpc_client_t* client = NULL;
opensomeip_rpc_client_create(&client, 0xCAFE);
opensomeip_rpc_client_initialize(client);

const uint8_t request[] = {0x01, 0x02, 0x03};
uint8_t response[256];
size_t  response_len = sizeof(response);

opensomeip_result_t rc = opensomeip_rpc_client_call_sync(
    client,
    0x1234,          /* service_id */
    0x0001,          /* method_id */
    request, sizeof(request),
    response, &response_len,
    5000             /* timeout_ms */
);

if (rc == OPENSOMEIP_RESULT_SUCCESS) {
    printf("Got %zu bytes back\n", response_len);
}

opensomeip_rpc_client_shutdown(client);
opensomeip_rpc_client_destroy(client);
```

### Asynchronous client call

```c
static void on_response(opensomeip_result_t result,
                         const uint8_t* data, size_t len,
                         void* user_data) {
    if (result == OPENSOMEIP_RESULT_SUCCESS) {
        printf("Async response: %zu bytes\n", len);
    }
}

uint32_t handle = 0;
opensomeip_rpc_client_call_async(
    client,
    0x1234, 0x0001,
    request, sizeof(request),
    on_response, NULL, /* user_data */
    5000,              /* timeout_ms */
    &handle
);
```

### Server with method handler

```c
static opensomeip_result_t echo_handler(
    uint16_t client_id, uint16_t session_id,
    const uint8_t* input, size_t input_len,
    uint8_t* output, size_t* output_len,
    void* user_data)
{
    if (*output_len < input_len) {
        *output_len = input_len;
        return OPENSOMEIP_RESULT_BUFFER_OVERFLOW;
    }
    memcpy(output, input, input_len);
    *output_len = input_len;
    return OPENSOMEIP_RESULT_SUCCESS;
}

opensomeip_rpc_server_t* server = NULL;
opensomeip_rpc_server_create(&server, 0x1234);
opensomeip_rpc_server_initialize(server);
opensomeip_rpc_server_register_method(server, 0x0001, echo_handler, NULL);

/* ... run event loop ... */

opensomeip_rpc_server_unregister_method(server, 0x0001);
opensomeip_rpc_server_shutdown(server);
opensomeip_rpc_server_destroy(server);
```

---

## Service Discovery

### Client: find services

```c
static void on_found(uint16_t service_id, uint16_t instance_id,
                      const opensomeip_endpoint_t* ep, void* user_data) {
    printf("Found service 0x%04X instance 0x%04X at %s:%u\n",
           service_id, instance_id, ep->address, ep->port);
}

opensomeip_sd_client_t* sd = NULL;
opensomeip_sd_client_create(&sd);
opensomeip_sd_client_initialize(sd);
opensomeip_sd_client_find_service(sd, 0x1234, on_found, NULL, 5000);
opensomeip_sd_client_shutdown(sd);
opensomeip_sd_client_destroy(sd);
```

### Server: offer a service

```c
opensomeip_sd_server_t* sd_srv = NULL;
opensomeip_sd_server_create(&sd_srv);
opensomeip_sd_server_initialize(sd_srv);

opensomeip_endpoint_t ep = {0};
strncpy(ep.address, "192.168.1.10", sizeof(ep.address) - 1);
ep.port = 30490;
ep.protocol = OPENSOMEIP_TRANSPORT_UDP;

opensomeip_sd_server_offer_service(sd_srv, 0x1234, 0x0001, &ep);

/* ... wait for clients ... */

opensomeip_sd_server_stop_offer(sd_srv, 0x1234, 0x0001);
opensomeip_sd_server_shutdown(sd_srv);
opensomeip_sd_server_destroy(sd_srv);
```

---

## Events (Publish/Subscribe)

### Publisher

```c
opensomeip_event_publisher_t* pub = NULL;
opensomeip_event_publisher_create(&pub, 0x1234, 0x0001);
opensomeip_event_publisher_initialize(pub);
opensomeip_event_publisher_register(pub, 0x8001, 0x01);

const uint8_t data[] = {0x42, 0x00};
opensomeip_event_publisher_notify(pub, 0x8001, data, sizeof(data));

opensomeip_event_publisher_unregister(pub, 0x8001);
opensomeip_event_publisher_shutdown(pub);
opensomeip_event_publisher_destroy(pub);
```

### Subscriber

```c
static void on_event(uint16_t service_id, uint16_t instance_id,
                      uint16_t event_id,
                      const uint8_t* data, size_t data_len,
                      void* user_data) {
    printf("Event 0x%04X: %zu bytes\n", event_id, data_len);
}

opensomeip_event_subscriber_t* sub = NULL;
opensomeip_event_subscriber_create(&sub, 0xCAFE);
opensomeip_event_subscriber_initialize(sub);

opensomeip_event_subscriber_subscribe(sub, 0x1234, 0x0001, 0x01,
                                       on_event, NULL);

/* ... receive events ... */

opensomeip_event_subscriber_unsubscribe(sub, 0x1234, 0x0001, 0x01);
opensomeip_event_subscriber_shutdown(sub);
opensomeip_event_subscriber_destroy(sub);
```

---

## TP (Transport Protocol / Large Messages)

For payloads exceeding the transport MTU, the TP layer handles
segmentation and reassembly.

```c
opensomeip_tp_manager_t* tp = NULL;
opensomeip_tp_manager_create(&tp);
opensomeip_tp_manager_initialize(tp);

/* Check if segmentation is needed */
int needs = 0;
opensomeip_tp_needs_segmentation(tp, large_payload, large_len, &needs);

if (needs) {
    uint8_t seg_buf[8192];
    size_t  seg_len = sizeof(seg_buf);
    opensomeip_tp_segment(tp, msg, seg_buf, &seg_len);
    /* Send seg_buf segments individually */
}

/* Reassembly side */
int complete = 0;
uint8_t reasm_buf[65536];
size_t  reasm_len = sizeof(reasm_buf);
opensomeip_tp_reassemble(tp, segment_data, segment_len,
                          reasm_buf, &reasm_len, &complete);
if (complete) {
    /* reasm_buf contains the fully reassembled payload */
}

opensomeip_tp_manager_shutdown(tp);
opensomeip_tp_manager_destroy(tp);
```

---

## E2E Protection

Apply or verify End-to-End CRC protection on messages:

```c
opensomeip_e2e_t* e2e = NULL;
opensomeip_e2e_create(&e2e);

/* Protect before sending */
opensomeip_e2e_protect(e2e, msg, /*data_id=*/0x0100, /*counter=*/0);

/* Check after receiving */
opensomeip_result_t rc = opensomeip_e2e_check(e2e, rx_msg, /*data_id=*/0x0100);
if (rc != OPENSOMEIP_RESULT_SUCCESS) {
    /* E2E check failed — message integrity compromised */
}

opensomeip_e2e_destroy(e2e);
```

---

## Thread Safety

- **Different handles from different threads**: safe without external
  synchronization. Each opaque handle is independent.
- **Same handle from multiple threads**: requires caller synchronization
  (e.g., a mutex) unless the function is documented as thread-safe.
- **Callbacks**: are invoked from the library's internal thread.
  Callbacks must not call `_destroy()` on the handle delivering the callback.

## Lifetime and Ownership Rules

1. Every `_create()` call must be paired with exactly one `_destroy()` call.
2. After `_destroy()`, the handle is invalid — do not use it.
3. Passing `NULL` to any function returns `OPENSOMEIP_RESULT_INVALID_ARGUMENT`
   without crashing.
4. **Copy-out semantics**: `get_payload`, `get_data`, and similar functions
   copy data into a caller-provided buffer. The library does not hand out
   internal pointers.
5. **Callback user_data**: the library stores the pointer but never frees it.
   The caller owns the user_data lifetime.

---

*See also: [C++ API Reference](index.md) | [Safety FMEA](../safety/FFI_C_API.md)*
