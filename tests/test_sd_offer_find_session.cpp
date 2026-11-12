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

/**
 * @file test_sd_offer_find_session.cpp
 * @brief UDP datagram peer for SD offer, find, and session wrap.
 *
 * The peer opens its own sockets and does not use the library transport.
 * OfferService is sent to the multicast address the test stores in SdConfig.
 * StopOffer is that same Offer entry with TTL 0. FindService for an offered
 * instance is answered with a unicast Offer. After the multicast SD session
 * counter passes 0xFFFF, the next datagram still parses and the reboot flag
 * is clear. The library default multicast group stays 239.255.255.251.
 */

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "platform/net.h"
#include "sd/sd_message.h"
#include "sd/sd_server.h"
#include "sd/sd_types.h"
#include "someip/message.h"
#include "someip/types.h"
#include "static_pool_init.h"

namespace {

using namespace someip;
using namespace someip::sd;

constexpr char kConfiguredMulticast[] = "239.0.0.1";
constexpr char kLibraryDefaultMulticast[] = "239.255.255.251";
constexpr char kLoopback[] = "127.0.0.1";
constexpr char kServiceEndpoint[] = "127.0.0.1:30509";
constexpr uint16_t kOfferServiceId = 0x1234;
constexpr uint16_t kOfferInstanceId = 0x0007;
constexpr uint32_t kOfferTtlSeconds = 30;
constexpr uint16_t kWrapServiceId = 0x2222;
constexpr uint16_t kWrapInstanceId = 0x0003;
constexpr uint32_t kSessionWrapSends = 65536U;
constexpr int kReceiveBufferBytes = 8 * 1024 * 1024;

struct Datagram {
    std::array<uint8_t, 2048> bytes{};
    size_t size{0};
    std::string destination;
};

struct WireSd {
    bool header_ok{false};
    bool sd_ok{false};
    uint16_t session_id{0};
    bool reboot{false};
    bool has_service{false};
    EntryType entry_type{EntryType::FIND_SERVICE};
    uint16_t service_id{0};
    uint16_t instance_id{0};
    uint32_t ttl{0};
    std::string destination;
};

class RawUdpPeer {
   public:
    RawUdpPeer() = default;
    ~RawUdpPeer()
    {
        close();
    }

    RawUdpPeer(const RawUdpPeer&) = delete;
    RawUdpPeer& operator=(const RawUdpPeer&) = delete;
    RawUdpPeer(RawUdpPeer&&) = delete;
    RawUdpPeer& operator=(RawUdpPeer&&) = delete;

    bool open()
    {
        close();
        fd_ = someip_socket(AF_INET, SOCK_DGRAM, 0);
        return fd_ != SOMEIP_INVALID_SOCKET;
    }

    void close()
    {
        if (fd_ != SOMEIP_INVALID_SOCKET) {
            someip_close_socket(fd_);
            fd_ = SOMEIP_INVALID_SOCKET;
        }
    }

    bool enable_reuse()
    {
        const int on = 1;
        if (someip_setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
            return false;
        }
#ifdef SO_REUSEPORT
        if (someip_setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0) {
            return false;
        }
#endif
        return true;
    }

    bool bind_to(const char* address, uint16_t port)
    {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (someip_inet_pton(AF_INET, address, &addr.sin_addr) != 1) {
            return false;
        }
        if (someip_bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            return false;
        }
        sockaddr_in bound{};
        socklen_t length = sizeof(bound);
        if (someip_getsockname(fd_, reinterpret_cast<sockaddr*>(&bound), &length) < 0) {
            return false;
        }
        port_ = ntohs(bound.sin_port);
        bound_address_ = address;
        return port_ != 0;
    }

    bool join_group(const char* group)
    {
        ip_mreq membership{};
        membership.imr_multiaddr.s_addr = someip_inet_addr(group);
        membership.imr_interface.s_addr = someip_inet_addr(kLoopback);
        if (someip_setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership,
                              sizeof(membership)) == 0) {
            return true;
        }
        membership.imr_interface.s_addr = htonl(INADDR_ANY);
        return someip_setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership,
                                 sizeof(membership)) == 0;
    }

    bool tune()
    {
        const int buffer_bytes = kReceiveBufferBytes;
        (void)someip_setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &buffer_bytes, sizeof(buffer_bytes));
        (void)someip_enable_recv_dest(fd_);
        return someip_set_nonblocking(fd_) == 0;
    }

    bool send_to(const char* address, uint16_t port, const uint8_t* data, size_t size)
    {
        sockaddr_in dest{};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(port);
        if (someip_inet_pton(AF_INET, address, &dest.sin_addr) != 1) {
            return false;
        }
        const ssize_t sent =
            someip_sendto(fd_, data, size, 0, reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
        return sent == static_cast<ssize_t>(size);
    }

    bool wait_readable(std::chrono::milliseconds timeout) const
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        for (;;) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                return false;
            }
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(fd_, &readable);
            timeval wait{};
            wait.tv_sec = static_cast<decltype(wait.tv_sec)>(left.count() / 1000);
            wait.tv_usec = static_cast<decltype(wait.tv_usec)>((left.count() % 1000) * 1000);
#if defined(_WIN32)
            const int nfds = 0;
#else
            const int nfds = fd_ + 1;
#endif
            const int ready = someip_select(nfds, &readable, nullptr, nullptr, &wait);
            if (ready > 0) {
                return true;
            }
            if (ready < 0 && someip_socket_errno() == SOMEIP_EINTR) {
                continue;
            }
            return false;
        }
    }

    bool try_recv(Datagram& out)
    {
        for (;;) {
            sockaddr_in source{};
            socklen_t length = sizeof(source);
            char destination[INET_ADDRSTRLEN] = {};
            const ssize_t received = someip_recvfrom_with_dest(
                fd_, out.bytes.data(), out.bytes.size(), 0, reinterpret_cast<sockaddr*>(&source),
                &length, destination, sizeof(destination));
            if (received > 0) {
                out.size = static_cast<size_t>(received);
                out.destination = destination;
                return true;
            }
            if (someip_socket_errno() == SOMEIP_EINTR) {
                continue;
            }
            out.size = 0;
            out.destination.clear();
            return false;
        }
    }

    [[nodiscard]] someip_socket_t fd() const
    {
        return fd_;
    }
    [[nodiscard]] uint16_t port() const
    {
        return port_;
    }
    [[nodiscard]] const std::string& bound_address() const
    {
        return bound_address_;
    }
    [[nodiscard]] bool valid() const
    {
        return fd_ != SOMEIP_INVALID_SOCKET;
    }

   private:
    someip_socket_t fd_{SOMEIP_INVALID_SOCKET};
    uint16_t port_{0};
    std::string bound_address_;
};

struct SdEndpoints {
    RawUdpPeer offer_group;
    RawUdpPeer other_group;
    std::unique_ptr<SdServer> server;
    uint16_t port{0};
    const char* offer_address{kLibraryDefaultMulticast};
};

struct JoinThread {
    std::atomic<bool>& stop;
    std::thread& worker;

    ~JoinThread()
    {
        stop.store(true, std::memory_order_release);
        if (worker.joinable()) {
            worker.join();
        }
    }
};

bool session_from_header(const uint8_t* data, size_t size, uint16_t& session)
{
    if (data == nullptr || size < 16U) {
        return false;
    }
    session = static_cast<uint16_t>((static_cast<uint16_t>(data[10]) << 8U) |
                                    static_cast<uint16_t>(data[11]));
    return true;
}

const ServiceEntry* first_service(const SdMessage& message)
{
    for (const auto& entry : message.get_entries()) {
        if (const auto* service = std::get_if<ServiceEntry>(&entry)) {
            return service;
        }
    }
    return nullptr;
}

WireSd parse_datagram(const Datagram& datagram)
{
    WireSd parsed;
    parsed.destination = datagram.destination;
    Message message;
    if (!message.deserialize(datagram.bytes.data(), datagram.size)) {
        return parsed;
    }
    parsed.header_ok = true;
    parsed.session_id = message.get_session_id();
    if (message.get_service_id() != SOMEIP_SD_SERVICE_ID ||
        message.get_method_id() != SOMEIP_SD_METHOD_ID) {
        return parsed;
    }
    SdMessage sd_message;
    if (!sd_message.deserialize(message.get_payload())) {
        return parsed;
    }
    parsed.sd_ok = true;
    parsed.reboot = sd_message.get_reboot_flag();
    if (const ServiceEntry* service = first_service(sd_message)) {
        parsed.has_service = true;
        parsed.entry_type = service->get_type();
        parsed.service_id = service->get_service_id();
        parsed.instance_id = service->get_instance_id();
        parsed.ttl = service->get_ttl();
    }
    return parsed;
}

WireSd parse_bytes(const std::vector<uint8_t>& bytes)
{
    Datagram datagram;
    if (bytes.size() > datagram.bytes.size()) {
        return {};
    }
    for (size_t i = 0; i < bytes.size(); ++i) {
        datagram.bytes[i] = bytes[i];
    }
    datagram.size = bytes.size();
    return parse_datagram(datagram);
}

bool is_live_offer(const WireSd& message, uint16_t service_id, uint16_t instance_id)
{
    return message.sd_ok && message.has_service && message.entry_type == EntryType::OFFER_SERVICE &&
           message.ttl > 0U && message.service_id == service_id &&
           message.instance_id == instance_id;
}

bool is_stop_offer(const WireSd& message, uint16_t service_id, uint16_t instance_id)
{
    return message.sd_ok && message.has_service && message.entry_type == EntryType::OFFER_SERVICE &&
           message.ttl == 0U && message.service_id == service_id &&
           message.instance_id == instance_id;
}

void expect_bound_destination(const WireSd& message, const char* address)
{
    if (!message.destination.empty()) {
        EXPECT_EQ(message.destination, address);
    }
}

void expect_quiet(RawUdpPeer& peer, std::chrono::milliseconds budget)
{
    if (!peer.wait_readable(budget)) {
        return;
    }
    Datagram datagram;
    if (!peer.try_recv(datagram)) {
        return;
    }
    const WireSd parsed = parse_datagram(datagram);
    ADD_FAILURE() << "unexpected datagram on " << peer.bound_address() << ":" << peer.port()
                  << " session=" << parsed.session_id << " sd_ok=" << parsed.sd_ok
                  << " service=" << parsed.service_id << " ttl=" << parsed.ttl
                  << " dest=" << parsed.destination;
}

SdConfig make_sd_config(uint16_t port, uint32_t initial_delay_ms, const char* multicast_address)
{
    SdConfig config;
    config.unicast_address = kLoopback;
    config.unicast_port = port;
    if (multicast_address != nullptr) {
        config.multicast_address = multicast_address;
    }
    config.multicast_port = port;
    config.has_initial_delay_override = true;
    config.initial_delay_override_ms = initial_delay_ms;
    const auto delay = std::chrono::milliseconds(initial_delay_ms);
    config.initial_delay_min = delay;
    config.initial_delay_max = delay;
    config.initial_delay = delay;
    config.repetition_base = std::chrono::milliseconds(60000);
    config.repetition_max = std::chrono::milliseconds(60000);
    config.cyclic_offer = std::chrono::milliseconds(60000);
    return config;
}

#if defined(_WIN32)
bool ipv4_is_multicast(const char* address)
{
    in_addr parsed{};
    if (someip_inet_pton(AF_INET, address, &parsed) != 1) {
        return false;
    }
    const uint32_t host_order = ntohl(parsed.s_addr);
    return (host_order & 0xF0000000U) == 0xE0000000U;
}
#endif

bool open_peer(RawUdpPeer& peer, const char* address, uint16_t port)
{
#if defined(_WIN32)
    // Winsock rejects bind() on a multicast group (WSAEADDRNOTAVAIL / 10049).
    // Bind the wildcard address and select the group with IP_ADD_MEMBERSHIP.
    const char* bind_address = ipv4_is_multicast(address) ? "0.0.0.0" : address;
#else
    const char* bind_address = address;
#endif
    if (!peer.open() || !peer.enable_reuse() || !peer.bind_to(bind_address, port)) {
        return false;
    }
    return true;
}

bool prepare_endpoints(SdEndpoints& endpoints, const char* multicast_address,
                       uint32_t initial_delay_ms, bool offer_now, uint16_t service_id,
                       uint16_t instance_id, uint32_t ttl_seconds)
{
    const bool library_default = multicast_address == nullptr ||
                                 std::strcmp(multicast_address, kLibraryDefaultMulticast) == 0;
    endpoints.offer_address = library_default ? kLibraryDefaultMulticast : multicast_address;
    const char* other_address = library_default ? kConfiguredMulticast : kLibraryDefaultMulticast;

    if (!open_peer(endpoints.offer_group, endpoints.offer_address, 0)) {
        ADD_FAILURE() << "offer-group socket failed for " << endpoints.offer_address << ", errno "
                      << someip_socket_errno();
        return false;
    }
    endpoints.port = endpoints.offer_group.port();
    if (!open_peer(endpoints.other_group, other_address, endpoints.port)) {
        ADD_FAILURE() << "other-group socket failed for " << other_address << ", errno "
                      << someip_socket_errno();
        return false;
    }
    if (!endpoints.offer_group.join_group(endpoints.offer_address) ||
        !endpoints.other_group.join_group(other_address)) {
        ADD_FAILURE() << "multicast join failed, errno " << someip_socket_errno();
        return false;
    }
    if (!endpoints.offer_group.tune() || !endpoints.other_group.tune()) {
        ADD_FAILURE() << "failed to tune peer sockets";
        return false;
    }

    endpoints.server = std::make_unique<SdServer>(make_sd_config(
        endpoints.port, initial_delay_ms, library_default ? nullptr : multicast_address));
    if (offer_now) {
        ServiceInstance instance(service_id, instance_id, 1, 0);
        instance.ttl_seconds = ttl_seconds;
        if (!endpoints.server->offer_service(instance, kServiceEndpoint)) {
            ADD_FAILURE() << "offer_service failed before initialize";
            return false;
        }
    }
    if (!endpoints.server->initialize()) {
        ADD_FAILURE() << "SdServer::initialize failed on port " << endpoints.port;
        return false;
    }
    if (endpoints.server->multicast_state() != MulticastState::JOINED) {
        ADD_FAILURE() << "SdServer did not join " << endpoints.offer_address;
        return false;
    }
    return true;
}

platform::ByteBuffer build_find(uint16_t service_id, uint16_t instance_id)
{
    ServiceEntry entry(EntryType::FIND_SERVICE);
    entry.set_service_id(service_id);
    entry.set_instance_id(instance_id);
    entry.set_major_version(1);
    entry.set_minor_version(0xFFFFFFFFU);
    entry.set_ttl(3);

    SdMessage sd_message;
    sd_message.set_reboot(true);
    sd_message.set_unicast(true);
    sd_message.add_entry(std::move(entry));

    Message message(MessageId(SOMEIP_SD_SERVICE_ID, SOMEIP_SD_METHOD_ID),
                    RequestId(SOMEIP_SD_CLIENT_ID, 1), MessageType::NOTIFICATION, ReturnCode::E_OK);
    message.set_interface_version(SOMEIP_SD_INTERFACE_VERSION);
    message.set_payload(sd_message.serialize());
    return message.serialize();
}

template <typename Predicate>
bool wait_until(RawUdpPeer& peer, std::chrono::milliseconds budget, WireSd& out,
                Predicate predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + budget;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0) {
            break;
        }
        if (!peer.wait_readable(left)) {
            return false;
        }
        Datagram datagram;
        if (!peer.try_recv(datagram)) {
            continue;
        }
        WireSd parsed = parse_datagram(datagram);
        if (predicate(parsed)) {
            out = std::move(parsed);
            return true;
        }
    }
    return false;
}

struct ReadyPeers {
    bool unicast{false};
    bool configured{false};
    bool default_group{false};
};

ReadyPeers wait_peers(RawUdpPeer& unicast, RawUdpPeer& configured, RawUdpPeer& default_group,
                      std::chrono::milliseconds timeout)
{
    ReadyPeers ready;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return ready;
        }
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(unicast.fd(), &readable);
        FD_SET(configured.fd(), &readable);
        FD_SET(default_group.fd(), &readable);
        timeval wait{};
        wait.tv_sec = static_cast<decltype(wait.tv_sec)>(left.count() / 1000);
        wait.tv_usec = static_cast<decltype(wait.tv_usec)>((left.count() % 1000) * 1000);
#if defined(_WIN32)
        const int nfds = 0;
#else
        int nfds = unicast.fd();
        if (configured.fd() > nfds) {
            nfds = configured.fd();
        }
        if (default_group.fd() > nfds) {
            nfds = default_group.fd();
        }
        ++nfds;
#endif
        const int result = someip_select(nfds, &readable, nullptr, nullptr, &wait);
        if (result < 0 && someip_socket_errno() == SOMEIP_EINTR) {
            continue;
        }
        if (result <= 0) {
            return ready;
        }
        ready.unicast = FD_ISSET(unicast.fd(), &readable) != 0;
        ready.configured = FD_ISSET(configured.fd(), &readable) != 0;
        ready.default_group = FD_ISSET(default_group.fd(), &readable) != 0;
        return ready;
    }
}

void note_multicast_offer(RawUdpPeer& peer, uint16_t service_id, uint16_t instance_id,
                          bool any_datagram, bool& bad_multicast)
{
    Datagram datagram;
    while (peer.try_recv(datagram)) {
        if (any_datagram) {
            bad_multicast = true;
            continue;
        }
        if (is_live_offer(parse_datagram(datagram), service_id, instance_id)) {
            bad_multicast = true;
        }
    }
}

// Address the kernel would use to reach a non-local host. That is not
// SdConfig::unicast_address, so a reply to the config constant misses the finder.
std::string choose_finder_address()
{
    RawUdpPeer probe;
    if (!probe.open()) {
        return {};
    }
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(9);
    if (someip_inet_pton(AF_INET, "192.0.2.1", &remote.sin_addr) != 1) {
        return {};
    }
    if (someip_connect(probe.fd(), reinterpret_cast<sockaddr*>(&remote), sizeof(remote)) < 0) {
        return {};
    }
    sockaddr_in local{};
    socklen_t length = sizeof(local);
    if (someip_getsockname(probe.fd(), reinterpret_cast<sockaddr*>(&local), &length) < 0) {
        return {};
    }
    char text[INET_ADDRSTRLEN] = {};
    if (someip_inet_ntop(AF_INET, &local.sin_addr, text, static_cast<socklen_t>(sizeof(text))) ==
        nullptr) {
        return {};
    }
    if (std::strcmp(text, "0.0.0.0") == 0 || std::strcmp(text, kLoopback) == 0) {
        return {};
    }
    return text;
}

TEST(SdOfferFindSession, LibraryDefaultMulticast)
{
    const SdConfig config;
    EXPECT_EQ(config.multicast_address, kLibraryDefaultMulticast);
    EXPECT_EQ(config.multicast_port, 30490u);
    EXPECT_STRNE(kConfiguredMulticast, kLibraryDefaultMulticast);
}

TEST(SdOfferFindSession, UnsetMulticastSendsOfferToLibraryDefault)
{
    SdEndpoints endpoints;
    ASSERT_TRUE(prepare_endpoints(endpoints, nullptr, 0, true, kOfferServiceId, kOfferInstanceId,
                                  kOfferTtlSeconds));

    WireSd offer;
    ASSERT_TRUE(wait_until(endpoints.offer_group, std::chrono::milliseconds(2000), offer,
                           [](const WireSd& message) {
                               return is_live_offer(message, kOfferServiceId, kOfferInstanceId);
                           }))
        << "OfferService with the multicast address left unset did not arrive on "
        << kLibraryDefaultMulticast;
    EXPECT_EQ(offer.ttl, kOfferTtlSeconds);
    EXPECT_EQ(offer.service_id, kOfferServiceId);
    expect_bound_destination(offer, kLibraryDefaultMulticast);
    expect_quiet(endpoints.other_group, std::chrono::milliseconds(100));
}

TEST(SdOfferFindSession, OfferAndStopUseConfiguredMulticast)
{
    SdEndpoints endpoints;
    ASSERT_TRUE(prepare_endpoints(endpoints, kConfiguredMulticast, 0, true, kOfferServiceId,
                                  kOfferInstanceId, kOfferTtlSeconds));

    WireSd offer;
    ASSERT_TRUE(wait_until(endpoints.offer_group, std::chrono::milliseconds(2000), offer,
                           [](const WireSd& message) {
                               return is_live_offer(message, kOfferServiceId, kOfferInstanceId);
                           }))
        << "OfferService did not arrive on " << kConfiguredMulticast;
    EXPECT_EQ(offer.service_id, kOfferServiceId);
    EXPECT_EQ(offer.instance_id, kOfferInstanceId);
    EXPECT_EQ(offer.ttl, kOfferTtlSeconds);
    EXPECT_EQ(offer.entry_type, EntryType::OFFER_SERVICE);
    expect_bound_destination(offer, kConfiguredMulticast);
    expect_quiet(endpoints.offer_group, std::chrono::milliseconds(50));
    expect_quiet(endpoints.other_group, std::chrono::milliseconds(100));

    ASSERT_TRUE(endpoints.server->stop_offer_service(kOfferServiceId, kOfferInstanceId));

    WireSd stop_offer;
    ASSERT_TRUE(wait_until(endpoints.offer_group, std::chrono::milliseconds(2000), stop_offer,
                           [](const WireSd& message) {
                               return is_stop_offer(message, kOfferServiceId, kOfferInstanceId);
                           }))
        << "StopOffer did not arrive on " << kConfiguredMulticast;
    EXPECT_EQ(stop_offer.ttl, 0u);
    EXPECT_EQ(stop_offer.service_id, kOfferServiceId);
    EXPECT_EQ(stop_offer.instance_id, kOfferInstanceId);
    EXPECT_TRUE(stop_offer.sd_ok);
    expect_bound_destination(stop_offer, kConfiguredMulticast);
    expect_quiet(endpoints.other_group, std::chrono::milliseconds(100));
}

TEST(SdOfferFindSession, FindServiceAnswerIsUnicastOffer)
{
    SdEndpoints endpoints;
    ASSERT_TRUE(prepare_endpoints(endpoints, kConfiguredMulticast, 0, true, kOfferServiceId,
                                  kOfferInstanceId, kOfferTtlSeconds));

    WireSd initial_offer;
    ASSERT_TRUE(wait_until(endpoints.offer_group, std::chrono::milliseconds(2000), initial_offer,
                           [](const WireSd& message) {
                               return is_live_offer(message, kOfferServiceId, kOfferInstanceId);
                           }))
        << "initial OfferService missing before Find";
    expect_quiet(endpoints.other_group, std::chrono::milliseconds(50));

#if defined(_WIN32)
    // UDP sent to this host's LAN address is not delivered back to a local
    // socket. The server listens on 0.0.0.0, so loopback still reaches it
    // and the Offer returns to that source instead of the multicast group.
    const std::string finder_address = kLoopback;
#else
    const std::string finder_address = choose_finder_address();
    ASSERT_FALSE(finder_address.empty())
        << "need a local address other than " << kLoopback << " to lock the unicast reply";
#endif
    RawUdpPeer finder;
    ASSERT_TRUE(finder.open());
    ASSERT_TRUE(finder.bind_to(finder_address.c_str(), 0))
        << "finder bind " << finder_address << " errno " << someip_socket_errno();
    ASSERT_TRUE(finder.tune());

    const platform::ByteBuffer find_bytes = build_find(kOfferServiceId, kOfferInstanceId);
    ASSERT_FALSE(find_bytes.empty());
    // Address the Find at the finder address. The server socket is 0.0.0.0, so it
    // receives that datagram. On Linux a socket bound to a non-loopback address
    // cannot send to 127.0.0.1, so the finder uses the LAN address there. The
    // Offer has to come back to this source.
    ASSERT_TRUE(finder.send_to(finder_address.c_str(), endpoints.port, find_bytes.data(),
                               find_bytes.size()))
        << "Find send to " << finder_address;

    bool bad_multicast = false;
    bool got_unicast = false;
    int finder_datagrams = 0;
    WireSd unicast_offer;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (!got_unicast && std::chrono::steady_clock::now() < deadline) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0) {
            break;
        }
        const ReadyPeers ready =
            wait_peers(finder, endpoints.offer_group, endpoints.other_group, left);
        if (!ready.unicast && !ready.configured && !ready.default_group) {
            break;
        }
        if (ready.configured) {
            note_multicast_offer(endpoints.offer_group, kOfferServiceId, kOfferInstanceId, false,
                                 bad_multicast);
        }
        if (ready.default_group) {
            note_multicast_offer(endpoints.other_group, kOfferServiceId, kOfferInstanceId, true,
                                 bad_multicast);
        }
        if (ready.unicast) {
            Datagram datagram;
            while (finder.try_recv(datagram)) {
                ++finder_datagrams;
                const WireSd parsed = parse_datagram(datagram);
                if (is_live_offer(parsed, kOfferServiceId, kOfferInstanceId)) {
                    unicast_offer = parsed;
                    got_unicast = true;
                }
            }
        }
    }

    const auto quiet_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() < quiet_deadline) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            quiet_deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0) {
            break;
        }
        if (endpoints.offer_group.wait_readable(left)) {
            note_multicast_offer(endpoints.offer_group, kOfferServiceId, kOfferInstanceId, false,
                                 bad_multicast);
        }
        if (endpoints.other_group.wait_readable(std::chrono::milliseconds(1))) {
            note_multicast_offer(endpoints.other_group, kOfferServiceId, kOfferInstanceId, true,
                                 bad_multicast);
        }
    }

    EXPECT_FALSE(bad_multicast) << "FindService was answered with another multicast Offer";
    ASSERT_TRUE(got_unicast) << "FindService was not answered with a unicast Offer to "
                             << finder_address << " datagrams=" << finder_datagrams;
    EXPECT_EQ(unicast_offer.service_id, kOfferServiceId);
    EXPECT_EQ(unicast_offer.instance_id, kOfferInstanceId);
    EXPECT_EQ(unicast_offer.ttl, kOfferTtlSeconds);
    EXPECT_EQ(unicast_offer.entry_type, EntryType::OFFER_SERVICE);
    EXPECT_TRUE(unicast_offer.sd_ok);
    if (!unicast_offer.destination.empty()) {
        EXPECT_EQ(unicast_offer.destination, finder_address);
    }
}

TEST(SdOfferFindSession, SessionWrapNextMessageParsesWithRebootClear)
{
    SdEndpoints endpoints;
    // Keep the offer timer from emitting its own Offer while StopOffer walks
    // the multicast session counter. OfferService and StopOffer share it.
    ASSERT_TRUE(prepare_endpoints(endpoints, kConfiguredMulticast, 120000, false, kWrapServiceId,
                                  kWrapInstanceId, 0));

    std::vector<uint8_t> limit_bytes;
    std::vector<uint8_t> next_bytes;
    std::atomic<bool> stop{false};
    std::atomic<uint32_t> received{0};
    std::atomic<uint16_t> last_session{0};
    std::atomic<bool> saw_limit{false};
    std::atomic<bool> saw_next{false};

    std::thread worker([&]() {
        uint16_t previous = 0;
        bool have_previous = false;
        bool stored_limit = false;
        bool stored_next = false;
        auto consume = [&](const Datagram& datagram) {
            received.fetch_add(1U, std::memory_order_relaxed);
            uint16_t session = 0;
            const bool header = session_from_header(datagram.bytes.data(), datagram.size, session);
            if (header) {
                last_session.store(session, std::memory_order_relaxed);
            }
            if (have_previous && previous == static_cast<uint16_t>(0xFFFF) && !stored_next) {
                next_bytes.assign(datagram.bytes.data(), datagram.bytes.data() + datagram.size);
                stored_next = true;
                saw_next.store(true, std::memory_order_release);
            }
            if (header && session == static_cast<uint16_t>(0xFFFF) && !stored_limit) {
                limit_bytes.assign(datagram.bytes.data(), datagram.bytes.data() + datagram.size);
                stored_limit = true;
                saw_limit.store(true, std::memory_order_release);
            }
            if (header) {
                previous = session;
                have_previous = true;
            }
        };

        while (!stop.load(std::memory_order_acquire)) {
            Datagram datagram;
            bool any = false;
            while (endpoints.offer_group.try_recv(datagram)) {
                any = true;
                consume(datagram);
            }
            if (!any) {
                (void)endpoints.offer_group.wait_readable(std::chrono::milliseconds(20));
            }
        }
        Datagram datagram;
        while (endpoints.offer_group.try_recv(datagram)) {
            consume(datagram);
        }
    });
    [[maybe_unused]] JoinThread join{stop, worker};

    uint32_t sent = 0;
    for (uint32_t index = 0; index < kSessionWrapSends; ++index) {
        ServiceInstance instance(kWrapServiceId, kWrapInstanceId, 1, 1);
        instance.ttl_seconds = 3;
        if (!endpoints.server->offer_service(instance, kServiceEndpoint)) {
            ADD_FAILURE() << "offer_service failed after " << sent << " SD messages";
            return;
        }
        if (!endpoints.server->stop_offer_service(kWrapServiceId, kWrapInstanceId)) {
            ADD_FAILURE() << "stop_offer_service failed after " << sent << " SD messages";
            return;
        }
        ++sent;
        if ((sent % 256U) == 0U) {
            const auto pace_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            while (received.load(std::memory_order_relaxed) + 16U < sent &&
                   std::chrono::steady_clock::now() < pace_deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    // Loopback delivery of the last StopOffer can land after sendto returns.
    // Leave the receiver running until that datagram is counted.
    const auto drain_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (received.load(std::memory_order_acquire) < sent &&
           std::chrono::steady_clock::now() < drain_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    stop.store(true, std::memory_order_release);
    if (worker.joinable()) {
        worker.join();
    }
    if (saw_limit.load(std::memory_order_acquire) && !saw_next.load(std::memory_order_acquire)) {
        Datagram leftover;
        while (endpoints.offer_group.try_recv(leftover)) {
            received.fetch_add(1U, std::memory_order_relaxed);
            uint16_t session = 0;
            if (session_from_header(leftover.bytes.data(), leftover.size, session)) {
                last_session.store(session, std::memory_order_relaxed);
            }
            next_bytes.assign(leftover.bytes.data(), leftover.bytes.data() + leftover.size);
            saw_next.store(true, std::memory_order_release);
            break;
        }
    }

    ASSERT_TRUE(saw_limit.load(std::memory_order_acquire))
        << "never saw session 0xFFFF; received=" << received.load()
        << " last_session=" << last_session.load();
    const WireSd limit = parse_bytes(limit_bytes);
    ASSERT_TRUE(limit.header_ok);
    ASSERT_TRUE(limit.sd_ok) << "session 0xFFFF SD message did not parse";
    EXPECT_EQ(limit.session_id, static_cast<uint16_t>(0xFFFF));
    EXPECT_TRUE(limit.reboot);
    EXPECT_TRUE(is_stop_offer(limit, kWrapServiceId, kWrapInstanceId));

    ASSERT_TRUE(saw_next.load(std::memory_order_acquire))
        << "no SD message followed session 0xFFFF; received=" << received.load()
        << " last_session=" << last_session.load();
    const WireSd next = parse_bytes(next_bytes);
    ASSERT_TRUE(next.header_ok);
    ASSERT_TRUE(next.sd_ok) << "SD message after session wrap did not parse";
    EXPECT_EQ(next.session_id, static_cast<uint16_t>(1));
    EXPECT_FALSE(next.reboot);
    EXPECT_TRUE(is_stop_offer(next, kWrapServiceId, kWrapInstanceId));
    EXPECT_GE(received.load(), kSessionWrapSends);
}

}  // namespace
