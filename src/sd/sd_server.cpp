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

#include "sd/sd_server.h"

// NOLINTNEXTLINE(misc-include-cleaner) - placement new used under SOMEIP_STATIC_ALLOC
#include <new>

#include "common/result.h"
// NOLINTNEXTLINE(misc-include-cleaner) - platform::String via containers dispatch header
#include "platform/containers.h"
#include "platform/thread.h"
#include "sd/sd_message.h"
#include "sd/sd_types.h"
#include "someip/message.h"
#include "someip/types.h"
#include "transport/endpoint.h"
#include "transport/transport.h"
#include "transport/udp_transport.h"
// NOLINTNEXTLINE(misc-include-cleaner) - someip_hton*/someip_ntoh* macros from byteorder_impl.h
#include "platform/byteorder.h"
// NOLINTNEXTLINE(misc-include-cleaner) - someip_inet_*/AF_INET/in_addr via net_impl.h
#include "platform/net.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace someip::sd {

namespace {
void uint16_to_str(uint16_t val, platform::String<>& out) {
    if (val == 0) {
        out.append("0");
        return;
    }
    std::array<char, 6> digits{};
    int pos = 5;
    while (val > 0) {
        --pos;
        digits.at(static_cast<size_t>(pos)) = static_cast<char>('0' + (val % 10));
        val /= 10;
    }
    out.append(digits.data() + pos,
               digits.data() + 5);
}
}  // namespace

// NOLINTBEGIN(misc-include-cleaner) - someip_hton*/someip_inet_*/in_addr_t are macros/types from
// platform/byteorder.h and platform/net.h that misc-include-cleaner cannot trace.

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
 * @brief Service Discovery Server implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_002
 * @satisfies feat_req_someipsd_200
 * @satisfies feat_req_someipsd_201
 * @satisfies feat_req_someipsd_202
 */
class SdServerImpl : public transport::ITransportListener {
public:
    explicit SdServerImpl(const SdConfig& config)
        : config_(config),
          transport_(create_sd_transport(config)),
          next_offer_delay_(config.initial_delay),
          running_(false) {

        transport_->set_listener(this);
    }

    ~SdServerImpl() override
    {
        shutdown();
    }

    SdServerImpl(const SdServerImpl&) = delete;
    SdServerImpl& operator=(const SdServerImpl&) = delete;
    SdServerImpl(SdServerImpl&&) = delete;
    SdServerImpl& operator=(SdServerImpl&&) = delete;

    /** @implements REQ_SD_080, REQ_SD_080_E01, REQ_SD_081, REQ_SD_082, REQ_SD_083, REQ_SD_083_E01, REQ_SD_084 */
    bool initialize() {
        if (running_) {
            return true;
        }

        if (transport_->start() != Result::SUCCESS) {
            return false;
        }

        // Join multicast group for SD messages
        if (!join_multicast_group()) {
            // Continue without multicast support in constrained environments
        }

        running_ = true;

        // Start offer timer
        start_offer_timer();

        return true;
    }

    /** @implements REQ_SD_090, REQ_SD_091, REQ_SD_092, REQ_SD_093, REQ_SD_094 */
    void shutdown() {
        if (!running_) {
            return;
        }

        running_ = false;

        // Stop offer timer
        stop_offer_timer();

        // Send stop offer messages for all services
        send_stop_offer_messages();

        // Clear offered services
        platform::ScopedLock const lock(offered_services_mutex_);
        offered_services_.clear();

        // Leave multicast group
        leave_multicast_group();

        transport_->stop();
    }

    /** @implements REQ_SD_100, REQ_SD_101, REQ_SD_102, REQ_SD_103, REQ_SD_110, REQ_SD_111, REQ_SD_112, REQ_SD_113, REQ_SD_130, REQ_SD_140, REQ_SD_141, REQ_SD_142, REQ_SD_150, REQ_SD_151, REQ_SD_152 */
    bool offer_service(const ServiceInstance& instance,
                      const platform::String<>& unicast_endpoint,
                      const platform::String<>& multicast_endpoint,
                      const platform::Vector<uint16_t>& eventgroup_ids) {
        if (!unicast_endpoint.empty()) {
            platform::String<> tmp_ip;
            uint16_t tmp_port = 0;
            if (!parse_endpoint_string(unicast_endpoint, tmp_ip, tmp_port)) {
                return false;
            }
        }

        platform::ScopedLock const lock(offered_services_mutex_);

        const auto it = std::find_if(offered_services_.begin(), offered_services_.end(),
            [&](const OfferedService& svc) {
                return svc.instance.service_id == instance.service_id &&
                       svc.instance.instance_id == instance.instance_id;
            });

        if (it != offered_services_.end()) {
            return false;
        }

        // Check service list limit (REQ_SD_040_E01)
        if (offered_services_.size() >= config_.max_services) {
            // Evict oldest service (LRU policy)
            if (!offered_services_.empty()) {
                offered_services_.erase(offered_services_.begin());
            }
        }

        if (offered_services_.size() >= offered_services_.max_size()) {
            return false;
        }

        OfferedService offered;
        offered.instance = instance;
        offered.unicast_endpoint = unicast_endpoint;
        offered.multicast_endpoint = multicast_endpoint;
        offered.last_offer_time = std::chrono::steady_clock::now();
        for (const uint16_t eg_id : eventgroup_ids) {
            OfferedEventGroup eg;
            eg.eventgroup_id = eg_id;
            offered.eventgroups.push_back(eg);
        }

        offered_services_.push_back(std::move(offered));

        // Send initial offer immediately
        send_service_offer(offered_services_.back());

        return true;
    }

    /** @implements REQ_SD_220, REQ_SD_221, REQ_SD_222, REQ_SD_223, REQ_SD_250, REQ_SD_251, REQ_SD_260 */
    bool stop_offer_service(uint16_t service_id, uint16_t instance_id) {
        platform::ScopedLock const lock(offered_services_mutex_);

        const auto it = std::find_if(offered_services_.begin(), offered_services_.end(),
            [&](const OfferedService& svc) {
                return svc.instance.service_id == service_id &&
                       svc.instance.instance_id == instance_id;
            });

        if (it == offered_services_.end()) {
            return false;
        }

        // Send stop offer message
        send_service_stop_offer(*it);

        offered_services_.erase(it);
        return true;
    }

    /** @implements REQ_SD_270, REQ_SD_272, REQ_SD_273 */
    bool update_service_ttl(uint16_t service_id, uint16_t instance_id, uint32_t ttl_seconds) {
        platform::ScopedLock const lock(offered_services_mutex_);

        const auto it = std::find_if(offered_services_.begin(), offered_services_.end(),
            [&](const OfferedService& svc) {
                return svc.instance.service_id == service_id &&
                       svc.instance.instance_id == instance_id;
            });

        if (it == offered_services_.end()) {
            return false;
        }

        it->instance.ttl_seconds = ttl_seconds;
        return true;
    }

    /** @implements REQ_SD_115, REQ_SD_115_E01, REQ_SD_115_E02, REQ_SD_117, REQ_SD_118, REQ_SD_119, REQ_SD_119_E01 */
    bool handle_eventgroup_subscription(uint16_t service_id, uint16_t instance_id,
                                       uint16_t eventgroup_id, const platform::String<>& client_address,
                                       bool acknowledge, uint32_t ttl_seconds = 3600) {

        // Create subscription response
        EventGroupEntry response_entry(
            acknowledge ? EntryType::SUBSCRIBE_EVENTGROUP_ACK : EntryType::SUBSCRIBE_EVENTGROUP_NACK);
        response_entry.set_service_id(service_id);
        response_entry.set_instance_id(instance_id);
        response_entry.set_eventgroup_id(eventgroup_id);
        response_entry.set_major_version(0x01);
        response_entry.set_ttl(acknowledge ? ttl_seconds : 0);
        response_entry.set_index1(0);
        response_entry.set_num_opts1(1);

        SdMessage response_message;
        response_message.add_entry(std::move(response_entry));

        // Add IPv4 multicast option (spec requires multicast option for ACK)
        IPv4MulticastOption multicast_option;
        // Convert multicast address to network byte order
        const in_addr_t multicast_addr = someip_inet_addr(config_.multicast_address.c_str());
        multicast_option.set_ipv4_address(multicast_addr);
        multicast_option.set_port(config_.multicast_port);
        response_message.add_option(std::move(multicast_option));

        platform::String<> client_ip = client_address;
        uint16_t client_port = config_.unicast_port;

        if (client_address.find(':') != platform::String<>::npos) {
            if (!parse_endpoint_string(client_address, client_ip, client_port)) {
                return false;
            }
        }

        const transport::Endpoint client_endpoint(client_ip, client_port);

        Message someip_message(MessageId(0xFFFF, SOMEIP_SD_METHOD_ID),
                                     RequestId(SOMEIP_SD_CLIENT_ID,
                                               next_unicast_session_id(client_ip)),
                                     MessageType::NOTIFICATION,
                                     ReturnCode::E_OK);
        auto serialized = response_message.serialize();
        if (serialized.empty()) {
            return false;
        }
        someip_message.set_payload(std::move(serialized));

        const Result result = transport_->send_message(someip_message, client_endpoint);
        return result == Result::SUCCESS;
    }

    platform::Vector<ServiceInstance> get_offered_services() const {
        platform::ScopedLock const lock(offered_services_mutex_);
        platform::Vector<ServiceInstance> result;
        result.reserve(offered_services_.size());

        for (const auto& service : offered_services_) {
            result.push_back(service.instance);
        }

        return result;
    }

    bool is_ready() const {
        return running_ && transport_->is_connected();
    }

    SdServer::Statistics get_statistics() const {
        // TODO: Implement statistics tracking
        return SdServer::Statistics{};
    }

private:
    struct OfferedService {
        ServiceInstance instance;
        platform::String<> unicast_endpoint;
        platform::String<> multicast_endpoint;
        std::chrono::steady_clock::time_point last_offer_time;
        platform::Vector<OfferedEventGroup> eventgroups;
    };

    static bool parse_endpoint_string(const platform::String<>& endpoint_str,
                                       platform::String<>& ip_out, uint16_t& port_out) {
        const size_t colon_pos = endpoint_str.find(':');
        if (colon_pos == platform::String<>::npos || colon_pos == 0) {
            return false;
        }

        const platform::String<> ip_str = endpoint_str.substr(0, colon_pos);
        const platform::String<> port_str = endpoint_str.substr(colon_pos + 1);

        if (port_str.empty()) {
            return false;
        }

        for (const char c : port_str) {
            if (c < '0' || c > '9') {
                return false;
            }
        }

        long port_val = 0;
        for (const char c : port_str) {
            port_val = port_val * 10 + (c - '0');
            if (port_val > 65535) {
                return false;
            }
        }

        if (port_val <= 0 || port_val > 65535) {
            return false;
        }

        int octet_count = 0;
        int octet_val = -1;
        for (const char c : ip_str) {
            if (c == '.') {
                if (octet_val < 0 || octet_val > 255) {
                    return false;
                }
                ++octet_count;
                octet_val = -1;
            } else if (c >= '0' && c <= '9') {
                octet_val = (octet_val < 0 ? 0 : octet_val * 10) + (c - '0');
            } else {
                return false;
            }
        }
        if (octet_val < 0 || octet_val > 255 || octet_count != 3) {
            return false;
        }

        ip_out = ip_str;
        port_out = static_cast<uint16_t>(port_val);
        return true;
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

    /** @implements REQ_SD_250, REQ_SD_251, REQ_SD_260 */
    void start_offer_timer() {
        if (offer_timer_thread_ && offer_timer_thread_->joinable()) {
            return;
        }

        offer_timer_thread_.emplace([this]() {
            while (running_) {
                platform::this_thread::sleep_for(next_offer_delay_);

                if (!running_) {
                    break;
                }

                // Send periodic offers
                send_periodic_offers();

                // Update next delay (exponential backoff with max)
                if (next_offer_delay_ < config_.repetition_max) {
                    next_offer_delay_ = std::chrono::milliseconds(
                        std::min(next_offer_delay_.count() * config_.repetition_multiplier,
                                config_.repetition_max.count()));
                }
            }
        });
    }

    void stop_offer_timer() {
        if (offer_timer_thread_ && offer_timer_thread_->joinable()) {
            offer_timer_thread_->join();
        }
    }

    /** @implements REQ_SD_250, REQ_SD_251, REQ_SD_260 */
    void send_periodic_offers() {
        platform::ScopedLock const lock(offered_services_mutex_);

        const auto now = std::chrono::steady_clock::now();
        for (auto& service : offered_services_) {
            const auto time_since_last_offer = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - service.last_offer_time);

            if (time_since_last_offer >= config_.cyclic_offer) {
                send_service_offer(service);
                service.last_offer_time = now;
            }
        }
    }

    void send_stop_offer_messages() {
        platform::ScopedLock const lock(offered_services_mutex_);

        for (const auto& service : offered_services_) {
            send_service_stop_offer(service);
        }
    }

    /** @implements REQ_SD_110, REQ_SD_111, REQ_SD_112, REQ_SD_113, REQ_SD_130, REQ_SD_140, REQ_SD_141, REQ_SD_142, REQ_SD_150, REQ_SD_151, REQ_SD_152 */
    void send_service_offer(const OfferedService& service) {
        // Create offer service entry
        ServiceEntry offer_entry(EntryType::OFFER_SERVICE);
        offer_entry.set_service_id(service.instance.service_id);
        offer_entry.set_instance_id(service.instance.instance_id);
        offer_entry.set_major_version(service.instance.major_version);
        offer_entry.set_minor_version(service.instance.minor_version);
        offer_entry.set_ttl(service.instance.ttl_seconds);
        offer_entry.set_index1(0);
        offer_entry.set_num_opts1(1);

        SdMessage sd_message;
        sd_message.add_entry(std::move(offer_entry));

        IPv4EndpointOption endpoint_option;

        platform::String<> ep_ip;
        uint16_t ep_port = 0;
        if (parse_endpoint_string(service.unicast_endpoint, ep_ip, ep_port)) {
            endpoint_option.set_ipv4_address_from_string(ep_ip);
            endpoint_option.set_port(ep_port);
            endpoint_option.set_protocol(0x11);
        } else {
            return;
        }

        sd_message.add_option(std::move(endpoint_option));

        auto serialized = sd_message.serialize();
        if (serialized.empty()) {
            return;
        }

        Message someip_message(MessageId(0xFFFF, SOMEIP_SD_METHOD_ID),
                                     RequestId(SOMEIP_SD_CLIENT_ID,
                                               next_multicast_session_id()),
                                     MessageType::NOTIFICATION,
                                     ReturnCode::E_OK);
        someip_message.set_payload(std::move(serialized));

        transport::Endpoint const multicast_endpoint(config_.multicast_address, config_.multicast_port);
        const Result result = transport_->send_message(someip_message, multicast_endpoint);
        if (result != Result::SUCCESS) {
            // Log error or handle failure
        }
    }

    /** @implements REQ_SD_220, REQ_SD_221, REQ_SD_222, REQ_SD_223 */
    void send_service_stop_offer(const OfferedService& service) {
        // Create stop offer service entry
        ServiceEntry stop_entry(EntryType::STOP_OFFER_SERVICE);
        stop_entry.set_service_id(service.instance.service_id);
        stop_entry.set_instance_id(service.instance.instance_id);
        stop_entry.set_major_version(service.instance.major_version);
        stop_entry.set_minor_version(service.instance.minor_version);
        stop_entry.set_ttl(0);  // TTL = 0 means stop offering

        SdMessage sd_message;
        sd_message.add_entry(std::move(stop_entry));

        auto serialized = sd_message.serialize();
        if (serialized.empty()) {
            return;
        }

        Message someip_message(MessageId(0xFFFF, SOMEIP_SD_METHOD_ID),
                                     RequestId(SOMEIP_SD_CLIENT_ID,
                                               next_multicast_session_id()),
                                     MessageType::NOTIFICATION,
                                     ReturnCode::E_OK);
        someip_message.set_payload(std::move(serialized));

        transport::Endpoint const multicast_endpoint(config_.multicast_address, config_.multicast_port);
        const Result result = transport_->send_message(someip_message, multicast_endpoint);
        if (result != Result::SUCCESS) {
            // Log error or handle failure
        }
    }

    /** @implements REQ_SD_290, REQ_SD_292 */
    void on_message_received(MessagePtr message, const transport::Endpoint& sender) override {
        // Check if this is an SD message (service ID 0xFFFF)
        if (message->get_service_id() != 0xFFFF) {
            return;
        }

        // Parse SD message
        SdMessage sd_message;
        if (!sd_message.deserialize(message->get_payload())) {
            return;
        }

        // Process SD entries
        process_sd_entries(sd_message, sender);
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

    /** @implements REQ_SD_300, REQ_SD_312 */
    void process_sd_entries(const SdMessage& message, const transport::Endpoint& sender) {
        for (const auto& entry_var : message.get_entries()) {
            const SdEntry* entry = get_entry_ptr(entry_var);
            switch (entry->get_type()) {
                case EntryType::FIND_SERVICE:
                    if (const auto* se = std::get_if<ServiceEntry>(&entry_var)) {
                        handle_find_service(*se, sender);
                    }
                    break;
                case EntryType::SUBSCRIBE_EVENTGROUP:
                    if (const auto* eg = std::get_if<EventGroupEntry>(&entry_var)) {
                        handle_eventgroup_subscription_request(*eg, message, sender);
                    }
                    break;
                default:
                    break;
            }
        }
    }

    /** @implements REQ_SD_330, REQ_SD_341, REQ_SD_342, REQ_SD_343, REQ_SD_344, REQ_SD_345 */
    void handle_find_service(const ServiceEntry& find_entry, const transport::Endpoint& sender) {
        platform::ScopedLock const lock(offered_services_mutex_);

        // Check if we offer the requested service
        for (const auto& service : offered_services_) {
            if (service.instance.service_id == find_entry.get_service_id() &&
                (find_entry.get_instance_id() == 0xFFFF ||  // Any instance
                 service.instance.instance_id == find_entry.get_instance_id())) {

                // Send unicast offer to the finder
                send_service_offer_to_client(service, sender);
                break;
            }
        }
    }

    /** @implements REQ_SD_347, REQ_SD_349, REQ_SD_350, REQ_SD_351, REQ_SD_352, REQ_SD_353, REQ_SD_354 */
    void handle_eventgroup_subscription_request(const EventGroupEntry& subscription_entry,
                                               const SdMessage& message,
                                               const transport::Endpoint& sender) {
        const uint16_t service_id = subscription_entry.get_service_id();
        const uint16_t instance_id = subscription_entry.get_instance_id();
        const uint16_t eventgroup_id = subscription_entry.get_eventgroup_id();
        const uint32_t ttl = subscription_entry.get_ttl();

        if (ttl == 0) {
            handle_stop_subscribe(service_id, instance_id, eventgroup_id, sender);
            return;
        }

        {
            platform::ScopedLock const lock(offered_services_mutex_);
            const auto svc_it = std::find_if(offered_services_.begin(), offered_services_.end(),
                [&](const OfferedService& svc) {
                    return svc.instance.service_id == service_id &&
                           svc.instance.instance_id == instance_id;
                });
            if (svc_it == offered_services_.end()) {
                send_subscribe_nack(subscription_entry, sender);
                return;
            }

            bool eventgroup_found = false;
            for (const auto& eg : svc_it->eventgroups) {
                if (eg.eventgroup_id == eventgroup_id) {
                    eventgroup_found = true;
                    break;
                }
            }
            if (!eventgroup_found && !svc_it->eventgroups.empty()) {
                send_subscribe_nack(subscription_entry, sender);
                return;
            }
        }

        platform::String<> client_ip = sender.get_address();
        uint16_t client_port = sender.get_port();
        uint8_t client_protocol = 0x11;

        const uint8_t index1 = subscription_entry.get_index1();
        const uint8_t run1 = subscription_entry.get_num_opts1();
        const auto& options = message.get_options();

        bool has_endpoint = false;
        bool has_conflicting_options = false;

        for (uint8_t i = 0; i < run1 && (index1 + i) < options.size(); ++i) {
            const auto& option_var = options[index1 + i];
            if (const auto* ep = std::get_if<IPv4EndpointOption>(&option_var)) {
                if (has_endpoint) {
                    has_conflicting_options = true;
                    break;
                }
                client_ip = ep->get_ipv4_address_string();
                client_port = ep->get_port();
                client_protocol = ep->get_protocol();
                has_endpoint = true;
            }
        }

        const uint8_t index2 = subscription_entry.get_index2();
        const uint8_t run2 = subscription_entry.get_num_opts2();
        for (uint8_t i = 0; i < run2 && (index2 + i) < options.size(); ++i) {
            const auto& option_var = options[index2 + i];
            if (const auto* ep = std::get_if<IPv4EndpointOption>(&option_var)) {
                if (has_endpoint) {
                    has_conflicting_options = true;
                    break;
                }
                client_ip = ep->get_ipv4_address_string();
                client_port = ep->get_port();
                client_protocol = ep->get_protocol();
                has_endpoint = true;
            }
        }

        if (has_conflicting_options) {
            send_subscribe_nack(subscription_entry, sender);
            return;
        }

        if (client_port == 0) {
            send_subscribe_nack(subscription_entry, sender);
            return;
        }

        (void)client_protocol;

        platform::String<> client_addr(client_ip);
        client_addr.append(":");
        uint16_to_str(client_port, client_addr);
        handle_eventgroup_subscription(
            service_id, instance_id, eventgroup_id,
            client_addr, true, ttl
        );
    }

    void handle_stop_subscribe(uint16_t service_id, uint16_t instance_id,
                               uint16_t eventgroup_id, const transport::Endpoint& sender) {
        platform::String<> client_addr(sender.get_address());
        client_addr.append(":");
        uint16_to_str(sender.get_port(), client_addr);
        handle_eventgroup_subscription(
            service_id, instance_id, eventgroup_id,
            client_addr, true, 0
        );
    }

    void send_subscribe_nack(const EventGroupEntry& entry, const transport::Endpoint& client) {
        EventGroupEntry nack_entry(EntryType::SUBSCRIBE_EVENTGROUP_NACK);
        nack_entry.set_service_id(entry.get_service_id());
        nack_entry.set_instance_id(entry.get_instance_id());
        nack_entry.set_eventgroup_id(entry.get_eventgroup_id());
        nack_entry.set_major_version(entry.get_major_version());
        nack_entry.set_ttl(0);

        SdMessage response;
        response.add_entry(std::move(nack_entry));

        Message someip_message(MessageId(0xFFFF, SOMEIP_SD_METHOD_ID),
                                     RequestId(SOMEIP_SD_CLIENT_ID,
                                               next_unicast_session_id(client.get_address())),
                                     MessageType::NOTIFICATION,
                                     ReturnCode::E_OK);
        auto serialized = response.serialize();
        if (serialized.empty()) {
            return;
        }
        someip_message.set_payload(std::move(serialized));
        static_cast<void>(transport_->send_message(someip_message, client));
    }

    /** @implements REQ_SD_280, REQ_SD_283 */
    void send_service_offer_to_client(const OfferedService& service, const transport::Endpoint& client) {
        // Create unicast offer message (similar to multicast but unicast)
        ServiceEntry offer_entry(EntryType::OFFER_SERVICE);
        offer_entry.set_service_id(service.instance.service_id);
        offer_entry.set_instance_id(service.instance.instance_id);
        offer_entry.set_major_version(service.instance.major_version);
        offer_entry.set_minor_version(service.instance.minor_version);
        offer_entry.set_ttl(service.instance.ttl_seconds);
        offer_entry.set_index1(0);
        offer_entry.set_num_opts1(1);

        SdMessage sd_message;
        sd_message.set_unicast(true);  // Unicast response
        sd_message.add_entry(std::move(offer_entry));

        IPv4EndpointOption endpoint_option;

        platform::String<> ep_ip;
        uint16_t ep_port = 0;
        if (parse_endpoint_string(service.unicast_endpoint, ep_ip, ep_port)) {
            endpoint_option.set_ipv4_address_from_string(ep_ip);
            endpoint_option.set_port(ep_port);
            endpoint_option.set_protocol(0x11);
        } else {
            return;
        }

        sd_message.add_option(std::move(endpoint_option));

        auto serialized = sd_message.serialize();
        if (serialized.empty()) {
            return;
        }

        Message someip_message(MessageId(0xFFFF, SOMEIP_SD_METHOD_ID),
                                     RequestId(SOMEIP_SD_CLIENT_ID,
                                               next_unicast_session_id(client.get_address())),
                                     MessageType::NOTIFICATION,
                                     ReturnCode::E_OK);
        someip_message.set_payload(std::move(serialized));

        const Result result = transport_->send_message(someip_message, client);
        if (result != Result::SUCCESS) {
            // Log error or handle failure
        }
    }

    SdConfig config_;
    std::shared_ptr<transport::UdpTransport> transport_;

    platform::Vector<OfferedService> offered_services_;
    mutable platform::Mutex offered_services_mutex_;

    std::optional<platform::Thread> offer_timer_thread_;
    std::chrono::milliseconds next_offer_delay_;
    std::atomic<bool> running_;

    SdSessionIdCounter multicast_session_id_;
    platform::UnorderedMap<platform::String<>, SdSessionIdCounter, 16> unicast_session_ids_;
    mutable platform::Mutex session_id_mutex_;

    uint16_t next_multicast_session_id() {
        platform::ScopedLock const lock(session_id_mutex_);
        return multicast_session_id_.next();
    }

    uint16_t next_unicast_session_id(const platform::String<>& peer) {
        platform::ScopedLock const lock(session_id_mutex_);
        if (unicast_session_ids_.size() >= unicast_session_ids_.max_size() &&
            unicast_session_ids_.find(peer) == unicast_session_ids_.end()) {
            return 0;
        }
        return unicast_session_ids_[peer].next();
    }
};

#ifdef SOMEIP_STATIC_ALLOC
static_assert(sizeof(SdServerImpl) <= SOMEIP_PIMPL_SDSERVER_SIZE,
              "SdServerImpl exceeds pimpl storage size; increase SOMEIP_PIMPL_SDSERVER_SIZE");
#endif

// SdServer implementation
SdServer::SdServer(const SdConfig& config)
#ifdef SOMEIP_STATIC_ALLOC
{
    new (impl_storage_) SdServerImpl(config);
}
#else
    : impl_(std::make_unique<SdServerImpl>(config)) {
}
#endif

SdServer::~SdServer() {
#ifdef SOMEIP_STATIC_ALLOC
    impl()->~SdServerImpl();
#endif
}

bool SdServer::initialize() {
    return impl()->initialize();
}

void SdServer::shutdown() {
    impl()->shutdown();
}

bool SdServer::offer_service(const ServiceInstance& instance,
                            const platform::String<>& unicast_endpoint,
                            const platform::String<>& multicast_endpoint,
                            const platform::Vector<uint16_t>& eventgroup_ids) {
    return impl()->offer_service(instance, unicast_endpoint, multicast_endpoint, eventgroup_ids);
}

bool SdServer::stop_offer_service(uint16_t service_id, uint16_t instance_id) {
    return impl()->stop_offer_service(service_id, instance_id);
}

bool SdServer::update_service_ttl(uint16_t service_id, uint16_t instance_id, uint32_t ttl_seconds) {
    return impl()->update_service_ttl(service_id, instance_id, ttl_seconds);
}

bool SdServer::handle_eventgroup_subscription(uint16_t service_id, uint16_t instance_id,
                                             uint16_t eventgroup_id, const platform::String<>& client_address,
                                             bool acknowledge) {
    return impl()->handle_eventgroup_subscription(service_id, instance_id, eventgroup_id,
                                                client_address, acknowledge, 3600);
}

platform::Vector<ServiceInstance> SdServer::get_offered_services() const {
    return impl()->get_offered_services();
}

bool SdServer::is_ready() const {
    return impl()->is_ready();
}

SdServer::Statistics SdServer::get_statistics() const {
    return impl()->get_statistics();
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::sd
