/********************************************************************************
 * Copyright (c) 2026 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_TEST_MULTICAST_FAKE_H
#define SOMEIP_TEST_MULTICAST_FAKE_H

#include "someip/message.h"
#include "transport/multicast_transport.h"
#include "transport/transport.h"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace someip::test {

/**
 * @brief ITransport that can fail multicast membership with join_result.
 *
 * SD tests set join_result to MULTICAST_ERROR so join failure does not depend
 * on a host interface address.
 */
class MulticastFake final : public transport::ITransport,
                            public transport::IMulticastTransport {
   public:
    Result send_message(const Message& message, const transport::Endpoint& endpoint) override
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
    Result connect(const transport::Endpoint&) override { return Result::NOT_IMPLEMENTED; }
    Result disconnect() override { return Result::NOT_IMPLEMENTED; }
    bool is_connected() const override { return running_; }
    transport::Endpoint get_local_endpoint() const override
    {
        return transport::Endpoint("192.0.2.20", 30490);
    }

    void set_listener(transport::ITransportListener* listener) override
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
        if (throw_on_stop) {
            throw std::runtime_error("MulticastFake::stop failed");
        }
        return Result::SUCCESS;
    }

    bool is_running() const override { return running_; }
    transport::IMulticastTransport* multicast_transport() noexcept override { return this; }

    Result join_multicast_group(const platform::String<>& group) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        joined.push_back(group);
        return join_result;
    }

    /// Number of join attempts, including failures. Safe to read from the test thread.
    std::size_t join_count()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return joined.size();
    }

    Result leave_multicast_group(const platform::String<>&) override { return Result::SUCCESS; }

    transport::ITransportListener* listener()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return listener_;
    }

    Result send_result{Result::SUCCESS};
    Result join_result{Result::SUCCESS};
    bool throw_on_stop{false};
    std::atomic<unsigned> starts{0};
    std::atomic<unsigned> stops{0};
    std::vector<Message> messages;
    std::vector<transport::Endpoint> destinations;
    std::vector<platform::String<>> joined;

   private:
    bool running_{false};
    std::mutex mutex_;
    transport::ITransportListener* listener_{nullptr};
};

}  // namespace someip::test

#endif  // SOMEIP_TEST_MULTICAST_FAKE_H
