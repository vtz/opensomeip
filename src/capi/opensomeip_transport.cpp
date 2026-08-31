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
 * @file opensomeip_transport.cpp
 * @brief C API wrappers for UDP/TCP transport operations.
 * @implements REQ_CAPI_002, REQ_CAPI_003, REQ_CAPI_007, REQ_CAPI_009
 */

#include "capi/opensomeip.h"
#include "capi_internal.h"
#include "transport/udp_transport.h"
#include "transport/tcp_transport.h"
#include "transport/endpoint.h"
#include <cstring>
#include <new>

static someip::transport::Endpoint to_cpp_endpoint(const opensomeip_endpoint_t* ep) {
    someip::transport::TransportProtocol proto = someip::transport::TransportProtocol::UDP;
    if (ep->protocol == OPENSOMEIP_TRANSPORT_TCP) {
        proto = someip::transport::TransportProtocol::TCP;
    }
    return someip::transport::Endpoint(
        someip::platform::String<>(ep->address),
        ep->port,
        proto
    );
}

static void to_c_endpoint(const someip::transport::Endpoint& cpp_ep, opensomeip_endpoint_t* out) {
    std::memset(out, 0, sizeof(*out));
    auto addr_str = cpp_ep.get_address();
    size_t copy_len = addr_str.size() < 63 ? addr_str.size() : 63;
    std::memcpy(out->address, addr_str.c_str(), copy_len);
    out->address[copy_len] = '\0';
    out->port = cpp_ep.get_port();
    out->protocol = (cpp_ep.get_protocol() == someip::transport::TransportProtocol::TCP)
        ? OPENSOMEIP_TRANSPORT_TCP : OPENSOMEIP_TRANSPORT_UDP;
}

struct opensomeip_udp_transport_s {
    someip::transport::UdpTransport transport;
    explicit opensomeip_udp_transport_s(const someip::transport::Endpoint& ep)
        : transport(ep) {}
};

struct opensomeip_tcp_transport_s {
    someip::transport::TcpTransport transport;
};

extern "C" opensomeip_result_t opensomeip_udp_transport_create(opensomeip_udp_transport_t** out,
                                                                const opensomeip_endpoint_t* local_ep) {
    if (!out || !local_ep) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto ep = to_cpp_endpoint(local_ep);
        *out = new opensomeip_udp_transport_s(ep);
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_udp_transport_destroy(opensomeip_udp_transport_t* t) {
    if (!t) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { delete t; return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_udp_transport_start(opensomeip_udp_transport_t* t) {
    if (!t) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto r = t->transport.start();
        return static_cast<opensomeip_result_t>(r);
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_udp_transport_stop(opensomeip_udp_transport_t* t) {
    if (!t) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto r = t->transport.stop();
        return static_cast<opensomeip_result_t>(r);
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_udp_transport_send(opensomeip_udp_transport_t* t,
                                                              const opensomeip_message_t* msg,
                                                              const opensomeip_endpoint_t* dest) {
    if (!t || !msg || !dest) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto ep = to_cpp_endpoint(dest);
        auto r = t->transport.send_message(msg->msg, ep);
        return static_cast<opensomeip_result_t>(r);
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_udp_transport_receive(opensomeip_udp_transport_t* t,
                                                                 opensomeip_message_t** out_msg,
                                                                 opensomeip_endpoint_t* out_sender) {
    if (!t || !out_msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        someip::transport::Endpoint sender;
        auto msg_ptr = t->transport.receive_message_with_sender(sender);
        if (!msg_ptr) {
            *out_msg = nullptr;
            return OPENSOMEIP_RESULT_TIMEOUT;
        }
        auto* cmsg = new opensomeip_message_s();
        cmsg->msg = *msg_ptr;
        *out_msg = cmsg;
        if (out_sender) {
            to_c_endpoint(sender, out_sender);
        }
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out_msg = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tcp_transport_create(opensomeip_tcp_transport_t** out) {
    if (!out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        *out = new opensomeip_tcp_transport_s();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tcp_transport_destroy(opensomeip_tcp_transport_t* t) {
    if (!t) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { delete t; return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tcp_transport_initialize(opensomeip_tcp_transport_t* t,
                                                                    const opensomeip_endpoint_t* local_ep) {
    if (!t || !local_ep) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto ep = to_cpp_endpoint(local_ep);
        auto r = t->transport.initialize(ep);
        return static_cast<opensomeip_result_t>(r);
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tcp_transport_start(opensomeip_tcp_transport_t* t) {
    if (!t) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto r = t->transport.start();
        return static_cast<opensomeip_result_t>(r);
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tcp_transport_stop(opensomeip_tcp_transport_t* t) {
    if (!t) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto r = t->transport.stop();
        return static_cast<opensomeip_result_t>(r);
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tcp_transport_connect(opensomeip_tcp_transport_t* t,
                                                                 const opensomeip_endpoint_t* remote_ep) {
    if (!t || !remote_ep) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto ep = to_cpp_endpoint(remote_ep);
        auto r = t->transport.connect(ep);
        return static_cast<opensomeip_result_t>(r);
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tcp_transport_disconnect(opensomeip_tcp_transport_t* t) {
    if (!t) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto r = t->transport.disconnect();
        return static_cast<opensomeip_result_t>(r);
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tcp_transport_send(opensomeip_tcp_transport_t* t,
                                                              const opensomeip_message_t* msg,
                                                              const opensomeip_endpoint_t* dest) {
    if (!t || !msg || !dest) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto ep = to_cpp_endpoint(dest);
        auto r = t->transport.send_message(msg->msg, ep);
        return static_cast<opensomeip_result_t>(r);
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}
