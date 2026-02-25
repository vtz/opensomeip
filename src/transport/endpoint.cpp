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
#include <sstream>
#include <functional>
#include <cstdlib>
#include <cstring>
#include <cctype>

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
    if (address.empty() || address.size() > 15) {
        return false;
    }

    int octets = 0;
    size_t pos = 0;

    while (pos < address.size() && octets < 4) {
        if (!std::isdigit(static_cast<unsigned char>(address[pos]))) {
            return false;
        }

        size_t start = pos;
        while (pos < address.size() && std::isdigit(static_cast<unsigned char>(address[pos]))) {
            ++pos;
        }

        size_t digit_len = pos - start;
        if (digit_len == 0 || digit_len > 3) {
            return false;
        }

        int val = std::atoi(address.substr(start, digit_len).c_str());
        if (val < 0 || val > 255) {
            return false;
        }

        // Reject leading zeros (e.g., "01.02.03.04")
        if (digit_len > 1 && address[start] == '0') {
            return false;
        }

        ++octets;

        if (octets < 4) {
            if (pos >= address.size() || address[pos] != '.') {
                return false;
            }
            ++pos;
        }
    }

    return octets == 4 && pos == address.size();
}

bool Endpoint::is_valid_ipv6(const std::string& address) const {
    if (address.empty() || address.size() > 39) {
        return false;
    }

    for (char c : address) {
        if (!std::isxdigit(static_cast<unsigned char>(c)) && c != ':') {
            return false;
        }
    }

    size_t double_colon = address.find("::");
    bool has_double_colon = (double_colon != std::string::npos);
    if (has_double_colon && address.find("::", double_colon + 2) != std::string::npos) {
        return false;
    }

    if (!has_double_colon) {
        if (address.front() == ':' || address.back() == ':') {
            return false;
        }
    }

    // Reject ":::" by checking for three consecutive colons
    if (address.find(":::") != std::string::npos) {
        return false;
    }

    int groups = 0;
    size_t pos = 0;
    bool compression_used = false;

    while (pos <= address.size()) {
        size_t next = address.find(':', pos);
        if (next == std::string::npos) {
            next = address.size();
        }
        size_t len = next - pos;

        if (len > 0) {
            if (len > 4) {
                return false;
            }
            ++groups;
        } else {
            // Allow a single "::" compression sequence (two adjacent colons).
            if (has_double_colon && !compression_used && pos == double_colon) {
                compression_used = true;
            } else if (has_double_colon && compression_used && pos == double_colon + 1) {
                // Second ':' from the same "::" token; valid, nothing to count.
            } else if (pos > 0 && next < address.size()) {
                return false;
            }
        }

        if (next == address.size()) {
            break;
        }
        pos = next + 1;
    }

    if (has_double_colon) {
        // "::" can compress one or more 16-bit groups, including all 8 groups.
        return groups <= 7;
    }
    return groups == 8;
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
