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

#ifndef SOMEIP_TRANSPORT_EVENT_DRIVEN_TCP_TRANSPORT_H
#define SOMEIP_TRANSPORT_EVENT_DRIVEN_TCP_TRANSPORT_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "common/result.h"
#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "platform/thread.h"
#include "transport/message_rejection.h"
#include "transport/tcp_socket_adapter.h"
#include "transport/transport.h"

namespace someip::transport {

/**
 * @brief Configuration for event-driven TCP transport (stream reassembly limits).
 */
struct EventDrivenTcpTransportConfig {
    size_t max_receive_buffer{65536};
};

/**
 * @brief ITransport implementation driven by an ITcpSocketAdapter.
 *
 * Unlike EventDrivenUdpTransport, TCP requires explicit lifecycle steps
 * that are not part of the ITransport interface. Callers must use the
 * concrete type to call initialize(), and optionally enable_server_mode()
 * / try_accept_connection() for server-side usage, before calling start().
 *
 * Receive contract (REQ_TRANSPORT_027/028/029): parse one complete SOME/IP
 * frame under the queue mutex, unlock, then deliver_or_enqueue (listener XOR
 * queue). Incomplete trailing bytes may remain in receive_buffer_. If the
 * reassembly buffer cannot retain those bytes (max_receive_buffer), the
 * buffer is discarded and BUFFER_OVERFLOW is reported via on_message_rejected.
 * allocate_message() failure consumes the current complete frame and reports
 * OUT_OF_MEMORY via on_error; parsing continues so the TCP stream is not
 * stalled with complete unparsed PDUs.
 */
class EventDrivenTcpTransport : public ITransport {
   public:
    explicit EventDrivenTcpTransport(
        ITcpSocketAdapter& adapter,
        const EventDrivenTcpTransportConfig& config = EventDrivenTcpTransportConfig());

    ~EventDrivenTcpTransport() override;

    EventDrivenTcpTransport(const EventDrivenTcpTransport&) = delete;
    EventDrivenTcpTransport& operator=(const EventDrivenTcpTransport&) = delete;
    EventDrivenTcpTransport(EventDrivenTcpTransport&&) = delete;
    EventDrivenTcpTransport& operator=(EventDrivenTcpTransport&&) = delete;

    /** @brief Concrete-type-only: bind the adapter to a local endpoint. */
    [[nodiscard]] Result initialize(const Endpoint& local_endpoint);

    /** @brief Concrete-type-only: switch to server mode after initialize(). */
    [[nodiscard]] Result enable_server_mode(int backlog = 5);

    /**
     * @brief Concrete-type-only: non-blocking accept (server mode).
     *        Adapter invokes connected/disconnected callbacks on completion.
     */
    [[nodiscard]] Result try_accept_connection(Endpoint& remote_out);

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

    /**
     * @brief Test seam: the next @p count allocate_message() calls in the
     *        receive path return nullptr. Not for production use.
     */
    void testing_fail_next_allocations(size_t count);

   private:
    enum class ParseOutcome : uint8_t {
        NEED_MORE,
        CONTROL_FRAME,
        REJECTED,
        MESSAGE
    };

    void on_adapter_receive(const platform::ByteBuffer& data);
    void on_adapter_connected(const Endpoint& remote);
    void on_adapter_disconnected();
    void deliver_or_enqueue(const MessagePtr& message, const Endpoint& sender);
    void notify_rejection(const MessageRejectionInfo& info);
    ParseOutcome parse_next_message(platform::ByteBuffer& buffer, MessagePtr& message,
                                    Result& rejection, MessageRejectionStage& stage);
    static bool is_magic_cookie(const platform::ByteBuffer& data, size_t offset);

    ITcpSocketAdapter& adapter_;
    EventDrivenTcpTransportConfig config_;
    Endpoint local_endpoint_;
    Endpoint connection_remote_;
    std::atomic<ITransportListener*> listener_{nullptr};

    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    std::atomic<bool> server_mode_{false};

    platform::ByteBuffer receive_buffer_;
    platform::Queue<std::pair<MessagePtr, Endpoint>> message_queue_;
    platform::Mutex queue_mutex_;
    size_t fail_next_allocations_{0};

    static const size_t SOMEIP_HEADER_SIZE;
    static const size_t MAX_MESSAGE_SIZE;
};

}  // namespace someip::transport

#endif  // SOMEIP_TRANSPORT_EVENT_DRIVEN_TCP_TRANSPORT_H
