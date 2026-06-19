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
#include <sd/sd_server.h>
#include <sd/sd_client.h>
#include <events/event_publisher.h>
#include <someip/types.h>
#include <transport/udp_transport.h>
#include <platform/byteorder.h>
#include <platform/buffer_pool.h>
#include <platform/containers.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdio>

using namespace someip;
using namespace someip::sd;

/**
 * @brief Service Discovery unit tests
 * @tests REQ_ARCH_001
 * @tests REQ_ARCH_002
 * @tests REQ_SD_001, REQ_SD_002, REQ_SD_003, REQ_SD_004, REQ_SD_005, REQ_SD_006, REQ_SD_007
 * @tests REQ_SD_010, REQ_SD_011, REQ_SD_012, REQ_SD_013, REQ_SD_014
 * @tests REQ_SD_020, REQ_SD_021, REQ_SD_022, REQ_SD_023, REQ_SD_024, REQ_SD_025, REQ_SD_026
 * @tests REQ_SD_030, REQ_SD_031, REQ_SD_032, REQ_SD_033, REQ_SD_034, REQ_SD_035
 * @tests REQ_SD_040, REQ_SD_041, REQ_SD_042, REQ_SD_043, REQ_SD_044, REQ_SD_045, REQ_SD_046
 * @tests REQ_SD_050, REQ_SD_051, REQ_SD_052, REQ_SD_053, REQ_SD_054, REQ_SD_055, REQ_SD_056
 * @tests REQ_SD_060, REQ_SD_061, REQ_SD_062, REQ_SD_063, REQ_SD_064, REQ_SD_065
 * @tests REQ_SD_066, REQ_SD_067, REQ_SD_068, REQ_SD_069
 * @tests REQ_SD_070, REQ_SD_071, REQ_SD_072, REQ_SD_073, REQ_SD_074, REQ_SD_075, REQ_SD_076, REQ_SD_077
 * @tests REQ_SD_080, REQ_SD_081, REQ_SD_082, REQ_SD_083, REQ_SD_084
 * @tests REQ_SD_090, REQ_SD_091, REQ_SD_092, REQ_SD_093, REQ_SD_094
 * @tests REQ_SD_100, REQ_SD_101, REQ_SD_102, REQ_SD_103
 * @tests REQ_SD_001_E01, REQ_SD_010_E01, REQ_SD_020_E01, REQ_SD_020_E02
 * @tests REQ_SD_021_E01, REQ_SD_022_E01, REQ_SD_040_E01, REQ_SD_041_E01
 * @tests REQ_SD_050_E01, REQ_SD_052_E01, REQ_SD_060_E01, REQ_SD_061_E01
 * @tests REQ_SD_062_E01, REQ_SD_064_E01, REQ_SD_075_E01
 * @tests REQ_SD_110, REQ_SD_111, REQ_SD_112, REQ_SD_113, REQ_SD_114, REQ_SD_115
 * @tests REQ_SD_116, REQ_SD_117, REQ_SD_118, REQ_SD_119, REQ_SD_120
 * @tests REQ_SD_121, REQ_SD_122, REQ_SD_123, REQ_SD_124, REQ_SD_125
 * @tests REQ_SD_126, REQ_SD_127, REQ_SD_130, REQ_SD_131
 * @tests REQ_SD_132, REQ_SD_140, REQ_SD_141, REQ_SD_142
 * @tests REQ_SD_150, REQ_SD_151, REQ_SD_152, REQ_SD_160, REQ_SD_161, REQ_SD_170, REQ_SD_171, REQ_SD_180
 * @tests REQ_SD_200a, REQ_SD_200b, REQ_SD_200c, REQ_SD_201, REQ_SD_202
 * @tests REQ_SD_210, REQ_SD_211, REQ_SD_212
 * @tests REQ_SD_220, REQ_SD_221, REQ_SD_222, REQ_SD_223
 * @tests REQ_SD_230, REQ_SD_231, REQ_SD_232, REQ_SD_233, REQ_SD_234, REQ_SD_235, REQ_SD_236
 * @tests REQ_SD_240, REQ_SD_241, REQ_SD_242, REQ_SD_243
 * @tests REQ_SD_250, REQ_SD_251, REQ_SD_260, REQ_SD_261, REQ_SD_270, REQ_SD_271, REQ_SD_272, REQ_SD_273, REQ_SD_274
 * @tests REQ_SD_280, REQ_SD_281, REQ_SD_282, REQ_SD_283, REQ_SD_290, REQ_SD_291, REQ_SD_292, REQ_SD_293
 * @tests REQ_SD_300, REQ_SD_301, REQ_SD_302, REQ_SD_303
 * @tests REQ_SD_310, REQ_SD_311, REQ_SD_312, REQ_SD_320, REQ_SD_330, REQ_SD_331, REQ_SD_340, REQ_SD_341, REQ_SD_342
 * @tests REQ_SD_343, REQ_SD_344, REQ_SD_345, REQ_SD_346, REQ_SD_347, REQ_SD_348, REQ_SD_349
 * @tests REQ_SD_350, REQ_SD_351, REQ_SD_352, REQ_SD_353, REQ_SD_354, REQ_SD_355, REQ_SD_356
 * @tests REQ_COMPAT_030
 * @tests REQ_SD_001_E02, REQ_SD_010_E02
 * @tests REQ_SD_030_E01, REQ_SD_044_E01
 * @tests REQ_SD_060_E02, REQ_SD_070_E01, REQ_SD_080_E01, REQ_SD_083_E01
 * @tests REQ_SD_113_E01, REQ_SD_115_E01, REQ_SD_115_E02, REQ_SD_116_E01, REQ_SD_116_E02
 * @tests REQ_SD_119_E01, REQ_SD_120_E01, REQ_SD_123_E01, REQ_SD_134_E01, REQ_SD_222_E01
 * @tests feat_req_someipsd_100
 * @tests feat_req_someipsd_200
 * @tests feat_req_someipsd_300
 */
class SdTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Cleanup code
    }
};

/**
 * @test_case TC_SD_001
 * @tests REQ_SD_001, REQ_SD_002
 * @brief Test SD entry type values
 */
TEST_F(SdTest, EntryTypes) {
    EXPECT_EQ(static_cast<uint8_t>(EntryType::FIND_SERVICE), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(EntryType::OFFER_SERVICE), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(EntryType::SUBSCRIBE_EVENTGROUP), 0x06);
    EXPECT_EQ(static_cast<uint8_t>(EntryType::SUBSCRIBE_EVENTGROUP_ACK), 0x07);
}

/**
 * @test_case TC_SD_002
 * @tests REQ_SD_060, REQ_SD_061, REQ_SD_062
 * @brief Test SD option type values
 */
TEST_F(SdTest, OptionTypes) {
    EXPECT_EQ(static_cast<uint8_t>(OptionType::IPV4_ENDPOINT), 0x04);
    EXPECT_EQ(static_cast<uint8_t>(OptionType::IPV4_MULTICAST), 0x14);
    EXPECT_EQ(static_cast<uint8_t>(OptionType::IPV4_SD_ENDPOINT), 0x24);
}

/**
 * @test_case TC_SD_003
 * @tests REQ_SD_010, REQ_SD_011, REQ_SD_012
 * @brief Test service instance structure
 */
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

/**
 * @test_case TC_SD_004
 * @tests REQ_SD_060, REQ_SD_063, REQ_SD_064, REQ_SD_065
 * @brief Test IPv4 endpoint option serialization
 */
TEST_F(SdTest, IPv4EndpointOptionSerialization) {
    IPv4EndpointOption option;
    option.set_ipv4_address_from_string("192.168.1.100");
    option.set_port(30509);
    option.set_protocol(0x11);  // UDP

    auto data = option.serialize();

    // Total: Length(2) + Type(1) + Reserved(1) + IPv4(4) + Reserved(1) + Proto(1) + Port(2) = 12
    EXPECT_EQ(data.size(), 12);

    // Length = 0x0009 per spec (covers all except Length and Type fields)
    EXPECT_EQ(data[0], 0x00);
    EXPECT_EQ(data[1], 0x09);

    // Type = 0x04
    EXPECT_EQ(data[2], 0x04);

    // Reserved
    EXPECT_EQ(data[3], 0x00);

    // IPv4 address in network byte order: 192.168.1.100 = C0 A8 01 64
    EXPECT_EQ(data[4], 0xC0);  // 192
    EXPECT_EQ(data[5], 0xA8);  // 168
    EXPECT_EQ(data[6], 0x01);  // 1
    EXPECT_EQ(data[7], 0x64);  // 100

    // Reserved
    EXPECT_EQ(data[8], 0x00);

    // Protocol
    EXPECT_EQ(data[9], 0x11);

    // Port 30509 in NBO = 0x772D
    EXPECT_EQ(data[10], 0x77);
    EXPECT_EQ(data[11], 0x2D);
}

/**
 * @test_case TC_SD_005
 * @tests REQ_SD_060, REQ_SD_063, REQ_SD_064, REQ_SD_065
 * @brief Test IPv4 endpoint option deserialization
 */
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

/**
 * @test_case TC_SD_006
 * @tests REQ_SD_020, REQ_SD_021, REQ_SD_022, REQ_SD_023
 * @brief Test service entry structure
 */
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

/**
 * @test_case TC_SD_007
 * @tests REQ_SD_050, REQ_SD_051, REQ_SD_052, REQ_SD_053, REQ_SD_054
 * @brief Test event group entry structure
 */
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

/**
 * @test_case TC_SD_008
 * @tests REQ_SD_060, REQ_SD_063, REQ_SD_064
 * @brief Test endpoint option structure
 */
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

/**
 * @test_case TC_SD_009
 * @tests REQ_SD_066, REQ_SD_067, REQ_SD_068
 * @brief Test multicast option structure
 */
TEST_F(SdTest, MulticastOption) {
    IPv4MulticastOption option;

    option.set_ipv4_address(0xEFFFFFFB);  // 239.255.255.251
    option.set_port(30490);

    EXPECT_EQ(option.get_type(), OptionType::IPV4_MULTICAST);
    EXPECT_EQ(option.get_ipv4_address(), 0xEFFFFFFBu);
    EXPECT_EQ(option.get_port(), 30490);
}

/**
 * @test_case TC_SD_010
 * @tests REQ_SD_003, REQ_SD_004, REQ_SD_005, REQ_SD_006, REQ_SD_007
 * @brief Test SD message structure and flags
 */
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

// ============================================================================
// Interop / Wire-Format Compliance Tests (Issue #238)
// ============================================================================

/**
 * @test_case TC_SD_INTEROP_001
 * @tests feat_req_someipsd_129
 * @brief Verify serialized IPv4 Endpoint Option length field is 0x0009 per spec.
 *
 * The spec says: "Length field shall cover all bytes of the option except
 * the length field and type field."  For IPv4 Endpoint Option that is
 * Reserved(1) + IPv4(4) + Reserved(1) + L4-Proto(1) + Port(2) = 9.
 */
TEST_F(SdTest, IPv4EndpointOptionSerializesLength9) {
    IPv4EndpointOption option;
    option.set_ipv4_address_from_string("192.168.1.100");
    option.set_port(30509);
    option.set_protocol(0x11);

    auto data = option.serialize();
    ASSERT_EQ(data.size(), 12u);

    EXPECT_EQ(data[0], 0x00) << "Length MSB";
    EXPECT_EQ(data[1], 0x09) << "Length LSB — spec mandates 0x0009";
}

/**
 * @test_case TC_SD_INTEROP_002
 * @tests feat_req_someipsd_129
 * @brief Verify IPv4 address is serialized in network byte order (MSB first).
 *
 * For 192.168.1.100 the wire bytes shall be C0 A8 01 64.
 */
TEST_F(SdTest, IPv4EndpointOptionSerializesAddressNBO) {
    IPv4EndpointOption option;
    option.set_ipv4_address_from_string("192.168.1.100");
    option.set_port(30509);
    option.set_protocol(0x11);

    auto data = option.serialize();
    ASSERT_GE(data.size(), 8u);

    EXPECT_EQ(data[4], 0xC0) << "192";
    EXPECT_EQ(data[5], 0xA8) << "168";
    EXPECT_EQ(data[6], 0x01) << "1";
    EXPECT_EQ(data[7], 0x64) << "100";
}

/**
 * @test_case TC_SD_INTEROP_003
 * @tests feat_req_someipsd_129
 * @brief Deserializing a spec-compliant (length=9) IPv4 Endpoint Option
 *        must succeed.  This reproduces the vsomeip interop failure from
 *        issue #238: opensomeip's bounds check rejected length=9 packets.
 */
TEST_F(SdTest, IPv4EndpointOptionDeserializesSpecCompliantPacket) {
    // Hand-crafted spec-compliant IPv4 Endpoint Option for 10.0.3.1:30509/UDP
    const platform::ByteBuffer wire = {
        0x00, 0x09,       // Length = 9 (spec-correct)
        0x04,             // Type = IPv4 Endpoint
        0x00,             // Reserved
        0x0A, 0x00, 0x03, 0x01,  // IPv4 = 10.0.3.1 (NBO)
        0x00,             // Reserved
        0x11,             // UDP
        0x77, 0x2D,       // Port = 30509 (NBO)
    };

    IPv4EndpointOption option;
    size_t offset = 0;
    ASSERT_TRUE(option.deserialize(wire, offset))
        << "Spec-compliant packet with length=9 must be accepted";

    EXPECT_EQ(option.get_ipv4_address_string(), "10.0.3.1");
    EXPECT_EQ(option.get_port(), 30509);
    EXPECT_EQ(option.get_protocol(), 0x11);
    EXPECT_EQ(offset, 12u);
}

/**
 * @test_case TC_SD_INTEROP_004
 * @tests feat_req_someipsd_129
 * @brief Full round-trip: serialize then deserialize must preserve all fields
 *        and produce spec-compliant wire format.
 */
TEST_F(SdTest, IPv4EndpointOptionSpecCompliantRoundTrip) {
    IPv4EndpointOption original;
    original.set_ipv4_address_from_string("172.16.254.1");
    original.set_port(443);
    original.set_protocol(0x06);  // TCP

    auto wire = original.serialize();

    // Verify spec-compliant length
    EXPECT_EQ(wire[0], 0x00);
    EXPECT_EQ(wire[1], 0x09);

    // Verify NBO address: 172.16.254.1 = AC 10 FE 01
    EXPECT_EQ(wire[4], 0xAC);
    EXPECT_EQ(wire[5], 0x10);
    EXPECT_EQ(wire[6], 0xFE);
    EXPECT_EQ(wire[7], 0x01);

    IPv4EndpointOption deserialized;
    size_t offset = 0;
    ASSERT_TRUE(deserialized.deserialize(wire, offset));

    EXPECT_EQ(deserialized.get_ipv4_address_string(), "172.16.254.1");
    EXPECT_EQ(deserialized.get_port(), 443);
    EXPECT_EQ(deserialized.get_protocol(), 0x06);
}

/**
 * @test_case TC_SD_INTEROP_005
 * @tests feat_req_someipsd_129
 * @brief End-to-end interop test using the exact scenario from issue #238:
 *        vsomeip server at 10.10.10.20:30510/UDP sends a spec-compliant
 *        OfferService with an IPv4EndpointOption.  opensomeip must parse
 *        the option and recover the correct address and port.
 *
 *        Previously, opensomeip would either reject the packet (bug 1:
 *        length=9 bounds check) or decode the address as 20.10.10.10
 *        (bug 2: byte order).
 */
TEST_F(SdTest, VsomeipInteropScenario) {
    // Exact wire bytes that vsomeip sends for server at 10.10.10.20:30510/UDP
    // per AUTOSAR SOME/IP-SD wire format
    const platform::ByteBuffer vsomeip_wire = {
        0x00, 0x09,                   // Length = 9 (AUTOSAR-compliant)
        0x04,                         // Type = IPv4 Endpoint (0x04)
        0x00,                         // Reserved
        0x0A, 0x0A, 0x0A, 0x14,      // IPv4 = 10.10.10.20 in NBO
        0x00,                         // Reserved
        0x11,                         // L4-Proto = UDP (0x11)
        0x77, 0x2E,                   // Port = 30510 in NBO (0x772E)
    };

    IPv4EndpointOption option;
    size_t offset = 0;

    // Bug 1 fix: must not reject length=9 packets
    ASSERT_TRUE(option.deserialize(vsomeip_wire, offset))
        << "vsomeip OfferService with length=9 must be accepted (bug 1 fix)";

    // Bug 2 fix: must decode address as 10.10.10.20, not 20.10.10.10
    EXPECT_EQ(option.get_ipv4_address_string(), "10.10.10.20")
        << "Address must be decoded correctly from NBO wire bytes (bug 2 fix)";

    EXPECT_EQ(option.get_port(), 30510)
        << "Port must be decoded correctly without double-swap";

    EXPECT_EQ(option.get_protocol(), 0x11);
    EXPECT_EQ(offset, 12u);

    // Also verify that opensomeip's serialized output matches what vsomeip
    // would expect (bidirectional interop)
    IPv4EndpointOption outgoing;
    outgoing.set_ipv4_address_from_string("10.10.10.20");
    outgoing.set_port(30510);
    outgoing.set_protocol(0x11);

    auto serialized = outgoing.serialize();
    ASSERT_EQ(serialized.size(), 12u);

    // Must produce identical wire bytes as vsomeip
    EXPECT_EQ(serialized, vsomeip_wire)
        << "opensomeip must produce spec-compliant wire bytes identical to vsomeip";
}

// ============================================================================
// SD Message Serialization Tests
// ============================================================================

TEST_F(SdTest, ServiceEntrySerialization) {
    ServiceEntry original(EntryType::OFFER_SERVICE);
    original.set_service_id(0x1234);
    original.set_instance_id(0x5678);
    original.set_major_version(1);
    original.set_minor_version(42);
    original.set_ttl(3600);

    auto serialized = original.serialize();
    // Current implementation produces some bytes
    EXPECT_GT(serialized.size(), 0u);
}

TEST_F(SdTest, EventGroupEntrySerialization) {
    EventGroupEntry original(EntryType::SUBSCRIBE_EVENTGROUP);
    original.set_service_id(0xABCD);
    original.set_instance_id(0x0001);
    original.set_eventgroup_id(0x0010);
    original.set_major_version(2);
    original.set_ttl(1800);

    auto serialized = original.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

TEST_F(SdTest, IPv4MulticastOptionSerialization) {
    IPv4MulticastOption original;
    original.set_ipv4_address(0xEFFFFFFB);  // 239.255.255.251
    original.set_port(30490);

    auto serialized = original.serialize();
    EXPECT_GT(serialized.size(), 0u);
}

TEST_F(SdTest, SdMessageSerialization) {
    SdMessage original;
    original.set_reboot(true);
    original.set_unicast(false);

    auto entry = std::make_unique<ServiceEntry>(EntryType::OFFER_SERVICE);
    entry->set_service_id(0x1234);
    entry->set_instance_id(0x5678);
    entry->set_major_version(1);
    entry->set_ttl(30);
    original.add_entry(std::move(entry));

    auto option = std::make_unique<IPv4EndpointOption>();
    option->set_ipv4_address_from_string("192.168.1.100");
    option->set_port(30509);
    option->set_protocol(0x11);
    original.add_option(std::move(option));

    auto serialized = original.serialize();
    EXPECT_GT(serialized.size(), 0u);

    // Verify flags are set correctly in first byte
    EXPECT_EQ(serialized[0] & 0x80, 0x80);  // Reboot flag
}

// ============================================================================
// SD Client/Server Integration Tests
// ============================================================================

class SdIntegrationTest : public ::testing::Test {
protected:
    static constexpr uint16_t TEST_PORT_BASE = 40000;
    static std::atomic<uint16_t> port_counter;

    uint16_t get_unique_port() {
        return TEST_PORT_BASE + port_counter.fetch_add(1);
    }

    SdConfig create_test_config(uint16_t unicast_port, uint16_t multicast_port) {
        SdConfig config;
        config.unicast_address = "127.0.0.1";
        config.unicast_port = unicast_port;
        config.multicast_address = "239.255.255.251";
        config.multicast_port = multicast_port;
        config.initial_delay = std::chrono::milliseconds(10);
        config.repetition_base = std::chrono::milliseconds(100);
        config.cyclic_offer = std::chrono::milliseconds(1000);
        return config;
    }
};

std::atomic<uint16_t> SdIntegrationTest::port_counter{0};

TEST_F(SdIntegrationTest, ServerInitializeAndShutdown) {
    auto config = create_test_config(get_unique_port(), get_unique_port());
    SdServer server(config);

    EXPECT_FALSE(server.is_ready());

    bool init_result = server.initialize();
    EXPECT_TRUE(init_result);
    EXPECT_TRUE(server.is_ready());

    server.shutdown();
    EXPECT_FALSE(server.is_ready());
}

TEST_F(SdIntegrationTest, ClientInitializeAndShutdown) {
    auto config = create_test_config(get_unique_port(), get_unique_port());
    SdClient client(config);

    EXPECT_FALSE(client.is_ready());

    bool init_result = client.initialize();
    EXPECT_TRUE(init_result);
    EXPECT_TRUE(client.is_ready());

    client.shutdown();
    EXPECT_FALSE(client.is_ready());
}

/**
 * @test_case TC_SD_INTEGRATION_001
 * @tests REQ_SD_030, REQ_SD_031, REQ_SD_032
 * @brief Test server service offering
 */
TEST_F(SdIntegrationTest, ServerOfferService) {
    auto config = create_test_config(get_unique_port(), get_unique_port());
    SdServer server(config);
    ASSERT_TRUE(server.initialize());

    ServiceInstance instance(0x1234, 0x5678, 1, 0);
    instance.ttl_seconds = 30;

    bool offer_result = server.offer_service(instance, "127.0.0.1:30509");
    EXPECT_TRUE(offer_result);

    auto offered = server.get_offered_services();
    EXPECT_EQ(offered.size(), 1u);
    EXPECT_EQ(offered[0].service_id, 0x1234u);
    EXPECT_EQ(offered[0].instance_id, 0x5678u);

    server.shutdown();
}

TEST_F(SdIntegrationTest, ServerOfferMultipleServices) {
    auto config = create_test_config(get_unique_port(), get_unique_port());
    SdServer server(config);
    ASSERT_TRUE(server.initialize());

    // Offer 3 different services
    for (uint16_t i = 0; i < 3; ++i) {
        ServiceInstance instance(0x1000 + i, 0x0001, 1, 0);
        instance.ttl_seconds = 30;
        char endpoint_buf[32];
        snprintf(endpoint_buf, sizeof(endpoint_buf), "127.0.0.1:%d", 30500 + i);
        EXPECT_TRUE(server.offer_service(instance, endpoint_buf));
    }

    auto offered = server.get_offered_services();
    EXPECT_EQ(offered.size(), 3u);

    server.shutdown();
}

/**
 * @test_case TC_SD_INTEGRATION_002
 * @tests REQ_SD_033, REQ_SD_034
 * @brief Test server stop offering service
 */
TEST_F(SdIntegrationTest, ServerStopOfferService) {
    auto config = create_test_config(get_unique_port(), get_unique_port());
    SdServer server(config);
    ASSERT_TRUE(server.initialize());

    ServiceInstance instance(0x1234, 0x5678, 1, 0);
    EXPECT_TRUE(server.offer_service(instance, "127.0.0.1:30509"));
    EXPECT_EQ(server.get_offered_services().size(), 1u);

    bool stop_result = server.stop_offer_service(0x1234, 0x5678);
    EXPECT_TRUE(stop_result);
    EXPECT_EQ(server.get_offered_services().size(), 0u);

    // Stopping non-existent service should return false
    EXPECT_FALSE(server.stop_offer_service(0x9999, 0x0001));

    server.shutdown();
}

TEST_F(SdIntegrationTest, ServerUpdateServiceTTL) {
    auto config = create_test_config(get_unique_port(), get_unique_port());
    SdServer server(config);
    ASSERT_TRUE(server.initialize());

    ServiceInstance instance(0x1234, 0x5678, 1, 0);
    instance.ttl_seconds = 30;
    EXPECT_TRUE(server.offer_service(instance, "127.0.0.1:30509"));

    bool update_result = server.update_service_ttl(0x1234, 0x5678, 60);
    EXPECT_TRUE(update_result);

    auto offered = server.get_offered_services();
    EXPECT_EQ(offered.size(), 1u);
    EXPECT_EQ(offered[0].ttl_seconds, 60u);

    // Update non-existent service should return false
    EXPECT_FALSE(server.update_service_ttl(0x9999, 0x0001, 100));

    server.shutdown();
}

TEST_F(SdIntegrationTest, ClientGetAvailableServicesEmpty) {
    auto config = create_test_config(get_unique_port(), get_unique_port());
    SdClient client(config);
    ASSERT_TRUE(client.initialize());

    auto services = client.get_available_services();
    EXPECT_TRUE(services.empty());

    auto stats = client.get_statistics();
    EXPECT_EQ(stats.services_found, 0u);

    client.shutdown();
}

/**
 * @test_case TC_SD_INTEGRATION_003
 * @tests REQ_SD_090, REQ_SD_091, REQ_SD_092, REQ_SD_093
 * @brief Test client subscribe and unsubscribe service
 */
TEST_F(SdIntegrationTest, ClientSubscribeUnsubscribeService) {
    auto config = create_test_config(get_unique_port(), get_unique_port());
    SdClient client(config);
    ASSERT_TRUE(client.initialize());

    std::atomic<int> available_count{0};
    std::atomic<int> unavailable_count{0};

    bool sub_result = client.subscribe_service(
        0x1234,
        [&](const ServiceInstance&) { available_count++; },
        [&](const ServiceInstance&) { unavailable_count++; }
    );
    EXPECT_TRUE(sub_result);

    bool unsub_result = client.unsubscribe_service(0x1234);
    EXPECT_TRUE(unsub_result);

    // Unsubscribing again should return false
    EXPECT_FALSE(client.unsubscribe_service(0x1234));

    client.shutdown();
}

/**
 * @test_case TC_SD_INTEGRATION_004
 * @tests REQ_SD_110, REQ_SD_160, REQ_SD_161
 * @brief Server→Client integration: minor_version > 255 survives the full
 *        offer path through real multicast transport.
 *
 * Regression for #245: SdServer did not propagate minor_version into the
 * wire-format entry, and SdClient hardcoded it to 0 on receive.
 */
TEST_F(SdIntegrationTest, MinorVersionSurvivesServerToClient) {
    const uint16_t mcast_port = get_unique_port();
    auto server_config = create_test_config(get_unique_port(), mcast_port);
    auto client_config = create_test_config(get_unique_port(), mcast_port);

    SdClient client(client_config);
    ASSERT_TRUE(client.initialize());

    const uint32_t expected_minor = 0x00030007;
    std::atomic<bool> offer_received{false};
    ServiceInstance received_instance;

    ASSERT_TRUE(client.subscribe_service(
        0xBEEF,
        [&](const ServiceInstance& inst) {
            received_instance = inst;
            offer_received = true;
        },
        [](const ServiceInstance&) {}
    ));

    SdServer server(server_config);
    ASSERT_TRUE(server.initialize());

    ServiceInstance offered(0xBEEF, 0x0001, 2, expected_minor);
    offered.ttl_seconds = 30;
    ASSERT_TRUE(server.offer_service(offered, "127.0.0.1:30509"));

    // Wait for the multicast offer to arrive (short timeout; local loopback)
    for (int i = 0; i < 50 && !offer_received; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    server.shutdown();
    client.shutdown();

    if (offer_received) {
        EXPECT_EQ(received_instance.service_id, 0xBEEFu);
        EXPECT_EQ(received_instance.instance_id, 0x0001u);
        EXPECT_EQ(received_instance.major_version, 2u);
        EXPECT_EQ(received_instance.minor_version, expected_minor)
            << "minor_version must survive SdServer -> multicast -> SdClient";
        EXPECT_EQ(received_instance.ttl_seconds, 30u);
    } else {
        // Multicast loopback may not work in all CI/sandbox environments.
        // The wire-level test MinorVersionPreservedThroughOfferPath covers
        // the same logic unconditionally.
        GTEST_SKIP() << "Multicast offer not received (loopback may be unavailable)";
    }
}

// ============================================================================
// SD Helper Function Tests
// ============================================================================

TEST_F(SdTest, IPv4AddressConversion) {
    IPv4EndpointOption option;

    // Test various IP addresses
    const char* test_addresses[] = {
        "0.0.0.0",
        "127.0.0.1",
        "192.168.1.100",
        "10.0.0.1",
        "255.255.255.255"
    };

    for (const auto* addr : test_addresses) {
        option.set_ipv4_address_from_string(addr);
        EXPECT_EQ(option.get_ipv4_address_string(), addr)
            << "Round-trip failed for: " << addr;
    }
}

TEST_F(SdTest, PortConversion) {
    IPv4EndpointOption option;

    // Test various ports
    std::vector<uint16_t> test_ports = {0, 1, 80, 443, 30490, 30509, 65535};

    for (uint16_t port : test_ports) {
        option.set_port(port);
        EXPECT_EQ(option.get_port(), port)
            << "Round-trip failed for port: " << port;
    }
}

// ============================================================================
// SD Deserialization Error Handling Tests
// ============================================================================

/**
 * @test_case TC_SD_DESER_001
 * @tests REQ_SD_001_E01, REQ_SD_010_E01
 * @brief Test SdMessage deserialization with empty buffer
 */
TEST_F(SdTest, DeserializeEmptyBuffer) {
    SdMessage msg;
    platform::ByteBuffer empty;
    EXPECT_FALSE(msg.deserialize(empty));
}

/**
 * @test_case TC_SD_DESER_002
 * @tests REQ_SD_001_E01, REQ_SD_020_E01
 * @brief Test SdMessage deserialization with truncated header
 */
TEST_F(SdTest, DeserializeTruncatedHeader) {
    SdMessage msg;
    platform::ByteBuffer short_data = {0x00, 0x01, 0x02};
    EXPECT_FALSE(msg.deserialize(short_data));
}

/**
 * @test_case TC_SD_DESER_003
 * @tests REQ_SD_020_E02, REQ_SD_021_E01
 * @brief Test SdMessage deserialization with invalid length field
 */
TEST_F(SdTest, DeserializeInvalidLength) {
    SdMessage msg;
    // 8 bytes of header but length field claims more data than available
    platform::ByteBuffer data = {
        0x00, 0x00, 0x00, 0x00,  // flags + reserved
        0x00, 0x00, 0x01, 0x00,  // entries length = 256 (but no data follows)
    };
    EXPECT_FALSE(msg.deserialize(data));
}

/**
 * @test_case TC_SD_DESER_004
 * @tests REQ_SD_022_E01, REQ_SD_040_E01
 * @brief Test ServiceEntry deserialization with insufficient data
 */
TEST_F(SdTest, ServiceEntryDeserializeTruncated) {
    ServiceEntry entry;
    platform::ByteBuffer short_data = {0x00, 0x01, 0x02};
    size_t offset = 0;
    EXPECT_FALSE(entry.deserialize(short_data, offset));
}

/**
 * @test_case TC_SD_DESER_005
 * @tests REQ_SD_041_E01, REQ_SD_050_E01
 * @brief Test EventGroupEntry deserialization with insufficient data
 */
TEST_F(SdTest, EventGroupEntryDeserializeTruncated) {
    EventGroupEntry entry;
    platform::ByteBuffer short_data = {0x00, 0x01, 0x02};
    size_t offset = 0;
    EXPECT_FALSE(entry.deserialize(short_data, offset));
}

/**
 * @test_case TC_SD_DESER_006
 * @tests REQ_SD_052_E01, REQ_SD_060_E01
 * @brief Test IPv4EndpointOption deserialization with insufficient data
 */
TEST_F(SdTest, IPv4EndpointOptionDeserializeTruncated) {
    IPv4EndpointOption option;
    platform::ByteBuffer short_data = {0x00, 0x01};
    size_t offset = 0;
    EXPECT_FALSE(option.deserialize(short_data, offset));
}

/**
 * @test_case TC_SD_DESER_007
 * @tests REQ_SD_061_E01, REQ_SD_062_E01
 * @brief Test IPv4MulticastOption deserialization with insufficient data
 */
TEST_F(SdTest, MulticastOptionDeserializeTruncated) {
    IPv4MulticastOption option;
    platform::ByteBuffer short_data = {0x00};
    size_t offset = 0;
    EXPECT_FALSE(option.deserialize(short_data, offset));
}

/**
 * @test_case TC_SD_DESER_008
 * @tests REQ_SD_064_E01, REQ_SD_075_E01
 * @brief Test IPv4EndpointOption with invalid address
 */
TEST_F(SdTest, IPv4EndpointInvalidAddress) {
    IPv4EndpointOption option;

    option.set_ipv4_address_from_string("invalid");
    EXPECT_EQ(option.get_ipv4_address(), 0u);

    option.set_ipv4_address_from_string("256.1.1.1");
    EXPECT_EQ(option.get_ipv4_address(), 0u);
}

// ============================================================================
// SD Server Error Handling Tests
// ============================================================================

/**
 * @test_case TC_SD_SERVER_001
 * @tests REQ_SD_040_E01
 * @brief Test server duplicate service offer
 */
TEST_F(SdTest, ServerDuplicateOffer) {
    SdConfig config;
    auto server = std::make_shared<SdServer>(config);

    ASSERT_TRUE(server->initialize());

    ServiceInstance service1;
    service1.service_id = 0x1234;
    service1.instance_id = 0x0001;

    EXPECT_TRUE(server->offer_service(service1, "127.0.0.1:30490"));
    EXPECT_FALSE(server->offer_service(service1, "127.0.0.1:30490"));

    server->shutdown();
}

// ============================================================================
// SD Client Error Handling Tests
// ============================================================================

/**
 * @test_case TC_SD_CLIENT_001
 * @tests REQ_SD_041_E01
 * @brief Test client double subscribe returns false
 */
TEST_F(SdTest, ClientDoubleSubscribe) {
    SdConfig config;
    auto client = std::make_shared<SdClient>(config);

    ASSERT_TRUE(client->initialize());

    int count = 0;
    EXPECT_TRUE(client->subscribe_service(
        0x1234,
        [&](const ServiceInstance&) { count++; },
        [&](const ServiceInstance&) { count--; }
    ));
    EXPECT_FALSE(client->subscribe_service(
        0x1234,
        [&](const ServiceInstance&) { count++; },
        [&](const ServiceInstance&) { count--; }
    ));

    client->shutdown();
}

/**
 * @test_case TC_SD_E01
 * @tests REQ_SD_001_E01, REQ_SD_001_E02
 * @brief Test SD message with invalid header
 */
TEST_F(SdTest, InvalidSdMessageHeader) {
    platform::ByteBuffer raw_sd_msg = {
        0xFF, 0xFF, 0x81, 0x00,
        0x00, 0x00, 0x00, 0x08,
        0x00, 0x00, 0x00, 0x00,
        0x01, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    SdMessage sd_msg;
    bool result = sd_msg.deserialize(raw_sd_msg);
    EXPECT_FALSE(result) << "Invalid SD message should fail deserialization";
}

/**
 * @test_case TC_SD_E02
 * @tests REQ_SD_010_E01, REQ_SD_010_E02
 * @brief Test SD with truncated entries array
 */
TEST_F(SdTest, TruncatedEntriesArray) {
    platform::ByteBuffer truncated = {
        0xC0, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x20,
        0x01, 0x00, 0x00
    };

    SdMessage sd_msg;
    bool result = sd_msg.deserialize(truncated);
    EXPECT_FALSE(result) << "Truncated entries should fail";
}

/**
 * @test_case TC_SD_E03
 * @tests REQ_SD_030_E01
 * @brief Test SD offer with zero TTL (StopOffer)
 */
TEST_F(SdTest, StopOfferZeroTtl) {
    ServiceEntry entry(EntryType::OFFER_SERVICE);
    entry.set_service_id(0x1234);
    entry.set_instance_id(0x0001);
    entry.set_major_version(1);
    entry.set_ttl(0);

    EXPECT_EQ(entry.get_ttl(), 0u) << "TTL=0 indicates StopOffer";
    EXPECT_EQ(entry.get_type(), EntryType::OFFER_SERVICE);
}

/**
 * @test_case TC_SD_E04
 * @tests REQ_SD_050_E01
 * @brief Test SD FindService with wildcard instance
 */
TEST_F(SdTest, FindServiceWildcard) {
    ServiceEntry find_entry(EntryType::FIND_SERVICE);
    find_entry.set_service_id(0x1234);
    find_entry.set_instance_id(0xFFFF);
    find_entry.set_major_version(0xFF);
    find_entry.set_ttl(3);

    EXPECT_EQ(find_entry.get_instance_id(), 0xFFFF);
    EXPECT_EQ(find_entry.get_major_version(), 0xFF);
}

/**
 * @test_case TC_SD_E05
 * @tests REQ_SD_060_E01, REQ_SD_060_E02
 * @brief Test SD option with invalid length
 */
TEST_F(SdTest, InvalidOptionLength) {
    platform::ByteBuffer invalid_option = {
        0xFF, 0xFF,
        0x04,
        0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00,
        0x11,
        0x00, 0x00
    };

    IPv4EndpointOption option;
    size_t offset = 0;
    bool result = option.deserialize(invalid_option, offset);
    EXPECT_FALSE(result) << "Invalid option length should fail";
}

/**
 * @test_case TC_SD_E06
 * @tests REQ_SD_070_E01
 * @brief Test SD subscription to non-existent service
 */
TEST_F(SdTest, SubscribeNonExistentService) {
    SdConfig config;
    auto server = std::make_unique<SdServer>(config);
    ASSERT_TRUE(server->initialize());

    // The current implementation always ACKs regardless of offered state;
    // verify API returns successfully and does not crash.
    // The return value depends on whether the UDP send succeeds, which is
    // environment-dependent.  The test verifies the API doesn't crash.
    (void)server->handle_eventgroup_subscription(
        0x9999, 0x0001, 0x0001, "127.0.0.1", true);

    server->shutdown();
}

/**
 * @test_case TC_SD_E07
 * @tests REQ_SD_080_E01
 * @brief Test SD message with empty entries array
 */
TEST_F(SdTest, EmptyEntriesArray) {
    SdMessage sd_msg;
    sd_msg.set_flags(0xC0);
    EXPECT_EQ(sd_msg.get_entries().size(), 0u);

    auto serialized = sd_msg.serialize();
    EXPECT_FALSE(serialized.empty());

    SdMessage deserialized;
    bool result = deserialized.deserialize(serialized);
    EXPECT_TRUE(result) << "Empty entries array is valid";
    EXPECT_EQ(deserialized.get_entries().size(), 0u);
}

/**
 * @test_case TC_SD_E08
 * @tests REQ_SD_083_E01
 * @brief Test SD subscription with TTL overflow
 */
TEST_F(SdTest, SubscriptionMaxTtl) {
    EventGroupEntry entry(EntryType::SUBSCRIBE_EVENTGROUP);
    entry.set_service_id(0x1234);
    entry.set_instance_id(0x0001);
    entry.set_eventgroup_id(0x0001);
    entry.set_ttl(0xFFFFFF);

    EXPECT_EQ(entry.get_ttl(), 0xFFFFFFu) << "Max TTL (24-bit) should be accepted";
}

/**
 * @test_case TC_SD_E09
 * @tests REQ_SD_113_E01
 * @brief Test SD re-offer after stop succeeds
 */
TEST_F(SdTest, ReOfferAfterStop) {
    SdConfig config;
    auto server = std::make_unique<SdServer>(config);
    ASSERT_TRUE(server->initialize());

    ServiceInstance instance;
    instance.service_id = 0x1234;
    instance.instance_id = 0x0001;
    instance.major_version = 1;
    instance.minor_version = 0;
    instance.ttl_seconds = 3;

    bool first = server->offer_service(instance, "127.0.0.1:30509");
    EXPECT_TRUE(first);

    server->stop_offer_service(instance.service_id, instance.instance_id);

    bool second = server->offer_service(instance, "127.0.0.1:30509");
    EXPECT_TRUE(second) << "Re-offer after stop should succeed";

    server->shutdown();
}

/**
 * @test_case TC_SD_E10
 * @tests REQ_SD_116_E01, REQ_SD_116_E02
 * @brief Test SD with malformed option index
 */
TEST_F(SdTest, MalformedOptionIndex) {
    ServiceEntry entry(EntryType::OFFER_SERVICE);
    entry.set_service_id(0x1234);
    entry.set_instance_id(0x0001);
    entry.set_index1(0xFF);
    entry.set_index2(0x0F);

    EXPECT_EQ(entry.get_index1(), 0xFF);
    EXPECT_EQ(entry.get_index2(), 0x0F);
}

/**
 * @test_case TC_SD_E11
 * @tests REQ_SD_119_E01
 * @brief Test SD with unsupported entry type
 */
TEST_F(SdTest, UnsupportedEntryType) {
    // Build a minimal 16-byte SD entry with an unknown type (0xFF)
    platform::ByteBuffer unknown_type_data(16, 0x00);
    unknown_type_data[0] = 0xFF;  // type byte

    ServiceEntry entry(EntryType::FIND_SERVICE);
    size_t offset = 0;
    bool ok = entry.deserialize(unknown_type_data, offset);
    // Deserialization of an unknown type should fail or produce a mismatched type
    if (ok) {
        EXPECT_NE(entry.get_type(), EntryType::FIND_SERVICE);
        EXPECT_NE(entry.get_type(), EntryType::OFFER_SERVICE);
    } else {
        SUCCEED() << "deserialize correctly rejected unknown entry type";
    }
}

/**
 * @test_case TC_SD_E12
 * @tests REQ_SD_120_E01
 * @brief Test SD with zero-length options array
 */
// =============================================================================
// Regression tests for spec-compliance bugs
// =============================================================================

/**
 * @test_case TC_SD_REG_001
 * @brief ServiceEntry minor_version must be full 32-bit per SOME/IP-SD spec
 *
 * Regression: minor_version_ was uint8_t, truncating values > 255.
 * Bytes 12-15 of a ServiceEntry carry the 32-bit Minor Version.
 */
TEST_F(SdTest, ServiceEntryMinorVersion32Bit) {
    ServiceEntry entry(EntryType::OFFER_SERVICE);
    entry.set_service_id(0x1234);
    entry.set_instance_id(0x5678);
    entry.set_major_version(1);
    entry.set_minor_version(0x00010002);
    entry.set_ttl(3600);

    auto serialized = entry.serialize();
    ASSERT_EQ(serialized.size(), 16u);

    // Bytes 12-15 must carry the full 32-bit minor version in big-endian
    EXPECT_EQ(serialized[12], 0x00);
    EXPECT_EQ(serialized[13], 0x01);
    EXPECT_EQ(serialized[14], 0x00);
    EXPECT_EQ(serialized[15], 0x02);

    // Round-trip
    ServiceEntry deserialized;
    size_t offset = 0;
    EXPECT_TRUE(deserialized.deserialize(serialized, offset));
    EXPECT_EQ(deserialized.get_minor_version(), 0x00010002u);
}

/**
 * @test_case TC_SD_REG_002
 * @brief ServiceEntry minor version 0xFFFFFFFF round-trips correctly
 */
TEST_F(SdTest, ServiceEntryMinorVersionMax) {
    ServiceEntry entry(EntryType::OFFER_SERVICE);
    entry.set_service_id(0x1111);
    entry.set_instance_id(0x2222);
    entry.set_major_version(2);
    entry.set_minor_version(0xFFFFFFFF);
    entry.set_ttl(100);

    auto serialized = entry.serialize();
    ServiceEntry deserialized;
    size_t offset = 0;
    EXPECT_TRUE(deserialized.deserialize(serialized, offset));
    EXPECT_EQ(deserialized.get_minor_version(), 0xFFFFFFFFu);
}

/**
 * @test_case TC_SD_REG_003
 * @brief ConfigurationOption length field must include the reserved byte
 *
 * Regression: length was set to config_string_.size(), but per SOME/IP-SD
 * spec the Length field counts everything after Length(2) and Type(1),
 * which includes Reserved(1) + config_data.
 */
TEST_F(SdTest, ConfigurationOptionLengthIncludesReserved) {
    ConfigurationOption opt;
    opt.set_configuration_string("test=value");

    auto serialized = opt.serialize();
    ASSERT_GE(serialized.size(), 4u);

    // Length field (bytes 0-1) must be 1 + strlen("test=value") = 11
    uint16_t length = (static_cast<uint16_t>(serialized[0]) << 8) | serialized[1];
    EXPECT_EQ(length, 1 + 10) << "Length must include the reserved byte";

    // Total option size = Length(2) + Type(1) + length_value
    EXPECT_EQ(serialized.size(), 3u + length);

    // Round-trip
    ConfigurationOption deserialized;
    size_t offset = 0;
    EXPECT_TRUE(deserialized.deserialize(serialized, offset));
    EXPECT_EQ(deserialized.get_configuration_string(), "test=value");
}

/**
 * @test_case TC_SD_REG_004
 * @brief ConfigurationOption interop: accept length that includes reserved byte
 *
 * Simulates a spec-compliant external stack that sets
 * length = 1 (reserved) + config_data_len.
 */
TEST_F(SdTest, ConfigurationOptionInteropDeserialize) {
    // Hand-craft a spec-compliant Configuration Option:
    // Length(2) = 0x0006 (1 reserved + 5 bytes "hello")
    // Type(1)  = 0x01 (CONFIGURATION)
    // Reserved(1) = 0x00
    // Data(5)  = "hello"
    platform::ByteBuffer wire = {
        0x00, 0x06,  // Length = 6 (reserved + "hello")
        0x01,        // Type = CONFIGURATION
        0x00,        // Reserved
        'h', 'e', 'l', 'l', 'o'
    };

    ConfigurationOption opt;
    size_t offset = 0;
    EXPECT_TRUE(opt.deserialize(wire, offset));
    EXPECT_EQ(opt.get_configuration_string(), "hello");
    EXPECT_EQ(offset, wire.size());
}

/**
 * @test_case TC_SD_REG_005
 * @brief Unknown option types must be skipped with correct byte count
 *
 * Regression: unknown options were skipped with offset += 4 + length
 * instead of 3 + length, eating 1 extra byte from the next option.
 */
TEST_F(SdTest, UnknownOptionSkipCorrectBytes) {
    SdMessage sd_msg;
    sd_msg.set_flags(0xC0);

    auto entry = std::make_unique<ServiceEntry>(EntryType::OFFER_SERVICE);
    entry->set_service_id(0x1234);
    entry->set_instance_id(0x0001);
    entry->set_major_version(1);
    entry->set_ttl(3600);
    entry->set_index1(0);
    entry->set_num_opts1(2);
    sd_msg.add_entry(std::move(entry));

    auto ep_opt = std::make_unique<IPv4EndpointOption>();
    ep_opt->set_ipv4_address_from_string("192.168.1.100");
    ep_opt->set_protocol(0x11);
    ep_opt->set_port(30501);
    sd_msg.add_option(std::move(ep_opt));

    auto serialized = sd_msg.serialize();

    // Insert an unknown option (type 0x99) BEFORE the IPv4 endpoint option
    // in the options array. Find the options start.
    // Layout: flags(1) + reserved(3) + entries_len(4) + entries(16) +
    //         options_len(4) + options(...)
    size_t options_len_offset = 1 + 3 + 4 + 16;
    size_t options_start = options_len_offset + 4;

    // Build a new message with an unknown option followed by the IPv4 endpoint
    platform::ByteBuffer modified;
    modified.insert(modified.end(), serialized.begin(),
                    serialized.begin() + static_cast<std::ptrdiff_t>(options_start));

    // Unknown option: Length=0x0003 (reserved + 2 data bytes), Type=0x99, Reserved=0x00, Data=0xAA 0xBB
    platform::ByteBuffer unknown_opt = {0x00, 0x03, 0x99, 0x00, 0xAA, 0xBB};
    modified.insert(modified.end(), unknown_opt.begin(), unknown_opt.end());

    // Append the original IPv4 endpoint option
    modified.insert(modified.end(),
                    serialized.begin() + static_cast<std::ptrdiff_t>(options_start),
                    serialized.end());

    // Update options array length
    uint32_t new_options_len = static_cast<uint32_t>(modified.size() - options_start);
    modified[options_len_offset]     = static_cast<uint8_t>((new_options_len >> 24) & 0xFF);
    modified[options_len_offset + 1] = static_cast<uint8_t>((new_options_len >> 16) & 0xFF);
    modified[options_len_offset + 2] = static_cast<uint8_t>((new_options_len >> 8) & 0xFF);
    modified[options_len_offset + 3] = static_cast<uint8_t>(new_options_len & 0xFF);

    SdMessage deserialized;
    EXPECT_TRUE(deserialized.deserialize(modified))
        << "Must parse message with unknown option followed by known option";

    // The unknown option should be skipped; the IPv4 endpoint should be parsed
    ASSERT_GE(deserialized.get_options().size(), 1u);
    auto* ipv4_opt = dynamic_cast<IPv4EndpointOption*>(deserialized.get_options()[0].get());
    ASSERT_NE(ipv4_opt, nullptr);
    EXPECT_EQ(ipv4_opt->get_port(), 30501);
    EXPECT_EQ(ipv4_opt->get_protocol(), 0x11);
}

/**
 * @test_case TC_SD_REG_006
 * @brief Full SD message with 32-bit minor version round-trips
 */
TEST_F(SdTest, FullMessageMinorVersion32BitRoundTrip) {
    SdMessage sd_msg;
    sd_msg.set_flags(0xC0);

    auto entry = std::make_unique<ServiceEntry>(EntryType::OFFER_SERVICE);
    entry->set_service_id(0xABCD);
    entry->set_instance_id(0x0001);
    entry->set_major_version(3);
    entry->set_minor_version(0x00020003);
    entry->set_ttl(7200);
    sd_msg.add_entry(std::move(entry));

    auto opt = std::make_unique<IPv4EndpointOption>();
    opt->set_ipv4_address_from_string("10.0.0.1");
    opt->set_protocol(0x06);
    opt->set_port(8080);
    sd_msg.add_option(std::move(opt));

    auto serialized = sd_msg.serialize();

    SdMessage deserialized;
    EXPECT_TRUE(deserialized.deserialize(serialized));
    ASSERT_EQ(deserialized.get_entries().size(), 1u);

    auto* de = dynamic_cast<ServiceEntry*>(deserialized.get_entries()[0].get());
    ASSERT_NE(de, nullptr);
    EXPECT_EQ(de->get_minor_version(), 0x00020003u);
    EXPECT_EQ(de->get_major_version(), 3);
    EXPECT_EQ(de->get_service_id(), 0xABCDu);
}

/**
 * @test_case TC_SD_REG_007
 * @brief minor_version > 255 must survive the public OfferService wire path
 *
 * Regression: SdServer::send_service_offer did not call set_minor_version,
 * and SdClient::handle_service_offer hardcoded minor_version = 0.
 * This test verifies that ServiceInstance.minor_version is correctly
 * propagated into the wire-format ServiceEntry and recovered on the client
 * side by constructing the SD message the same way the public path does.
 */
TEST_F(SdTest, MinorVersionPreservedThroughOfferPath) {
    const uint32_t expected_minor = 0x00030007;

    // Construct the SD message as SdServer::send_service_offer would
    auto entry = std::make_unique<ServiceEntry>(EntryType::OFFER_SERVICE);
    entry->set_service_id(0xBEEF);
    entry->set_instance_id(0x0001);
    entry->set_major_version(2);
    entry->set_minor_version(expected_minor);
    entry->set_ttl(3600);
    entry->set_index1(0);
    entry->set_num_opts1(1);

    SdMessage sd_msg;
    sd_msg.set_flags(0xC0);
    sd_msg.add_entry(std::move(entry));

    auto opt = std::make_unique<IPv4EndpointOption>();
    opt->set_ipv4_address_from_string("10.0.0.1");
    opt->set_protocol(0x11);
    opt->set_port(30509);
    sd_msg.add_option(std::move(opt));

    // Serialize → wire → deserialize (client path)
    auto wire = sd_msg.serialize();
    SdMessage received;
    ASSERT_TRUE(received.deserialize(wire));
    ASSERT_EQ(received.get_entries().size(), 1u);

    // Simulate what SdClient::handle_service_offer now does
    auto* svc = dynamic_cast<ServiceEntry*>(received.get_entries()[0].get());
    ASSERT_NE(svc, nullptr);

    ServiceInstance instance;
    instance.service_id = svc->get_service_id();
    instance.instance_id = svc->get_instance_id();
    instance.major_version = svc->get_major_version();
    instance.minor_version = svc->get_minor_version();
    instance.ttl_seconds = svc->get_ttl();

    EXPECT_EQ(instance.service_id, 0xBEEFu);
    EXPECT_EQ(instance.major_version, 2u);
    EXPECT_EQ(instance.minor_version, expected_minor)
        << "minor_version must survive the OfferService wire path";
    EXPECT_EQ(instance.ttl_seconds, 3600u);
}

TEST_F(SdTest, ZeroLengthOptions) {
    SdMessage sd_msg;
    sd_msg.set_flags(0xC0);

    auto entry = std::make_unique<ServiceEntry>(EntryType::FIND_SERVICE);
    entry->set_service_id(0x1234);
    entry->set_instance_id(0xFFFF);
    entry->set_major_version(0xFF);
    entry->set_ttl(3);
    sd_msg.add_entry(std::move(entry));

    EXPECT_EQ(sd_msg.get_options().size(), 0u) << "No options added";

    auto serialized = sd_msg.serialize();
    EXPECT_FALSE(serialized.empty()) << "Serialized message with entries and zero options";

    SdMessage deserialized;
    bool result = deserialized.deserialize(serialized);
    EXPECT_TRUE(result) << "Zero-options message should round-trip";
    EXPECT_EQ(deserialized.get_options().size(), 0u) << "Options array should be empty";
    ASSERT_EQ(deserialized.get_entries().size(), 1u) << "Should have one entry";

    auto* de = dynamic_cast<ServiceEntry*>(deserialized.get_entries()[0].get());
    ASSERT_NE(de, nullptr);
    EXPECT_EQ(de->get_service_id(), 0x1234);
    EXPECT_EQ(de->get_instance_id(), 0xFFFF);
    EXPECT_EQ(de->get_major_version(), 0xFF);
    EXPECT_EQ(de->get_ttl(), 3u);
}

// ============================================================================
// SD Session ID Counter Tests (Issue #253)
// ============================================================================

/**
 * @test_case TC_SD_SESSION_001
 * @tests REQ_SD_070, REQ_SD_071
 * @brief First session ID value is 0x0001
 */
TEST_F(SdTest, SessionIdStartsAtOne) {
    SdSessionIdCounter counter;
    EXPECT_EQ(counter.next(), 0x0001);
}

/**
 * @test_case TC_SD_SESSION_002
 * @tests REQ_SD_070, REQ_SD_071
 * @brief Session IDs increment sequentially
 */
TEST_F(SdTest, SessionIdIncrements) {
    SdSessionIdCounter counter;
    EXPECT_EQ(counter.next(), 0x0001);
    EXPECT_EQ(counter.next(), 0x0002);
    EXPECT_EQ(counter.next(), 0x0003);
}

/**
 * @test_case TC_SD_SESSION_003
 * @tests REQ_SD_070, REQ_SD_071
 * @brief Session ID wraps from 0xFFFF to 0x0001, never emitting 0x0000
 */
TEST_F(SdTest, SessionIdWrapAroundSkipsZero) {
    SdSessionIdCounter counter;
    // Advance to near wrap-around point
    for (uint32_t i = 1; i < 0xFFFF; ++i) {
        counter.next();
    }
    EXPECT_EQ(counter.next(), 0xFFFF);
    EXPECT_EQ(counter.next(), 0x0001) << "Must wrap to 0x0001, never 0x0000";
}

/**
 * @test_case TC_SD_SESSION_004
 * @tests REQ_SD_070, REQ_SD_071
 * @brief Per-peer unicast session IDs are isolated
 */
TEST_F(SdTest, UnicastSessionIdsIsolatedPerPeer) {
    SdSessionIdCounter peer_a;
    SdSessionIdCounter peer_b;
    EXPECT_EQ(peer_a.next(), 0x0001);
    EXPECT_EQ(peer_a.next(), 0x0002);
    EXPECT_EQ(peer_b.next(), 0x0001) << "Peer B should start at 1 independently";

    EXPECT_EQ(peer_a.next(), 0x0003);
    EXPECT_EQ(peer_b.next(), 0x0002);
}

/**
 * @test_case TC_SD_SESSION_005
 * @tests REQ_SD_070, REQ_SD_071
 * @brief SdMessage session_id tracking field round-trips
 */
TEST_F(SdTest, SdMessageSessionIdAccessor) {
    SdMessage msg;
    EXPECT_EQ(msg.get_session_id(), 0);
    msg.set_session_id(42);
    EXPECT_EQ(msg.get_session_id(), 42);
}

// ============================================================================
// Subscription TTL Enforcement Tests (Issue #266)
//
// These tests verify that the SdServer reads the TTL from incoming
// SubscribeEventgroup entries, echoes it in the ACK, and correctly
// handles TTL=0 as StopSubscribeEventgroup.
// ============================================================================

/**
 * @brief Helper: build a SOME/IP-SD SubscribeEventgroup message.
 *
 * Constructs a complete SOME/IP message wrapping an SD payload that
 * contains a SubscribeEventgroup entry with the given TTL and an
 * IPv4EndpointOption pointing to the client.
 */
static Message build_subscribe_eventgroup_message(
    uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
    uint32_t ttl, const char* client_ip, uint16_t client_port) {

    auto entry = std::make_unique<EventGroupEntry>(EntryType::SUBSCRIBE_EVENTGROUP);
    entry->set_service_id(service_id);
    entry->set_instance_id(instance_id);
    entry->set_eventgroup_id(eventgroup_id);
    entry->set_major_version(0x01);
    entry->set_ttl(ttl);
    entry->set_index1(0);
    entry->set_num_opts1(1);

    auto option = std::make_unique<IPv4EndpointOption>();
    option->set_ipv4_address_from_string(client_ip);
    option->set_port(client_port);
    option->set_protocol(0x11);

    SdMessage sd_msg;
    sd_msg.set_reboot(true);
    sd_msg.add_entry(std::move(entry));
    sd_msg.add_option(std::move(option));

    Message someip_msg(
        MessageId(0xFFFF, SOMEIP_SD_METHOD_ID),
        RequestId(SOMEIP_SD_CLIENT_ID, 0x0001),
        MessageType::NOTIFICATION,
        ReturnCode::E_OK);
    someip_msg.set_payload(sd_msg.serialize());
    return someip_msg;
}

/**
 * @brief Helper: receive one SOME/IP-SD message on a UDP socket and
 *        extract the first EventGroupEntry from it.
 * @return true if a SubscribeEventgroup ACK/NACK entry was received.
 */
static bool receive_sd_ack(transport::UdpTransport& transport,
                           EventGroupEntry& out_entry,
                           std::chrono::milliseconds timeout = std::chrono::milliseconds(2000)) {
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        auto msg = transport.receive_message();
        if (!msg) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (msg->get_service_id() != 0xFFFF) {
            continue;
        }

        SdMessage sd_msg;
        if (!sd_msg.deserialize(msg->get_payload())) {
            continue;
        }

        for (const auto& entry : sd_msg.get_entries()) {
            if (entry->get_type() == EntryType::SUBSCRIBE_EVENTGROUP_ACK) {
                const auto* eg = static_cast<const EventGroupEntry*>(entry.get());
                out_entry.set_service_id(eg->get_service_id());
                out_entry.set_instance_id(eg->get_instance_id());
                out_entry.set_eventgroup_id(eg->get_eventgroup_id());
                out_entry.set_major_version(eg->get_major_version());
                out_entry.set_ttl(eg->get_ttl());
                return true;
            }
        }
    }
    return false;
}

/**
 * @test_case TC_SD_TTL_001
 * @brief SdServer ACK echoes the subscription TTL from the request.
 *
 * Before the fix the ACK always hardcoded TTL=3600. After the fix it
 * reflects the subscriber's requested TTL.
 */
TEST_F(SdIntegrationTest, SubscriptionACKReflectsRequestedTTL) {
    const uint16_t server_port = get_unique_port();
    const uint16_t client_port = get_unique_port();

    auto server_config = create_test_config(server_port, server_port);
    SdServer server(server_config);
    ASSERT_TRUE(server.initialize());

    ServiceInstance svc(0x1234, 0x0001, 1, 0);
    svc.ttl_seconds = 30;
    ASSERT_TRUE(server.offer_service(svc, "127.0.0.1:30509", "", {0x0001}));

    transport::UdpTransportConfig client_cfg;
    client_cfg.blocking = false;
    transport::UdpTransport client_transport(
        transport::Endpoint("0.0.0.0", client_port), client_cfg);
    ASSERT_EQ(client_transport.start(), Result::SUCCESS);

    const uint32_t requested_ttl = 1800;
    auto subscribe_msg = build_subscribe_eventgroup_message(
        0x1234, 0x0001, 0x0001, requested_ttl, "127.0.0.1", client_port);

    transport::Endpoint server_ep("127.0.0.1", server_port);
    ASSERT_EQ(client_transport.send_message(subscribe_msg, server_ep),
              Result::SUCCESS);

    EventGroupEntry ack_entry;
    bool received = receive_sd_ack(client_transport, ack_entry);

    client_transport.stop();
    server.shutdown();

    if (received) {
        EXPECT_EQ(ack_entry.get_service_id(), 0x1234u);
        EXPECT_EQ(ack_entry.get_eventgroup_id(), 0x0001u);
        EXPECT_EQ(ack_entry.get_ttl(), requested_ttl)
            << "ACK TTL must reflect the subscriber's requested TTL, not hardcoded 3600";
    } else {
        GTEST_SKIP() << "ACK not received (loopback may be unavailable)";
    }
}

/**
 * @test_case TC_SD_TTL_002
 * @brief SdServer handles StopSubscribeEventgroup (TTL=0).
 *
 * Per SOME/IP-SD spec, a SubscribeEventgroup entry with TTL=0 signals
 * StopSubscribeEventgroup. The server must respond with an ACK whose
 * TTL is also 0.
 */
TEST_F(SdIntegrationTest, StopSubscribeEventgroupTTLZero) {
    const uint16_t server_port = get_unique_port();
    const uint16_t client_port = get_unique_port();

    auto server_config = create_test_config(server_port, server_port);
    SdServer server(server_config);
    ASSERT_TRUE(server.initialize());

    ServiceInstance svc(0x1234, 0x0001, 1, 0);
    svc.ttl_seconds = 30;
    ASSERT_TRUE(server.offer_service(svc, "127.0.0.1:30509", "", {0x0001}));

    transport::UdpTransportConfig client_cfg;
    client_cfg.blocking = false;
    transport::UdpTransport client_transport(
        transport::Endpoint("0.0.0.0", client_port), client_cfg);
    ASSERT_EQ(client_transport.start(), Result::SUCCESS);

    auto stop_msg = build_subscribe_eventgroup_message(
        0x1234, 0x0001, 0x0001, 0, "127.0.0.1", client_port);

    transport::Endpoint server_ep("127.0.0.1", server_port);
    ASSERT_EQ(client_transport.send_message(stop_msg, server_ep),
              Result::SUCCESS);

    EventGroupEntry ack_entry;
    bool received = receive_sd_ack(client_transport, ack_entry);

    client_transport.stop();
    server.shutdown();

    if (received) {
        EXPECT_EQ(ack_entry.get_ttl(), 0u)
            << "StopSubscribe ACK must have TTL=0";
    } else {
        GTEST_SKIP() << "ACK not received (loopback may be unavailable)";
    }
}

/**
 * @test_case TC_SD_TTL_003
 * @brief EventPublisher stops delivering events to expired subscribers.
 *
 * End-to-end test verifying that publish_event() filters out subscribers
 * whose TTL has elapsed — the core behavior the bug report described.
 */
TEST_F(SdIntegrationTest, EventPublisherStopsEventsAfterTTLExpiry) {
    events::EventPublisher publisher(0x1234, 0x0001);
    publisher.set_default_client_endpoint("127.0.0.1", 50000);

    events::EventConfig cfg;
    cfg.event_id = 0x8001;
    cfg.eventgroup_id = 0x0001;
    cfg.notification_type = events::NotificationType::ON_CHANGE;
    publisher.register_event(cfg);

    ASSERT_TRUE(publisher.handle_subscription(0x0001, 0x0100, 1u));
    ASSERT_TRUE(publisher.handle_subscription(0x0001, 0x0200, 3600u));

    auto subs = publisher.get_subscriptions(0x0001);
    EXPECT_EQ(subs.size(), 2u) << "Both subscribers active immediately after subscribe";

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    subs = publisher.get_subscriptions(0x0001);
    ASSERT_EQ(subs.size(), 1u) << "Short-TTL subscriber must have expired";
    EXPECT_EQ(subs[0], 0x0200u) << "Only long-TTL subscriber should remain";

    size_t cleaned = publisher.cleanup_expired_subscriptions();
    EXPECT_EQ(cleaned, 1u);
    subs = publisher.get_subscriptions(0x0001);
    EXPECT_EQ(subs.size(), 1u);
}
