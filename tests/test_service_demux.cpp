/********************************************************************************
 * Copyright (c) 2026 Vinicius Tadeu Zein
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

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <mutex>

#include "platform/buffer_pool.h"
#include "platform/net.h"
#include "rpc/rpc_server.h"
#include "someip/message.h"
#include "someip/types.h"
#include "static_pool_init.h"
#include "transport/service_demux.h"
#include "transport/tcp_transport.h"
#include "transport/udp_transport.h"

/**
 * @brief Shared-socket demux driven by a raw UDP or TCP peer.
 * @tests REQ_TRANSPORT_013, REQ_TRANSPORT_017, REQ_TRANSPORT_022
 * @tests feat_req_someip_445, feat_req_someip_732
 */

namespace {

using someip::Message;
using someip::MessageId;
using someip::MessagePtr;
using someip::MessageType;
using someip::RequestId;
using someip::Result;
using someip::ReturnCode;
using someip::rpc::RpcResult;
using someip::rpc::RpcServer;
using someip::transport::Endpoint;
using someip::transport::ITransport;
using someip::transport::ITransportListener;
using someip::transport::MAX_DEMUX_SERVICES;
using someip::transport::ServiceDemux;
using someip::transport::TcpTransport;
using someip::transport::TcpTransportConfig;
using someip::transport::TransportProtocol;
using someip::transport::UdpTransport;
using someip::transport::UdpTransportConfig;

constexpr uint16_t kClientId = 0x00A1;
constexpr uint16_t kMethodId = 0x0007;
constexpr uint16_t kServiceA = 0x1001;
constexpr uint16_t kServiceB = 0x2002;
constexpr uint8_t kMarkerA = 0xA1;
constexpr uint8_t kMarkerB = 0xB2;
constexpr int kIoTimeoutMs = 2000;

class NopListener : public ITransportListener {
   public:
    void on_message_received(MessagePtr /*message*/, const Endpoint& /*sender*/) override
    {
    }
    void on_connection_lost(const Endpoint& /*endpoint*/) override
    {
    }
    void on_connection_established(const Endpoint& /*endpoint*/) override
    {
    }
    void on_error(Result /*error*/) override
    {
    }
};

class RecordingHandler : public ITransportListener {
   public:
    RecordingHandler(ITransport& transport, uint8_t marker) : transport_(transport), marker_(marker)
    {
    }

    void on_message_received(MessagePtr message, const Endpoint& sender) override
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            sender_ = sender;
            received_ = true;
        }
        Message response(message->get_message_id(), message->get_request_id(),
                         MessageType::RESPONSE, ReturnCode::E_OK);
        response.set_interface_version(message->get_interface_version());
        const uint8_t body[] = {marker_};
        response.set_payload(body, 1);
        static_cast<void>(transport_.send_message(response, sender));
    }

    void on_connection_lost(const Endpoint& /*endpoint*/) override
    {
    }
    void on_connection_established(const Endpoint& /*endpoint*/) override
    {
    }
    void on_error(Result /*error*/) override
    {
    }

    [[nodiscard]] bool received() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return received_;
    }

    [[nodiscard]] Endpoint sender() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return sender_;
    }

   private:
    ITransport& transport_;
    uint8_t marker_;
    mutable std::mutex mutex_;
    Endpoint sender_{};
    bool received_{false};
};

class Socket {
   public:
    Socket(int domain, int type, int protocol) : fd_(someip_socket(domain, type, protocol))
    {
    }
    ~Socket()
    {
        if (fd_ != SOMEIP_INVALID_SOCKET) {
            someip_close_socket(fd_);
        }
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    [[nodiscard]] someip_socket_t get() const
    {
        return fd_;
    }
    [[nodiscard]] explicit operator bool() const
    {
        return fd_ != SOMEIP_INVALID_SOCKET;
    }

   private:
    someip_socket_t fd_{SOMEIP_INVALID_SOCKET};
};

UdpTransportConfig udp_config()
{
    UdpTransportConfig config;
    config.blocking = true;
    config.enable_tp = false;
    return config;
}

TcpTransportConfig tcp_config()
{
    TcpTransportConfig config;
    config.magic_cookie_enabled = false;
    config.connection_timeout = std::chrono::milliseconds(kIoTimeoutMs);
    config.receive_timeout = std::chrono::milliseconds(100);
    config.send_timeout = std::chrono::milliseconds(kIoTimeoutMs);
    config.max_receive_buffer = 4096;
    return config;
}

bool register_echo(RpcServer& server, uint8_t marker)
{
    return server.register_method(kMethodId, [marker](uint16_t /*client*/, uint16_t /*session*/,
                                                      const someip::platform::ByteBuffer& /*input*/,
                                                      someip::platform::ByteBuffer& output) {
        output.resize(1);
        output[0] = marker;
        return RpcResult::SUCCESS;
    });
}

someip::platform::ByteBuffer make_request(uint16_t service_id, uint16_t session_id)
{
    Message request(MessageId(service_id, kMethodId), RequestId(kClientId, session_id),
                    MessageType::REQUEST, ReturnCode::E_OK);
    const uint8_t body[] = {0x42};
    request.set_payload(body, 1);
    return request.serialize();
}

void expect_echo(const Message& response, uint16_t service_id, uint16_t session_id, uint8_t marker)
{
    EXPECT_EQ(response.get_service_id(), service_id);
    EXPECT_EQ(response.get_method_id(), kMethodId);
    EXPECT_EQ(response.get_client_id(), kClientId);
    EXPECT_EQ(response.get_session_id(), session_id);
    EXPECT_EQ(response.get_message_type(), MessageType::RESPONSE);
    EXPECT_EQ(response.get_return_code(), ReturnCode::E_OK);
    ASSERT_EQ(response.get_payload().size(), 1U);
    EXPECT_EQ(response.get_payload()[0], marker);
}

bool set_timeouts(someip_socket_t fd)
{
    return someip_set_socket_timeout(fd, SO_RCVTIMEO, kIoTimeoutMs) == 0 &&
           someip_set_socket_timeout(fd, SO_SNDTIMEO, kIoTimeoutMs) == 0;
}

bool bind_udp(someip_socket_t fd, uint16_t& port)
{
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(0);
    if (someip_inet_pton(AF_INET, "127.0.0.1", &local.sin_addr) != 1) {
        return false;
    }
    if (someip_bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
        return false;
    }
    socklen_t length = sizeof(local);
    if (someip_getsockname(fd, reinterpret_cast<sockaddr*>(&local), &length) != 0) {
        return false;
    }
    port = ntohs(local.sin_port);
    return port != 0;
}

bool read_exact(someip_socket_t fd, uint8_t* data, std::size_t size)
{
    std::size_t got = 0;
    while (got < size) {
        const ssize_t nread = someip_recv(fd, data + got, size - got, 0);
        if (nread <= 0) {
            return false;
        }
        got += static_cast<std::size_t>(nread);
    }
    return true;
}

bool is_magic_cookie(const uint8_t* header)
{
    const someip::platform::ByteBuffer server_cookie = TcpTransport::make_magic_cookie_server();
    const someip::platform::ByteBuffer client_cookie = TcpTransport::make_magic_cookie_client();
    const bool server =
        server_cookie.size() == 16U && std::memcmp(header, server_cookie.data(), 16) == 0;
    const bool client =
        client_cookie.size() == 16U && std::memcmp(header, client_cookie.data(), 16) == 0;
    return server || client;
}

bool read_someip(someip_socket_t fd, Message& message)
{
    for (int frame = 0; frame < 4; ++frame) {
        std::array<uint8_t, 16> header{};
        if (!read_exact(fd, header.data(), header.size())) {
            return false;
        }
        if (is_magic_cookie(header.data())) {
            continue;
        }
        const uint32_t length =
            (static_cast<uint32_t>(header[4]) << 24U) | (static_cast<uint32_t>(header[5]) << 16U) |
            (static_cast<uint32_t>(header[6]) << 8U) | static_cast<uint32_t>(header[7]);
        if (length < 8U || length > 1400U) {
            return false;
        }
        someip::platform::ByteBuffer frame_bytes(static_cast<std::size_t>(8U + length));
        if (frame_bytes.size() != 8U + length) {
            return false;
        }
        std::memcpy(frame_bytes.data(), header.data(), header.size());
        const std::size_t rest = static_cast<std::size_t>(length) - 8U;
        if (rest > 0U && !read_exact(fd, frame_bytes.data() + header.size(), rest)) {
            return false;
        }
        return message.deserialize(frame_bytes.data(), frame_bytes.size());
    }
    return false;
}

bool udp_exchange(someip_socket_t fd, uint16_t server_port, uint16_t service_id,
                  uint16_t session_id, Message& response, sockaddr_in* source)
{
    const someip::platform::ByteBuffer bytes = make_request(service_id, session_id);
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(server_port);
    if (someip_inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr) != 1) {
        return false;
    }
    const ssize_t sent = someip_sendto(fd, bytes.data(), bytes.size(), 0,
                                       reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
    if (sent != static_cast<ssize_t>(bytes.size())) {
        return false;
    }

    std::array<uint8_t, 256> buffer{};
    sockaddr_in src{};
    socklen_t src_len = sizeof(src);
    const ssize_t nread = someip_recvfrom(fd, buffer.data(), buffer.size(), 0,
                                          reinterpret_cast<sockaddr*>(&src), &src_len);
    if (nread < static_cast<ssize_t>(Message::get_header_size())) {
        return false;
    }
    if (source != nullptr) {
        *source = src;
    }
    return response.deserialize(buffer.data(), static_cast<std::size_t>(nread));
}

bool tcp_exchange(someip_socket_t fd, uint16_t service_id, uint16_t session_id, Message& response)
{
    const someip::platform::ByteBuffer bytes = make_request(service_id, session_id);
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const ssize_t nwrite = someip_send(fd, bytes.data() + sent, bytes.size() - sent, 0);
        if (nwrite <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(nwrite);
    }
    return read_someip(fd, response);
}

}  // namespace

/**
 * @test_case TC_DEMUX_CAPACITY
 * @tests REQ_TRANSPORT_013
 * @brief The fixed table keeps the first service id when a later registration does not fit.
 */
TEST(ServiceDemux, FixedCapacityKeepsExistingServices)
{
    UdpTransport transport(Endpoint("127.0.0.1", 0), udp_config());
    ServiceDemux demux(transport);
    std::array<NopListener, MAX_DEMUX_SERVICES> listeners;

    for (uint16_t service_id = 0; service_id < MAX_DEMUX_SERVICES; ++service_id) {
        ASSERT_EQ(demux.register_service(service_id, listeners[service_id]), Result::SUCCESS);
    }

    NopListener extra;
    EXPECT_EQ(demux.register_service(0, extra), Result::INVALID_STATE);
    EXPECT_EQ(demux.register_service(static_cast<uint16_t>(MAX_DEMUX_SERVICES), extra),
              Result::RESOURCE_EXHAUSTED);
    EXPECT_TRUE(demux.contains(0));
    EXPECT_TRUE(demux.contains(1));

    ASSERT_EQ(demux.unregister_service(0), Result::SUCCESS);
    EXPECT_FALSE(demux.contains(0));
    EXPECT_TRUE(demux.contains(1));
    ASSERT_EQ(demux.register_service(0, extra), Result::SUCCESS);
    EXPECT_TRUE(demux.contains(0));
    EXPECT_TRUE(demux.contains(1));
}

/**
 * @test_case TC_DEMUX_UDP_TWO_SERVICES
 * @tests REQ_TRANSPORT_013, feat_req_someip_732
 * @brief Two service ids on one UDP port are answered by their own handlers.
 */
TEST(ServiceDemux, TwoServiceIdsShareOneUdpPort)
{
    UdpTransport transport(Endpoint("127.0.0.1", 0), udp_config());
    ServiceDemux demux(transport);
    RpcServer first(kServiceA, demux);
    RpcServer second(kServiceB, demux);
    ASSERT_TRUE(register_echo(first, kMarkerA));
    ASSERT_TRUE(register_echo(second, kMarkerB));
    ASSERT_TRUE(first.initialize());
    ASSERT_TRUE(second.initialize());
    ASSERT_EQ(demux.start(), Result::SUCCESS);
    EXPECT_TRUE(first.is_ready());
    EXPECT_TRUE(second.is_ready());

    const uint16_t server_port = transport.get_local_endpoint().get_port();
    ASSERT_NE(server_port, 0);

    Socket peer(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_TRUE(peer);
    ASSERT_TRUE(set_timeouts(peer.get()));
    uint16_t peer_port = 0;
    ASSERT_TRUE(bind_udp(peer.get(), peer_port));

    Message response_a;
    sockaddr_in source_a{};
    ASSERT_TRUE(udp_exchange(peer.get(), server_port, kServiceA, 0x0001, response_a, &source_a));
    expect_echo(response_a, kServiceA, 0x0001, kMarkerA);
    EXPECT_EQ(ntohs(source_a.sin_port), server_port);

    Message response_b;
    sockaddr_in source_b{};
    ASSERT_TRUE(udp_exchange(peer.get(), server_port, kServiceB, 0x0002, response_b, &source_b));
    expect_echo(response_b, kServiceB, 0x0002, kMarkerB);
    EXPECT_EQ(ntohs(source_b.sin_port), server_port);
    EXPECT_NE(peer_port, server_port);
    EXPECT_EQ(demux.stop(), Result::SUCCESS);
}

/**
 * @test_case TC_DEMUX_UDP_REREGISTER
 * @tests REQ_TRANSPORT_013, feat_req_someip_445
 * @brief Registering a service id that is already on the socket fails and leaves both handlers.
 */
TEST(ServiceDemux, ReregisteringServiceIdFails)
{
    UdpTransport transport(Endpoint("127.0.0.1", 0), udp_config());
    ServiceDemux demux(transport);
    RpcServer first(kServiceA, demux);
    RpcServer second(kServiceB, demux);
    ASSERT_TRUE(register_echo(first, kMarkerA));
    ASSERT_TRUE(register_echo(second, kMarkerB));
    ASSERT_TRUE(first.initialize());
    ASSERT_TRUE(second.initialize());
    ASSERT_EQ(demux.start(), Result::SUCCESS);
    const uint16_t server_port = transport.get_local_endpoint().get_port();

    {
        RpcServer duplicate(kServiceA, demux);
        ASSERT_TRUE(register_echo(duplicate, 0xFF));
        EXPECT_FALSE(duplicate.initialize());
        EXPECT_EQ(duplicate.get_transport_result(), Result::INVALID_STATE);
        EXPECT_TRUE(demux.contains(kServiceA));
        EXPECT_TRUE(demux.contains(kServiceB));
    }

    Socket peer(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_TRUE(peer);
    ASSERT_TRUE(set_timeouts(peer.get()));
    uint16_t peer_port = 0;
    ASSERT_TRUE(bind_udp(peer.get(), peer_port));

    Message response_a;
    ASSERT_TRUE(udp_exchange(peer.get(), server_port, kServiceA, 0x0003, response_a, nullptr));
    expect_echo(response_a, kServiceA, 0x0003, kMarkerA);

    Message response_b;
    ASSERT_TRUE(udp_exchange(peer.get(), server_port, kServiceB, 0x0004, response_b, nullptr));
    expect_echo(response_b, kServiceB, 0x0004, kMarkerB);
    EXPECT_EQ(demux.stop(), Result::SUCCESS);
}

/**
 * @test_case TC_DEMUX_UDP_INSTANCES
 * @tests REQ_TRANSPORT_022, feat_req_someip_445
 * @brief Two instances of one service id use two ports. The second port is a second socket.
 */
TEST(ServiceDemux, TwoInstancesOfOneServiceUseTwoPorts)
{
    UdpTransport first_transport(Endpoint("127.0.0.1", 0), udp_config());
    UdpTransport second_transport(Endpoint("127.0.0.1", 0), udp_config());
    ServiceDemux first_demux(first_transport);
    ServiceDemux second_demux(second_transport);
    RpcServer first(kServiceA, first_demux);
    RpcServer second(kServiceA, second_demux);
    ASSERT_TRUE(register_echo(first, kMarkerA));
    ASSERT_TRUE(register_echo(second, kMarkerB));
    ASSERT_TRUE(first.initialize());
    ASSERT_TRUE(second.initialize());
    ASSERT_EQ(first_demux.start(), Result::SUCCESS);
    ASSERT_EQ(second_demux.start(), Result::SUCCESS);

    const uint16_t first_port = first_transport.get_local_endpoint().get_port();
    const uint16_t second_port = second_transport.get_local_endpoint().get_port();
    ASSERT_NE(first_port, 0);
    ASSERT_NE(second_port, 0);
    EXPECT_NE(first_port, second_port);

    RpcServer rejected(kServiceA, first_demux);
    ASSERT_TRUE(register_echo(rejected, 0xFF));
    EXPECT_FALSE(rejected.initialize());
    EXPECT_TRUE(first_demux.contains(kServiceA));
    EXPECT_TRUE(second_demux.contains(kServiceA));

    Socket peer(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_TRUE(peer);
    ASSERT_TRUE(set_timeouts(peer.get()));
    uint16_t peer_port = 0;
    ASSERT_TRUE(bind_udp(peer.get(), peer_port));

    Message from_first;
    sockaddr_in source_first{};
    ASSERT_TRUE(udp_exchange(peer.get(), first_port, kServiceA, 0x0005, from_first, &source_first));
    expect_echo(from_first, kServiceA, 0x0005, kMarkerA);
    EXPECT_EQ(ntohs(source_first.sin_port), first_port);

    Message from_second;
    sockaddr_in source_second{};
    ASSERT_TRUE(
        udp_exchange(peer.get(), second_port, kServiceA, 0x0006, from_second, &source_second));
    expect_echo(from_second, kServiceA, 0x0006, kMarkerB);
    EXPECT_EQ(ntohs(source_second.sin_port), second_port);
    EXPECT_EQ(first_demux.stop(), Result::SUCCESS);
    EXPECT_EQ(second_demux.stop(), Result::SUCCESS);
}

/**
 * @test_case TC_DEMUX_UDP_SENDER
 * @tests REQ_TRANSPORT_013
 * @brief The sender endpoint delivered to the transport listener reaches the service handler.
 */
TEST(ServiceDemux, SenderEndpointReachesHandler)
{
    UdpTransport transport(Endpoint("127.0.0.1", 0), udp_config());
    ServiceDemux demux(transport);
    RecordingHandler handler(demux.transport(), kMarkerA);
    ASSERT_EQ(demux.register_service(kServiceA, handler), Result::SUCCESS);
    ASSERT_EQ(demux.start(), Result::SUCCESS);
    const uint16_t server_port = transport.get_local_endpoint().get_port();

    Socket peer(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_TRUE(peer);
    ASSERT_TRUE(set_timeouts(peer.get()));
    uint16_t peer_port = 0;
    ASSERT_TRUE(bind_udp(peer.get(), peer_port));

    Message response;
    ASSERT_TRUE(udp_exchange(peer.get(), server_port, kServiceA, 0x0007, response, nullptr));
    expect_echo(response, kServiceA, 0x0007, kMarkerA);
    ASSERT_TRUE(handler.received());
    const Endpoint sender = handler.sender();
    EXPECT_STREQ(sender.get_address().c_str(), "127.0.0.1");
    EXPECT_EQ(sender.get_port(), peer_port);
    EXPECT_EQ(sender.get_protocol(), TransportProtocol::UDP);
    EXPECT_EQ(demux.stop(), Result::SUCCESS);
}

/**
 * @test_case TC_DEMUX_TCP_TWO_SERVICES
 * @tests REQ_TRANSPORT_013, REQ_TRANSPORT_017, feat_req_someip_732
 * @brief One TCP listener demuxes two service ids for a raw connected peer.
 */
TEST(ServiceDemux, TwoServiceIdsShareOneTcpListener)
{
    TcpTransport transport(tcp_config());
    ASSERT_EQ(transport.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(transport.enable_server_mode(), Result::SUCCESS);
    ServiceDemux demux(transport);
    RpcServer first(kServiceA, demux);
    RpcServer second(kServiceB, demux);
    ASSERT_TRUE(register_echo(first, kMarkerA));
    ASSERT_TRUE(register_echo(second, kMarkerB));
    ASSERT_TRUE(first.initialize());
    ASSERT_TRUE(second.initialize());
    ASSERT_EQ(demux.start(), Result::SUCCESS);
    const uint16_t server_port = transport.get_local_endpoint().get_port();
    ASSERT_NE(server_port, 0);

    {
        RpcServer duplicate(kServiceA, demux);
        ASSERT_TRUE(register_echo(duplicate, 0xFF));
        EXPECT_FALSE(duplicate.initialize());
        EXPECT_TRUE(demux.contains(kServiceA));
        EXPECT_TRUE(demux.contains(kServiceB));
    }

    Socket peer(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_TRUE(peer);
    ASSERT_TRUE(set_timeouts(peer.get()));
    const int nodelay = 1;
    ASSERT_EQ(someip_setsockopt(peer.get(), IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)),
              0);

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(server_port);
    ASSERT_EQ(someip_inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr), 1);
    ASSERT_EQ(someip_connect(peer.get(), reinterpret_cast<sockaddr*>(&dest), sizeof(dest)), 0)
        << someip_socket_errno();

    Message response_a;
    ASSERT_TRUE(tcp_exchange(peer.get(), kServiceA, 0x0008, response_a));
    expect_echo(response_a, kServiceA, 0x0008, kMarkerA);

    Message response_b;
    ASSERT_TRUE(tcp_exchange(peer.get(), kServiceB, 0x0009, response_b));
    expect_echo(response_b, kServiceB, 0x0009, kMarkerB);
    EXPECT_EQ(demux.stop(), Result::SUCCESS);
}
