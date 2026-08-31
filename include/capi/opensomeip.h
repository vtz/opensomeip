/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * @file opensomeip.h
 * @brief OpenSOME/IP C API — stable extern "C" FFI boundary.
 *
 * This header is the single public include for C (and FFI) consumers.
 * It exposes opaque handles, POD types, and function declarations only.
 * No C++ types appear in this header.
 *
 * @implements REQ_CAPI_001, REQ_CAPI_005
 */

#ifndef OPENSOMEIP_CAPI_H
#define OPENSOMEIP_CAPI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──────────────────────────────────────────────────────────────────────────
 * Version macros
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_005 */
#define OPENSOMEIP_CAPI_VERSION_MAJOR 0
#define OPENSOMEIP_CAPI_VERSION_MINOR 1
#define OPENSOMEIP_CAPI_VERSION_PATCH 0

/* ──────────────────────────────────────────────────────────────────────────
 * Result codes  (mirrors someip::Result)
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_002 */
typedef enum {
    OPENSOMEIP_RESULT_SUCCESS              = 0x00,

    OPENSOMEIP_RESULT_NETWORK_ERROR        = 0x01,
    OPENSOMEIP_RESULT_NOT_CONNECTED        = 0x02,
    OPENSOMEIP_RESULT_CONNECTION_LOST      = 0x03,
    OPENSOMEIP_RESULT_CONNECTION_REFUSED   = 0x04,
    OPENSOMEIP_RESULT_TIMEOUT              = 0x05,
    OPENSOMEIP_RESULT_INVALID_ENDPOINT     = 0x06,

    OPENSOMEIP_RESULT_INVALID_MESSAGE      = 0x10,
    OPENSOMEIP_RESULT_INVALID_MESSAGE_TYPE = 0x11,
    OPENSOMEIP_RESULT_INVALID_SERVICE_ID   = 0x12,
    OPENSOMEIP_RESULT_INVALID_METHOD_ID    = 0x13,
    OPENSOMEIP_RESULT_INVALID_PROTOCOL_VERSION = 0x14,
    OPENSOMEIP_RESULT_INVALID_INTERFACE_VERSION = 0x15,
    OPENSOMEIP_RESULT_MALFORMED_MESSAGE    = 0x16,

    OPENSOMEIP_RESULT_INVALID_SESSION_ID   = 0x20,
    OPENSOMEIP_RESULT_SESSION_EXPIRED      = 0x21,
    OPENSOMEIP_RESULT_SESSION_NOT_FOUND    = 0x22,

    OPENSOMEIP_RESULT_OUT_OF_MEMORY        = 0x30,
    OPENSOMEIP_RESULT_BUFFER_OVERFLOW      = 0x31,
    OPENSOMEIP_RESULT_RESOURCE_EXHAUSTED   = 0x32,

    OPENSOMEIP_RESULT_SERVICE_NOT_FOUND    = 0x40,
    OPENSOMEIP_RESULT_SERVICE_UNAVAILABLE  = 0x41,
    OPENSOMEIP_RESULT_SUBSCRIPTION_FAILED  = 0x42,

    OPENSOMEIP_RESULT_SAFETY_VIOLATION     = 0x50,
    OPENSOMEIP_RESULT_FAULT_DETECTED       = 0x51,
    OPENSOMEIP_RESULT_RECOVERY_FAILED      = 0x52,

    OPENSOMEIP_RESULT_NOT_IMPLEMENTED      = 0x60,
    OPENSOMEIP_RESULT_INVALID_ARGUMENT     = 0x61,
    OPENSOMEIP_RESULT_PERMISSION_DENIED    = 0x62,
    OPENSOMEIP_RESULT_INTERNAL_ERROR       = 0x63,
    OPENSOMEIP_RESULT_NOT_INITIALIZED      = 0x64,
    OPENSOMEIP_RESULT_INVALID_STATE        = 0x65,

    OPENSOMEIP_RESULT_UNKNOWN_ERROR        = 0xFF
} opensomeip_result_t;

/* ──────────────────────────────────────────────────────────────────────────
 * SOME/IP message types  (mirrors someip::MessageType)
 * ────────────────────────────────────────────────────────────────────────── */

typedef enum {
    OPENSOMEIP_MSG_REQUEST            = 0x00,
    OPENSOMEIP_MSG_REQUEST_NO_RETURN  = 0x01,
    OPENSOMEIP_MSG_NOTIFICATION       = 0x02,
    OPENSOMEIP_MSG_REQUEST_ACK        = 0x40,
    OPENSOMEIP_MSG_RESPONSE           = 0x80,
    OPENSOMEIP_MSG_ERROR              = 0x81,
    OPENSOMEIP_MSG_RESPONSE_ACK       = 0xC0,
    OPENSOMEIP_MSG_ERROR_ACK          = 0xC1,
    OPENSOMEIP_MSG_TP_REQUEST         = 0x20,
    OPENSOMEIP_MSG_TP_REQUEST_NO_RETURN = 0x21,
    OPENSOMEIP_MSG_TP_NOTIFICATION    = 0x22
} opensomeip_message_type_t;

/* ──────────────────────────────────────────────────────────────────────────
 * SOME/IP return codes  (mirrors someip::ReturnCode)
 * ────────────────────────────────────────────────────────────────────────── */

typedef enum {
    OPENSOMEIP_RC_E_OK                    = 0x00,
    OPENSOMEIP_RC_E_NOT_OK                = 0x01,
    OPENSOMEIP_RC_E_UNKNOWN_SERVICE       = 0x02,
    OPENSOMEIP_RC_E_UNKNOWN_METHOD        = 0x03,
    OPENSOMEIP_RC_E_NOT_READY             = 0x04,
    OPENSOMEIP_RC_E_NOT_REACHABLE         = 0x05,
    OPENSOMEIP_RC_E_TIMEOUT               = 0x06,
    OPENSOMEIP_RC_E_WRONG_PROTOCOL_VERSION = 0x07,
    OPENSOMEIP_RC_E_WRONG_INTERFACE_VERSION = 0x08,
    OPENSOMEIP_RC_E_MALFORMED_MESSAGE     = 0x09,
    OPENSOMEIP_RC_E_WRONG_MESSAGE_TYPE    = 0x0A
} opensomeip_return_code_t;

/* ──────────────────────────────────────────────────────────────────────────
 * Transport protocol
 * ────────────────────────────────────────────────────────────────────────── */

typedef enum {
    OPENSOMEIP_TRANSPORT_UDP = 0,
    OPENSOMEIP_TRANSPORT_TCP = 1
} opensomeip_transport_protocol_t;

/* ──────────────────────────────────────────────────────────────────────────
 * Opaque handle types
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003 */
typedef struct opensomeip_message_s      opensomeip_message_t;
typedef struct opensomeip_serializer_s   opensomeip_serializer_t;
typedef struct opensomeip_deserializer_s opensomeip_deserializer_t;
typedef struct opensomeip_udp_transport_s opensomeip_udp_transport_t;
typedef struct opensomeip_tcp_transport_s opensomeip_tcp_transport_t;
typedef struct opensomeip_rpc_client_s   opensomeip_rpc_client_t;
typedef struct opensomeip_rpc_server_s   opensomeip_rpc_server_t;
typedef struct opensomeip_sd_client_s    opensomeip_sd_client_t;
typedef struct opensomeip_sd_server_s    opensomeip_sd_server_t;
typedef struct opensomeip_event_publisher_s  opensomeip_event_publisher_t;
typedef struct opensomeip_event_subscriber_s opensomeip_event_subscriber_t;
typedef struct opensomeip_tp_manager_s   opensomeip_tp_manager_t;
typedef struct opensomeip_e2e_s          opensomeip_e2e_t;

/* ──────────────────────────────────────────────────────────────────────────
 * POD structs
 * ────────────────────────────────────────────────────────────────────────── */

/** Network endpoint (C-friendly POD). */
typedef struct {
    char     address[64];
    uint16_t port;
    opensomeip_transport_protocol_t protocol;
} opensomeip_endpoint_t;

/** RPC async completion callback. */
typedef void (*opensomeip_rpc_callback_t)(
    opensomeip_result_t result,
    const uint8_t*      return_data,
    size_t              return_data_len,
    void*               user_data
);

/** RPC method handler (server side). */
typedef opensomeip_result_t (*opensomeip_method_handler_t)(
    uint16_t       client_id,
    uint16_t       session_id,
    const uint8_t* input_data,
    size_t         input_len,
    uint8_t*       output_data,
    size_t*        output_len,
    void*          user_data
);

/** SD service-found callback. */
typedef void (*opensomeip_sd_found_callback_t)(
    uint16_t                     service_id,
    uint16_t                     instance_id,
    const opensomeip_endpoint_t* endpoint,
    void*                        user_data
);

/** SD service availability callback. */
typedef void (*opensomeip_sd_availability_callback_t)(
    uint16_t service_id,
    uint16_t instance_id,
    int      available,
    void*    user_data
);

/** Event notification callback. */
typedef void (*opensomeip_event_callback_t)(
    uint16_t       service_id,
    uint16_t       instance_id,
    uint16_t       event_id,
    const uint8_t* data,
    size_t         data_len,
    void*          user_data
);

/* ──────────────────────────────────────────────────────────────────────────
 * Version query
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_005
 *  Thread-safe. */
uint32_t opensomeip_capi_version(void);

/* ──────────────────────────────────────────────────────────────────────────
 * Message API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_008
 *  Thread-safe: each handle is independent. */
opensomeip_result_t opensomeip_message_create(opensomeip_message_t** out_msg);

/** @implements REQ_CAPI_003, REQ_CAPI_008 */
opensomeip_result_t opensomeip_message_destroy(opensomeip_message_t* msg);

/** @implements REQ_CAPI_008 */
opensomeip_result_t opensomeip_message_set_service_id(opensomeip_message_t* msg, uint16_t service_id);
opensomeip_result_t opensomeip_message_get_service_id(const opensomeip_message_t* msg, uint16_t* out);
opensomeip_result_t opensomeip_message_set_method_id(opensomeip_message_t* msg, uint16_t method_id);
opensomeip_result_t opensomeip_message_get_method_id(const opensomeip_message_t* msg, uint16_t* out);
opensomeip_result_t opensomeip_message_set_client_id(opensomeip_message_t* msg, uint16_t client_id);
opensomeip_result_t opensomeip_message_get_client_id(const opensomeip_message_t* msg, uint16_t* out);
opensomeip_result_t opensomeip_message_set_session_id(opensomeip_message_t* msg, uint16_t session_id);
opensomeip_result_t opensomeip_message_get_session_id(const opensomeip_message_t* msg, uint16_t* out);
opensomeip_result_t opensomeip_message_set_protocol_version(opensomeip_message_t* msg, uint8_t version);
opensomeip_result_t opensomeip_message_get_protocol_version(const opensomeip_message_t* msg, uint8_t* out);
opensomeip_result_t opensomeip_message_set_interface_version(opensomeip_message_t* msg, uint8_t version);
opensomeip_result_t opensomeip_message_get_interface_version(const opensomeip_message_t* msg, uint8_t* out);
opensomeip_result_t opensomeip_message_set_message_type(opensomeip_message_t* msg, opensomeip_message_type_t type);
opensomeip_result_t opensomeip_message_get_message_type(const opensomeip_message_t* msg, opensomeip_message_type_t* out);
opensomeip_result_t opensomeip_message_set_return_code(opensomeip_message_t* msg, opensomeip_return_code_t code);
opensomeip_result_t opensomeip_message_get_return_code(const opensomeip_message_t* msg, opensomeip_return_code_t* out);

/** @implements REQ_CAPI_008
 *  Copies data into internal payload buffer. */
opensomeip_result_t opensomeip_message_set_payload(opensomeip_message_t* msg,
                                                    const uint8_t* data, size_t len);

/** @implements REQ_CAPI_008
 *  Copies payload into caller buffer; *out_len is in/out (capacity → actual). */
opensomeip_result_t opensomeip_message_get_payload(const opensomeip_message_t* msg,
                                                    uint8_t* buf, size_t* out_len);

/** @implements REQ_CAPI_008
 *  Returns payload size without copying. */
opensomeip_result_t opensomeip_message_get_payload_length(const opensomeip_message_t* msg,
                                                           size_t* out_len);

/** @implements REQ_CAPI_008
 *  Serialize message to wire format into caller buffer. */
opensomeip_result_t opensomeip_message_serialize(const opensomeip_message_t* msg,
                                                  uint8_t* buf, size_t* buf_len);

/** @implements REQ_CAPI_008
 *  Deserialize wire data into message. */
opensomeip_result_t opensomeip_message_deserialize(opensomeip_message_t* msg,
                                                    const uint8_t* data, size_t len);

/* ──────────────────────────────────────────────────────────────────────────
 * Serializer API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_008 */
opensomeip_result_t opensomeip_serializer_create(opensomeip_serializer_t** out);
opensomeip_result_t opensomeip_serializer_destroy(opensomeip_serializer_t* ser);
opensomeip_result_t opensomeip_serializer_reset(opensomeip_serializer_t* ser);

opensomeip_result_t opensomeip_serializer_write_uint8(opensomeip_serializer_t* ser, uint8_t val);
opensomeip_result_t opensomeip_serializer_write_uint16(opensomeip_serializer_t* ser, uint16_t val);
opensomeip_result_t opensomeip_serializer_write_uint32(opensomeip_serializer_t* ser, uint32_t val);
opensomeip_result_t opensomeip_serializer_write_uint64(opensomeip_serializer_t* ser, uint64_t val);
opensomeip_result_t opensomeip_serializer_write_bytes(opensomeip_serializer_t* ser,
                                                       const uint8_t* data, size_t len);

/** Get serialized data. *out_len is in/out. */
opensomeip_result_t opensomeip_serializer_get_data(const opensomeip_serializer_t* ser,
                                                    uint8_t* buf, size_t* out_len);
opensomeip_result_t opensomeip_serializer_get_size(const opensomeip_serializer_t* ser,
                                                    size_t* out_len);

/* ──────────────────────────────────────────────────────────────────────────
 * Deserializer API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_008 */
opensomeip_result_t opensomeip_deserializer_create(opensomeip_deserializer_t** out,
                                                    const uint8_t* data, size_t len);
opensomeip_result_t opensomeip_deserializer_destroy(opensomeip_deserializer_t* de);

opensomeip_result_t opensomeip_deserializer_read_uint8(opensomeip_deserializer_t* de, uint8_t* out);
opensomeip_result_t opensomeip_deserializer_read_uint16(opensomeip_deserializer_t* de, uint16_t* out);
opensomeip_result_t opensomeip_deserializer_read_uint32(opensomeip_deserializer_t* de, uint32_t* out);
opensomeip_result_t opensomeip_deserializer_read_uint64(opensomeip_deserializer_t* de, uint64_t* out);
opensomeip_result_t opensomeip_deserializer_read_bytes(opensomeip_deserializer_t* de,
                                                        uint8_t* buf, size_t* len);

opensomeip_result_t opensomeip_deserializer_get_remaining(const opensomeip_deserializer_t* de,
                                                           size_t* out);

/* ──────────────────────────────────────────────────────────────────────────
 * UDP Transport API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_009 */
opensomeip_result_t opensomeip_udp_transport_create(opensomeip_udp_transport_t** out,
                                                     const opensomeip_endpoint_t* local_ep);
opensomeip_result_t opensomeip_udp_transport_destroy(opensomeip_udp_transport_t* t);
opensomeip_result_t opensomeip_udp_transport_start(opensomeip_udp_transport_t* t);
opensomeip_result_t opensomeip_udp_transport_stop(opensomeip_udp_transport_t* t);
opensomeip_result_t opensomeip_udp_transport_send(opensomeip_udp_transport_t* t,
                                                   const opensomeip_message_t* msg,
                                                   const opensomeip_endpoint_t* dest);
opensomeip_result_t opensomeip_udp_transport_receive(opensomeip_udp_transport_t* t,
                                                      opensomeip_message_t** out_msg,
                                                      opensomeip_endpoint_t* out_sender);

/* ──────────────────────────────────────────────────────────────────────────
 * TCP Transport API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_009 */
opensomeip_result_t opensomeip_tcp_transport_create(opensomeip_tcp_transport_t** out);
opensomeip_result_t opensomeip_tcp_transport_destroy(opensomeip_tcp_transport_t* t);
opensomeip_result_t opensomeip_tcp_transport_initialize(opensomeip_tcp_transport_t* t,
                                                         const opensomeip_endpoint_t* local_ep);
opensomeip_result_t opensomeip_tcp_transport_start(opensomeip_tcp_transport_t* t);
opensomeip_result_t opensomeip_tcp_transport_stop(opensomeip_tcp_transport_t* t);
opensomeip_result_t opensomeip_tcp_transport_connect(opensomeip_tcp_transport_t* t,
                                                      const opensomeip_endpoint_t* remote_ep);
opensomeip_result_t opensomeip_tcp_transport_disconnect(opensomeip_tcp_transport_t* t);
opensomeip_result_t opensomeip_tcp_transport_send(opensomeip_tcp_transport_t* t,
                                                   const opensomeip_message_t* msg,
                                                   const opensomeip_endpoint_t* dest);

/* ──────────────────────────────────────────────────────────────────────────
 * RPC Client API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_010 */
opensomeip_result_t opensomeip_rpc_client_create(opensomeip_rpc_client_t** out,
                                                  uint16_t client_id);
opensomeip_result_t opensomeip_rpc_client_destroy(opensomeip_rpc_client_t* c);
opensomeip_result_t opensomeip_rpc_client_initialize(opensomeip_rpc_client_t* c);
opensomeip_result_t opensomeip_rpc_client_shutdown(opensomeip_rpc_client_t* c);

/** @implements REQ_CAPI_010
 *  Synchronous call. output_data/output_len are in/out (capacity → actual). */
opensomeip_result_t opensomeip_rpc_client_call_sync(opensomeip_rpc_client_t* c,
                                                     uint16_t service_id,
                                                     uint16_t method_id,
                                                     const uint8_t* input_data,
                                                     size_t input_len,
                                                     uint8_t* output_data,
                                                     size_t* output_len,
                                                     uint32_t timeout_ms);

/** @implements REQ_CAPI_004, REQ_CAPI_010
 *  Asynchronous call with callback. */
opensomeip_result_t opensomeip_rpc_client_call_async(opensomeip_rpc_client_t* c,
                                                      uint16_t service_id,
                                                      uint16_t method_id,
                                                      const uint8_t* input_data,
                                                      size_t input_len,
                                                      opensomeip_rpc_callback_t callback,
                                                      void* user_data,
                                                      uint32_t timeout_ms,
                                                      uint32_t* out_handle);

opensomeip_result_t opensomeip_rpc_client_cancel(opensomeip_rpc_client_t* c, uint32_t handle);

/* ──────────────────────────────────────────────────────────────────────────
 * RPC Server API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_010 */
opensomeip_result_t opensomeip_rpc_server_create(opensomeip_rpc_server_t** out,
                                                  uint16_t service_id);
opensomeip_result_t opensomeip_rpc_server_destroy(opensomeip_rpc_server_t* s);
opensomeip_result_t opensomeip_rpc_server_initialize(opensomeip_rpc_server_t* s);
opensomeip_result_t opensomeip_rpc_server_shutdown(opensomeip_rpc_server_t* s);

/** @implements REQ_CAPI_004, REQ_CAPI_010 */
opensomeip_result_t opensomeip_rpc_server_register_method(opensomeip_rpc_server_t* s,
                                                           uint16_t method_id,
                                                           opensomeip_method_handler_t handler,
                                                           void* user_data);

opensomeip_result_t opensomeip_rpc_server_unregister_method(opensomeip_rpc_server_t* s,
                                                             uint16_t method_id);

/* ──────────────────────────────────────────────────────────────────────────
 * Service Discovery Client API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_011 */
opensomeip_result_t opensomeip_sd_client_create(opensomeip_sd_client_t** out);
opensomeip_result_t opensomeip_sd_client_destroy(opensomeip_sd_client_t* c);
opensomeip_result_t opensomeip_sd_client_initialize(opensomeip_sd_client_t* c);
opensomeip_result_t opensomeip_sd_client_shutdown(opensomeip_sd_client_t* c);

/** @implements REQ_CAPI_004, REQ_CAPI_011 */
opensomeip_result_t opensomeip_sd_client_find_service(opensomeip_sd_client_t* c,
                                                       uint16_t service_id,
                                                       opensomeip_sd_found_callback_t callback,
                                                       void* user_data,
                                                       uint32_t timeout_ms);

opensomeip_result_t opensomeip_sd_client_subscribe_availability(opensomeip_sd_client_t* c,
                                                                 uint16_t service_id,
                                                                 opensomeip_sd_availability_callback_t callback,
                                                                 void* user_data);

opensomeip_result_t opensomeip_sd_client_subscribe_eventgroup(opensomeip_sd_client_t* c,
                                                               uint16_t service_id,
                                                               uint16_t instance_id,
                                                               uint16_t eventgroup_id);

/* ──────────────────────────────────────────────────────────────────────────
 * Service Discovery Server API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_011 */
opensomeip_result_t opensomeip_sd_server_create(opensomeip_sd_server_t** out);
opensomeip_result_t opensomeip_sd_server_destroy(opensomeip_sd_server_t* s);
opensomeip_result_t opensomeip_sd_server_initialize(opensomeip_sd_server_t* s);
opensomeip_result_t opensomeip_sd_server_shutdown(opensomeip_sd_server_t* s);

/** @implements REQ_CAPI_011 */
opensomeip_result_t opensomeip_sd_server_offer_service(opensomeip_sd_server_t* s,
                                                        uint16_t service_id,
                                                        uint16_t instance_id,
                                                        const opensomeip_endpoint_t* endpoint);

opensomeip_result_t opensomeip_sd_server_stop_offer(opensomeip_sd_server_t* s,
                                                     uint16_t service_id,
                                                     uint16_t instance_id);

/* ──────────────────────────────────────────────────────────────────────────
 * Event Publisher API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_012 */
opensomeip_result_t opensomeip_event_publisher_create(opensomeip_event_publisher_t** out,
                                                       uint16_t service_id,
                                                       uint16_t instance_id);
opensomeip_result_t opensomeip_event_publisher_destroy(opensomeip_event_publisher_t* p);
opensomeip_result_t opensomeip_event_publisher_initialize(opensomeip_event_publisher_t* p);
opensomeip_result_t opensomeip_event_publisher_shutdown(opensomeip_event_publisher_t* p);

/** @implements REQ_CAPI_012 */
opensomeip_result_t opensomeip_event_publisher_register(opensomeip_event_publisher_t* p,
                                                         uint16_t event_id,
                                                         uint16_t eventgroup_id);
opensomeip_result_t opensomeip_event_publisher_unregister(opensomeip_event_publisher_t* p,
                                                           uint16_t event_id);
opensomeip_result_t opensomeip_event_publisher_notify(opensomeip_event_publisher_t* p,
                                                       uint16_t event_id,
                                                       const uint8_t* data, size_t len);

/* ──────────────────────────────────────────────────────────────────────────
 * Event Subscriber API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_012 */
opensomeip_result_t opensomeip_event_subscriber_create(opensomeip_event_subscriber_t** out,
                                                        uint16_t client_id);
opensomeip_result_t opensomeip_event_subscriber_destroy(opensomeip_event_subscriber_t* s);
opensomeip_result_t opensomeip_event_subscriber_initialize(opensomeip_event_subscriber_t* s);
opensomeip_result_t opensomeip_event_subscriber_shutdown(opensomeip_event_subscriber_t* s);

/** @implements REQ_CAPI_004, REQ_CAPI_012 */
opensomeip_result_t opensomeip_event_subscriber_subscribe(opensomeip_event_subscriber_t* s,
                                                           uint16_t service_id,
                                                           uint16_t instance_id,
                                                           uint16_t eventgroup_id,
                                                           opensomeip_event_callback_t callback,
                                                           void* user_data);

opensomeip_result_t opensomeip_event_subscriber_unsubscribe(opensomeip_event_subscriber_t* s,
                                                             uint16_t service_id,
                                                             uint16_t instance_id,
                                                             uint16_t eventgroup_id);

/* ──────────────────────────────────────────────────────────────────────────
 * TP (Transport Protocol) API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_013 */
opensomeip_result_t opensomeip_tp_manager_create(opensomeip_tp_manager_t** out);
opensomeip_result_t opensomeip_tp_manager_destroy(opensomeip_tp_manager_t* tp);
opensomeip_result_t opensomeip_tp_manager_initialize(opensomeip_tp_manager_t* tp);
opensomeip_result_t opensomeip_tp_manager_shutdown(opensomeip_tp_manager_t* tp);

/** @implements REQ_CAPI_013
 *  Check if payload needs TP segmentation (returns 1 if yes, 0 if no). */
opensomeip_result_t opensomeip_tp_needs_segmentation(const opensomeip_tp_manager_t* tp,
                                                      const uint8_t* payload, size_t len,
                                                      int* out_needs);

/** @implements REQ_CAPI_013
 *  Segment payload. Segments are written to out_buf (concatenated with 4-byte length prefix each). */
opensomeip_result_t opensomeip_tp_segment(opensomeip_tp_manager_t* tp,
                                           const opensomeip_message_t* msg,
                                           uint8_t* out_buf, size_t* out_len);

/** @implements REQ_CAPI_013
 *  Feed a received segment; when complete, *complete is set to 1 and out_buf filled. */
opensomeip_result_t opensomeip_tp_reassemble(opensomeip_tp_manager_t* tp,
                                              const uint8_t* segment_data, size_t segment_len,
                                              uint8_t* out_buf, size_t* out_len,
                                              int* complete);

/* ──────────────────────────────────────────────────────────────────────────
 * E2E (End-to-End Protection) API
 * ────────────────────────────────────────────────────────────────────────── */

/** @implements REQ_CAPI_003, REQ_CAPI_013 */
opensomeip_result_t opensomeip_e2e_create(opensomeip_e2e_t** out);
opensomeip_result_t opensomeip_e2e_destroy(opensomeip_e2e_t* e);

/** @implements REQ_CAPI_013
 *  Protect message (adds E2E header). */
opensomeip_result_t opensomeip_e2e_protect(opensomeip_e2e_t* e,
                                            opensomeip_message_t* msg,
                                            uint16_t data_id,
                                            uint32_t counter);

/** @implements REQ_CAPI_013
 *  Check message E2E protection. */
opensomeip_result_t opensomeip_e2e_check(opensomeip_e2e_t* e,
                                          const opensomeip_message_t* msg,
                                          uint16_t data_id);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OPENSOMEIP_CAPI_H */
