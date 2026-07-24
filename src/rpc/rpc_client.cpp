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

#include "rpc/rpc_client.h"

// NOLINTNEXTLINE(misc-include-cleaner) - placement new used under SOMEIP_STATIC_ALLOC
#include <new>

#include "common/result.h"
#include "core/session_manager.h"
// NOLINTNEXTLINE(misc-include-cleaner) - platform::UnorderedMap via containers dispatch header
#include "platform/containers.h"
#include "platform/thread.h"
#include "rpc/rpc_types.h"
#include "someip/message.h"
#include "someip/types.h"
#include "transport/endpoint.h"
#include "transport/transport.h"
#include "transport/udp_transport.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

namespace someip::rpc {

// NOLINTBEGIN(misc-include-cleaner) - platform::Mutex / platform::this_thread from platform/thread.h (IWYU false positives in impl).

/**
 * @brief RPC Client implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_002
 * @satisfies feat_req_someip_700
 * @satisfies feat_req_someip_701
 * @satisfies feat_req_someip_702
 */
class RpcClientImpl : public transport::ITransportListener {
public:
    explicit RpcClientImpl(uint16_t client_id)
        : client_id_(client_id),
          transport_(transport::Endpoint("127.0.0.1", 0)),
          next_call_handle_(1),
          running_(false) {

        transport_.set_listener(this);
    }

    ~RpcClientImpl() noexcept override
    {
#ifdef __cpp_exceptions
        try {
            shutdown();
        } catch (...) {}  // NOLINT(bugprone-empty-catch) destructor must not throw
#else
        shutdown();
#endif
    }

    RpcClientImpl(const RpcClientImpl&) = delete;
    RpcClientImpl& operator=(const RpcClientImpl&) = delete;
    RpcClientImpl(RpcClientImpl&&) = delete;
    RpcClientImpl& operator=(RpcClientImpl&&) = delete;

    bool initialize() {
        if (running_) {
            return true;
        }

        if (transport_.start() != Result::SUCCESS) {
            return false;
        }

        running_ = true;
        return true;
    }

    void shutdown() {
        if (!running_) {
            return;
        }

        running_ = false;

        platform::Vector<std::pair<RpcCallback, RpcResponse>> shutdown_cbs;
        {
            platform::ScopedLock const lock(pending_calls_mutex_);
            for (auto& pair : pending_calls_) {
                if (pair.second.callback) {
                    shutdown_cbs.emplace_back(
                        pair.second.callback,
                        RpcResponse(pair.second.service_id, pair.second.method_id,
                                    client_id_, pair.second.session_id, RpcResult::INTERNAL_ERROR));
                }
            }
            pending_calls_.clear();
        }
        for (auto& [cb, resp] : shutdown_cbs) {
            cb(resp);
        }

        transport_.stop();
    }

    RpcSyncResult call_method_sync(uint16_t service_id, MethodId method_id,
                                   const platform::ByteBuffer& parameters,
                                   const RpcTimeout& timeout) {

        struct SyncState {
            platform::Mutex mtx;
            std::optional<RpcResponse> resp;
            std::atomic<bool> ready{false};
        };
        SyncState state;

        const auto handle = call_method_async(service_id, method_id, parameters,
            [&state](const RpcResponse& response) {
                platform::ScopedLock const lk(state.mtx);
                state.resp.emplace(response);
                state.ready.store(true);
            }, timeout);

        if (handle == 0) {
            return {RpcResult::INTERNAL_ERROR, {}, std::chrono::milliseconds(0)};
        }

        const auto deadline = std::chrono::steady_clock::now()
                       + std::chrono::milliseconds(timeout.response_timeout);
        while (!state.ready.load()) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                cancel_call(handle);
                return {RpcResult::TIMEOUT, {}, timeout.response_timeout};
            }
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            const auto sleep_time = std::min(remaining, std::chrono::milliseconds(1));
            platform::this_thread::sleep_for(sleep_time);
        }

        {
            platform::ScopedLock const lk(state.mtx);
            return {state.resp->result, state.resp->return_values, std::chrono::milliseconds(0)};
        }
    }

    /** @implements REQ_MSG_114, REQ_MSG_114_E01, REQ_MSG_114_E02, REQ_MSG_118, REQ_MSG_118_E01, REQ_MSG_120, REQ_MSG_120_E01 */
    RpcCallHandle call_method_async(uint16_t service_id, MethodId method_id,
                                    const platform::ByteBuffer& parameters,
                                    RpcCallback callback,
                                    const RpcTimeout& timeout) {

        if (!running_) {
            return 0;
        }

        // Create session for this call
        const uint16_t session_id = session_manager_.create_session(client_id_);

        // Create request message
        MessageId const msg_id(service_id, method_id);
        RequestId const req_id(client_id_, session_id);
        Message request(msg_id, req_id, MessageType::REQUEST, ReturnCode::E_OK);
        request.set_payload(parameters);

        // Create pending call record
        PendingCall call_info{
            service_id, method_id, session_id,
            std::chrono::steady_clock::now(),
            timeout, std::move(callback)
        };

        RpcCallHandle handle = 0;
        {
            platform::ScopedLock const lock(pending_calls_mutex_);
            if (pending_calls_.size() >= pending_calls_.max_size()) {
                return 0;
            }
            handle = next_call_handle_++;
            pending_calls_[handle] = std::move(call_info);
        }

        // Send request
        const transport::Endpoint server_endpoint("127.0.0.1", 30490); // TODO: Make configurable
        if (transport_.send_message(request, server_endpoint) != Result::SUCCESS) {
            platform::ScopedLock const lock(pending_calls_mutex_);
            pending_calls_.erase(handle);
            return 0;
        }

        return handle;
    }

    bool cancel_call(RpcCallHandle handle) {
        RpcCallback cancel_cb;
        RpcResponse cancel_resp({}, {}, {}, {}, RpcResult::INTERNAL_ERROR);
        {
            platform::ScopedLock const lock(pending_calls_mutex_);
            auto it = pending_calls_.find(handle);
            if (it == pending_calls_.end()) {
                return false;
            }
            if (it->second.callback) {
                cancel_cb = it->second.callback;
                cancel_resp = RpcResponse(it->second.service_id, it->second.method_id,
                                          client_id_, it->second.session_id, RpcResult::INTERNAL_ERROR);
            }
            pending_calls_.erase(it);
        }
        if (cancel_cb) {
            cancel_cb(cancel_resp);
        }
        return true;
    }

    bool is_ready() const {
        return running_ && transport_.is_connected();
    }

    RpcClient::Statistics get_statistics() const {
        // TODO: Implement statistics tracking
        return RpcClient::Statistics{};
    }

private:
    struct PendingCall {
        uint16_t service_id{};
        MethodId method_id{};
        uint16_t session_id{};
        std::chrono::steady_clock::time_point start_time;
        RpcTimeout timeout;
        RpcCallback callback;
    };

    /** @implements REQ_MSG_118, REQ_MSG_118_E01 */
    void on_message_received(MessagePtr message, const transport::Endpoint& /*sender*/) override {
        // Check if this is a response to one of our pending calls
        if (!message->is_response()) {
            return;
        }

        RpcCallback recv_cb;
        RpcResponse recv_resp({}, {}, {}, {}, RpcResult::INTERNAL_ERROR);
        {
            platform::ScopedLock const lock(pending_calls_mutex_);
            for (auto it = pending_calls_.begin(); it != pending_calls_.end(); ++it) {
                if (it->second.session_id == message->get_session_id() &&
                    it->second.service_id == message->get_service_id() &&
                    it->second.method_id == message->get_method_id()) {

                    const RpcResult result = (message->is_success()) ? RpcResult::SUCCESS : RpcResult::INTERNAL_ERROR;
                    recv_resp = RpcResponse(message->get_service_id(), message->get_method_id(),
                                            message->get_client_id(), message->get_session_id(), result);
                    recv_resp.return_values = message->get_payload();

                    if (it->second.callback) {
                        recv_cb = it->second.callback;
                    }
                    pending_calls_.erase(it);
                    break;
                }
            }
        }
        if (recv_cb) {
            recv_cb(recv_resp);
        }
    }

    void on_connection_lost(const transport::Endpoint& /*endpoint*/) override {
        // TODO: Handle connection loss
    }

    void on_connection_established(const transport::Endpoint& /*endpoint*/) override {
        // TODO: Handle connection establishment
    }

    void on_error(Result /*error*/) override {
        // TODO: Handle transport errors
    }

    uint16_t client_id_;
    SessionManager session_manager_;
    transport::UdpTransport transport_;

    platform::UnorderedMap<RpcCallHandle, PendingCall, 32> pending_calls_;
    mutable platform::Mutex pending_calls_mutex_;
    std::atomic<RpcCallHandle> next_call_handle_;
    std::atomic<bool> running_;
};

#ifdef SOMEIP_STATIC_ALLOC
static_assert(sizeof(RpcClientImpl) <= SOMEIP_PIMPL_RPCCLIENT_SIZE,
              "RpcClientImpl exceeds pimpl storage size; increase SOMEIP_PIMPL_RPCCLIENT_SIZE");
#endif

// RpcClient implementation
RpcClient::RpcClient(uint16_t client_id)
#ifdef SOMEIP_STATIC_ALLOC
{
    new (impl_storage_) RpcClientImpl(client_id);
}
#else
    : impl_(std::make_unique<RpcClientImpl>(client_id)) {
}
#endif

RpcClient::~RpcClient() {
#ifdef SOMEIP_STATIC_ALLOC
    impl()->~RpcClientImpl();
#endif
}

bool RpcClient::initialize() {
    return impl()->initialize();
}

void RpcClient::shutdown() {
    impl()->shutdown();
}

RpcSyncResult RpcClient::call_method_sync(uint16_t service_id, MethodId method_id,
                                         const platform::ByteBuffer& parameters,
                                         const RpcTimeout& timeout) {
    return impl()->call_method_sync(service_id, method_id, parameters, timeout);
}

RpcCallHandle RpcClient::call_method_async(uint16_t service_id, MethodId method_id,
                                          const platform::ByteBuffer& parameters,
                                          RpcCallback callback,
                                          const RpcTimeout& timeout) {
    return impl()->call_method_async(service_id, method_id, parameters, std::move(callback), timeout);
}

bool RpcClient::cancel_call(RpcCallHandle handle) {
    return impl()->cancel_call(handle);
}

bool RpcClient::is_ready() const {
    return impl()->is_ready();
}

RpcClient::Statistics RpcClient::get_statistics() const {
    return impl()->get_statistics();
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::rpc
