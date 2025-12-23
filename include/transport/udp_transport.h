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

#ifndef SOMEIP_TRANSPORT_UDP_TRANSPORT_H
#define SOMEIP_TRANSPORT_UDP_TRANSPORT_H

#include "transport/transport.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <netinet/in.h>

namespace someip {
namespace transport {

/**
 * @brief UDP transport implementation
 *
 * This class provides UDP-based transport for SOME/IP messages.
 * It supports both unicast and multicast communication.
 */
class UdpTransport : public ITransport {
public:
    /**
     * @brief Constructor
     * @param local_endpoint Local endpoint to bind to
     */
    explicit UdpTransport(const Endpoint& local_endpoint);

    /**
     * @brief Destructor
     */
    ~UdpTransport() override;

    // ITransport interface implementation
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

    // Multicast support
    Result join_multicast_group(const std::string& multicast_address);
    Result leave_multicast_group(const std::string& multicast_address);

private:
    Endpoint local_endpoint_;
    int socket_fd_{-1};
    std::atomic<bool> running_;
    std::thread receive_thread_;
    ITransportListener* listener_{nullptr};

    // Thread-safe message queue
    std::queue<MessagePtr> receive_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Socket management
    std::mutex socket_mutex_;

    // Constants
    static constexpr size_t MAX_UDP_PAYLOAD = 65507; // Maximum UDP payload size
    static constexpr size_t RECEIVE_BUFFER_SIZE = 8192;

    // Private methods
    Result create_socket();
    Result bind_socket();
    Result configure_multicast(const Endpoint& endpoint);
    void receive_loop();
    Result send_data(const std::vector<uint8_t>& data, const Endpoint& endpoint);
    Result receive_data(std::vector<uint8_t>& data, Endpoint& sender);
    sockaddr_in create_sockaddr(const Endpoint& endpoint) const;
    Endpoint sockaddr_to_endpoint(const sockaddr_in& addr) const;
    bool is_multicast_address(const std::string& address) const;

    // Disable copy and assignment
    UdpTransport(const UdpTransport&) = delete;
    UdpTransport& operator=(const UdpTransport&) = delete;
};

} // namespace transport
} // namespace someip

#endif // SOMEIP_TRANSPORT_UDP_TRANSPORT_H
