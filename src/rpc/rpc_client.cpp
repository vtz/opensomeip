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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <new>  // NOLINT(misc-include-cleaner) - static allocation placement new
#include <optional>
#include <unordered_map>
#include <utility>

#include "../transport/transport_session.h"
#include "common/result.h"
#include "core/session_manager.h"
#include "platform/containers.h"  // NOLINT(misc-include-cleaner) - PAL dispatch
#include "platform/thread.h"
#include "rpc/rpc_types.h"
#include "someip/message.h"
#include "someip/types.h"
#include "transport/endpoint.h"
#include "transport/transport.h"

namespace someip::rpc {

// NOLINTBEGIN(misc-include-cleaner) - platform::Mutex / platform::this_thread from platform/thread.h (IWYU false positives in impl).

/**
 * @brief RPC Client implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_002
 * @satisfies feat_req_someip_700
 * @satisfies feat_req_someip_701
 * @satisfies feat_req_someip_702
 * @satisfies feat_req_someip_92
 */
class RpcClientImpl : public transport::ITransportListener {
    struct SyncWaiter {
        std::optional<RpcResponse> response;
    };

    class SessionRegistration {
       public:
        SessionRegistration(SessionManager& manager, uint16_t session_id)
            : manager_(manager), session_id_(session_id)
        {
        }
        ~SessionRegistration()
        {
            if (session_id_ != 0) {
                manager_.remove_session(session_id_);
            }
        }
        SessionRegistration(const SessionRegistration&) = delete;
        SessionRegistration& operator=(const SessionRegistration&) = delete;
        SessionRegistration(SessionRegistration&&) = delete;
        SessionRegistration& operator=(SessionRegistration&&) = delete;
        void release()
        {
            session_id_ = 0;
        }

       private:
        SessionManager& manager_;
        uint16_t session_id_;
    };

    // Registration ownership is explicit; public handle 0 remains reserved for failure.
    class PendingRegistration {
       public:
        explicit PendingRegistration(RpcClientImpl& client) : client_(client)
        {
        }
        ~PendingRegistration()
        {
            if (armed_) {
                platform::ScopedLock const lock(client_.pending_calls_mutex_);
                auto it = client_.pending_calls_.find(handle_);
                if (it != client_.pending_calls_.end()) {
                    client_.session_manager_.remove_session(it->second.session_id);
                    client_.pending_calls_.erase(it);
                }
            }
        }
        PendingRegistration(const PendingRegistration&) = delete;
        PendingRegistration& operator=(const PendingRegistration&) = delete;
        PendingRegistration(PendingRegistration&&) = delete;
        PendingRegistration& operator=(PendingRegistration&&) = delete;

        // Marks `handle` as owned by this registration; must be called with a
        // handle already present in pending_calls_.
        void arm(RpcCallHandle handle)
        {
            handle_ = handle;
            armed_ = true;
        }

        // Disarms the registration (the caller takes over cleanup responsibility,
        // or the entry has already been erased elsewhere) and returns the handle.
        RpcCallHandle release()
        {
            armed_ = false;
            return handle_;
        }

       private:
        RpcClientImpl& client_;
        RpcCallHandle handle_{0};
        bool armed_{false};
    };

public:
    template <typename Transport>
    RpcClientImpl(uint16_t client_id, uint8_t interface_version, Transport&& transport)
        : client_id_(client_id),
          interface_version_(interface_version),
          transport_session_(std::forward<Transport>(transport)),
          transport_(transport_session_.get()),
          next_call_handle_(1),
          running_(false)
    {
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

    Result get_transport_result() const
    {
        return transport_session_.result();
    }

    bool initialize() {
        if (running_) {
            return true;
        }

        if (transport_session_.start(*this) != Result::SUCCESS) {
            return false;
        }

        running_ = true;
        return true;
    }

    void shutdown() {
        if (!running_) {
            transport_session_.stop();
            return;
        }

        running_ = false;
        transport_session_.stop();

        platform::Vector<std::pair<RpcCallback, RpcResponse>> shutdown_cbs;
        {
            platform::ScopedLock const lock(pending_calls_mutex_);
            // Complete stack-owned waiters before copying or invoking application callbacks.
            for (auto& pair : pending_calls_) {
                auto& call = pair.second;
                if (call.waiter != nullptr) {
                    call.waiter->response.emplace(call.service_id, call.method_id, client_id_,
                                                  call.session_id, RpcResult::INTERNAL_ERROR);
                }
            }
            for (auto& pair : pending_calls_) {
                session_manager_.remove_session(pair.second.session_id);
                if (pair.second.callback) {
                    shutdown_cbs.emplace_back(
                        std::move(pair.second.callback),
                        RpcResponse(pair.second.service_id, pair.second.method_id, client_id_,
                                    pair.second.session_id, RpcResult::INTERNAL_ERROR));
                }
            }
            pending_calls_.clear();
        }
        // shutdown() is a void public API that application RAII wrappers routinely
        // call from their own destructors, which are implicitly noexcept. Every
        // pending callback is still attempted, but a throwing callback must not
        // prevent later callbacks from running or escape this function.
        for (auto& [cb, resp] : shutdown_cbs) {
#ifdef __cpp_exceptions
            try {
                cb(resp);
            }
            catch (...) {}  // NOLINT(bugprone-empty-catch) shutdown() must not throw
#else
            cb(resp);
#endif
            cb = nullptr;
        }
    }

    void set_remote_endpoint(const transport::Endpoint& ep) {
        platform::ScopedLock const lock(remote_mutex_);
        remote_endpoint_ = ep;
    }

    transport::Endpoint get_local_endpoint() const {
        return transport_.get_local_endpoint();
    }

    std::optional<transport::Endpoint> remote_endpoint() const {
        platform::ScopedLock const lock(remote_mutex_);
        return remote_endpoint_;
    }

    RpcSyncResult call_method_sync(uint16_t service_id, MethodId method_id,
                                   const platform::ByteBuffer& parameters,
                                   const RpcTimeout& timeout) {
        auto dest = remote_endpoint();
        if (!dest.has_value()) {
            return {RpcResult::SERVICE_NOT_AVAILABLE, {}, std::chrono::milliseconds(0)};
        }
        return call_method_sync(service_id, method_id, parameters, *dest, timeout);
    }

    RpcSyncResult call_method_sync(uint16_t service_id, MethodId method_id,
                                   const platform::ByteBuffer& parameters,
                                   const transport::Endpoint& server_endpoint,
                                   const RpcTimeout& timeout) {
        SyncWaiter waiter;
        PendingRegistration registration(*this);
        RpcResult submit_failure = RpcResult::INTERNAL_ERROR;
        // submit_call performs the unbounded, blocking transport send; the
        // response-time budget must only cover the wait for a reply, so the
        // clock starts after submission succeeds, not before it.
        const RpcCallHandle handle = submit_call(service_id, method_id, parameters, nullptr,
                                                 server_endpoint, timeout, &waiter,
                                                 &submit_failure);
        if (handle == 0) {
            return {submit_failure, {}, std::chrono::milliseconds(0)};
        }
        registration.arm(handle);

        const auto started = std::chrono::steady_clock::now();
        const auto deadline = started + timeout.response_timeout;
        while (true) {
            {
                platform::ScopedLock const lock(pending_calls_mutex_);
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - started);
                // Completion and timeout removal arbitrate under the same mutex;
                // an already-delivered response must win over an expired deadline.
                if (waiter.response) {
                    // on_message_received already erased pending_calls_ for this
                    // handle; just disarm so ~PendingRegistration does not
                    // re-acquire the mutex to erase an entry that is gone.
                    registration.release();
                    return {waiter.response->result, std::move(waiter.response->return_values),
                            elapsed};
                }
                if (std::chrono::steady_clock::now() >= deadline) {
                    const auto timed_out_handle = registration.release();
                    auto it = pending_calls_.find(timed_out_handle);
                    if (it != pending_calls_.end()) {
                        session_manager_.remove_session(it->second.session_id);
                        pending_calls_.erase(it);
                    }
                    return {RpcResult::TIMEOUT, {}, elapsed};
                }
            }
            platform::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    RpcCallHandle call_method_async(uint16_t service_id, MethodId method_id,
                                    const platform::ByteBuffer& parameters,
                                    RpcCallback callback,
                                    const RpcTimeout& timeout) {
        auto dest = remote_endpoint();
        if (!dest.has_value()) {
            return 0;
        }
        return call_method_async(service_id, method_id, parameters, std::move(callback),
                                 *dest, timeout);
    }

    /** @implements REQ_MSG_114, REQ_MSG_114_E01, REQ_MSG_114_E02, REQ_MSG_118, REQ_MSG_118_E01, REQ_MSG_120, REQ_MSG_120_E01 */
    RpcCallHandle call_method_async(uint16_t service_id, MethodId method_id,
                                    const platform::ByteBuffer& parameters,
                                    RpcCallback callback,
                                    const transport::Endpoint& server_endpoint,
                                    const RpcTimeout& timeout) {
        return submit_call(service_id, method_id, parameters, std::move(callback), server_endpoint,
                           timeout, nullptr);
    }

   private:
    RpcCallHandle submit_call(uint16_t service_id, MethodId method_id,
                              const platform::ByteBuffer& parameters, RpcCallback callback,
                              const transport::Endpoint& server_endpoint, const RpcTimeout& timeout,
                              SyncWaiter* waiter, RpcResult* failure_reason = nullptr)
    {
        auto fail = [failure_reason](RpcResult reason) -> RpcCallHandle {
            if (failure_reason != nullptr) {
                *failure_reason = reason;
            }
            return 0;
        };

        if (!running_) {
            return fail(RpcResult::SERVICE_NOT_AVAILABLE);
        }

        // Create session for this call. SOME/IP session ids are never 0; a 0
        // return means the session table has no capacity left.
        const uint16_t session_id = session_manager_.create_session(client_id_);
        if (session_id == 0) {
            return fail(RpcResult::SERVICE_NOT_AVAILABLE);
        }
        SessionRegistration session(session_manager_, session_id);

        // Create request message — Interface Version is the service major
        MessageId const msg_id(service_id, method_id);
        RequestId const req_id(client_id_, session_id);
        Message request(msg_id, req_id, MessageType::REQUEST, ReturnCode::E_OK);
        request.set_interface_version(interface_version_);
        request.set_payload(parameters);

        // Create pending call record
        PendingCall call_info{
            service_id, method_id,           session_id, std::chrono::steady_clock::now(),
            timeout,    std::move(callback), waiter};

        // Erase on failed sends and exceptions before the caller's waiter can expire.
        PendingRegistration registration(*this);
        {
            platform::ScopedLock const lock(pending_calls_mutex_);
            if (!running_) {
                return fail(RpcResult::SERVICE_NOT_AVAILABLE);
            }
            if (pending_calls_.size() >= pending_calls_.max_size()) {
                return fail(RpcResult::SERVICE_NOT_AVAILABLE);
            }
            // Never overwrite an outstanding registration when the counter wraps.
            RpcCallHandle handle = next_call_handle_.fetch_add(1, std::memory_order_relaxed);
            while (handle == 0 || pending_calls_.find(handle) != pending_calls_.end()) {
                handle = next_call_handle_.fetch_add(1, std::memory_order_relaxed);
            }
            pending_calls_.insert({handle, std::move(call_info)});
            registration.arm(handle);
            session.release();
        }

        if (transport_.send_message(request, server_endpoint) != Result::SUCCESS) {
            // ~PendingRegistration erases the entry and releases its session.
            return fail(RpcResult::INTERNAL_ERROR);
        }

        return registration.release();
    }

   public:
    /** @implements REQ_MSG_052 */
    bool send_request_no_return(uint16_t service_id, MethodId method_id,
                                const platform::ByteBuffer& params,
                                const transport::Endpoint& dest) {
        if (!running_) {
            return false;
        }

        const uint16_t session_id = session_manager_.create_session(client_id_);
        if (session_id == 0) {
            return false;
        }
        const SessionRegistration session(session_manager_, session_id);
        MessageId const msg_id(service_id, method_id);
        RequestId const req_id(client_id_, session_id);
        Message request(msg_id, req_id, MessageType::REQUEST_NO_RETURN, ReturnCode::E_OK);
        request.set_interface_version(interface_version_);
        request.set_payload(params);

        return transport_.send_message(request, dest) == Result::SUCCESS;
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
            if (it->second.waiter != nullptr) {
                it->second.waiter->response.emplace(it->second.service_id, it->second.method_id,
                                                    client_id_, it->second.session_id,
                                                    RpcResult::INTERNAL_ERROR);
            }
            if (it->second.callback) {
                cancel_cb = std::move(it->second.callback);
                cancel_resp = RpcResponse(it->second.service_id, it->second.method_id,
                                          client_id_, it->second.session_id, RpcResult::INTERNAL_ERROR);
            }
            session_manager_.remove_session(it->second.session_id);
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

    void set_message_rejection_handler(
        platform::Function<void(const transport::MessageRejectionInfo&)> handler) {
        platform::ScopedLock const lock(rejection_mutex_);
        rejection_handler_ = std::move(handler);
    }

private:
    struct PendingCall {
        uint16_t service_id{};
        MethodId method_id{};
        uint16_t session_id{};
        std::chrono::steady_clock::time_point start_time;
        RpcTimeout timeout;
        RpcCallback callback;
        SyncWaiter* waiter{nullptr};
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

                    RpcResult result = RpcResult::INTERNAL_ERROR;
                    if (message->get_interface_version() != interface_version_) {
                        result = RpcResult::WRONG_INTERFACE_VERSION;
                    } else if (message->is_success()) {
                        result = RpcResult::SUCCESS;
                    }
                    recv_resp = RpcResponse(message->get_service_id(), message->get_method_id(),
                                            message->get_client_id(), message->get_session_id(), result);
                    recv_resp.return_values = message->get_payload();

                    if (it->second.waiter != nullptr) {
                        it->second.waiter->response.emplace(std::move(recv_resp));
                        session_manager_.remove_session(it->second.session_id);
                        pending_calls_.erase(it);
                        return;
                    }
                    else if (it->second.callback) {
                        recv_cb = std::move(it->second.callback);
                    }
                    session_manager_.remove_session(it->second.session_id);
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

    /** @implements REQ_TRANSPORT_026 */
    void on_message_rejected(const transport::MessageRejectionInfo& info) override {
        platform::Function<void(const transport::MessageRejectionInfo&)> handler;
        {
            platform::ScopedLock const lock(rejection_mutex_);
            handler = rejection_handler_;
        }
        if (handler) {
            handler(info);
        }
    }

    uint16_t client_id_;
    uint8_t interface_version_;
    SessionManager session_manager_;
    transport::detail::TransportSession transport_session_;
    transport::ITransport& transport_;

    std::optional<transport::Endpoint> remote_endpoint_;
    mutable platform::Mutex remote_mutex_;

    platform::UnorderedMap<RpcCallHandle, PendingCall, 32> pending_calls_;
    mutable platform::Mutex pending_calls_mutex_;
    std::atomic<RpcCallHandle> next_call_handle_;
    std::atomic<bool> running_;

    platform::Function<void(const transport::MessageRejectionInfo&)> rejection_handler_;
    mutable platform::Mutex rejection_mutex_;
};

#ifdef SOMEIP_STATIC_ALLOC
static_assert(sizeof(RpcClientImpl) <= SOMEIP_PIMPL_RPCCLIENT_SIZE,
              "RpcClientImpl exceeds pimpl storage size; increase SOMEIP_PIMPL_RPCCLIENT_SIZE");
#endif

// RpcClient implementation
RpcClient::RpcClient(uint16_t client_id, uint8_t interface_version,
                     const transport::Endpoint& local_bind)
#ifdef SOMEIP_STATIC_ALLOC
{
    new (impl_storage_) RpcClientImpl(client_id, interface_version, local_bind);
}
#else
    : impl_(std::make_unique<RpcClientImpl>(client_id, interface_version, local_bind)) {
}
#endif

RpcClient::RpcClient(uint16_t client_id, transport::ITransport& transport,
                     uint8_t interface_version)
#ifdef SOMEIP_STATIC_ALLOC
{
    new (impl_storage_) RpcClientImpl(client_id, interface_version, transport);
}
#else
    : impl_(std::make_unique<RpcClientImpl>(client_id, interface_version, transport))
{
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

Result RpcClient::get_transport_result() const
{
    return impl()->get_transport_result();
}

void RpcClient::set_remote_endpoint(const transport::Endpoint& ep) {
    impl()->set_remote_endpoint(ep);
}

transport::Endpoint RpcClient::get_local_endpoint() const {
    return impl()->get_local_endpoint();
}

RpcSyncResult RpcClient::call_method_sync(uint16_t service_id, MethodId method_id,
                                         const platform::ByteBuffer& parameters,
                                         const RpcTimeout& timeout) {
    return impl()->call_method_sync(service_id, method_id, parameters, timeout);
}

RpcSyncResult RpcClient::call_method_sync(uint16_t service_id, MethodId method_id,
                                         const platform::ByteBuffer& parameters,
                                         const transport::Endpoint& server_endpoint,
                                         const RpcTimeout& timeout) {
    return impl()->call_method_sync(service_id, method_id, parameters, server_endpoint, timeout);
}

RpcCallHandle RpcClient::call_method_async(uint16_t service_id, MethodId method_id,
                                          const platform::ByteBuffer& parameters,
                                          RpcCallback callback,
                                          const RpcTimeout& timeout) {
    return impl()->call_method_async(service_id, method_id, parameters, std::move(callback), timeout);
}

RpcCallHandle RpcClient::call_method_async(uint16_t service_id, MethodId method_id,
                                          const platform::ByteBuffer& parameters,
                                          RpcCallback callback,
                                          const transport::Endpoint& server_endpoint,
                                          const RpcTimeout& timeout) {
    return impl()->call_method_async(service_id, method_id, parameters, std::move(callback),
                                     server_endpoint, timeout);
}

bool RpcClient::send_request_no_return(uint16_t service_id, MethodId method_id,
                                       const platform::ByteBuffer& params,
                                       const transport::Endpoint& dest) {
    return impl()->send_request_no_return(service_id, method_id, params, dest);
}

bool RpcClient::cancel_call(RpcCallHandle handle) {
    return impl()->cancel_call(handle);
}

bool RpcClient::is_ready() const {
    return impl()->is_ready();
}

void RpcClient::set_message_rejection_handler(
    platform::Function<void(const transport::MessageRejectionInfo&)> handler) {
    impl()->set_message_rejection_handler(std::move(handler));
}

RpcClient::Statistics RpcClient::get_statistics() const {
    return impl()->get_statistics();
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::rpc
