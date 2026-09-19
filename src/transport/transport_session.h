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

#ifndef SOMEIP_TRANSPORT_SESSION_H
#define SOMEIP_TRANSPORT_SESSION_H

#include <atomic>
#include <optional>

#include "common/result.h"
#include "transport/transport.h"
#include "transport/udp_transport.h"

namespace someip::transport::detail {

class TransportSession {
   public:
    explicit TransportSession(const Endpoint& endpoint)
        : owned_(std::in_place, endpoint), transport_(*owned_)
    {
    }

    explicit TransportSession(ITransport& transport) : transport_(transport)
    {
    }

    TransportSession(const TransportSession&) = delete;
    TransportSession& operator=(const TransportSession&) = delete;
    TransportSession(TransportSession&&) = delete;
    TransportSession& operator=(TransportSession&&) = delete;

    ITransport& get() const
    {
        return transport_;
    }

    Result start(ITransportListener& listener)
    {
        if (transport_.is_running()) {
            result_ = Result::INVALID_STATE;
            return Result::INVALID_STATE;
        }

        transport_.set_listener(&listener);
        const Result started = transport_.start();
        if (started != Result::SUCCESS) {
            transport_.set_listener(nullptr);
            const Result stopped = transport_.stop();
            result_ = stopped == Result::SUCCESS ? started : stopped;
            return result_.load();
        }
        active_ = true;
        result_ = Result::SUCCESS;
        return Result::SUCCESS;
    }

    void stop()
    {
        if (!active_) {
            return;
        }
        transport_.set_listener(nullptr);
        result_ = transport_.stop();
        active_ = false;
    }

    Result result() const
    {
        return result_.load();
    }

   private:
    std::optional<UdpTransport> owned_;
    ITransport& transport_;
    std::atomic<Result> result_{Result::SUCCESS};
    bool active_{false};
};

}  // namespace someip::transport::detail

#endif  // SOMEIP_TRANSPORT_SESSION_H
