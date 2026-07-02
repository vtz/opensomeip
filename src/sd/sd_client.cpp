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

#include "sd/sd_client.h"

#ifdef SOMEIP_STATIC_ALLOC
#include <new>
#endif

#include "common/result.h"
// NOLINTNEXTLINE(misc-include-cleaner) - platform::UnorderedMap via containers dispatch header
#include "platform/containers.h"
#include "platform/thread.h"
#include "sd/sd_message.h"
#include "sd/sd_types.h"
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
#include <optional>
#include <unordered_map>
#include <utility>

namespace someip::sd {

// NOLINTBEGIN(misc-include-cleaner) - platform::Mutex from platform/thread.h (IWYU false positives in impl).

namespace {

// TODO: Replace with pool-based or aligned-storage transport for full no-heap compliance
std::shared_ptr<transport::UdpTransport> create_sd_transport(const SdConfig& config) {
    transport::UdpTransportConfig cfg;
    cfg.reuse_port = true;
    cfg.multicast_interface = config.unicast_address;
    return std::make_shared<transport::UdpTransport>(
        transport::Endpoint("0.0.0.0", config.multicast_port), cfg);
}

}  // namespace

/**
 * @brief Service Discovery Client implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_002
 * @satisfies feat_req_someipsd_100
 * @satisfies feat_req_someipsd_101
 * @satisfies feat_req_someipsd_102
 */
class SdClientImpl : public transport::ITransportListener {
public:
    explicit SdClientImpl(const SdConfig& config)
        : config_(config),
          transport_(create_sd_transport(config)),
          next_request_id_(1),
          running_(false) {

        transport_->set_listener(this);
    }

    ~SdClientImpl() override
    {
        shutdown();
    }

    SdClientImpl(const SdClientImpl&) = delete;
    SdClientImpl& operator=(const SdClientImpl&) = delete;
    SdClientImpl(SdClientImpl&&) = delete;
    SdClientImpl& operator=(SdClientImpl&&) = delete;

    /** @implements REQ_SD_080, REQ_SD_081, REQ_SD_082, REQ_SD_083, REQ_SD_084 */
    bool initialize() {
        if (running_) {
            return true;
        }

        if (transport_->start() != Result::SUCCESS) {
            return false;
        }

        if (!join_multicast_group()) {
            transport_->stop();
            return false;
        }

        running_ = true;
        start_maintenance_loop();
        return true;
    }

    /** @implements REQ_SD_090, REQ_SD_091, REQ_SD_092, REQ_SD_093, REQ_SD_094 */
    void shutdown() {
        if (!running_) {
            return;
        }

        running_ = false;

        stop_maintenance_loop();

        {
            platform::ScopedLock const lock(subscriptions_mutex_);
            service_subscriptions_.clear();
        }

        leave_multicast_group();

        transport_->stop();
    }

    /** @implements REQ_SD_100, REQ_SD_101, REQ_SD_102, REQ_SD_103, REQ_SD_127, REQ_SD_131, REQ_SD_210, REQ_SD_211, REQ_SD_212 */
    bool find_service(uint16_t service_id, FindServiceCallback callback,
                     std::chrono::milliseconds timeout) {

        if (!running_) {
            return false;
        }

        // Create find service entry
        ServiceEntry find_entry(EntryType::FIND_SERVICE);
        find_entry.set_service_id(service_id);
        find_entry.set_instance_id(0xFFFF);  // Find any instance
        find_entry.set_major_version(0xFF);  // Any version
        find_entry.set_ttl(3);  // 3 seconds TTL for find

        // Create SD message
        SdMessage sd_message;
        sd_message.add_entry(std::move(find_entry));

        Message someip_message(MessageId(0xFFFF, SOMEIP_SD_METHOD_ID),
                                     RequestId(SOMEIP_SD_CLIENT_ID, next_multicast_session_id()),
                                     MessageType::NOTIFICATION,
                                     ReturnCode::E_OK);
        auto serialized = sd_message.serialize();
        if (serialized.empty()) {
            return false;
        }
        someip_message.set_payload(std::move(serialized));

        transport::Endpoint const multicast_endpoint(config_.multicast_address, config_.multicast_port);
        if (transport_->send_message(someip_message, multicast_endpoint) != Result::SUCCESS) {
            return false;
        }

        // Store callback for responses
        const uint32_t request_id = next_request_id_++;
        {
            platform::ScopedLock const lock(pending_finds_mutex_);
            pending_finds_[request_id] = {
                service_id, std::move(callback),
                std::chrono::steady_clock::now(),
                timeout.count() == 0 ? std::chrono::milliseconds(5000) : timeout
            };
        }

        return true;
    }

    /** @implements REQ_SD_114, REQ_SD_116 */
    bool subscribe_service(uint16_t service_id,
                          ServiceAvailableCallback available_callback,
                          ServiceUnavailableCallback unavailable_callback) {
        platform::ScopedLock const lock(subscriptions_mutex_);

        // Check if already subscribed
        bool const already_exists = service_subscriptions_.count(service_id) > 0;
        if (!already_exists) {
            service_subscriptions_[service_id] = {
                std::move(available_callback),
                std::move(unavailable_callback)
            };
        }
        return !already_exists;
    }

    /** @implements REQ_SD_114, REQ_SD_116, REQ_SD_116_E01, REQ_SD_116_E02 */
    bool unsubscribe_service(uint16_t service_id) {
        platform::ScopedLock const lock(subscriptions_mutex_);
        return service_subscriptions_.erase(service_id) > 0;
    }

    /** @implements REQ_SD_120_E01, REQ_SD_123_E01, REQ_SD_211, REQ_SD_230, REQ_SD_231, REQ_SD_232, REQ_SD_233, REQ_SD_234, REQ_SD_235, REQ_SD_240, REQ_SD_241 */
    bool subscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id) {
        if (!running_) {
            return false;
        }

        EventGroupEntry subscribe_entry(EntryType::SUBSCRIBE_EVENTGROUP);
        subscribe_entry.set_service_id(service_id);
        subscribe_entry.set_instance_id(instance_id);
        subscribe_entry.set_eventgroup_id(eventgroup_id);
        subscribe_entry.set_major_version(0x01);
        subscribe_entry.set_ttl(3600);

        subscribe_entry.set_index1(0);
        subscribe_entry.set_num_opts1(1);

        SdMessage sd_message;
        sd_message.add_entry(std::move(subscribe_entry));

        IPv4EndpointOption endpoint_option;
        endpoint_option.set_ipv4_address_from_string(config_.unicast_address);
        endpoint_option.set_port(transport_->get_local_endpoint().get_port());
        endpoint_option.set_protocol(0x11);  // UDP
        sd_message.add_option(std::move(endpoint_option));

        auto serialized = sd_message.serialize();
        if (serialized.empty()) {
            return false;
        }

        Message someip_message(MessageId(0xFFFF, SOMEIP_SD_METHOD_ID),
                                     RequestId(SOMEIP_SD_CLIENT_ID, next_multicast_session_id()),
                                     MessageType::NOTIFICATION,
                                     ReturnCode::E_OK);
        someip_message.set_payload(std::move(serialized));

        transport::Endpoint const multicast_endpoint(config_.multicast_address, config_.multicast_port);
        const bool sent = transport_->send_message(someip_message, multicast_endpoint) == Result::SUCCESS;

        if (sent) {
            platform::ScopedLock const lock(eventgroup_subscriptions_mutex_);
            const uint64_t key = (static_cast<uint64_t>(service_id) << 32U) |
                                 (static_cast<uint64_t>(instance_id) << 16U) |
                                 eventgroup_id;
            EventGroupSubscription sub;
            sub.service_id = service_id;
            sub.instance_id = instance_id;
            sub.eventgroup_id = eventgroup_id;
            sub.major_version = 0x01;
            eventgroup_subscriptions_[key] = sub;
        }
        return sent;
    }

    /** @implements REQ_SD_120_E01, REQ_SD_123_E01, REQ_SD_230, REQ_SD_231, REQ_SD_232, REQ_SD_233, REQ_SD_234, REQ_SD_235, REQ_SD_240 */
    bool unsubscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id) {
        if (!running_) {
            return false;
        }

        EventGroupEntry unsubscribe_entry(EntryType::STOP_SUBSCRIBE_EVENTGROUP);
        unsubscribe_entry.set_service_id(service_id);
        unsubscribe_entry.set_instance_id(instance_id);
        unsubscribe_entry.set_eventgroup_id(eventgroup_id);
        unsubscribe_entry.set_major_version(0x01);
        unsubscribe_entry.set_ttl(0);  // TTL = 0 means unsubscribe

        SdMessage sd_message;
        sd_message.add_entry(std::move(unsubscribe_entry));

        auto serialized = sd_message.serialize();
        if (serialized.empty()) {
            return false;
        }

        Message someip_message(MessageId(0xFFFF, SOMEIP_SD_METHOD_ID),
                                     RequestId(SOMEIP_SD_CLIENT_ID, next_multicast_session_id()),
                                     MessageType::NOTIFICATION,
                                     ReturnCode::E_OK);
        someip_message.set_payload(std::move(serialized));

        transport::Endpoint const multicast_endpoint(config_.multicast_address, config_.multicast_port);
        const bool sent = transport_->send_message(someip_message, multicast_endpoint) == Result::SUCCESS;

        if (sent) {
            platform::ScopedLock const lock(eventgroup_subscriptions_mutex_);
            const uint64_t key = (static_cast<uint64_t>(service_id) << 32U) |
                                 (static_cast<uint64_t>(instance_id) << 16U) |
                                 eventgroup_id;
            eventgroup_subscriptions_.erase(key);
        }
        return sent;
    }

    platform::Vector<ServiceInstance> get_available_services(uint16_t service_id) const {
        platform::ScopedLock const lock(available_services_mutex_);
        platform::Vector<ServiceInstance> result;

        for (const auto& service : available_services_) {
            if (service_id == 0 || service.service_id == service_id) {
                result.push_back(service);
            }
        }

        return result;
    }

    bool is_ready() const {
        return running_ && transport_->is_connected();
    }

    SdClient::Statistics get_statistics() const {
        // TODO: Implement statistics tracking
        return SdClient::Statistics{};
    }

private:
    struct ServiceSubscription {
        ServiceAvailableCallback available_callback;
        ServiceUnavailableCallback unavailable_callback;
    };

    struct PendingFind {
        uint16_t service_id{};
        FindServiceCallback callback;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::milliseconds timeout{};
    };

    struct CachedService {
        ServiceInstance instance;
        std::chrono::steady_clock::time_point received_time;
        uint16_t last_session_id{0};
        bool reboot_flag{false};
    };

    void start_maintenance_loop() {
        if (maintenance_thread_ && maintenance_thread_->joinable()) {
            return;
        }
        maintenance_thread_.emplace([this]() {
            while (running_) {
                platform::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (!running_) { break; }
                process_find_timeouts();
                process_ttl_expiry();
            }
        });
    }

    void stop_maintenance_loop() {
        if (maintenance_thread_ && maintenance_thread_->joinable()) {
            maintenance_thread_->join();
        }
    }

    void process_find_timeouts() {
        platform::Vector<FindServiceCallback> timed_out_cbs;
        {
            platform::ScopedLock const lock(pending_finds_mutex_);
            auto const now = std::chrono::steady_clock::now();
            for (auto it = pending_finds_.begin(); it != pending_finds_.end(); ) {
                auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - it->second.start_time);
                if (elapsed >= it->second.timeout) {
                    if (it->second.callback) {
                        timed_out_cbs.push_back(it->second.callback);
                    }
                    it = pending_finds_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        for (const auto& cb : timed_out_cbs) {
            cb(platform::Vector<ServiceInstance>{});
        }
    }

    void process_ttl_expiry() {
        platform::Vector<ServiceInstance> expired;
        {
            platform::ScopedLock const lock(available_services_mutex_);
            auto const now = std::chrono::steady_clock::now();
            for (auto it = cached_services_.begin(); it != cached_services_.end(); ) {
                if (it->second.instance.ttl_seconds > 0) {
                    auto const age = std::chrono::duration_cast<std::chrono::seconds>(
                        now - it->second.received_time);
                    if (age.count() >= static_cast<long long>(it->second.instance.ttl_seconds)) {
                        expired.push_back(it->second.instance);
                        const auto rm_it = std::remove_if(
                            available_services_.begin(), available_services_.end(),
                            [&](const ServiceInstance& svc) {
                                return svc.service_id == it->second.instance.service_id &&
                                       svc.instance_id == it->second.instance.instance_id;
                            });
                        available_services_.erase(rm_it, available_services_.end());
                        it = cached_services_.erase(it);
                        continue;
                    }
                }
                ++it;
            }
        }
        for (const auto& inst : expired) {
            ServiceUnavailableCallback unavail_cb;
            {
                platform::ScopedLock const lock(subscriptions_mutex_);
                const auto sub_it = service_subscriptions_.find(inst.service_id);
                if (sub_it != service_subscriptions_.end() && sub_it->second.unavailable_callback) {
                    unavail_cb = sub_it->second.unavailable_callback;
                }
            }
            if (unavail_cb) {
                unavail_cb(inst);
            }
        }
    }

    static uint64_t make_service_key(uint16_t service_id, uint16_t instance_id) {
        return (static_cast<uint64_t>(service_id) << 16U) | instance_id;
    }

    bool join_multicast_group() {
        const auto udp_transport = std::dynamic_pointer_cast<transport::UdpTransport>(transport_);
        if (!udp_transport) {
            return false;
        }

        return udp_transport->join_multicast_group(config_.multicast_address) == Result::SUCCESS;
    }

    void leave_multicast_group() {
        const auto udp_transport = std::dynamic_pointer_cast<transport::UdpTransport>(transport_);
        if (udp_transport) {
            udp_transport->leave_multicast_group(config_.multicast_address);
        }
    }

    /** @implements REQ_SD_116_E01, REQ_SD_120_E01, REQ_SD_123_E01, REQ_SD_311, REQ_SD_331 */
    void on_message_received(MessagePtr message, const transport::Endpoint& /*sender*/) override {
        // Check if this is an SD message (service ID 0xFFFF)
        if (message->get_service_id() != 0xFFFF) {
            return;
        }

        SdMessage sd_message;
        if (!sd_message.deserialize(message->get_payload())) {
            return;
        }
        sd_message.set_session_id(message->get_session_id());

        process_sd_entries(sd_message);
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

    /** @implements REQ_SD_311, REQ_SD_331 */
    void process_sd_entries(const SdMessage& message) {
        for (const auto& entry_var : message.get_entries()) {
            const SdEntry* entry = get_entry_ptr(entry_var);
            switch (entry->get_type()) {
                case EntryType::OFFER_SERVICE:
                    if (const auto* se = std::get_if<ServiceEntry>(&entry_var)) {
                        if (entry->get_ttl() == 0) {
                            handle_service_stop_offer(*se);
                        } else {
                            handle_service_offer(*se, message);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }

    /** @implements REQ_SD_160, REQ_SD_161, REQ_SD_211, REQ_SD_230, REQ_SD_233, REQ_SD_234, REQ_SD_235, REQ_SD_240, REQ_SD_346, REQ_SD_348 */
    void handle_service_offer(const ServiceEntry& entry, const SdMessage& message) {
        ServiceInstance instance;
        instance.service_id = entry.get_service_id();
        instance.instance_id = entry.get_instance_id();
        instance.major_version = entry.get_major_version();
        instance.minor_version = entry.get_minor_version();
        instance.ttl_seconds = entry.get_ttl();

        // Extract endpoint information from options
        const auto& options = message.get_options();
        uint8_t const index1 = entry.get_index1();
        uint8_t const run1 = entry.get_num_opts1();

        for (uint8_t i = 0; i < run1 && (index1 + i) < options.size(); ++i) {
            const auto& option_var = options[index1 + i];
            if (const auto* ep = std::get_if<IPv4EndpointOption>(&option_var)) {
                instance.ip_address = ep->get_ipv4_address_string();
                instance.port = ep->get_port();
                instance.protocol = ep->get_protocol();
                break;  // Found the endpoint option
            }
        }

        bool rebooted = false;
        {
            platform::ScopedLock const lock(available_services_mutex_);
            const uint64_t key = make_service_key(instance.service_id, instance.instance_id);
            const uint16_t incoming_session = message.get_session_id();
            const bool incoming_reboot_flag = message.get_reboot_flag();

            auto cache_it = cached_services_.find(key);
            if (cache_it != cached_services_.end()) {
                const auto& prev = cache_it->second;
                if (incoming_reboot_flag != prev.reboot_flag ||
                    (incoming_session < prev.last_session_id && incoming_reboot_flag)) {
                    rebooted = true;
                }
            }

            if (rebooted) {
                const auto rm_it = std::remove_if(
                    available_services_.begin(), available_services_.end(),
                    [&](const ServiceInstance& svc) {
                        return svc.service_id == instance.service_id &&
                               svc.instance_id == instance.instance_id;
                    });
                available_services_.erase(rm_it, available_services_.end());
                cached_services_.erase(key);
            }

            const auto it = std::find_if(available_services_.begin(), available_services_.end(),
                [&](const ServiceInstance& svc) {
                    return svc.service_id == instance.service_id &&
                           svc.instance_id == instance.instance_id;
                });
            if (it == available_services_.end()) {
                available_services_.push_back(instance);
            } else {
                *it = instance;
            }

            cached_services_[key] = CachedService{
                instance,
                std::chrono::steady_clock::now(),
                incoming_session,
                incoming_reboot_flag
            };
        }

        if (rebooted) {
            handle_remote_reboot(instance);
        }

        ServiceAvailableCallback avail_cb;
        {
            platform::ScopedLock const lock(subscriptions_mutex_);
            const auto sub_it = service_subscriptions_.find(instance.service_id);
            if (sub_it != service_subscriptions_.end() && sub_it->second.available_callback) {
                avail_cb = sub_it->second.available_callback;
            }
        }

        platform::Vector<FindServiceCallback> find_cbs;
        {
            platform::ScopedLock const lock(pending_finds_mutex_);
            for (auto it = pending_finds_.begin(); it != pending_finds_.end(); ) {
                if (it->second.service_id == instance.service_id) {
                    if (it->second.callback) {
                        find_cbs.push_back(it->second.callback);
                    }
                    it = pending_finds_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        if (avail_cb) {
            avail_cb(instance);
        }
        for (const auto& cb : find_cbs) {
            const platform::Vector<ServiceInstance> found_services = {instance};
            cb(found_services);
        }
    }

    /** @implements REQ_SD_274 */
    void handle_service_stop_offer(const ServiceEntry& entry) {
        ServiceInstance instance;
        instance.service_id = entry.get_service_id();
        instance.instance_id = entry.get_instance_id();

        {
            platform::ScopedLock const lock(available_services_mutex_);
            const auto it = std::remove_if(available_services_.begin(), available_services_.end(),
                [&](const ServiceInstance& svc) {
                    return svc.service_id == instance.service_id &&
                           svc.instance_id == instance.instance_id;
                });
            available_services_.erase(it, available_services_.end());
        }

        ServiceUnavailableCallback unavail_cb;
        {
            platform::ScopedLock const lock(subscriptions_mutex_);
            const auto sub_it = service_subscriptions_.find(instance.service_id);
            if (sub_it != service_subscriptions_.end() && sub_it->second.unavailable_callback) {
                unavail_cb = sub_it->second.unavailable_callback;
            }
        }

        if (unavail_cb) {
            unavail_cb(instance);
        }
    }

    SdConfig config_;
    std::shared_ptr<transport::UdpTransport> transport_;

    platform::UnorderedMap<uint16_t, ServiceSubscription, 32> service_subscriptions_;
    mutable platform::Mutex subscriptions_mutex_;

    platform::Vector<ServiceInstance> available_services_;
    mutable platform::Mutex available_services_mutex_;

    platform::UnorderedMap<uint32_t, PendingFind, 16> pending_finds_;
    mutable platform::Mutex pending_finds_mutex_;

    std::atomic<uint32_t> next_request_id_;
    std::atomic<bool> running_;

    std::optional<platform::Thread> maintenance_thread_;

    platform::UnorderedMap<uint64_t, CachedService, 32> cached_services_;
    platform::UnorderedMap<uint64_t, EventGroupSubscription, 32> eventgroup_subscriptions_;
    mutable platform::Mutex eventgroup_subscriptions_mutex_;

    SdSessionIdCounter multicast_session_id_;
    mutable platform::Mutex session_id_mutex_;

    uint16_t next_multicast_session_id() {
        platform::ScopedLock const lock(session_id_mutex_);
        return multicast_session_id_.next();
    }


    /** @implements REQ_SD_340, REQ_SD_341, REQ_SD_342, REQ_SD_343, REQ_SD_344, REQ_SD_346, REQ_SD_348 */
    void handle_remote_reboot(const ServiceInstance& instance) {
        ServiceUnavailableCallback unavail_cb;
        {
            platform::ScopedLock const lock(subscriptions_mutex_);
            const auto sub_it = service_subscriptions_.find(instance.service_id);
            if (sub_it != service_subscriptions_.end() && sub_it->second.unavailable_callback) {
                unavail_cb = sub_it->second.unavailable_callback;
            }
        }
        if (unavail_cb) {
            unavail_cb(instance);
        }

        replay_subscriptions(instance.service_id, instance.instance_id);
    }

    void replay_subscriptions(uint16_t service_id, uint16_t instance_id) {
        platform::Vector<EventGroupSubscription> subs_to_renew;

        {
            platform::ScopedLock const lock(eventgroup_subscriptions_mutex_);
            for (const auto& eg : eventgroup_subscriptions_) {
                if (eg.second.service_id == service_id &&
                    eg.second.instance_id == instance_id) {
                    subs_to_renew.push_back(eg.second);
                }
            }
        }

        for (const auto& sub : subs_to_renew) {
            subscribe_eventgroup(sub.service_id, sub.instance_id,
                                 sub.eventgroup_id);
        }
    }
};

#ifdef SOMEIP_STATIC_ALLOC
static_assert(sizeof(SdClientImpl) <= SOMEIP_PIMPL_SDCLIENT_SIZE,
              "SdClientImpl exceeds pimpl storage size; increase SOMEIP_PIMPL_SDCLIENT_SIZE");
#endif

// SdClient implementation
SdClient::SdClient(const SdConfig& config)
#ifdef SOMEIP_STATIC_ALLOC
{
    new (impl_storage_) SdClientImpl(config);
}
#else
    : impl_(std::make_unique<SdClientImpl>(config)) {
}
#endif

SdClient::~SdClient() {
#ifdef SOMEIP_STATIC_ALLOC
    impl()->~SdClientImpl();
#endif
}

bool SdClient::initialize() {
    return impl()->initialize();
}

void SdClient::shutdown() {
    impl()->shutdown();
}

bool SdClient::find_service(uint16_t service_id, FindServiceCallback callback,
                           std::chrono::milliseconds timeout) {
    return impl()->find_service(service_id, std::move(callback), timeout);
}

bool SdClient::subscribe_service(uint16_t service_id,
                                ServiceAvailableCallback available_callback,
                                ServiceUnavailableCallback unavailable_callback) {
    return impl()->subscribe_service(service_id, std::move(available_callback),
                                   std::move(unavailable_callback));
}

bool SdClient::unsubscribe_service(uint16_t service_id) {
    return impl()->unsubscribe_service(service_id);
}

bool SdClient::subscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id) {
    return impl()->subscribe_eventgroup(service_id, instance_id, eventgroup_id);
}

bool SdClient::unsubscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id) {
    return impl()->unsubscribe_eventgroup(service_id, instance_id, eventgroup_id);
}

platform::Vector<ServiceInstance> SdClient::get_available_services(uint16_t service_id) const {
    return impl()->get_available_services(service_id);
}

bool SdClient::is_ready() const {
    return impl()->is_ready();
}

SdClient::Statistics SdClient::get_statistics() const {
    return impl()->get_statistics();
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::sd
