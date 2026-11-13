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
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "common/result.h"
#include "events/event_publisher.h"
#include "events/event_types.h"
#include "platform/net.h"
#include "someip/message.h"
#include "someip/types.h"
#include "static_pool_init.h"
#include "transport/endpoint.h"
#include "transport/udp_transport.h"

/**
 * @brief Datagram counts for eventgroup multicast threshold.
 *
 * Subscribers are recorded with handle_subscription, which is the server side
 * of SubscribeEventgroup. Each expectation below is a recvfrom count on a raw
 * UDP socket, not a callback count.
 */

using namespace someip;

namespace {

constexpr uint16_t TEST_SERVICE_ID = 0x1234;
constexpr uint16_t TEST_INSTANCE_ID = 0x0001;
constexpr uint16_t TEST_EVENTGROUP_ID = 0x0001;
constexpr uint16_t TEST_EVENT_ID = 0x8001;
constexpr uint16_t TEST_CLIENT_A = 0x0100;
constexpr uint16_t TEST_CLIENT_B = 0x0200;
constexpr uint16_t TEST_CLIENT_C = 0x0300;
constexpr uint32_t TEST_TTL_INFINITE = 0xFFFFFFu;
constexpr char TEST_INTERFACE[] = "127.0.0.1";
constexpr char TEST_GROUP_ADDRESS[] = "239.127.13.81";
constexpr std::chrono::milliseconds QUIET_WINDOW{200};
constexpr std::chrono::milliseconds COLLECT_LIMIT{1000};

transport::UdpTransportConfig loopback_udp_config()
{
    transport::UdpTransportConfig config;
    config.blocking = false;
    config.reuse_address = true;
    config.enable_tp = false;
    config.multicast_interface = TEST_INTERFACE;
    config.multicast_ttl = 1;
    return config;
}

class BoundDatagram {
   public:
    BoundDatagram() = default;
    ~BoundDatagram()
    {
        reset();
    }

    BoundDatagram(const BoundDatagram&) = delete;
    BoundDatagram& operator=(const BoundDatagram&) = delete;

    bool bind_unicast(const char* address)
    {
        return open_bound(address, nullptr, nullptr);
    }

    bool bind_multicast(const char* group, const char* iface)
    {
        return open_bound("0.0.0.0", group, iface);
    }

    uint16_t port() const
    {
        return port_;
    }
    someip_socket_t fd() const
    {
        return fd_;
    }
    int last_errno() const
    {
        return last_errno_;
    }

    void reset()
    {
        if (fd_ != SOMEIP_INVALID_SOCKET) {
            someip_close_socket(fd_);
            fd_ = SOMEIP_INVALID_SOCKET;
        }
    }

   private:
    bool open_bound(const char* bind_address, const char* group, const char* iface)
    {
        reset();
        fd_ = someip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd_ == SOMEIP_INVALID_SOCKET) {
            last_errno_ = someip_socket_errno();
            return false;
        }

        int const reuse = 1;
        if (someip_setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            last_errno_ = someip_socket_errno();
            reset();
            return false;
        }
#ifdef SO_REUSEPORT
        if (group != nullptr) {
            // Sharing the group port is optional for a single receiver.
            static_cast<void>(
                someip_setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)));
        }
#endif

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(0);
        addr.sin_addr.s_addr = someip_inet_addr(bind_address);
        if (someip_bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            last_errno_ = someip_socket_errno();
            reset();
            return false;
        }

        socklen_t addr_len = sizeof(addr);
        if (someip_getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) < 0) {
            last_errno_ = someip_socket_errno();
            reset();
            return false;
        }
        port_ = ntohs(addr.sin_port);
        if (port_ == 0) {
            reset();
            return false;
        }

        if (group != nullptr) {
            ip_mreq membership{};
            membership.imr_multiaddr.s_addr = someip_inet_addr(group);
            membership.imr_interface.s_addr = someip_inet_addr(iface);
            if (someip_setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership,
                                  sizeof(membership)) < 0) {
                last_errno_ = someip_socket_errno();
                reset();
                return false;
            }
        }

        static_cast<void>(someip_enable_recv_dest(fd_));
        if (someip_set_nonblocking(fd_) < 0) {
            last_errno_ = someip_socket_errno();
            reset();
            return false;
        }
        return true;
    }

    someip_socket_t fd_{SOMEIP_INVALID_SOCKET};
    uint16_t port_{0};
    int last_errno_{0};
};

struct SocketWatch {
    someip_socket_t fd{SOMEIP_INVALID_SOCKET};
    std::size_t expected{0};
    std::vector<std::vector<uint8_t>> datagrams;
    std::vector<std::string> destinations;
};

void collect_datagrams(std::vector<SocketWatch>& watches)
{
    for (auto& watch : watches) {
        watch.datagrams.clear();
        watch.destinations.clear();
    }

    const auto start = std::chrono::steady_clock::now();
    const auto limit = start + COLLECT_LIMIT;
    auto quiet_until = start + QUIET_WINDOW;

    while (std::chrono::steady_clock::now() < limit) {
        bool all_met = true;
        fd_set read_fds;
        FD_ZERO(&read_fds);
        int max_fd = 0;
        for (const auto& watch : watches) {
            FD_SET(watch.fd, &read_fds);
#ifndef _WIN32
            if (watch.fd > max_fd) {
                max_fd = watch.fd;
            }
#endif
            if (watch.datagrams.size() < watch.expected) {
                all_met = false;
            }
        }
        if (all_met && std::chrono::steady_clock::now() >= quiet_until) {
            break;
        }

        timeval wait{};
        wait.tv_sec = 0;
        wait.tv_usec = 50 * 1000;
#ifdef _WIN32
        const int nfds = 0;
#else
        const int nfds = max_fd + 1;
#endif
        const int ready = someip_select(nfds, &read_fds, nullptr, nullptr, &wait);
        if (ready < 0) {
            if (someip_socket_errno() == SOMEIP_EINTR) {
                continue;
            }
            break;
        }
        if (ready == 0) {
            continue;
        }

        bool received = false;
        for (auto& watch : watches) {
            if (!FD_ISSET(watch.fd, &read_fds)) {
                continue;
            }
            for (;;) {
                std::array<uint8_t, 2048> buffer{};
                std::array<char, INET_ADDRSTRLEN> destination{};
                sockaddr_in source{};
                socklen_t source_len = sizeof(source);
                const ssize_t bytes = someip_recvfrom_with_dest(
                    watch.fd, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&source),
                    &source_len, destination.data(), destination.size());
                if (bytes < 0) {
                    break;
                }
                watch.datagrams.emplace_back(buffer.begin(),
                                             buffer.begin() + static_cast<std::size_t>(bytes));
                watch.destinations.emplace_back(destination.data());
                received = true;
            }
        }
        if (received) {
            quiet_until = std::chrono::steady_clock::now() + QUIET_WINDOW;
        }
    }
}

::testing::AssertionResult datagrams_match(const SocketWatch& watch, std::size_t expected,
                                           uint8_t payload, const char* destination)
{
    if (watch.datagrams.size() != expected) {
        return ::testing::AssertionFailure()
               << "recvfrom count " << watch.datagrams.size() << ", expected " << expected;
    }
    for (std::size_t index = 0; index < watch.datagrams.size(); ++index) {
        const std::vector<uint8_t>& bytes = watch.datagrams[index];
        Message message;
        if (message.try_deserialize(bytes.data(), bytes.size()) != Result::SUCCESS) {
            return ::testing::AssertionFailure() << "datagram " << index << " is not SOME/IP";
        }
        if (message.get_service_id() != TEST_SERVICE_ID ||
            message.get_method_id() != TEST_EVENT_ID ||
            message.get_message_type() != MessageType::NOTIFICATION) {
            return ::testing::AssertionFailure() << "datagram " << index << " has the wrong header";
        }
        const platform::ByteBuffer& body = message.get_payload();
        if (body.size() != 1 || body[0] != payload) {
            return ::testing::AssertionFailure()
                   << "datagram " << index << " has the wrong payload";
        }
        if (destination != nullptr && !watch.destinations[index].empty() &&
            watch.destinations[index] != destination) {
            return ::testing::AssertionFailure()
                   << "datagram " << index << " arrived for " << watch.destinations[index]
                   << ", expected " << destination;
        }
    }
    return ::testing::AssertionSuccess();
}

struct PublisherHarness {
    transport::UdpTransport udp;
    events::EventPublisher publisher;

    PublisherHarness()
        : udp(transport::Endpoint(TEST_INTERFACE, 0), loopback_udp_config()),
          publisher(TEST_SERVICE_ID, TEST_INSTANCE_ID, udp)
    {
    }
};

bool register_plain_event(events::EventPublisher& publisher, bool is_field)
{
    events::EventConfig config;
    config.event_id = TEST_EVENT_ID;
    config.eventgroup_id = TEST_EVENTGROUP_ID;
    config.notification_type = events::NotificationType::ON_CHANGE;
    config.is_field = is_field;
    return publisher.register_event(config);
}

bool configure_group(events::EventPublisher& publisher, uint16_t threshold, uint16_t group_port)
{
    events::EventgroupConfig config;
    config.eventgroup_id = TEST_EVENTGROUP_ID;
    config.multicast_threshold = threshold;
    config.multicast_address = TEST_GROUP_ADDRESS;
    config.multicast_port = group_port;
    return publisher.configure_eventgroup(config);
}

bool subscribe_client(events::EventPublisher& publisher, uint16_t client_id, uint16_t port)
{
    publisher.set_default_client_endpoint(TEST_INTERFACE, port);
    return publisher.handle_subscription(TEST_EVENTGROUP_ID, client_id, TEST_TTL_INFINITE);
}

platform::ByteBuffer payload_byte(uint8_t value)
{
    platform::ByteBuffer payload;
    payload.push_back(value);
    return payload;
}

}  // namespace

/**
 * @test_case TC_EVT_MCAST_005
 * @tests REQ_TRANSPORT_012
 * @brief A non-zero threshold requires a caller-supplied IPv4 multicast endpoint.
 */
TEST(EventgroupMulticastConfig, RejectsThresholdWithoutMulticastEndpoint)
{
    events::EventPublisher publisher(TEST_SERVICE_ID, TEST_INSTANCE_ID);

    events::EventgroupConfig missing;
    missing.eventgroup_id = TEST_EVENTGROUP_ID;
    missing.multicast_threshold = 1;
    EXPECT_FALSE(publisher.configure_eventgroup(missing));

    events::EventgroupConfig unicast;
    unicast.eventgroup_id = TEST_EVENTGROUP_ID;
    unicast.multicast_threshold = 2;
    unicast.multicast_address = TEST_INTERFACE;
    unicast.multicast_port = 40000;
    EXPECT_FALSE(publisher.configure_eventgroup(unicast));

    events::EventgroupConfig no_port;
    no_port.eventgroup_id = TEST_EVENTGROUP_ID;
    no_port.multicast_threshold = 1;
    no_port.multicast_address = TEST_GROUP_ADDRESS;
    no_port.multicast_port = 0;
    EXPECT_FALSE(publisher.configure_eventgroup(no_port));

    events::EventgroupConfig unicast_only;
    unicast_only.eventgroup_id = TEST_EVENTGROUP_ID;
    unicast_only.multicast_threshold = 0;
    EXPECT_TRUE(publisher.configure_eventgroup(unicast_only));
}

/**
 * @test_case TC_EVT_MCAST_001
 * @tests REQ_TRANSPORT_012
 * @satisfies feat_req_someip_813
 * @brief Threshold 1 is one datagram on the configured group and no unicast copy.
 */
TEST(EventgroupMulticastWire, ThresholdOneIsOneGroupDatagram)
{
    BoundDatagram client_a;
    BoundDatagram client_b;
    BoundDatagram group;
    ASSERT_TRUE(client_a.bind_unicast(TEST_INTERFACE)) << client_a.last_errno();
    ASSERT_TRUE(client_b.bind_unicast(TEST_INTERFACE)) << client_b.last_errno();
    ASSERT_TRUE(group.bind_multicast(TEST_GROUP_ADDRESS, TEST_INTERFACE)) << group.last_errno();

    PublisherHarness harness;
    ASSERT_TRUE(harness.publisher.initialize());
    ASSERT_TRUE(register_plain_event(harness.publisher, false));
    ASSERT_TRUE(configure_group(harness.publisher, 1, group.port()));
    ASSERT_TRUE(subscribe_client(harness.publisher, TEST_CLIENT_A, client_a.port()));

    constexpr uint8_t kOneSubscriber = 0x11;
    ASSERT_TRUE(harness.publisher.publish_event(TEST_EVENT_ID, payload_byte(kOneSubscriber)));

    std::vector<SocketWatch> one_subscriber{
        {client_a.fd(), 0, {}, {}},
        {client_b.fd(), 0, {}, {}},
        {group.fd(), 1, {}, {}},
    };
    collect_datagrams(one_subscriber);
    EXPECT_TRUE(datagrams_match(one_subscriber[0], 0, kOneSubscriber, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(one_subscriber[1], 0, kOneSubscriber, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(one_subscriber[2], 1, kOneSubscriber, TEST_GROUP_ADDRESS));

    ASSERT_TRUE(subscribe_client(harness.publisher, TEST_CLIENT_B, client_b.port()));
    constexpr uint8_t kTwoSubscribers = 0x12;
    ASSERT_TRUE(harness.publisher.publish_event(TEST_EVENT_ID, payload_byte(kTwoSubscribers)));

    std::vector<SocketWatch> two_subscribers{
        {client_a.fd(), 0, {}, {}},
        {client_b.fd(), 0, {}, {}},
        {group.fd(), 1, {}, {}},
    };
    collect_datagrams(two_subscribers);
    EXPECT_TRUE(datagrams_match(two_subscribers[0], 0, kTwoSubscribers, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(two_subscribers[1], 0, kTwoSubscribers, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(two_subscribers[2], 1, kTwoSubscribers, TEST_GROUP_ADDRESS));

    harness.publisher.shutdown();
}

/**
 * @test_case TC_EVT_MCAST_002
 * @tests REQ_TRANSPORT_012
 * @satisfies feat_req_someip_813
 * @brief Threshold 0 unicasts once per subscriber and does not touch the group.
 */
TEST(EventgroupMulticastWire, ThresholdZeroUnicastsOncePerSubscriber)
{
    BoundDatagram client_a;
    BoundDatagram client_b;
    BoundDatagram group;
    ASSERT_TRUE(client_a.bind_unicast(TEST_INTERFACE)) << client_a.last_errno();
    ASSERT_TRUE(client_b.bind_unicast(TEST_INTERFACE)) << client_b.last_errno();
    ASSERT_TRUE(group.bind_multicast(TEST_GROUP_ADDRESS, TEST_INTERFACE)) << group.last_errno();

    PublisherHarness harness;
    ASSERT_TRUE(harness.publisher.initialize());
    ASSERT_TRUE(register_plain_event(harness.publisher, false));
    ASSERT_TRUE(configure_group(harness.publisher, 0, group.port()));
    ASSERT_TRUE(subscribe_client(harness.publisher, TEST_CLIENT_A, client_a.port()));

    constexpr uint8_t kOne = 0x21;
    ASSERT_TRUE(harness.publisher.publish_event(TEST_EVENT_ID, payload_byte(kOne)));

    std::vector<SocketWatch> one_subscriber{
        {client_a.fd(), 1, {}, {}},
        {client_b.fd(), 0, {}, {}},
        {group.fd(), 0, {}, {}},
    };
    collect_datagrams(one_subscriber);
    EXPECT_TRUE(datagrams_match(one_subscriber[0], 1, kOne, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(one_subscriber[1], 0, kOne, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(one_subscriber[2], 0, kOne, TEST_GROUP_ADDRESS));

    ASSERT_TRUE(subscribe_client(harness.publisher, TEST_CLIENT_B, client_b.port()));
    constexpr uint8_t kTwo = 0x22;
    ASSERT_TRUE(harness.publisher.publish_event(TEST_EVENT_ID, payload_byte(kTwo)));

    std::vector<SocketWatch> two_subscribers{
        {client_a.fd(), 1, {}, {}},
        {client_b.fd(), 1, {}, {}},
        {group.fd(), 0, {}, {}},
    };
    collect_datagrams(two_subscribers);
    EXPECT_TRUE(datagrams_match(two_subscribers[0], 1, kTwo, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(two_subscribers[1], 1, kTwo, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(two_subscribers[2], 0, kTwo, TEST_GROUP_ADDRESS));

    harness.publisher.shutdown();
}

/**
 * @test_case TC_EVT_MCAST_004
 * @tests REQ_TRANSPORT_012
 * @satisfies feat_req_someip_813
 * @brief Threshold 2 unicasts below the count and multicasts once the count is reached.
 */
TEST(EventgroupMulticastWire, ThresholdAboveOneSwitchesWithSubscriberCount)
{
    BoundDatagram client_a;
    BoundDatagram client_b;
    BoundDatagram group;
    ASSERT_TRUE(client_a.bind_unicast(TEST_INTERFACE)) << client_a.last_errno();
    ASSERT_TRUE(client_b.bind_unicast(TEST_INTERFACE)) << client_b.last_errno();
    ASSERT_TRUE(group.bind_multicast(TEST_GROUP_ADDRESS, TEST_INTERFACE)) << group.last_errno();

    PublisherHarness harness;
    ASSERT_TRUE(harness.publisher.initialize());
    ASSERT_TRUE(register_plain_event(harness.publisher, false));
    ASSERT_TRUE(configure_group(harness.publisher, 2, group.port()));
    ASSERT_TRUE(subscribe_client(harness.publisher, TEST_CLIENT_A, client_a.port()));

    constexpr uint8_t kBelow = 0x33;
    ASSERT_TRUE(harness.publisher.publish_event(TEST_EVENT_ID, payload_byte(kBelow)));

    std::vector<SocketWatch> below{
        {client_a.fd(), 1, {}, {}},
        {client_b.fd(), 0, {}, {}},
        {group.fd(), 0, {}, {}},
    };
    collect_datagrams(below);
    EXPECT_TRUE(datagrams_match(below[0], 1, kBelow, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(below[1], 0, kBelow, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(below[2], 0, kBelow, TEST_GROUP_ADDRESS));

    ASSERT_TRUE(subscribe_client(harness.publisher, TEST_CLIENT_B, client_b.port()));
    constexpr uint8_t kAt = 0x34;
    ASSERT_TRUE(harness.publisher.publish_event(TEST_EVENT_ID, payload_byte(kAt)));

    std::vector<SocketWatch> at_threshold{
        {client_a.fd(), 0, {}, {}},
        {client_b.fd(), 0, {}, {}},
        {group.fd(), 1, {}, {}},
    };
    collect_datagrams(at_threshold);
    EXPECT_TRUE(datagrams_match(at_threshold[0], 0, kAt, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(at_threshold[1], 0, kAt, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(at_threshold[2], 1, kAt, TEST_GROUP_ADDRESS));

    harness.publisher.shutdown();
}

/**
 * @test_case TC_EVT_MCAST_006
 * @tests REQ_TRANSPORT_012
 * @satisfies feat_req_someip_813
 * @brief Threshold 3 stays on unicast at two subscribers and multicasts at three.
 */
TEST(EventgroupMulticastWire, ThresholdThreeUsesTheConfiguredCount)
{
    BoundDatagram client_a;
    BoundDatagram client_b;
    BoundDatagram client_c;
    BoundDatagram group;
    ASSERT_TRUE(client_a.bind_unicast(TEST_INTERFACE)) << client_a.last_errno();
    ASSERT_TRUE(client_b.bind_unicast(TEST_INTERFACE)) << client_b.last_errno();
    ASSERT_TRUE(client_c.bind_unicast(TEST_INTERFACE)) << client_c.last_errno();
    ASSERT_TRUE(group.bind_multicast(TEST_GROUP_ADDRESS, TEST_INTERFACE)) << group.last_errno();

    PublisherHarness harness;
    ASSERT_TRUE(harness.publisher.initialize());
    ASSERT_TRUE(register_plain_event(harness.publisher, false));
    ASSERT_TRUE(configure_group(harness.publisher, 3, group.port()));
    ASSERT_TRUE(subscribe_client(harness.publisher, TEST_CLIENT_A, client_a.port()));
    ASSERT_TRUE(subscribe_client(harness.publisher, TEST_CLIENT_B, client_b.port()));

    constexpr uint8_t kBelow = 0x35;
    ASSERT_TRUE(harness.publisher.publish_event(TEST_EVENT_ID, payload_byte(kBelow)));

    std::vector<SocketWatch> below{
        {client_a.fd(), 1, {}, {}},
        {client_b.fd(), 1, {}, {}},
        {client_c.fd(), 0, {}, {}},
        {group.fd(), 0, {}, {}},
    };
    collect_datagrams(below);
    EXPECT_TRUE(datagrams_match(below[0], 1, kBelow, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(below[1], 1, kBelow, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(below[2], 0, kBelow, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(below[3], 0, kBelow, TEST_GROUP_ADDRESS));

    ASSERT_TRUE(subscribe_client(harness.publisher, TEST_CLIENT_C, client_c.port()));
    constexpr uint8_t kAt = 0x36;
    ASSERT_TRUE(harness.publisher.publish_event(TEST_EVENT_ID, payload_byte(kAt)));

    std::vector<SocketWatch> at_threshold{
        {client_a.fd(), 0, {}, {}},
        {client_b.fd(), 0, {}, {}},
        {client_c.fd(), 0, {}, {}},
        {group.fd(), 1, {}, {}},
    };
    collect_datagrams(at_threshold);
    EXPECT_TRUE(datagrams_match(at_threshold[0], 0, kAt, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(at_threshold[1], 0, kAt, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(at_threshold[2], 0, kAt, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(at_threshold[3], 1, kAt, TEST_GROUP_ADDRESS));

    harness.publisher.shutdown();
}

/**
 * @test_case TC_EVT_MCAST_003
 * @tests REQ_TRANSPORT_012
 * @satisfies feat_req_someip_813
 * @brief An initial field event stays unicast when the group threshold is 1.
 */
TEST(EventgroupMulticastWire, InitialFieldEventStaysUnicast)
{
    BoundDatagram client_a;
    BoundDatagram group;
    ASSERT_TRUE(client_a.bind_unicast(TEST_INTERFACE)) << client_a.last_errno();
    ASSERT_TRUE(group.bind_multicast(TEST_GROUP_ADDRESS, TEST_INTERFACE)) << group.last_errno();

    PublisherHarness harness;
    ASSERT_TRUE(harness.publisher.initialize());
    ASSERT_TRUE(register_plain_event(harness.publisher, true));
    ASSERT_TRUE(configure_group(harness.publisher, 1, group.port()));

    constexpr uint8_t kInitial = 0x41;
    ASSERT_TRUE(harness.publisher.publish_field(TEST_EVENT_ID, payload_byte(kInitial)));

    std::vector<SocketWatch> before_subscribe{
        {client_a.fd(), 0, {}, {}},
        {group.fd(), 0, {}, {}},
    };
    collect_datagrams(before_subscribe);
    EXPECT_TRUE(datagrams_match(before_subscribe[0], 0, kInitial, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(before_subscribe[1], 0, kInitial, TEST_GROUP_ADDRESS));

    ASSERT_TRUE(subscribe_client(harness.publisher, TEST_CLIENT_A, client_a.port()));
    std::vector<SocketWatch> initial{
        {client_a.fd(), 1, {}, {}},
        {group.fd(), 0, {}, {}},
    };
    collect_datagrams(initial);
    EXPECT_TRUE(datagrams_match(initial[0], 1, kInitial, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(initial[1], 0, kInitial, TEST_GROUP_ADDRESS));

    constexpr uint8_t kUpdate = 0x42;
    ASSERT_TRUE(harness.publisher.publish_event(TEST_EVENT_ID, payload_byte(kUpdate)));
    std::vector<SocketWatch> update{
        {client_a.fd(), 0, {}, {}},
        {group.fd(), 1, {}, {}},
    };
    collect_datagrams(update);
    EXPECT_TRUE(datagrams_match(update[0], 0, kUpdate, TEST_INTERFACE));
    EXPECT_TRUE(datagrams_match(update[1], 1, kUpdate, TEST_GROUP_ADDRESS));

    harness.publisher.shutdown();
}
