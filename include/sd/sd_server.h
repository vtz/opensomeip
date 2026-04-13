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

#ifndef SOMEIP_SD_SERVER_H
#define SOMEIP_SD_SERVER_H

#include "sd_types.h"
#include <memory>
#include <vector>

namespace someip::sd {

/**
 * @brief Forward declaration
 */
class SdServerImpl;

/**
 * @brief SOME/IP Service Discovery Server
 *
 * This server allows services to offer themselves via the SOME/IP-SD protocol,
 * responding to client find requests and managing service availability.
 */
class SdServer {
public:
    /**
     * @brief Constructor
     * @param config SD configuration
     */
    explicit SdServer(const SdConfig& config = SdConfig());

    /**
     * @brief Destructor
     */
    ~SdServer();

    // Delete copy and move operations
    SdServer(const SdServer&) = delete;
    SdServer& operator=(const SdServer&) = delete;
    SdServer(SdServer&&) = delete;
    SdServer& operator=(SdServer&&) = delete;

    /**
     * @brief Initialize the SD server
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * @brief Shutdown the SD server
     */
    void shutdown();

    /**
     * @brief Offer a service instance
     *
     * @param instance Service instance information
     * @param unicast_endpoint Unicast endpoint for the service
     * @param multicast_endpoint Multicast endpoint (optional)
     * @return true if service offered, false on error
     */
    bool offer_service(const ServiceInstance& instance,
                      const std::string& unicast_endpoint,
                      const std::string& multicast_endpoint = "");

    /**
     * @brief Stop offering a service instance
     *
     * @param service_id Service ID
     * @param instance_id Instance ID
     * @return true if service stopped, false if not found
     */
    bool stop_offer_service(uint16_t service_id, uint16_t instance_id);

    /**
     * @brief Update service TTL
     *
     * @param service_id Service ID
     * @param instance_id Instance ID
     * @param ttl_seconds New TTL in seconds
     * @return true if updated, false if service not found
     */
    bool update_service_ttl(uint16_t service_id, uint16_t instance_id, uint32_t ttl_seconds);

    /**
     * @brief Handle event group subscription request
     *
     * @param service_id Service ID
     * @param instance_id Instance ID
     * @param eventgroup_id Event group ID
     * @param client_address Client IP address
     * @param acknowledge Whether to acknowledge the subscription
     * @return true if handled, false on error
     */
    bool handle_eventgroup_subscription(uint16_t service_id, uint16_t instance_id,
                                       uint16_t eventgroup_id, const std::string& client_address,
                                       bool acknowledge = true);

    /**
     * @brief Get currently offered services
     *
     * @return Vector of offered service instances
     */
    std::vector<ServiceInstance> get_offered_services() const;

    /**
     * @brief Check if server is initialized and ready
     *
     * @return true if ready for SD operations
     */
    bool is_ready() const;

    /**
     * @brief Get server statistics
     *
     * @return Statistics about SD operations
     */
    struct Statistics {
        uint32_t services_offered{0};
        uint32_t find_requests_received{0};
        uint32_t offers_sent{0};
        uint32_t subscriptions_received{0};
        uint32_t subscriptions_acknowledged{0};
    };
    Statistics get_statistics() const;

private:
    std::unique_ptr<SdServerImpl> impl_;
};

}  // namespace someip::sd

#endif // SOMEIP_SD_SERVER_H
