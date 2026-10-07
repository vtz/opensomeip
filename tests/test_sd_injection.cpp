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
 * @file test_sd_injection.cpp
 * @brief Exclusive transport injection for service discovery.
 */

#include <atomic>
#include <mutex>
#include <vector>

#include <gtest/gtest.h>

#include "events/event_subscriber.h"
#include "sd/sd_client.h"
#include "sd/sd_server.h"
#include "sd/sd_types.h"
#include "someip/message.h"
#include "static_pool_init.h"
#include "transport/multicast_transport.h"
#include "transport/transport.h"

namespace {

using namespace someip;
using transport::Endpoint;
using transport::IMulticastTransport;
using transport::ITransport;
using transport::ITransportListener;

class MulticastFake final : public ITransport, public IMulticastTransport {
   public:
    Result send_message(const Message& message, const Endpoint& endpoint) override
    {
        if (!running_) {
            return Result::NOT_CONNECTED;
        }
        if (send_result != Result::SUCCESS) {
            return send_result;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        messages.push_back(message);
        destinations.push_back(endpoint);
        return Result::SUCCESS;
    }

    MessagePtr receive_message() override { return nullptr; }
    Result connect(const Endpoint&) override { return Result::NOT_IMPLEMENTED; }
    Result disconnect() override { return Result::NOT_IMPLEMENTED; }
    bool is_connected() const override { return running_; }
    Endpoint get_local_endpoint() const override { return Endpoint("192.0.2.20", 30490); }

    void set_listener(ITransportListener* listener) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        listener_ = listener;
    }

    Result start() override
    {
        ++starts;
        running_ = true;
        return Result::SUCCESS;
    }

    Result stop() override
    {
        ++stops;
        running_ = false;
        return Result::SUCCESS;
    }

    bool is_running() const override { return running_; }
    IMulticastTransport* multicast_transport() noexcept override { return this; }

    Result join_multicast_group(const platform::String<>& group) override
    {
        joined.push_back(group);
        return join_result;
    }

    Result leave_multicast_group(const platform::String<>&) override { return Result::SUCCESS; }

    ITransportListener* listener()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return listener_;
    }

    Result send_result{Result::SUCCESS};
    Result join_result{Result::SUCCESS};
    std::atomic<unsigned> starts{0};
    std::atomic<unsigned> stops{0};
    std::vector<Message> messages;
    std::vector<Endpoint> destinations;
    std::vector<platform::String<>> joined;

   private:
    bool running_{false};
    std::mutex mutex_;
    ITransportListener* listener_{nullptr};
};

class PlainFake final : public ITransport {
   public:
    Result send_message(const Message&, const Endpoint&) override { return Result::SUCCESS; }
    MessagePtr receive_message() override { return nullptr; }
    Result connect(const Endpoint&) override { return Result::NOT_IMPLEMENTED; }
    Result disconnect() override { return Result::NOT_IMPLEMENTED; }
    bool is_connected() const override { return running_; }
    Endpoint get_local_endpoint() const override { return Endpoint("192.0.2.21", 30501); }
    void set_listener(ITransportListener* listener) override { listener_ = listener; }
    Result start() override
    {
        ++starts;
        running_ = true;
        return Result::SUCCESS;
    }
    Result stop() override
    {
        ++stops;
        running_ = false;
        return Result::SUCCESS;
    }
    bool is_running() const override { return running_; }
    std::atomic<unsigned> starts{0};
    std::atomic<unsigned> stops{0};

   private:
    bool running_{false};
    ITransportListener* listener_{nullptr};
};

sd::SdConfig immediate_config()
{
    sd::SdConfig config;
    config.has_initial_delay_override = true;
    config.initial_delay_override_ms = 0;
    return config;
}

}  // namespace

/**
 * @test_case TC_SD_INJECT_EXCLUSIVE
 * @tests REQ_TRANSPORT_030
 * @brief Each facade borrows one transport. Stopping one does not stop the others.
 */
TEST(SdInjection, SdFacadesUseExclusiveTransports)
{
    MulticastFake client_net;
    MulticastFake server_net;
    MulticastFake subscriber_net;
    const auto config = immediate_config();

    {
        sd::SdClient client(config, client_net);
        sd::SdServer server(config, server_net);
        events::EventSubscriber subscriber(0x0007, subscriber_net);
        EXPECT_EQ(client_net.listener(), nullptr);
        EXPECT_EQ(client_net.starts, 0u);

        ASSERT_TRUE(client.initialize());
        ASSERT_TRUE(server.initialize());
        subscriber.set_default_endpoint("192.0.2.1", 30501);
        ASSERT_EQ(subscriber.subscribe_eventgroup(0x1234, 0x0001, 0x0001,
                                                  [](const events::EventNotification&) {}),
                  Result::INVALID_STATE);
        ASSERT_TRUE(subscriber.initialize());
        ASSERT_EQ(subscriber.subscribe_eventgroup(0x1234, 0x0001, 0x0001,
                                                  [](const events::EventNotification&) {}),
                  Result::SUCCESS);

        EXPECT_NE(client_net.listener(), nullptr);
        EXPECT_NE(server_net.listener(), nullptr);
        EXPECT_NE(subscriber_net.listener(), nullptr);
        EXPECT_NE(client_net.listener(), server_net.listener());
        EXPECT_NE(client_net.listener(), subscriber_net.listener());
        ASSERT_FALSE(client_net.joined.empty());
        EXPECT_EQ(client_net.joined.front(), config.multicast_address);
        ASSERT_FALSE(server_net.joined.empty());

        const size_t server_sent = server_net.messages.size();
        ASSERT_TRUE(client.find_service(0x1234, [](const platform::Vector<sd::ServiceInstance>&) {}));
        EXPECT_FALSE(client_net.messages.empty());
        EXPECT_EQ(server_net.messages.size(), server_sent);
        EXPECT_EQ(client_net.destinations.back().get_port(), config.multicast_port);

        client.shutdown();
        EXPECT_EQ(client_net.stops, 1u);
        EXPECT_EQ(client_net.listener(), nullptr);
        EXPECT_TRUE(server_net.is_running());
        EXPECT_NE(server_net.listener(), nullptr);
        EXPECT_EQ(server_net.stops, 0u);
        EXPECT_TRUE(subscriber_net.is_running());
        EXPECT_EQ(subscriber_net.stops, 0u);
    }
    EXPECT_EQ(client_net.stops, 1u);
    EXPECT_EQ(server_net.stops, 1u);
    EXPECT_EQ(subscriber_net.stops, 1u);

    sd::SdClient replacement(config, client_net);
    ASSERT_TRUE(replacement.initialize());
    EXPECT_EQ(client_net.starts, 2u);
    EXPECT_EQ(server_net.starts, 1u);
    replacement.shutdown();
    EXPECT_EQ(client_net.stops, 2u);
    EXPECT_EQ(server_net.stops, 1u);
}

/**
 * @test_case TC_SD_INJECT_MULTICAST
 * @tests REQ_TRANSPORT_030
 * @brief Service discovery rejects a transport that cannot join a multicast group.
 */
TEST(SdInjection, RejectsTransportWithoutMulticast)
{
    PlainFake transport;
    sd::SdClient client(immediate_config(), transport);
    EXPECT_FALSE(client.initialize());
    EXPECT_EQ(transport.starts, 0u);
    EXPECT_EQ(transport.stops, 0u);

    sd::SdServer server(immediate_config(), transport);
    EXPECT_FALSE(server.initialize());
    EXPECT_EQ(transport.starts, 0u);
}

/**
 * @test_case TC_SD_INJECT_JOIN_FAIL
 * @tests REQ_TRANSPORT_030
 * @brief A failed SD join stops the client transport and can be replaced.
 */
TEST(SdInjection, FailedClientJoinStopsTheBorrowedTransport)
{
    MulticastFake transport;
    transport.join_result = Result::MULTICAST_ERROR;
    sd::SdClient client(immediate_config(), transport);
    EXPECT_FALSE(client.initialize());
    EXPECT_EQ(transport.starts, 1u);
    EXPECT_EQ(transport.stops, 1u);
    EXPECT_EQ(transport.listener(), nullptr);
    EXPECT_EQ(client.get_transport_result(), Result::SUCCESS);

    transport.join_result = Result::SUCCESS;
    sd::SdClient replacement(immediate_config(), transport);
    ASSERT_TRUE(replacement.initialize());
    replacement.shutdown();
}
