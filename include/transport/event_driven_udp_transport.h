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

#ifndef SOMEIP_TRANSPORT_EVENT_DRIVEN_UDP_TRANSPORT_H
#define SOMEIP_TRANSPORT_EVENT_DRIVEN_UDP_TRANSPORT_H

#include "transport/transport.h"
#include "transport/udp_socket_adapter.h"
#include "platform/thread.h"
#include <atomic>
#include <queue>
#include <string>

namespace someip {
namespace transport {

/**
 * @brief Configuration for event-driven UDP transport.
 */
struct EventDrivenUdpTransportConfig {
    std::string multicast_interface{};
    size_t max_message_size{1400};
};

/**
 * @brief ITransport implementation driven by an IUdpSocketAdapter.
 */
class EventDrivenUdpTransport : public ITransport {
public:
    explicit EventDrivenUdpTransport(IUdpSocketAdapter& adapter,
                                   const Endpoint& local_endpoint,
                                   const EventDrivenUdpTransportConfig& config = EventDrivenUdpTransportConfig());

    ~EventDrivenUdpTransport() override;

    EventDrivenUdpTransport(const EventDrivenUdpTransport&) = delete;
    EventDrivenUdpTransport& operator=(const EventDrivenUdpTransport&) = delete;

    [[nodiscard]] Result send_message(const Message& message, const Endpoint& endpoint) override;
    MessagePtr receive_message() override;
    Result connect(const Endpoint& endpoint) override;
    Result disconnect() override;
    bool is_connected() const override;
    Endpoint get_local_endpoint() const override;
    void set_listener(ITransportListener* listener) override;
    Result start() override;
    Result stop() override;
    bool is_running() const override;

    Result join_multicast_group(const std::string& multicast_address);
    Result leave_multicast_group(const std::string& multicast_address);

private:
    void on_adapter_receive(const std::vector<uint8_t>& data, const Endpoint& sender);
    static bool is_multicast_ipv4(const std::string& address);

    IUdpSocketAdapter& adapter_;
    Endpoint local_endpoint_;
    EventDrivenUdpTransportConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> opened_{false};
    std::atomic<ITransportListener*> listener_{nullptr};

    std::queue<MessagePtr> receive_queue_;
    platform::Mutex queue_mutex_;

    static constexpr size_t MAX_UDP_PAYLOAD = 65507;
};

} // namespace transport
} // namespace someip

#endif // SOMEIP_TRANSPORT_EVENT_DRIVEN_UDP_TRANSPORT_H
