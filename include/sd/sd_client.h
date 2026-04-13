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

#ifndef SOMEIP_SD_CLIENT_H
#define SOMEIP_SD_CLIENT_H

#include "sd_types.h"
#include <memory>
#include <vector>

namespace someip::sd {

/**
 * @brief Forward declaration
 */
class SdClientImpl;

/**
 * @brief SOME/IP Service Discovery Client
 *
 * This client allows applications to discover available services and subscribe
 * to event groups using the SOME/IP-SD protocol.
 */
class SdClient {
public:
    /**
     * @brief Constructor
     * @param config SD configuration
     */
    explicit SdClient(const SdConfig& config = SdConfig());

    /**
     * @brief Destructor
     */
    ~SdClient();

    // Delete copy and move operations
    SdClient(const SdClient&) = delete;
    SdClient& operator=(const SdClient&) = delete;
    SdClient(SdClient&&) = delete;
    SdClient& operator=(SdClient&&) = delete;

    /**
     * @brief Initialize the SD client
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * @brief Shutdown the SD client
     */
    void shutdown();

    /**
     * @brief Find available service instances
     *
     * @param service_id Service to search for
     * @param callback Callback invoked with found services
     * @param timeout Search timeout (0 = use default)
     * @return true if search initiated, false on error
     */
    bool find_service(uint16_t service_id,
                     FindServiceCallback callback,
                     std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief Subscribe to service availability notifications
     *
     * @param service_id Service to monitor
     * @param available_callback Callback when service becomes available
     * @param unavailable_callback Callback when service becomes unavailable
     * @return true if subscription successful, false on error
     */
    bool subscribe_service(uint16_t service_id,
                          ServiceAvailableCallback available_callback,
                          ServiceUnavailableCallback unavailable_callback);

    /**
     * @brief Unsubscribe from service availability notifications
     *
     * @param service_id Service to stop monitoring
     * @return true if unsubscribed, false if not found
     */
    bool unsubscribe_service(uint16_t service_id);

    /**
     * @brief Subscribe to event group
     *
     * @param service_id Service ID
     * @param instance_id Service instance ID
     * @param eventgroup_id Event group ID
     * @return true if subscription request sent, false on error
     */
    bool subscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id);

    /**
     * @brief Unsubscribe from event group
     *
     * @param service_id Service ID
     * @param instance_id Service instance ID
     * @param eventgroup_id Event group ID
     * @return true if unsubscription request sent, false on error
     */
    bool unsubscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id);

    /**
     * @brief Get currently available services
     *
     * @param service_id Service to query (0 = all services)
     * @return Vector of available service instances
     */
    std::vector<ServiceInstance> get_available_services(uint16_t service_id = 0) const;

    /**
     * @brief Check if client is initialized and ready
     *
     * @return true if ready for SD operations
     */
    bool is_ready() const;

    /**
     * @brief Get client statistics
     *
     * @return Statistics about SD operations
     */
    struct Statistics {
        uint32_t find_requests_sent{0};
        uint32_t services_found{0};
        uint32_t services_lost{0};
        uint32_t subscriptions_active{0};
        uint32_t eventgroup_subscriptions{0};
    };
    Statistics get_statistics() const;

private:
    std::unique_ptr<SdClientImpl> impl_;
};

}  // namespace someip::sd

#endif // SOMEIP_SD_CLIENT_H
