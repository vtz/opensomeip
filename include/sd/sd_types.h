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

#ifndef SOMEIP_SD_TYPES_H
#define SOMEIP_SD_TYPES_H

#include "platform/buffer_pool.h"
#include "platform/containers.h"

#include <chrono>
#include <cstdint>
#include <memory>

namespace someip::sd {

/** @implements REQ_SD_242 */
enum class EntryType : uint8_t {
    FIND_SERVICE = 0x00,           // Client searching for service
    OFFER_SERVICE = 0x01,          // Service offering itself
    STOP_OFFER_SERVICE = 0x01,     // Service stopping offer (with TTL=0)
    REQUEST_SUBSCRIBE_EVENTGROUP = 0x06,
    SUBSCRIBE_EVENTGROUP = 0x06,
    STOP_SUBSCRIBE_EVENTGROUP = 0x06,
    SUBSCRIBE_EVENTGROUP_ACK = 0x07,
    SUBSCRIBE_EVENTGROUP_NACK = 0x07
};

/** @implements REQ_SD_121, REQ_SD_124 */
enum class OptionType : uint8_t {
    CONFIGURATION = 0x01,
    LOAD_BALANCING = 0x02,
    IPV4_ENDPOINT = 0x04,
    IPV6_ENDPOINT = 0x06,
    IPV4_MULTICAST = 0x14,
    IPV6_MULTICAST = 0x16,
    IPV4_SD_ENDPOINT = 0x24,
    IPV6_SD_ENDPOINT = 0x26
};

/** @implements REQ_SD_340 */
enum class SdResult : uint8_t {
    SUCCESS,
    SERVICE_NOT_FOUND,
    SERVICE_ALREADY_EXISTS,
    NETWORK_ERROR,
    TIMEOUT,
    INVALID_PARAMETERS
};

/** @implements REQ_MSG_110, REQ_SD_293, REQ_SD_356 */
struct ServiceInstance {
    uint16_t service_id{0};
    uint16_t instance_id{0};
    uint8_t major_version{0};
    uint32_t minor_version{0};
    platform::String<> ip_address;
    uint16_t port{0};
    uint8_t protocol{0x11};  // Default to UDP (0x11)
    uint32_t ttl_seconds{0};  // Time to live

    explicit ServiceInstance(uint16_t svc_id = 0, uint16_t inst_id = 0,
                   uint8_t maj_ver = 0, uint32_t min_ver = 0)
        : service_id(svc_id), instance_id(inst_id),
          major_version(maj_ver), minor_version(min_ver) {}
};

/** @implements REQ_MSG_113, REQ_SD_271, REQ_SD_355 */
struct EventGroup {
    uint16_t eventgroup_id{0};
    uint8_t major_version{0};
    uint8_t minor_version{0};
    platform::Vector<uint16_t> event_ids;

    explicit EventGroup(uint16_t eg_id = 0, uint8_t maj_ver = 0, uint8_t min_ver = 0)
        : eventgroup_id(eg_id), major_version(maj_ver), minor_version(min_ver) {}
};

/** @implements REQ_SD_131, REQ_SD_180, REQ_SD_281, REQ_SD_310, REQ_COMPAT_030 */
struct SdConfig {
    platform::String<> multicast_address{"239.255.255.251"};  // Deployment default SD multicast group
    uint16_t multicast_port{30490};                           // Specified SOME/IP SD port
    platform::String<> unicast_address{"127.0.0.1"};         // Local unicast address
    uint16_t unicast_port{0};                          // Auto-assign port
    std::chrono::milliseconds initial_delay_min{0};    // Initial Wait Phase minimum
    std::chrono::milliseconds initial_delay_max{100};  // Initial Wait Phase maximum
    std::chrono::milliseconds initial_delay{100};      // Source-compat alias of initial_delay_max
    std::chrono::milliseconds repetition_base{2000};   // Base repetition interval
    std::chrono::milliseconds repetition_max{3600000}; // Max repetition interval (1 hour)
    uint8_t repetition_multiplier{2};                   // Exponential backoff multiplier
    std::chrono::milliseconds cyclic_offer{30000};     // Cyclic offer interval (30s)
    std::chrono::milliseconds ttl{3600000};           // Default TTL (1 hour, stored as milliseconds)
    size_t max_services{100};                          // Maximum number of services to track
    /// When true, pick_initial_wait_ms() returns initial_delay_override_ms exactly.
    bool has_initial_delay_override{false};
    uint32_t initial_delay_override_ms{0};
};

/**
 * @brief Pick the Initial Wait delay in milliseconds.
 *
 * Uses [initial_delay_min, max(initial_delay_max, initial_delay)] unless an
 * override is set for deterministic tests.
 */
uint32_t pick_initial_wait_ms(const SdConfig& config);

/**
 * @brief Service discovery callback types
 */
using ServiceAvailableCallback = platform::Function<void(const ServiceInstance&)>;
using ServiceUnavailableCallback = platform::Function<void(const ServiceInstance&)>;
using FindServiceCallback = platform::Function<void(const platform::Vector<ServiceInstance>&)>;

/** @implements REQ_SD_271 */
enum class SubscriptionState : uint8_t {
    REQUESTED,
    SUBSCRIBED,
    PENDING_ACK,
    REJECTED
};

/**
 * @brief SD Session ID counter per SOME/IP-SD spec.
 *
 * Session IDs start at 0x0001, increment per message, and wrap from
 * 0xFFFF back to 0x0001 (0x0000 is never emitted). The reboot flag is
 * true until the first wrap, then false.
 *
 * Sample reboot_flag() before next() when stamping a message so the
 * 0xFFFF datagram still carries Reboot=1.
 */
class SdSessionIdCounter {
public:
    SdSessionIdCounter() = default;
    explicit SdSessionIdCounter(uint16_t start)
        : next_id_(start == 0 ? uint16_t{1} : start) {}

    uint16_t next() {
        const uint16_t val = next_id_;
        if (next_id_ == 0xFFFF) {
            next_id_ = 1;
            wrapped_ = true;
        } else {
            ++next_id_;
            if (next_id_ == 0) {
                next_id_ = 1;
            }
        }
        return val;
    }

    uint16_t current() const { return next_id_; }
    bool reboot_flag() const { return !wrapped_; }

private:
    uint16_t next_id_{1};
    bool wrapped_{false};
};

/**
 * @brief Event group subscription info
 */
struct EventGroupSubscription {
    uint16_t service_id{0};
    uint16_t instance_id{0};
    uint16_t eventgroup_id{0};
    uint8_t major_version{0};
    SubscriptionState state{SubscriptionState::REQUESTED};
    std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};

    explicit EventGroupSubscription(uint16_t svc_id = 0, uint16_t inst_id = 0, uint16_t eg_id = 0)
        : service_id(svc_id), instance_id(inst_id), eventgroup_id(eg_id) {
        timestamp = std::chrono::steady_clock::now();
    }
};

/**
 * @brief Offered event group definition for subscription validation.
 */
struct OfferedEventGroup {
    uint16_t eventgroup_id{0};
};

}  // namespace someip::sd

#endif // SOMEIP_SD_TYPES_H
