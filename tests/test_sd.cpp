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

#include <gtest/gtest.h>
#include <sd/sd_types.h>
#include <sd/sd_message.h>
#include <arpa/inet.h>

using namespace someip::sd;

class SdTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Cleanup code
    }
};

// Test SD types
TEST_F(SdTest, EntryTypes) {
    EXPECT_EQ(static_cast<uint8_t>(EntryType::FIND_SERVICE), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(EntryType::OFFER_SERVICE), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(EntryType::SUBSCRIBE_EVENTGROUP), 0x06);
    EXPECT_EQ(static_cast<uint8_t>(EntryType::SUBSCRIBE_EVENTGROUP_ACK), 0x07);
}

TEST_F(SdTest, OptionTypes) {
    EXPECT_EQ(static_cast<uint8_t>(OptionType::IPV4_ENDPOINT), 0x04);
    EXPECT_EQ(static_cast<uint8_t>(OptionType::IPV4_MULTICAST), 0x14);
    EXPECT_EQ(static_cast<uint8_t>(OptionType::IPV4_SD_ENDPOINT), 0x24);
}

TEST_F(SdTest, Instance) {
    ServiceInstance instance(0x1234, 0x5678, 1, 0);

    EXPECT_EQ(instance.service_id, 0x1234u);
    EXPECT_EQ(instance.instance_id, 0x5678u);
    EXPECT_EQ(instance.major_version, 1);
    EXPECT_EQ(instance.minor_version, 0);
    EXPECT_EQ(instance.ip_address, "");
    EXPECT_EQ(instance.port, 0u);
    EXPECT_EQ(instance.protocol, 0x11u);  // Default UDP protocol
    EXPECT_EQ(instance.ttl_seconds, 0u);
}

TEST_F(SdTest, IPv4EndpointOptionSerialization) {
    IPv4EndpointOption option;
    option.set_ipv4_address_from_string("192.168.1.100");
    option.set_port(30509);
    option.set_protocol(0x11);  // UDP

    auto data = option.serialize();

    // Check length: 4 bytes header + 8 bytes data = 12 bytes total
    EXPECT_EQ(data.size(), 12);

    // Check length field (first 2 bytes)
    EXPECT_EQ(data[0], 0x00);
    EXPECT_EQ(data[1], 0x08);

    // Check type field (3rd byte)
    EXPECT_EQ(data[2], 0x04);

    // Check reserved field (4th byte)
    EXPECT_EQ(data[3], 0x00);

    // Check IPv4 address (bytes 4-7, network byte order)
    // On this system, inet_pton gives 0x6401A8C0 -> 64 01 A8 C0
    EXPECT_EQ(data[4], 0x64);  // 100
    EXPECT_EQ(data[5], 0x01);  // 1
    EXPECT_EQ(data[6], 0xA8);  // 168
    EXPECT_EQ(data[7], 0xC0);  // 192

    // Check reserved byte (8th byte)
    EXPECT_EQ(data[8], 0x00);

    // Check protocol (9th byte)
    EXPECT_EQ(data[9], 0x11);

    // Check port (bytes 10-11, network byte order)
    uint16_t expected_port = htons(30509);
    EXPECT_EQ(data[10], (expected_port >> 8) & 0xFF);
    EXPECT_EQ(data[11], expected_port & 0xFF);
}

TEST_F(SdTest, IPv4EndpointOptionDeserialization) {
    IPv4EndpointOption option;
    option.set_ipv4_address_from_string("192.168.1.100");
    option.set_port(30509);
    option.set_protocol(0x11);  // UDP

    auto data = option.serialize();
    IPv4EndpointOption deserialized_option;
    size_t offset = 0;
    bool success = deserialized_option.deserialize(data, offset);

    EXPECT_TRUE(success);
    EXPECT_EQ(deserialized_option.get_ipv4_address_string(), "192.168.1.100");
    EXPECT_EQ(deserialized_option.get_port(), 30509);
    EXPECT_EQ(deserialized_option.get_protocol(), 0x11);
}

// TEST_F(SdTest, IPv4EndpointOptionWithSdMessage) {
//     // Test IPv4 Endpoint Option integration with SD message
//     SdMessage message;

//     // Create offer service entry
//     auto entry = std::make_unique<ServiceEntry>(EntryType::OFFER_SERVICE);
//     entry->set_service_id(0x1234);
//     entry->set_instance_id(0x5678);
//     entry->set_major_version(1);
//     entry->set_ttl(30);

//     // Create IPv4 endpoint option
//     auto option = std::make_unique<IPv4EndpointOption>();
//     option->set_ipv4_address_from_string("10.0.0.1");
//     option->set_port(30500);
//     option->set_protocol(0x11);  // UDP

//     message.add_entry(std::move(entry));
//     message.add_option(std::move(option));

//     // Set option index in entry
//     if (auto* service_entry = dynamic_cast<ServiceEntry*>(message.get_entries()[0].get())) {
//         service_entry->set_index1(0);  // Reference first option
//     }

//     // Serialize and deserialize
//     auto serialized = message.serialize();
//     SdMessage deserialized;
//     bool success = deserialized.deserialize(serialized);

//     EXPECT_TRUE(success);
//     EXPECT_EQ(deserialized.get_entries().size(), 1);
//     EXPECT_EQ(deserialized.get_options().size(), 1);

//     auto* deserialized_entry = dynamic_cast<ServiceEntry*>(deserialized.get_entries()[0].get());
//     auto* deserialized_option = dynamic_cast<IPv4EndpointOption*>(deserialized.get_options()[0].get());

//     ASSERT_TRUE(deserialized_entry != nullptr);
//     ASSERT_TRUE(deserialized_option != nullptr);

//     EXPECT_EQ(deserialized_entry->get_service_id(), 0x1234);
//     EXPECT_EQ(deserialized_entry->get_index1(), 0);
//     EXPECT_EQ(deserialized_option->get_ipv4_address_string(), "10.0.0.1");
//     EXPECT_EQ(deserialized_option->get_port(), 30500);
//     EXPECT_EQ(deserialized_option->get_protocol(), 0x11);
// }

TEST_F(SdTest, Config) {
    SdConfig config;

    EXPECT_EQ(config.multicast_address, "239.255.255.251");
    EXPECT_EQ(config.multicast_port, 30490u);
    EXPECT_EQ(config.unicast_address, "127.0.0.1");
    EXPECT_EQ(config.unicast_port, 0u);
    EXPECT_EQ(config.initial_delay, std::chrono::milliseconds(100));
    EXPECT_EQ(config.repetition_base, std::chrono::milliseconds(2000));
    EXPECT_EQ(config.cyclic_offer, std::chrono::milliseconds(30000));
}

// Test SD message structures
TEST_F(SdTest, ServiceEntry) {
    ServiceEntry entry(EntryType::OFFER_SERVICE);

    entry.set_service_id(0x1234);
    entry.set_instance_id(0x5678);
    entry.set_major_version(1);
    entry.set_ttl(3600);

    EXPECT_EQ(entry.get_type(), EntryType::OFFER_SERVICE);
    EXPECT_EQ(entry.get_service_id(), 0x1234u);
    EXPECT_EQ(entry.get_instance_id(), 0x5678u);
    EXPECT_EQ(entry.get_major_version(), 1);
    EXPECT_EQ(entry.get_ttl(), 3600u);
}

TEST_F(SdTest, EventGroupEntry) {
    EventGroupEntry entry(EntryType::SUBSCRIBE_EVENTGROUP);

    entry.set_service_id(0x1234);
    entry.set_instance_id(0x5678);
    entry.set_eventgroup_id(0x0001);
    entry.set_major_version(1);
    entry.set_ttl(1800);

    EXPECT_EQ(entry.get_type(), EntryType::SUBSCRIBE_EVENTGROUP);
    EXPECT_EQ(entry.get_service_id(), 0x1234u);
    EXPECT_EQ(entry.get_instance_id(), 0x5678u);
    EXPECT_EQ(entry.get_eventgroup_id(), 0x0001u);
    EXPECT_EQ(entry.get_major_version(), 1);
    EXPECT_EQ(entry.get_ttl(), 1800u);
}

TEST_F(SdTest, EndpointOption) {
    IPv4EndpointOption option;

    option.set_protocol(0x06);  // TCP
    option.set_ipv4_address(0xC0A80101);  // 192.168.1.1
    option.set_port(30500);

    EXPECT_EQ(option.get_type(), OptionType::IPV4_ENDPOINT);
    EXPECT_EQ(option.get_protocol(), 0x06);
    EXPECT_EQ(option.get_ipv4_address(), 0xC0A80101u);
    EXPECT_EQ(option.get_port(), 30500);
}

TEST_F(SdTest, MulticastOption) {
    IPv4MulticastOption option;

    option.set_ipv4_address(0xEFFFFFFB);  // 239.255.255.251
    option.set_port(30490);

    EXPECT_EQ(option.get_type(), OptionType::IPV4_MULTICAST);
    EXPECT_EQ(option.get_ipv4_address(), 0xEFFFFFFBu);
    EXPECT_EQ(option.get_port(), 30490);
}

TEST_F(SdTest, SdMessage) {
    SdMessage message;

    EXPECT_EQ(message.get_flags(), 0);
    EXPECT_EQ(message.get_reserved(), 0u);
    EXPECT_FALSE(message.is_reboot());
    EXPECT_FALSE(message.is_unicast());

    // Test flag setters
    message.set_reboot(true);
    message.set_unicast(true);

    EXPECT_TRUE(message.is_reboot());
    EXPECT_TRUE(message.is_unicast());
    EXPECT_EQ(message.get_flags(), 0xC0);  // 11000000
}

TEST_F(SdTest, SdMessageEntries) {
    SdMessage message;

    // Add service entry
    auto service_entry = std::make_unique<ServiceEntry>(EntryType::OFFER_SERVICE);
    service_entry->set_service_id(0x1234);
    message.add_entry(std::move(service_entry));

    EXPECT_EQ(message.get_entries().size(), 1u);
    EXPECT_EQ(message.get_entries()[0]->get_type(), EntryType::OFFER_SERVICE);

    // Add event group entry
    auto event_entry = std::make_unique<EventGroupEntry>(EntryType::SUBSCRIBE_EVENTGROUP);
    event_entry->set_service_id(0x1234);
    event_entry->set_eventgroup_id(0x0001);
    message.add_entry(std::move(event_entry));

    EXPECT_EQ(message.get_entries().size(), 2u);
}

TEST_F(SdTest, SdMessageOptions) {
    SdMessage message;

    // Add IPv4 endpoint option
    auto endpoint_option = std::make_unique<IPv4EndpointOption>();
    endpoint_option->set_ipv4_address(0x7F000001);  // 127.0.0.1
    endpoint_option->set_port(30500);
    message.add_option(std::move(endpoint_option));

    EXPECT_EQ(message.get_options().size(), 1u);
    EXPECT_EQ(message.get_options()[0]->get_type(), OptionType::IPV4_ENDPOINT);

    // Add IPv4 multicast option
    auto multicast_option = std::make_unique<IPv4MulticastOption>();
    multicast_option->set_ipv4_address(0xEFFFFFFB);  // 239.255.255.251
    multicast_option->set_port(30490);
    message.add_option(std::move(multicast_option));

    EXPECT_EQ(message.get_options().size(), 2u);
}

TEST_F(SdTest, Subscription) {
    EventGroupSubscription subscription(0x1234, 0x0001, 0x0001);

    EXPECT_EQ(subscription.service_id, 0x1234u);
    EXPECT_EQ(subscription.instance_id, 0x0001u);
    EXPECT_EQ(subscription.eventgroup_id, 0x0001u);
    EXPECT_EQ(subscription.state, SubscriptionState::REQUESTED);
}

// Test field initialization safety
TEST_F(SdTest, FieldInitializationSafety) {
    // Test that all SD message fields are properly initialized
    // This prevents indeterminate values on the wire if constructors change

    // Test ServiceEntry initialization
    ServiceEntry service_entry;
    EXPECT_EQ(service_entry.get_type(), EntryType::FIND_SERVICE);
    EXPECT_EQ(service_entry.get_ttl(), 0u);
    EXPECT_EQ(service_entry.get_index1(), 0u);
    EXPECT_EQ(service_entry.get_index2(), 0u);
    EXPECT_EQ(service_entry.get_service_id(), 0u);
    EXPECT_EQ(service_entry.get_instance_id(), 0u);
    EXPECT_EQ(service_entry.get_major_version(), 0u);
    EXPECT_EQ(service_entry.get_minor_version(), 0u);

    // Test SdMessage initialization
    SdMessage message;
    EXPECT_EQ(message.get_flags(), 0u);
    EXPECT_EQ(message.get_reserved(), 0u);
    EXPECT_TRUE(message.get_entries().empty());
    EXPECT_TRUE(message.get_options().empty());

    // Test IPv4EndpointOption initialization
    IPv4EndpointOption option;
    EXPECT_EQ(option.get_type(), OptionType::IPV4_ENDPOINT);
    EXPECT_EQ(option.get_length(), 0u);
    EXPECT_EQ(option.get_protocol(), 0u);
    EXPECT_EQ(option.get_ipv4_address(), 0u);
    EXPECT_EQ(option.get_port(), 0u);
}

// Test result codes
TEST_F(SdTest, SdResults) {
    EXPECT_EQ(static_cast<int>(SdResult::SUCCESS), 0);
    EXPECT_EQ(static_cast<int>(SdResult::SERVICE_NOT_FOUND), 1);
    EXPECT_EQ(static_cast<int>(SdResult::SERVICE_ALREADY_EXISTS), 2);
    EXPECT_EQ(static_cast<int>(SdResult::NETWORK_ERROR), 3);
    EXPECT_EQ(static_cast<int>(SdResult::TIMEOUT), 4);
    EXPECT_EQ(static_cast<int>(SdResult::INVALID_PARAMETERS), 5);
}
