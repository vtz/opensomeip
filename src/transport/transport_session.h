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
#include "transport/multicast_transport.h"
#include "transport/transport.h"
#include "transport/udp_transport.h"

namespace someip::transport::detail {

/**
 * @brief Exclusive transport lifecycle for RPC, event, and SD facades.
 * @implements REQ_ARCH_002, REQ_ARCH_003, REQ_TRANSPORT_030
 * @note Lifecycle calls require external serialization; only result() is concurrent.
 */
class TransportSession {
   public:
    explicit TransportSession(const Endpoint& endpoint)
        : owned_(std::in_place, endpoint), transport_(*owned_), multicast_(&*owned_)
    {
    }

    TransportSession(const Endpoint& endpoint, const UdpTransportConfig& config)
        : owned_(std::in_place, endpoint, config), transport_(*owned_), multicast_(&*owned_)
    {
    }

    explicit TransportSession(ITransport& transport)
        : transport_(transport), multicast_(dynamic_cast<IMulticastTransport*>(&transport))
    {
    }

    ~TransportSession()
    {
        // Facades must still stop before destroying their callback-owned state.
        stop();
    }

    TransportSession(const TransportSession&) = delete;
    TransportSession& operator=(const TransportSession&) = delete;
    TransportSession(TransportSession&&) = delete;
    TransportSession& operator=(TransportSession&&) = delete;

    ITransport& get() const
    {
        return transport_;
    }

    /**
     * @brief Multicast membership of the owned or borrowed backend, if it has one.
     * @return Null when the borrowed transport does not implement IMulticastTransport.
     */
    IMulticastTransport* multicast() const
    {
        return multicast_;
    }

    /**
     * @brief Register the listener and start an exclusive transport.
     * @param listener Callback target, kept alive through the stop barrier.
     * @return Start error, cleanup error in preference, or INVALID_STATE for an active transport.
     */
    Result start(ITransportListener& listener)
    {
        if (active_ || transport_.is_running()) {
            result_ = Result::INVALID_STATE;
            return Result::INVALID_STATE;
        }

        transport_.set_listener(&listener);
        active_ = true;
        const Result started = transport_.start();
        if (started != Result::SUCCESS) {
            const Result stopped = stop();
            result_ = stopped == Result::SUCCESS ? started : stopped;
            return result_.load();
        }
        session_started_ = true;
        result_ = Result::SUCCESS;
        return Result::SUCCESS;
    }

    /**
     * @brief Quiesce delivery before detaching and releasing the receive queue.
     * @note A still-running backend retains cleanup ownership for a later retry.
     * @note The exchange prevents duplicate backend stops, but a non-claiming
     *       caller returns without waiting for quiescence. Lifecycle operations
     *       and destruction still require external serialization.
     */
    Result stop()
    {
        if (!active_.exchange(false)) {
            return Result::SUCCESS;
        }
        // stop() is the callback barrier; detaching first could switch live input to polling.
        const Result stopped = transport_.stop();
        transport_.set_listener(nullptr);
        const bool still_running = transport_.is_running();
        // Re-arm if the backend is still running so the documented retry path still works.
        active_ = still_running;
        if (!still_running && session_started_) {
            while (transport_.receive_message()) {
                // Discard final queued arrivals from this facade's completed receive session.
            }
            session_started_ = false;
        }
        // A backend that reports success while still running is not a successful stop.
        // Return the same status stored for get_transport_result().
        const Result status = (stopped == Result::SUCCESS && still_running)
                                  ? Result::INVALID_STATE
                                  : stopped;
        result_ = status;
        return status;
    }

    /**
     * @return The last lifecycle result; reading it does not serialize lifecycle calls.
     */
    Result result() const
    {
        return result_.load();
    }

   private:
    std::optional<UdpTransport> owned_;
    ITransport& transport_;
    IMulticastTransport* multicast_{nullptr};
    std::atomic<Result> result_{Result::SUCCESS};
    std::atomic<bool> active_{false};
    std::atomic<bool> session_started_{false};
};

}  // namespace someip::transport::detail

#endif  // SOMEIP_TRANSPORT_SESSION_H
