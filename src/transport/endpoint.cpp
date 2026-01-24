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

#include "transport/endpoint.h"
#include <regex>
#include <sstream>
#include <functional>

namespace someip {
namespace transport {

/**
 * @brief Transport endpoint implementation
 * @implements REQ_TRANSPORT_006
 */

// Predefined endpoints
const Endpoint SOMEIP_SD_MULTICAST_ENDPOINT("239.118.122.69", 30490, TransportProtocol::MULTICAST_UDP);
const Endpoint SOMEIP_DEFAULT_UDP_ENDPOINT("127.0.0.1", 30490, TransportProtocol::UDP);
const Endpoint SOMEIP_DEFAULT_TCP_ENDPOINT("127.0.0.1", 30490, TransportProtocol::TCP);

Endpoint::Endpoint()
    : address_("127.0.0.1"), port_(30490), protocol_(TransportProtocol::UDP) {
}

Endpoint::Endpoint(const std::string& address, uint16_t port, TransportProtocol protocol)
    : address_(address), port_(port), protocol_(protocol) {
}

// NOLINTNEXTLINE(modernize-use-equals-default) - explicit copy for clarity
Endpoint::Endpoint(const Endpoint& other)
    : address_(other.address_), port_(other.port_), protocol_(other.protocol_) {
}

Endpoint::Endpoint(Endpoint&& other) noexcept
    : address_(std::move(other.address_)), port_(other.port_), protocol_(other.protocol_) {
}

Endpoint& Endpoint::operator=(const Endpoint& other) {
    if (this != &other) {
        address_ = other.address_;
        port_ = other.port_;
        protocol_ = other.protocol_;
    }
    return *this;
}

Endpoint& Endpoint::operator=(Endpoint&& other) noexcept {
    if (this != &other) {
        address_ = std::move(other.address_);
        port_ = other.port_;
        protocol_ = other.protocol_;
    }
    return *this;
}

bool Endpoint::is_valid() const {
    // Check port range (allow 0 for auto-assignment)
    if (port_ > 65535) {
        return false;
    }

    // Check address format
    return is_valid_ipv4(address_) || is_valid_ipv6(address_);
}

bool Endpoint::is_multicast() const {
    return is_multicast_ipv4(address_);
}

bool Endpoint::is_ipv4() const {
    return is_valid_ipv4(address_);
}

bool Endpoint::is_ipv6() const {
    return is_valid_ipv6(address_);
}

std::string Endpoint::to_string() const {
    std::stringstream ss;

    switch (protocol_) {
        case TransportProtocol::UDP:
            ss << "udp://";
            break;
        case TransportProtocol::TCP:
            ss << "tcp://";
            break;
        case TransportProtocol::MULTICAST_UDP:
            ss << "multicast://";
            break;
    }

    ss << address_ << ":" << port_;
    return ss.str();
}

bool Endpoint::operator==(const Endpoint& other) const {
    return address_ == other.address_ &&
           port_ == other.port_ &&
           protocol_ == other.protocol_;
}

bool Endpoint::operator!=(const Endpoint& other) const {
    return !(*this == other);
}

bool Endpoint::operator<(const Endpoint& other) const {
    if (protocol_ != other.protocol_) {
        return protocol_ < other.protocol_;
    }
    if (address_ != other.address_) {
        return address_ < other.address_;
    }
    return port_ < other.port_;
}

size_t Endpoint::Hash::operator()(const Endpoint& endpoint) const {
    size_t hash = 0;
    hash = std::hash<std::string>()(endpoint.address_);
    hash = hash * 31 + std::hash<uint16_t>()(endpoint.port_);
    hash = hash * 31 + std::hash<int>()(static_cast<int>(endpoint.protocol_));
    return hash;
}

bool Endpoint::is_valid_ipv4(const std::string& address) const {
    // Basic IPv4 validation regex
    std::regex ipv4_pattern(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
    std::smatch match;

    if (!std::regex_match(address, match, ipv4_pattern)) {
        return false;
    }

    // Check that each octet is in range 0-255
    for (size_t i = 1; i <= 4; ++i) {
        int octet = std::stoi(match[i].str());
        if (octet < 0 || octet > 255) {
            return false;
        }
    }

    return true;
}

bool Endpoint::is_valid_ipv6(const std::string& address) const {
    // Basic IPv6 validation (simplified)
    if (address.empty() || address.length() > 39) {
        return false;
    }

    // Check for valid IPv6 characters and structure
    std::regex ipv6_pattern(R"(^([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$|^::([0-9a-fA-F]{1,4}:){0,6}[0-9a-fA-F]{1,4}$|^([0-9a-fA-F]{1,4}:){1,7}:$|^::$)");

    return std::regex_match(address, ipv6_pattern);
}

bool Endpoint::is_multicast_ipv4(const std::string& address) const {
    if (!is_valid_ipv4(address)) {
        return false;
    }

    // Extract first octet
    size_t first_dot = address.find('.');
    if (first_dot == std::string::npos) {
        return false;
    }

    int first_octet = std::stoi(address.substr(0, first_dot));

    // IPv4 multicast range: 224.0.0.0 to 239.255.255.255
    return first_octet >= 224 && first_octet <= 239;
}

} // namespace transport
} // namespace someip
