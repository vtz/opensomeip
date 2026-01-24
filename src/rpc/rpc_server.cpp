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

#include "rpc/rpc_server.h"
#include "rpc/rpc_types.h"
#include "transport/udp_transport.h"
#include "transport/endpoint.h"
#include "transport/transport.h"
#include "someip/message.h"
#include "common/result.h"
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace someip {
namespace rpc {

/**
 * @brief RPC Server implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_002
 * @satisfies feat_req_someip_710
 * @satisfies feat_req_someip_711
 * @satisfies feat_req_someip_712
 */
class RpcServerImpl : public transport::ITransportListener {
public:
    RpcServerImpl(uint16_t service_id)
        : service_id_(service_id),
          transport_(std::make_shared<transport::UdpTransport>(transport::Endpoint("127.0.0.1", 30490))),
          running_(false) {

        transport_->set_listener(this);
    }

    ~RpcServerImpl() {
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

        // Clear all method handlers
        std::scoped_lock lock(methods_mutex_);
        method_handlers_.clear();

        transport_->stop();
    }

    bool register_method(MethodId method_id, MethodHandler handler) {
        std::scoped_lock lock(methods_mutex_);

        // Check if already registered
        bool already_exists = method_handlers_.count(method_id) > 0;
        if (!already_exists) {
            method_handlers_[method_id] = handler;
        }
        return !already_exists;
    }

    bool unregister_method(MethodId method_id) {
        std::scoped_lock lock(methods_mutex_);
        return method_handlers_.erase(method_id) > 0;
    }

    bool is_method_registered(MethodId method_id) const {
        std::scoped_lock lock(methods_mutex_);
        return method_handlers_.find(method_id) != method_handlers_.end();
    }

    std::vector<MethodId> get_registered_methods() const {
        std::scoped_lock lock(methods_mutex_);
        std::vector<MethodId> methods;
        methods.reserve(method_handlers_.size());
        for (const auto& pair : method_handlers_) {
            methods.push_back(pair.first);
        }
        return methods;
    }

    bool is_ready() const {
        return running_ && transport_->is_connected();
    }

    RpcServer::Statistics get_statistics() const {
        // TODO: Implement statistics tracking
        return RpcServer::Statistics{};
    }

private:
    void on_message_received(MessagePtr message, const transport::Endpoint& sender) override {
        // Check if this is for our service and is a request
        if (message->get_service_id() != service_id_ || !message->is_request()) {
            return;
        }

        // Find method handler
        MethodHandler handler;
        {
            std::scoped_lock lock(methods_mutex_);
            auto it = method_handlers_.find(message->get_method_id());
            if (it == method_handlers_.end()) {
                // Method not found - send error response
                send_error_response(message, sender, ReturnCode::E_UNKNOWN_METHOD);
                return;
            }
            handler = it->second;
        }

        // Process the method call
        std::vector<uint8_t> output_params;
        RpcResult result = handler(message->get_client_id(), message->get_session_id(),
                                  message->get_payload(), output_params);

        // Send response
        if (result == RpcResult::SUCCESS) {
            send_success_response(message, sender, output_params);
        } else {
            send_error_response(message, sender, map_rpc_result_to_return_code(result));
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

    void send_success_response(MessagePtr request, const transport::Endpoint& sender,
                              const std::vector<uint8_t>& return_values) {
        MessageId response_msg_id(request->get_service_id(), request->get_method_id());
        Message response(response_msg_id, request->get_request_id(),
                        MessageType::RESPONSE, ReturnCode::E_OK);
        response.set_payload(return_values);

        Result result = transport_->send_message(response, sender);
        if (result != Result::SUCCESS) {
            // Log error or handle send failure
        }
    }

    void send_error_response(MessagePtr request, const transport::Endpoint& sender, ReturnCode error_code) {
        MessageId response_msg_id(request->get_service_id(), request->get_method_id());
        Message response(response_msg_id, request->get_request_id(),
                        MessageType::ERROR, error_code);

        Result result = transport_->send_message(response, sender);
        if (result != Result::SUCCESS) {
            // Log error or handle send failure
        }
    }

    ReturnCode map_rpc_result_to_return_code(RpcResult result) {
        switch (result) {
            case RpcResult::SUCCESS:
                return ReturnCode::E_OK;
            case RpcResult::INVALID_PARAMETERS:
                return ReturnCode::E_MALFORMED_MESSAGE;
            case RpcResult::METHOD_NOT_FOUND:
                return ReturnCode::E_UNKNOWN_METHOD;
            case RpcResult::SERVICE_NOT_AVAILABLE:
                return ReturnCode::E_NOT_REACHABLE;
            case RpcResult::TIMEOUT:
                return ReturnCode::E_TIMEOUT;
            default:
                return ReturnCode::E_NOT_OK;
        }
    }

    uint16_t service_id_;
    std::shared_ptr<transport::UdpTransport> transport_;

    std::unordered_map<MethodId, MethodHandler> method_handlers_;
    mutable std::mutex methods_mutex_;

    std::atomic<bool> running_;
};

// RpcServer implementation
RpcServer::RpcServer(uint16_t service_id)
    : impl_(std::make_unique<RpcServerImpl>(service_id)) {
}

RpcServer::~RpcServer() = default;

bool RpcServer::initialize() {
    return impl_->initialize();
}

void RpcServer::shutdown() {
    impl_->shutdown();
}

bool RpcServer::register_method(MethodId method_id, MethodHandler handler) {
    return impl_->register_method(method_id, handler);
}

bool RpcServer::unregister_method(MethodId method_id) {
    return impl_->unregister_method(method_id);
}

bool RpcServer::is_method_registered(MethodId method_id) const {
    return impl_->is_method_registered(method_id);
}

std::vector<MethodId> RpcServer::get_registered_methods() const {
    return impl_->get_registered_methods();
}

bool RpcServer::is_ready() const {
    return impl_->is_ready();
}

RpcServer::Statistics RpcServer::get_statistics() const {
    return impl_->get_statistics();
}

} // namespace rpc
} // namespace someip
