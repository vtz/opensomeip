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
 * @file opensomeip_events.cpp
 * @brief C API wrappers for event publisher/subscriber operations.
 * @implements REQ_CAPI_002, REQ_CAPI_003, REQ_CAPI_004, REQ_CAPI_007, REQ_CAPI_012
 */

#include "capi/opensomeip.h"
#include "capi_internal.h"
#include "events/event_publisher.h"
#include "events/event_subscriber.h"
#include "events/event_types.h"
#include <cstring>

struct opensomeip_event_publisher_s {
    someip::events::EventPublisher publisher;
    opensomeip_event_publisher_s(uint16_t svc, uint16_t inst)
        : publisher(svc, inst) {}
};

struct opensomeip_event_subscriber_s {
    someip::events::EventSubscriber subscriber;
    explicit opensomeip_event_subscriber_s(uint16_t client_id)
        : subscriber(client_id) {}
};

extern "C" opensomeip_result_t opensomeip_event_publisher_create(opensomeip_event_publisher_t** out,
                                                                  uint16_t service_id,
                                                                  uint16_t instance_id) {
    if (!out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    CAPI_ENSURE_INIT();
    try {
        *out = new opensomeip_event_publisher_s(service_id, instance_id);
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_event_publisher_destroy(opensomeip_event_publisher_t* p) {
    if (!p) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { delete p; return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_event_publisher_initialize(opensomeip_event_publisher_t* p) {
    if (!p) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return p->publisher.initialize() ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_event_publisher_shutdown(opensomeip_event_publisher_t* p) {
    if (!p) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { p->publisher.shutdown(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_event_publisher_register(opensomeip_event_publisher_t* p,
                                                                    uint16_t event_id,
                                                                    uint16_t eventgroup_id) {
    if (!p) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        someip::events::EventConfig config;
        config.event_id = event_id;
        config.eventgroup_id = eventgroup_id;
        return p->publisher.register_event(config)
            ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_event_publisher_unregister(opensomeip_event_publisher_t* p,
                                                                      uint16_t event_id) {
    if (!p) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return p->publisher.unregister_event(event_id)
            ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_event_publisher_notify(opensomeip_event_publisher_t* p,
                                                                  uint16_t event_id,
                                                                  const uint8_t* data, size_t len) {
    if (!p) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    if (len > 0 && !data) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        someip::platform::ByteBuffer payload(data, data + len);
        return p->publisher.publish_event(event_id, payload)
            ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_event_subscriber_create(opensomeip_event_subscriber_t** out,
                                                                   uint16_t client_id) {
    if (!out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    CAPI_ENSURE_INIT();
    try {
        *out = new opensomeip_event_subscriber_s(client_id);
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_event_subscriber_destroy(opensomeip_event_subscriber_t* s) {
    if (!s) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { delete s; return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_event_subscriber_initialize(opensomeip_event_subscriber_t* s) {
    if (!s) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return s->subscriber.initialize() ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_event_subscriber_shutdown(opensomeip_event_subscriber_t* s) {
    if (!s) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { s->subscriber.shutdown(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_event_subscriber_subscribe(opensomeip_event_subscriber_t* s,
                                                                      uint16_t service_id,
                                                                      uint16_t instance_id,
                                                                      uint16_t eventgroup_id,
                                                                      opensomeip_event_callback_t callback,
                                                                      void* user_data) {
    if (!s || !callback) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto cb = callback;
        auto ud = user_data;
        bool ok = s->subscriber.subscribe_eventgroup(
            service_id, instance_id, eventgroup_id,
            [cb, ud](const someip::events::EventNotification& notif) {
                cb(notif.service_id, notif.instance_id, notif.event_id,
                   notif.event_data.data(), notif.event_data.size(), ud);
            }
        );
        return ok ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_event_subscriber_unsubscribe(opensomeip_event_subscriber_t* s,
                                                                        uint16_t service_id,
                                                                        uint16_t instance_id,
                                                                        uint16_t eventgroup_id) {
    if (!s) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return s->subscriber.unsubscribe_eventgroup(service_id, instance_id, eventgroup_id)
            ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}
