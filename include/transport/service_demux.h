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

#ifndef SOMEIP_TRANSPORT_SERVICE_DEMUX_H
#define SOMEIP_TRANSPORT_SERVICE_DEMUX_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "common/result.h"
#include "platform/thread.h"
#include "transport/endpoint.h"
#include "transport/message_rejection.h"
#include "transport/transport.h"

#ifndef SOMEIP_MAX_DEMUX_SERVICES     // NOLINT(cppcoreguidelines-macro-usage)
#define SOMEIP_MAX_DEMUX_SERVICES 16  // NOLINT(cppcoreguidelines-macro-usage)
#endif

namespace someip::transport {

inline constexpr std::size_t MAX_DEMUX_SERVICES = SOMEIP_MAX_DEMUX_SERVICES;

static_assert(MAX_DEMUX_SERVICES > 0, "SOMEIP_MAX_DEMUX_SERVICES must be at least 1");
static_assert(MAX_DEMUX_SERVICES <= 256, "SOMEIP_MAX_DEMUX_SERVICES is bounded");

/**
 * @brief Dispatch one borrowed socket to several service handlers.
 *
 * One UdpTransport or TcpTransport is the socket. This object is its only
 * listener. The service id in the SOME/IP header selects the handler, and
 * the sender endpoint passed by the transport is forwarded unchanged.
 * Registering a different service id does not replace an existing one.
 * Registering a service id that is already on this socket fails. Two
 * instances of one service id use two sockets and therefore two ports.
 *
 * The table has a fixed capacity in every build, including SOMEIP_STATIC_ALLOC.
 * Dispatch runs on the transport's existing receive thread. This class does
 * not create a thread and does not open a socket.
 *
 * The transport must outlive this object. Handlers must outlive their
 * registration. start() and stop() must not be called from a handler callback.
 * unregister_service() waits until an in-flight callback for that service id
 * returns, so it must not be called from that callback. A callback may
 * unregister a different service. Broadcasts claim each handler only when
 * its callback is about to run, and an exception releases that claim.
 *
 * @implements REQ_TRANSPORT_013, REQ_TRANSPORT_022
 * @satisfies feat_req_someip_445
 * @satisfies feat_req_someip_732
 */
class ServiceDemux : public ITransportListener {
   public:
    /**
     * @brief Borrow a stopped transport.
     * @param transport Socket owner. Must outlive this demux and have no listener.
     */
    explicit ServiceDemux(ITransport& transport);

    ~ServiceDemux() override;

    ServiceDemux(const ServiceDemux&) = delete;
    ServiceDemux& operator=(const ServiceDemux&) = delete;
    ServiceDemux(ServiceDemux&&) = delete;
    ServiceDemux& operator=(ServiceDemux&&) = delete;

    /**
     * @brief Add a handler for one service id.
     *
     * A second registration of a different service id keeps the handlers
     * already in the table. A second registration of the same service id
     * fails and does not replace the first handler.
     *
     * @param service_id SOME/IP service id carried in the message header.
     * @param handler Receives messages for that service id. Must outlive the registration.
     * @return SUCCESS, INVALID_STATE when the id is already registered, or
     *         RESOURCE_EXHAUSTED when the table is full.
     */
    [[nodiscard]] Result register_service(uint16_t service_id, ITransportListener& handler);

    /**
     * @brief Remove the handler for one service id.
     *
     * Waits until an in-flight callback for this id returns. Do not call this
     * from that callback. Calling it for a different id from a callback is
     * safe, including when that id is later in the same broadcast.
     *
     * @param service_id Service id previously registered on this demux.
     * @return SUCCESS, or SERVICE_NOT_FOUND when the id is not registered.
     */
    [[nodiscard]] Result unregister_service(uint16_t service_id);

    /**
     * @brief Report whether a service id currently has a handler.
     * @param service_id Service id to query.
     * @return true when register_service() succeeded and unregister has not finished.
     */
    [[nodiscard]] bool contains(uint16_t service_id) const;

    /**
     * @brief Borrowed socket, used by handlers to send replies.
     * @return The transport passed to the constructor.
     */
    [[nodiscard]] ITransport& transport() const noexcept
    {
        return transport_;
    }

    /**
     * @brief Install this demux as the listener and start the transport.
     *
     * The transport must be stopped. A transport that is already running is
     * left unchanged and start() returns INVALID_STATE.
     *
     * @return The transport start result. SUCCESS when already started by this demux.
     */
    [[nodiscard]] Result start();

    /**
     * @brief Stop the borrowed transport and detach this listener.
     *
     * Handlers stay registered. Their callbacks have returned when stop()
     * returns, because the transport joins its receive thread.
     *
     * @return The transport stop result, or SUCCESS when this demux is not started.
     */
    [[nodiscard]] Result stop();

    void on_message_received(MessagePtr message, const Endpoint& sender) override;

    void on_message_received(const MessagePtr& message, const Endpoint& sender,
                             const Endpoint& destination) override;

    void on_connection_lost(const Endpoint& endpoint) override;

    void on_connection_established(const Endpoint& endpoint) override;

    void on_error(Result error) override;

    void on_message_rejected(const MessageRejectionInfo& info) override;

   private:
    struct Slot {
        uint16_t service_id{0};
        ITransportListener* handler{nullptr};
        uint32_t in_flight{0};
        bool occupied{false};
        bool closing{false};
    };

    struct Claimed {
        ITransportListener* handler{nullptr};
        std::size_t index{0};
    };

    struct BroadcastTarget {
        std::size_t index{0};
        uint16_t service_id{0};
    };

    class FlightGuard {
       public:
        FlightGuard(ServiceDemux& demux, std::size_t index);
        ~FlightGuard();
        FlightGuard(const FlightGuard&) = delete;
        FlightGuard& operator=(const FlightGuard&) = delete;
        FlightGuard(FlightGuard&&) = delete;
        FlightGuard& operator=(FlightGuard&&) = delete;

       private:
        ServiceDemux* demux_;
        std::size_t index_;
    };

    [[nodiscard]] static bool slot_is_free(const Slot& slot);
    [[nodiscard]] Slot& slot_at(std::size_t index);
    [[nodiscard]] static BroadcastTarget& target_at(
        std::array<BroadcastTarget, MAX_DEMUX_SERVICES>& targets, std::size_t index);
    [[nodiscard]] bool claim(uint16_t service_id, Claimed& claimed);
    [[nodiscard]] bool claim_at(std::size_t index, uint16_t service_id, Claimed& claimed);
    std::size_t snapshot_targets(std::array<BroadcastTarget, MAX_DEMUX_SERVICES>& targets);
    template <typename Callback>
    void broadcast(Callback&& callback);
    void release_flight(std::size_t index);

    ITransport& transport_;
    std::array<Slot, MAX_DEMUX_SERVICES> slots_{};
    mutable platform::Mutex table_mutex_;
    platform::ConditionVariable drained_;
    platform::Mutex lifecycle_mutex_;
    bool started_{false};
};

}  // namespace someip::transport

#endif  // SOMEIP_TRANSPORT_SERVICE_DEMUX_H
