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
 * @file opensomeip_sd.cpp
 * @brief C API wrappers for service discovery operations.
 * @implements REQ_CAPI_002, REQ_CAPI_003, REQ_CAPI_004, REQ_CAPI_007, REQ_CAPI_011
 */

#include "capi/opensomeip.h"
#include "capi_internal.h"
#include "sd/sd_client.h"
#include "sd/sd_server.h"
#include <cstring>

struct opensomeip_sd_client_s {
    someip::sd::SdClient client;
};

struct opensomeip_sd_server_s {
    someip::sd::SdServer server;
};

extern "C" opensomeip_result_t opensomeip_sd_client_create(opensomeip_sd_client_t** out) {
    if (!out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    CAPI_ENSURE_INIT();
    try {
        *out = new opensomeip_sd_client_s();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_sd_client_destroy(opensomeip_sd_client_t* c) {
    if (!c) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { delete c; return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_sd_client_initialize(opensomeip_sd_client_t* c) {
    if (!c) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return c->client.initialize() ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_sd_client_shutdown(opensomeip_sd_client_t* c) {
    if (!c) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { c->client.shutdown(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_sd_client_find_service(opensomeip_sd_client_t* c,
                                                                  uint16_t service_id,
                                                                  opensomeip_sd_found_callback_t callback,
                                                                  void* user_data,
                                                                  uint32_t timeout_ms) {
    if (!c || !callback) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto cb = callback;
        auto ud = user_data;
        bool ok = c->client.find_service(
            service_id,
            [cb, ud](const someip::platform::Vector<someip::sd::ServiceInstance>& instances) {
                for (const auto& inst : instances) {
                    opensomeip_endpoint_t ep;
                    std::memset(&ep, 0, sizeof(ep));
                    auto addr = inst.ip_address;
                    size_t copy_len = addr.size() < 63 ? addr.size() : 63;
                    std::memcpy(ep.address, addr.c_str(), copy_len);
                    ep.address[copy_len] = '\0';
                    ep.port = inst.port;
                    cb(inst.service_id, inst.instance_id, &ep, ud);
                }
            },
            std::chrono::milliseconds(timeout_ms)
        );
        return ok ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_sd_client_subscribe_availability(opensomeip_sd_client_t* c,
                                                                            uint16_t service_id,
                                                                            opensomeip_sd_availability_callback_t callback,
                                                                            void* user_data) {
    if (!c || !callback) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto cb = callback;
        auto ud = user_data;
        bool ok = c->client.subscribe_service(
            service_id,
            [cb, ud, service_id](const someip::sd::ServiceInstance& inst) {
                cb(service_id, inst.instance_id, 1, ud);
            },
            [cb, ud, service_id](const someip::sd::ServiceInstance& inst) {
                cb(service_id, inst.instance_id, 0, ud);
            }
        );
        return ok ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_sd_client_subscribe_eventgroup(opensomeip_sd_client_t* c,
                                                                          uint16_t service_id,
                                                                          uint16_t instance_id,
                                                                          uint16_t eventgroup_id) {
    if (!c) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return c->client.subscribe_eventgroup(service_id, instance_id, eventgroup_id)
            ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_sd_server_create(opensomeip_sd_server_t** out) {
    if (!out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    CAPI_ENSURE_INIT();
    try {
        *out = new opensomeip_sd_server_s();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_sd_server_destroy(opensomeip_sd_server_t* s) {
    if (!s) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { delete s; return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_sd_server_initialize(opensomeip_sd_server_t* s) {
    if (!s) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return s->server.initialize() ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_sd_server_shutdown(opensomeip_sd_server_t* s) {
    if (!s) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { s->server.shutdown(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_sd_server_offer_service(opensomeip_sd_server_t* s,
                                                                   uint16_t service_id,
                                                                   uint16_t instance_id,
                                                                   const opensomeip_endpoint_t* endpoint) {
    if (!s || !endpoint) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        someip::sd::ServiceInstance inst;
        inst.service_id = service_id;
        inst.instance_id = instance_id;
        inst.port = endpoint->port;
        inst.ip_address = someip::platform::String<>(endpoint->address);

        someip::platform::String<> ep_str(endpoint->address);
        ep_str += ':';
        char port_buf[6];
        char* p = port_buf + sizeof(port_buf) - 1;
        *p = '\0';
        uint16_t port_val = endpoint->port;
        if (port_val == 0) { *--p = '0'; }
        else { while (port_val > 0) { *--p = static_cast<char>('0' + (port_val % 10)); port_val /= 10; } }
        ep_str += p;
        bool ok = s->server.offer_service(inst, ep_str);
        return ok ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_sd_server_stop_offer(opensomeip_sd_server_t* s,
                                                                uint16_t service_id,
                                                                uint16_t instance_id) {
    if (!s) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return s->server.stop_offer_service(service_id, instance_id)
            ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}
