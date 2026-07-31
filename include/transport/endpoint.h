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

#ifndef SOMEIP_TRANSPORT_ENDPOINT_H
#define SOMEIP_TRANSPORT_ENDPOINT_H

#include "platform/containers.h"

#include <cstdint>
#include <string>

namespace someip::transport {

/**
 * @brief Transport protocol types
 */
enum class TransportProtocol : uint8_t {
    UDP,
    TCP,
    MULTICAST_UDP
};

/**
 * @brief Network endpoint representation
 *
 * This class represents a network endpoint (IP address + port) for SOME/IP communication.
 */
class Endpoint {
public:
    /**
     * @brief Default constructor
     */
    Endpoint();

    /**
     * @brief Constructor with address and port
     * @param address IP address (IPv4 or IPv6)
     * @param port Port number
     * @param protocol Transport protocol
     */
    Endpoint(const platform::String<>& address, uint16_t port,
             TransportProtocol protocol = TransportProtocol::UDP);

    /**
     * @brief Copy constructor
     */
    Endpoint(const Endpoint& other);

    /**
     * @brief Move constructor
     */
    Endpoint(Endpoint&& other) noexcept;

    /**
     * @brief Assignment operator
     */
    Endpoint& operator=(const Endpoint& other);

    /**
     * @brief Move assignment operator
     */
    Endpoint& operator=(Endpoint&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~Endpoint() = default;

    // Accessors
    const platform::String<>& get_address() const { return address_; }
    void set_address(const platform::String<>& address) { address_ = address; }

    uint16_t get_port() const { return port_; }
    void set_port(uint16_t port) { port_ = port; }

    TransportProtocol get_protocol() const { return protocol_; }
    void set_protocol(TransportProtocol protocol) { protocol_ = protocol; }

    // Utility methods
    bool is_valid() const;
    bool is_multicast() const;
    bool is_ipv4() const;
    bool is_ipv6() const;
    std::string to_string() const;

    // Comparison operators
    bool operator==(const Endpoint& other) const;
    bool operator!=(const Endpoint& other) const;
    bool operator<(const Endpoint& other) const;

    // Hash function for use in unordered containers
    struct Hash {
        size_t operator()(const Endpoint& endpoint) const;
    };

private:
    platform::String<> address_;
    uint16_t port_;
    TransportProtocol protocol_;

    // Helper methods
    bool is_valid_ipv4(const platform::String<>& address) const;
    bool is_valid_ipv6(const platform::String<>& address) const;
    bool is_multicast_ipv4(const platform::String<>& address) const;
};

// Predefined endpoints for common SOME/IP usage
extern const Endpoint SOMEIP_SD_MULTICAST_ENDPOINT;
extern const Endpoint SOMEIP_DEFAULT_UDP_ENDPOINT;
extern const Endpoint SOMEIP_DEFAULT_TCP_ENDPOINT;

}  // namespace someip::transport

#endif // SOMEIP_TRANSPORT_ENDPOINT_H
