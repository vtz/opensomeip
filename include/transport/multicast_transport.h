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

#ifndef SOMEIP_TRANSPORT_MULTICAST_TRANSPORT_H
#define SOMEIP_TRANSPORT_MULTICAST_TRANSPORT_H

#include "common/result.h"
#include <string>

namespace someip {
namespace transport {

/**
 * @brief Interface for transports that support IPv4 multicast group management.
 *
 * Both UdpTransport (BSD sockets) and EventDrivenUdpTransport (adapter-based)
 * implement this so that SD and other callers can join/leave multicast groups
 * without knowing the concrete transport type.
 */
class IMulticastTransport {
public:
    virtual ~IMulticastTransport() = default;

    [[nodiscard]] virtual Result join_multicast_group(const std::string& multicast_address) = 0;
    [[nodiscard]] virtual Result leave_multicast_group(const std::string& multicast_address) = 0;
};

} // namespace transport
} // namespace someip

#endif // SOMEIP_TRANSPORT_MULTICAST_TRANSPORT_H
