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
#include "rpc/rpc_types.h"
#include "transport/udp_transport.h"
#include "transport/endpoint.h"
#include "transport/transport.h"
#include "someip/message.h"
#include "core/session_manager.h"
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <future>
#include <condition_variable>

namespace someip {
namespace rpc {

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
    RpcClientImpl(uint16_t client_id)
        : client_id_(client_id),
          session_manager_(std::make_unique<SessionManager>()),
          transport_(std::make_shared<transport::UdpTransport>(transport::Endpoint("127.0.0.1", 0))),
          next_call_handle_(1),
          running_(false) {

        transport_->set_listener(this);
    }

    ~RpcClientImpl() {
        shutdown();
    }

    bool initialize() {
        if (running_) {
            return true;
        }

        if (transport_->start() != Result::SUCCESS) {
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

        // Cancel all pending calls
        {
            std::scoped_lock lock(pending_calls_mutex_);
            for (auto& pair : pending_calls_) {
                if (pair.second.callback) {
                    RpcResponse response(pair.second.service_id, pair.second.method_id,
                                       client_id_, pair.second.session_id, RpcResult::INTERNAL_ERROR);
                    pair.second.callback(response);
                }
            }
            pending_calls_.clear();
        }

        transport_->stop();
    }

    RpcSyncResult call_method_sync(uint16_t service_id, MethodId method_id,
                                   const std::vector<uint8_t>& parameters,
                                   const RpcTimeout& timeout) {

        // Create promise/future for synchronization
        std::promise<RpcResponse> promise;
        auto future = promise.get_future();

        // Make async call with callback that sets the promise
        auto handle = call_method_async(service_id, method_id, parameters,
            [&promise](const RpcResponse& response) {
                promise.set_value(response);
            }, timeout);

        if (handle == 0) {
            return {RpcResult::INTERNAL_ERROR, {}, std::chrono::milliseconds(0)};
        }

        // Wait for response with timeout
        auto status = future.wait_for(std::chrono::milliseconds(timeout.response_timeout));
        if (status != std::future_status::ready) {
            cancel_call(handle);
            return {RpcResult::TIMEOUT, {}, timeout.response_timeout};
        }

        auto response = future.get();
        auto response_time = std::chrono::milliseconds(0); // TODO: Track actual response time

        return {response.result, response.return_values, response_time};
    }

    RpcCallHandle call_method_async(uint16_t service_id, MethodId method_id,
                                    const std::vector<uint8_t>& parameters,
                                    RpcCallback callback,
                                    const RpcTimeout& timeout) {

        if (!running_) {
            return 0;
        }

        // Create session for this call
        uint16_t session_id = session_manager_->create_session(client_id_);

        // Create request message
        MessageId msg_id(service_id, method_id);
        RequestId req_id(client_id_, session_id);
        Message request(msg_id, req_id, MessageType::REQUEST, ReturnCode::E_OK);
        request.set_payload(parameters);

        // Create pending call record
        PendingCall call_info{
            service_id, method_id, session_id,
            std::chrono::steady_clock::now(),
            timeout, callback
        };

        RpcCallHandle handle;
        {
            std::scoped_lock lock(pending_calls_mutex_);
            handle = next_call_handle_++;
            pending_calls_[handle] = std::move(call_info);
        }

        // Send request
        transport::Endpoint server_endpoint("127.0.0.1", 30490); // TODO: Make configurable
        if (transport_->send_message(request, server_endpoint) != Result::SUCCESS) {
            std::scoped_lock lock(pending_calls_mutex_);
            pending_calls_.erase(handle);
            return 0;
        }

        return handle;
    }

    bool cancel_call(RpcCallHandle handle) {
        std::scoped_lock lock(pending_calls_mutex_);
        auto it = pending_calls_.find(handle);
        if (it == pending_calls_.end()) {
            return false;
        }

        // Call callback with cancellation result
        if (it->second.callback) {
            RpcResponse response(it->second.service_id, it->second.method_id,
                               client_id_, it->second.session_id, RpcResult::INTERNAL_ERROR);
            it->second.callback(response);
        }

        pending_calls_.erase(it);
        return true;
    }

    bool is_ready() const {
        return running_ && transport_->is_connected();
    }

    RpcClient::Statistics get_statistics() const {
        // TODO: Implement statistics tracking
        return RpcClient::Statistics{};
    }

private:
    struct PendingCall {
        uint16_t service_id;
        MethodId method_id;
        uint16_t session_id;
        std::chrono::steady_clock::time_point start_time;
        RpcTimeout timeout;
        RpcCallback callback;
    };

    void on_message_received(MessagePtr message, const transport::Endpoint& sender) override {
        // Check if this is a response to one of our pending calls
        if (!message->is_response()) {
            return;
        }

        std::scoped_lock lock(pending_calls_mutex_);

        // Find matching pending call by session ID
        for (auto it = pending_calls_.begin(); it != pending_calls_.end(); ++it) {
            if (it->second.session_id == message->get_session_id() &&
                it->second.service_id == message->get_service_id() &&
                it->second.method_id == message->get_method_id()) {

                // Create response
                RpcResult result = (message->is_success()) ? RpcResult::SUCCESS : RpcResult::INTERNAL_ERROR;
                RpcResponse response(message->get_service_id(), message->get_method_id(),
                                   message->get_client_id(), message->get_session_id(), result);
                response.return_values = message->get_payload();

                // Call callback
                if (it->second.callback) {
                    it->second.callback(response);
                }

                // Remove pending call
                pending_calls_.erase(it);
                break;
            }
        }
    }

    void on_connection_lost(const transport::Endpoint& endpoint) override {
        // TODO: Handle connection loss
    }

    void on_connection_established(const transport::Endpoint& endpoint) override {
        // TODO: Handle connection establishment
    }

    void on_error(Result error) override {
        // TODO: Handle transport errors
    }

    uint16_t client_id_;
    std::unique_ptr<SessionManager> session_manager_;
    std::shared_ptr<transport::UdpTransport> transport_;

    std::unordered_map<RpcCallHandle, PendingCall> pending_calls_;
    mutable std::mutex pending_calls_mutex_;
    std::atomic<RpcCallHandle> next_call_handle_;
    std::atomic<bool> running_;
};

// RpcClient implementation
RpcClient::RpcClient(uint16_t client_id)
    : impl_(std::make_unique<RpcClientImpl>(client_id)) {
}

RpcClient::~RpcClient() = default;

bool RpcClient::initialize() {
    return impl_->initialize();
}

void RpcClient::shutdown() {
    impl_->shutdown();
}

RpcSyncResult RpcClient::call_method_sync(uint16_t service_id, MethodId method_id,
                                         const std::vector<uint8_t>& parameters,
                                         const RpcTimeout& timeout) {
    return impl_->call_method_sync(service_id, method_id, parameters, timeout);
}

RpcCallHandle RpcClient::call_method_async(uint16_t service_id, MethodId method_id,
                                          const std::vector<uint8_t>& parameters,
                                          RpcCallback callback,
                                          const RpcTimeout& timeout) {
    return impl_->call_method_async(service_id, method_id, parameters, callback, timeout);
}

bool RpcClient::cancel_call(RpcCallHandle handle) {
    return impl_->cancel_call(handle);
}

bool RpcClient::is_ready() const {
    return impl_->is_ready();
}

RpcClient::Statistics RpcClient::get_statistics() const {
    return impl_->get_statistics();
}

} // namespace rpc
} // namespace someip
