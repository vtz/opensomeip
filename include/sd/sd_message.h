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

#ifndef SOMEIP_SD_MESSAGE_H
#define SOMEIP_SD_MESSAGE_H

#include "sd_types.h"

#include "platform/buffer_pool.h"
#include "platform/containers.h"

#include <memory>

namespace someip::sd {

/** @implements REQ_SD_200A, REQ_SD_200B, REQ_SD_200C */
class SdEntry {
public:
    explicit SdEntry(EntryType type, uint32_t ttl = 0)
        : type_(type), ttl_(ttl) {}

    virtual ~SdEntry() = default;

    SdEntry(const SdEntry&) = delete;
    SdEntry& operator=(const SdEntry&) = delete;
    SdEntry(SdEntry&&) = delete;
    SdEntry& operator=(SdEntry&&) = delete;

    EntryType get_type() const { return type_; }
    uint32_t get_ttl() const { return ttl_; }
    void set_ttl(uint32_t ttl) { ttl_ = ttl; }

    uint8_t get_index1() const { return index1_; }
    void set_index1(uint8_t index) { index1_ = index; }
    uint8_t get_num_opts1() const { return num_opts1_; }
    void set_num_opts1(uint8_t n) { num_opts1_ = n; }

    uint8_t get_index2() const { return index2_; }
    void set_index2(uint8_t index) { index2_ = index; }
    uint8_t get_num_opts2() const { return num_opts2_; }
    void set_num_opts2(uint8_t n) { num_opts2_ = n; }

    virtual platform::ByteBuffer serialize() const = 0;
    virtual bool deserialize(const platform::ByteBuffer& data, size_t& offset) = 0;

protected:
    EntryType type_{EntryType::FIND_SERVICE};
    uint8_t index1_{0};
    uint8_t index2_{0};
    uint8_t num_opts1_{0};
    uint8_t num_opts2_{0};
    uint32_t ttl_{0};
};

/** @implements REQ_SD_200B */
class ServiceEntry : public SdEntry {
public:
    explicit ServiceEntry(EntryType type = EntryType::FIND_SERVICE)
        : SdEntry(type) {}

    uint16_t get_service_id() const { return service_id_; }
    void set_service_id(uint16_t id) { service_id_ = id; }

    uint16_t get_instance_id() const { return instance_id_; }
    void set_instance_id(uint16_t id) { instance_id_ = id; }

    uint8_t get_major_version() const { return major_version_; }
    void set_major_version(uint8_t version) { major_version_ = version; }

    uint32_t get_minor_version() const { return minor_version_; }
    void set_minor_version(uint32_t version) { minor_version_ = version; }

    platform::ByteBuffer serialize() const override;
    bool deserialize(const platform::ByteBuffer& data, size_t& offset) override;

private:
    uint16_t service_id_{0};
    uint16_t instance_id_{0};
    uint8_t major_version_{0};
    uint32_t minor_version_{0};
};

/**
 * @brief Event Group Entry (Subscribe/Unsubscribe Event Group)
 */
class EventGroupEntry : public SdEntry {
public:
    explicit EventGroupEntry(EntryType type = EntryType::SUBSCRIBE_EVENTGROUP)
        : SdEntry(type) {}

    uint16_t get_service_id() const { return service_id_; }
    void set_service_id(uint16_t id) { service_id_ = id; }

    uint16_t get_instance_id() const { return instance_id_; }
    void set_instance_id(uint16_t id) { instance_id_ = id; }

    uint16_t get_eventgroup_id() const { return eventgroup_id_; }
    void set_eventgroup_id(uint16_t id) { eventgroup_id_ = id; }

    uint8_t get_major_version() const { return major_version_; }
    void set_major_version(uint8_t version) { major_version_ = version; }

    platform::ByteBuffer serialize() const override;
    bool deserialize(const platform::ByteBuffer& data, size_t& offset) override;

private:
    uint16_t service_id_{0};
    uint16_t instance_id_{0};
    uint16_t eventgroup_id_{0};
    uint8_t major_version_{0};
};

/**
 * @brief SD Option base class
 */
class SdOption {
public:
    explicit SdOption(OptionType type) : type_(type) {}
    virtual ~SdOption() = default;

    SdOption(const SdOption&) = delete;
    SdOption& operator=(const SdOption&) = delete;
    SdOption(SdOption&&) = delete;
    SdOption& operator=(SdOption&&) = delete;

    OptionType get_type() const { return type_; }
    uint16_t get_length() const { return length_; }

    virtual platform::ByteBuffer serialize() const = 0;
    virtual bool deserialize(const platform::ByteBuffer& data, size_t& offset) = 0;

protected:
    OptionType type_{OptionType::IPV4_ENDPOINT};
    uint16_t length_{0};
};

/**
 * @brief IPv4 Endpoint Option
 */
class IPv4EndpointOption : public SdOption {
public:
    IPv4EndpointOption() : SdOption(OptionType::IPV4_ENDPOINT) {}

    uint8_t get_protocol() const { return protocol_; }
    void set_protocol(uint8_t protocol) { protocol_ = protocol; }

    uint32_t get_ipv4_address() const { return ipv4_address_; }
    void set_ipv4_address(uint32_t address) { ipv4_address_ = address; }

    uint16_t get_port() const { return port_; }
    void set_port(uint16_t port) { port_ = port; }

    // Helper methods
    void set_ipv4_address_from_string(const platform::String<>& ip_address);
    platform::String<> get_ipv4_address_string() const;

    platform::ByteBuffer serialize() const override;
    bool deserialize(const platform::ByteBuffer& data, size_t& offset) override;

private:
    uint8_t protocol_{0};      // 0x06 = TCP, 0x11 = UDP
    uint32_t ipv4_address_{0}; // IPv4 address in network byte order
    uint16_t port_{0};         // Port in network byte order
};

/**
 * @brief IPv4 Multicast Option
 */
class IPv4MulticastOption : public SdOption {
public:
    IPv4MulticastOption() : SdOption(OptionType::IPV4_MULTICAST) {}

    uint32_t get_ipv4_address() const { return ipv4_address_; }
    void set_ipv4_address(uint32_t address) { ipv4_address_ = address; }

    uint8_t get_protocol() const { return protocol_; }
    void set_protocol(uint8_t protocol) { protocol_ = protocol; }

    uint16_t get_port() const { return port_; }
    void set_port(uint16_t port) { port_ = port; }

    platform::ByteBuffer serialize() const override;
    bool deserialize(const platform::ByteBuffer& data, size_t& offset) override;

private:
    uint32_t ipv4_address_{0}; // IPv4 address in network byte order
    uint8_t protocol_{0x11};   // L4 protocol (default UDP per spec)
    uint16_t port_{0};
};

/**
 * @brief Configuration option for SD messages
 */
class ConfigurationOption : public SdOption {
public:
    ConfigurationOption() : SdOption(OptionType::CONFIGURATION) {}

    const platform::String<>& get_configuration_string() const { return config_string_; }
    void set_configuration_string(const platform::String<>& config) { config_string_ = config; }

    platform::ByteBuffer serialize() const override;
    bool deserialize(const platform::ByteBuffer& data, size_t& offset) override;

private:
    platform::String<> config_string_;
};

/** @implements REQ_SD_200A, REQ_SD_200C, REQ_MSG_113 */
class SdMessage {
public:
    SdMessage() = default;

    uint8_t get_flags() const { return flags_; }
    void set_flags(uint8_t flags) { flags_ = flags; }

    uint32_t get_reserved() const { return reserved_; }
    void set_reserved(uint32_t reserved) { reserved_ = reserved; }

    const platform::Vector<std::unique_ptr<SdEntry>>& get_entries() const { return entries_; }
    void add_entry(std::unique_ptr<SdEntry> entry);

    const platform::Vector<std::unique_ptr<SdOption>>& get_options() const { return options_; }
    void add_option(std::unique_ptr<SdOption> option);

    platform::ByteBuffer serialize() const;
    bool deserialize(const platform::ByteBuffer& data);

    // Helper methods
    bool is_reboot() const { return (static_cast<uint32_t>(flags_) & 0x80U) != 0; }
    bool is_unicast() const { return (static_cast<uint32_t>(flags_) & 0x40U) != 0; }

    uint16_t get_session_id() const { return session_id_; }
    void set_session_id(uint16_t id) { session_id_ = id; }

    bool get_reboot_flag() const { return is_reboot(); }

    void set_reboot(bool reboot) {
        if (reboot) {
            flags_ = static_cast<uint8_t>(static_cast<uint32_t>(flags_) | 0x80U);
        } else {
            flags_ = static_cast<uint8_t>(static_cast<uint32_t>(flags_) & ~0x80U);
        }
    }

    void set_unicast(bool unicast) {
        if (unicast) {
            flags_ = static_cast<uint8_t>(static_cast<uint32_t>(flags_) | 0x40U);
        } else {
            flags_ = static_cast<uint8_t>(static_cast<uint32_t>(flags_) & ~0x40U);
        }
    }

private:
    uint8_t flags_{0};
    uint32_t reserved_{0};
    uint16_t session_id_{0};

    platform::Vector<std::unique_ptr<SdEntry>> entries_;
    platform::Vector<std::unique_ptr<SdOption>> options_;
};

// Type aliases for convenience
using SdEntryPtr = std::unique_ptr<SdEntry>;
using SdOptionPtr = std::unique_ptr<SdOption>;
using ServiceEntryPtr = std::unique_ptr<ServiceEntry>;
using EventGroupEntryPtr = std::unique_ptr<EventGroupEntry>;
using IPv4EndpointOptionPtr = std::unique_ptr<IPv4EndpointOption>;
using IPv4MulticastOptionPtr = std::unique_ptr<IPv4MulticastOption>;

}  // namespace someip::sd

#endif // SOMEIP_SD_MESSAGE_H
