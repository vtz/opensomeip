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

#include <cstdint>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <functional>

namespace someip {
namespace sd {

/**
 * @brief SD (Service Discovery) entry types
 */
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

/**
 * @brief SD option types
 */
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

/**
 * @brief Service discovery result codes
 */
enum class SdResult : uint8_t {
    SUCCESS,
    SERVICE_NOT_FOUND,
    SERVICE_ALREADY_EXISTS,
    NETWORK_ERROR,
    TIMEOUT,
    INVALID_PARAMETERS
};

/**
 * @brief Service instance information
 */
struct ServiceInstance {
    uint16_t service_id{0};
    uint16_t instance_id{0};
    uint8_t major_version{0};
    uint8_t minor_version{0};
    std::string ip_address;
    uint16_t port{0};
    uint8_t protocol{0x11};  // Default to UDP (0x11)
    uint32_t ttl_seconds{0};  // Time to live

    ServiceInstance(uint16_t svc_id = 0, uint16_t inst_id = 0,
                   uint8_t maj_ver = 0, uint8_t min_ver = 0)
        : service_id(svc_id), instance_id(inst_id),
          major_version(maj_ver), minor_version(min_ver) {}
};

/**
 * @brief Event group information
 */
struct EventGroup {
    uint16_t eventgroup_id{0};
    uint8_t major_version{0};
    uint8_t minor_version{0};
    std::vector<uint16_t> event_ids;

    EventGroup(uint16_t eg_id = 0, uint8_t maj_ver = 0, uint8_t min_ver = 0)
        : eventgroup_id(eg_id), major_version(maj_ver), minor_version(min_ver) {}
};

/**
 * @brief Service discovery configuration
 */
struct SdConfig {
    std::string multicast_address{"239.255.255.251"};  // Default SOME/IP SD multicast
    uint16_t multicast_port{30490};                    // Default SOME/IP SD port
    std::string unicast_address{"127.0.0.1"};         // Local unicast address
    uint16_t unicast_port{0};                          // Auto-assign port
    std::chrono::milliseconds initial_delay{100};      // Initial offer delay
    std::chrono::milliseconds repetition_base{2000};   // Base repetition interval
    std::chrono::milliseconds repetition_max{3600000}; // Max repetition interval (1 hour)
    uint8_t repetition_multiplier{2};                   // Exponential backoff multiplier
    std::chrono::milliseconds cyclic_offer{30000};     // Cyclic offer interval (30s)
    std::chrono::milliseconds ttl{3600000};           // Default TTL (1 hour)
};

/**
 * @brief Service discovery callback types
 */
using ServiceAvailableCallback = std::function<void(const ServiceInstance&)>;
using ServiceUnavailableCallback = std::function<void(const ServiceInstance&)>;
using FindServiceCallback = std::function<void(const std::vector<ServiceInstance>&)>;

/**
 * @brief Subscription state
 */
enum class SubscriptionState : uint8_t {
    REQUESTED,
    SUBSCRIBED,
    PENDING_ACK,
    REJECTED
};

/**
 * @brief Event group subscription info
 */
struct EventGroupSubscription {
    uint16_t service_id{0};
    uint16_t instance_id{0};
    uint16_t eventgroup_id{0};
    SubscriptionState state{SubscriptionState::REQUESTED};
    std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};

    EventGroupSubscription(uint16_t svc_id = 0, uint16_t inst_id = 0, uint16_t eg_id = 0)
        : service_id(svc_id), instance_id(inst_id), eventgroup_id(eg_id) {
        timestamp = std::chrono::steady_clock::now();
    }
};

} // namespace sd
} // namespace someip

#endif // SOMEIP_SD_TYPES_H
