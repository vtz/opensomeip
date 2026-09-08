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

// NOLINTNEXTLINE(misc-include-cleaner) - placement new used under SOMEIP_STATIC_ALLOC
#include <new>

#include "common/result.h"
// NOLINTNEXTLINE(misc-include-cleaner) - platform::UnorderedMap via containers dispatch header
#include "platform/containers.h"
#include "platform/thread.h"
#include "rpc/rpc_types.h"
#include "someip/message.h"
#include "someip/types.h"
#include "transport/endpoint.h"
#include "transport/transport.h"
#include "transport/udp_transport.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

namespace someip::rpc {

// NOLINTBEGIN(misc-include-cleaner) - platform::Mutex from platform/thread.h (IWYU false positives in impl).

/**
 * @brief RPC Server implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_002
 * @satisfies feat_req_someip_710
 * @satisfies feat_req_someip_711
 * @satisfies feat_req_someip_712
 * @satisfies feat_req_someip_92
 */
class RpcServerImpl : public transport::ITransportListener {
public:
    RpcServerImpl(uint16_t service_id, uint8_t interface_version,
                  const transport::Endpoint& bind_endpoint)
        : service_id_(service_id),
          interface_version_(interface_version),
          transport_(bind_endpoint),
          running_(false) {

        transport_.set_listener(this);
    }

    ~RpcServerImpl() override
    {
        shutdown();
    }

    RpcServerImpl(const RpcServerImpl&) = delete;
    RpcServerImpl& operator=(const RpcServerImpl&) = delete;
    RpcServerImpl(RpcServerImpl&&) = delete;
    RpcServerImpl& operator=(RpcServerImpl&&) = delete;

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

        // Clear all method handlers
        platform::ScopedLock const lock(methods_mutex_);
        method_handlers_.clear();

        transport_.stop();
    }

    bool register_method(MethodId method_id, MethodHandler handler, MethodSemantics semantics) {
        platform::ScopedLock const lock(methods_mutex_);

        // Check if already registered
        const bool already_exists = method_handlers_.count(method_id) > 0;
        if (!already_exists) {
            if (method_handlers_.size() >= method_handlers_.max_size()) {
                return false;
            }
            method_handlers_[method_id] = RegisteredMethod{std::move(handler), semantics};
        }
        return !already_exists;
    }

    bool unregister_method(MethodId method_id) {
        platform::ScopedLock const lock(methods_mutex_);
        return method_handlers_.erase(method_id) > 0;
    }

    bool is_method_registered(MethodId method_id) const {
        platform::ScopedLock const lock(methods_mutex_);
        return method_handlers_.find(method_id) != method_handlers_.end();
    }

    platform::Vector<MethodId> get_registered_methods() const {
        platform::ScopedLock const lock(methods_mutex_);
        platform::Vector<MethodId> methods;
        methods.reserve(method_handlers_.size());
        for (const auto& pair : method_handlers_) {
            methods.push_back(pair.first);
        }
        return methods;
    }

    transport::Endpoint get_local_endpoint() const {
        return transport_.get_local_endpoint();
    }

    bool is_ready() const {
        return running_ && transport_.is_connected();
    }

    RpcServer::Statistics get_statistics() const {
        // TODO: Implement statistics tracking
        return RpcServer::Statistics{};
    }

private:
    struct RegisteredMethod {
        MethodHandler handler;
        MethodSemantics semantics{MethodSemantics::REQUEST_RESPONSE};
    };

    static bool message_expects_response(MessageType type) {
        return type == MessageType::REQUEST || type == MessageType::TP_REQUEST;
    }

    static bool is_no_return(MessageType type) {
        return type == MessageType::REQUEST_NO_RETURN ||
               type == MessageType::TP_REQUEST_NO_RETURN;
    }

    /** @implements REQ_MSG_042, REQ_MSG_052, REQ_MSG_111, REQ_MSG_116, REQ_MSG_127, REQ_MSG_128, REQ_MSG_130, REQ_MSG_132A, REQ_MSG_133C, REQ_MSG_134, REQ_COMPAT_003 */
    void on_message_received(MessagePtr message, const transport::Endpoint& sender) override {
        // Check if this is for our service and is a request
        if (message->get_service_id() != service_id_ || !message->is_request()) {
            return;
        }

        const MessageType type = message->get_message_type();
        const bool expects_response = message_expects_response(type);

        // Interface Version is the service major; mismatch is an RPC error, not a header drop.
        if (message->get_interface_version() != interface_version_) {
            if (expects_response) {
                send_error_response(message, sender, ReturnCode::E_WRONG_INTERFACE_VERSION);
            }
            return;
        }

        // Find method handler
        RegisteredMethod registered;
        {
            platform::ScopedLock const lock(methods_mutex_);
            const auto it = method_handlers_.find(message->get_method_id());
            if (it == method_handlers_.end()) {
                if (expects_response) {
                    send_error_response(message, sender, ReturnCode::E_UNKNOWN_METHOD);
                }
                return;
            }
            registered = it->second;
        }

        const bool fire_and_forget = (registered.semantics == MethodSemantics::FIRE_AND_FORGET);

        if (expects_response && fire_and_forget) {
            send_error_response(message, sender, ReturnCode::E_WRONG_MESSAGE_TYPE);
            return;
        }

        if (is_no_return(type) && !fire_and_forget) {
            // Client does not wait; do not run the request/response handler.
            return;
        }

        // Process the method call
        platform::ByteBuffer output_params;
        const RpcResult result = registered.handler(message->get_client_id(), message->get_session_id(),
                                  message->get_payload(), output_params);

        if (!expects_response) {
            return;
        }

        // Send response
        if (result == RpcResult::SUCCESS) {
            send_success_response(message, sender, output_params);
        } else {
            send_error_response(message, sender, map_rpc_result_to_return_code(result));
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

    void stamp_interface_version(Message& msg) const {
        msg.set_interface_version(interface_version_);
    }

    /** @implements REQ_MSG_115, REQ_MSG_117, REQ_MSG_117_E01 */
    void send_success_response(MessagePtr const& request, const transport::Endpoint& sender,
                              const platform::ByteBuffer& return_values) {
        const MessageId response_msg_id(request->get_service_id(), request->get_method_id());
        Message response(response_msg_id, request->get_request_id(),
                        MessageType::RESPONSE, ReturnCode::E_OK);
        stamp_interface_version(response);
        response.set_payload(return_values);

        const Result result = transport_.send_message(response, sender);
        if (result != Result::SUCCESS) {
            // Log error or handle send failure
        }
    }

    /** @implements REQ_MSG_042, REQ_MSG_115, REQ_MSG_117, REQ_MSG_117_E01, REQ_MSG_129 */
    void send_error_response(MessagePtr const& request, const transport::Endpoint& sender, ReturnCode error_code) {
        const MessageId response_msg_id(request->get_service_id(), request->get_method_id());
        Message response(response_msg_id, request->get_request_id(),
                        MessageType::ERROR, error_code);
        response.set_interface_version(request->get_interface_version());

        const Result result = transport_.send_message(response, sender);
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
    uint8_t interface_version_;
    transport::UdpTransport transport_;

    platform::UnorderedMap<MethodId, RegisteredMethod, 32> method_handlers_;
    mutable platform::Mutex methods_mutex_;

    std::atomic<bool> running_;
};

#ifdef SOMEIP_STATIC_ALLOC
static_assert(sizeof(RpcServerImpl) <= SOMEIP_PIMPL_RPCSERVER_SIZE,
              "RpcServerImpl exceeds pimpl storage size; increase SOMEIP_PIMPL_RPCSERVER_SIZE");
#endif

// RpcServer implementation
RpcServer::RpcServer(uint16_t service_id, uint8_t interface_version,
                     const transport::Endpoint& bind_endpoint)
#ifdef SOMEIP_STATIC_ALLOC
{
    new (impl_storage_) RpcServerImpl(service_id, interface_version, bind_endpoint);
}
#else
    : impl_(std::make_unique<RpcServerImpl>(service_id, interface_version, bind_endpoint)) {
}
#endif

RpcServer::~RpcServer() {
#ifdef SOMEIP_STATIC_ALLOC
    impl()->~RpcServerImpl();
#endif
}

bool RpcServer::initialize() {
    return impl()->initialize();
}

void RpcServer::shutdown() {
    impl()->shutdown();
}

bool RpcServer::register_method(MethodId method_id, MethodHandler handler, MethodSemantics semantics) {
    return impl()->register_method(method_id, std::move(handler), semantics);
}

bool RpcServer::unregister_method(MethodId method_id) {
    return impl()->unregister_method(method_id);
}

bool RpcServer::is_method_registered(MethodId method_id) const {
    return impl()->is_method_registered(method_id);
}

platform::Vector<MethodId> RpcServer::get_registered_methods() const {
    return impl()->get_registered_methods();
}

transport::Endpoint RpcServer::get_local_endpoint() const {
    return impl()->get_local_endpoint();
}

bool RpcServer::is_ready() const {
    return impl()->is_ready();
}

RpcServer::Statistics RpcServer::get_statistics() const {
    return impl()->get_statistics();
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::rpc
