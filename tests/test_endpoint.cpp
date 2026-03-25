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
#include "transport/endpoint.h"
#include <unordered_set>

using namespace someip::transport;

/**
 * @brief Endpoint unit tests with MC/DC coverage for address validation logic.
 *
 * MC/DC (Modified Condition/Decision Coverage) requires that each boolean
 * sub-condition in a compound decision independently affects the outcome.
 * Tests below are structured so that each condition in multi-condition
 * predicates is toggled while holding others constant.
 *
 * @tests REQ_TRANSPORT_006
 */

// ============================================================================
// Construction & accessors
// ============================================================================

class EndpointTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(EndpointTest, DefaultConstructor) {
    Endpoint ep;
    EXPECT_EQ(ep.get_address(), "127.0.0.1");
    EXPECT_EQ(ep.get_port(), 30490);
    EXPECT_EQ(ep.get_protocol(), TransportProtocol::UDP);
}

TEST_F(EndpointTest, ParameterizedConstructor) {
    Endpoint ep("192.168.1.1", 5000, TransportProtocol::TCP);
    EXPECT_EQ(ep.get_address(), "192.168.1.1");
    EXPECT_EQ(ep.get_port(), 5000);
    EXPECT_EQ(ep.get_protocol(), TransportProtocol::TCP);
}

TEST_F(EndpointTest, DefaultProtocolIsUdp) {
    Endpoint ep("10.0.0.1", 1234);
    EXPECT_EQ(ep.get_protocol(), TransportProtocol::UDP);
}

TEST_F(EndpointTest, CopyConstructor) {
    Endpoint original("10.0.0.1", 8080, TransportProtocol::TCP);
    Endpoint copy(original);

    EXPECT_EQ(copy.get_address(), original.get_address());
    EXPECT_EQ(copy.get_port(), original.get_port());
    EXPECT_EQ(copy.get_protocol(), original.get_protocol());
}

TEST_F(EndpointTest, MoveConstructor) {
    Endpoint original("10.0.0.1", 8080, TransportProtocol::TCP);
    Endpoint moved(std::move(original));

    EXPECT_EQ(moved.get_address(), "10.0.0.1");
    EXPECT_EQ(moved.get_port(), 8080);
    EXPECT_EQ(moved.get_protocol(), TransportProtocol::TCP);
}

TEST_F(EndpointTest, CopyAssignment) {
    Endpoint a("10.0.0.1", 1111, TransportProtocol::TCP);
    Endpoint b("192.168.0.1", 2222, TransportProtocol::UDP);

    b = a;
    EXPECT_EQ(b.get_address(), "10.0.0.1");
    EXPECT_EQ(b.get_port(), 1111);
    EXPECT_EQ(b.get_protocol(), TransportProtocol::TCP);
}

TEST_F(EndpointTest, CopyAssignmentSelfAssignment) {
    Endpoint a("10.0.0.1", 1111, TransportProtocol::TCP);
    Endpoint& ref = a;
    a = ref;
    EXPECT_EQ(a.get_address(), "10.0.0.1");
    EXPECT_EQ(a.get_port(), 1111);
}

TEST_F(EndpointTest, MoveAssignment) {
    Endpoint a("10.0.0.1", 1111, TransportProtocol::TCP);
    Endpoint b;

    b = std::move(a);
    EXPECT_EQ(b.get_address(), "10.0.0.1");
    EXPECT_EQ(b.get_port(), 1111);
    EXPECT_EQ(b.get_protocol(), TransportProtocol::TCP);
}

TEST_F(EndpointTest, MoveAssignmentSelfAssignment) {
    Endpoint a("10.0.0.1", 1111, TransportProtocol::TCP);
    Endpoint& ref = a;
    a = std::move(ref);
    EXPECT_EQ(a.get_port(), 1111);
}

TEST_F(EndpointTest, Setters) {
    Endpoint ep;
    ep.set_address("192.168.0.1");
    ep.set_port(9999);
    ep.set_protocol(TransportProtocol::MULTICAST_UDP);

    EXPECT_EQ(ep.get_address(), "192.168.0.1");
    EXPECT_EQ(ep.get_port(), 9999);
    EXPECT_EQ(ep.get_protocol(), TransportProtocol::MULTICAST_UDP);
}

// ============================================================================
// IPv4 validation — MC/DC
//
// is_valid_ipv4 has these compound decisions:
//   D1: address.empty() || address.size() > 15
//   D2: digit_len == 0 || digit_len > 3
//   D3: val < 0 || val > 255          (val < 0 unreachable with atoi on digits)
//   D4: digit_len > 1 && address[start] == '0'
//   D5: octets == 4 && pos == address.size()
// ============================================================================

TEST_F(EndpointTest, IPv4Valid_BasicAddresses) {
    EXPECT_TRUE(Endpoint("0.0.0.0", 1).is_valid());
    EXPECT_TRUE(Endpoint("255.255.255.255", 1).is_valid());
    EXPECT_TRUE(Endpoint("127.0.0.1", 1).is_valid());
    EXPECT_TRUE(Endpoint("192.168.1.100", 1).is_valid());
    EXPECT_TRUE(Endpoint("1.2.3.4", 1).is_valid());
}

// D1: address.empty()=T  →  return false
TEST_F(EndpointTest, IPv4_MCDC_D1_EmptyAddress) {
    Endpoint ep("", 1);
    EXPECT_FALSE(ep.is_valid());
    EXPECT_FALSE(ep.is_ipv4());
}

// D1: address.size()>15=T, empty=F  →  return false
TEST_F(EndpointTest, IPv4_MCDC_D1_TooLong) {
    Endpoint ep("1234.1234.1234.12", 1);  // 17 chars
    EXPECT_FALSE(ep.is_valid());
    EXPECT_FALSE(ep.is_ipv4());
}

// D1: both F  →  continue
TEST_F(EndpointTest, IPv4_MCDC_D1_NormalLength) {
    EXPECT_TRUE(Endpoint("1.2.3.4", 1).is_ipv4());
}

// D2: digit_len==0=T  →  consecutive dots "1..2.3"
TEST_F(EndpointTest, IPv4_MCDC_D2_EmptyOctet) {
    EXPECT_FALSE(Endpoint("1..2.3", 1).is_ipv4());
}

// D2: digit_len>3=T, digit_len==0=F  →  4-digit octet
TEST_F(EndpointTest, IPv4_MCDC_D2_FourDigitOctet) {
    EXPECT_FALSE(Endpoint("1234.0.0.1", 1).is_ipv4());
}

// D2: both F  →  valid digit count (1-3)
TEST_F(EndpointTest, IPv4_MCDC_D2_ValidDigitLen) {
    EXPECT_TRUE(Endpoint("10.20.30.40", 1).is_ipv4());
}

// D3: val > 255  →  e.g. "256.0.0.1"
TEST_F(EndpointTest, IPv4_MCDC_D3_OctetTooLarge) {
    EXPECT_FALSE(Endpoint("256.0.0.1", 1).is_ipv4());
    EXPECT_FALSE(Endpoint("0.0.0.999", 1).is_ipv4());
}

// D3: val in range  →  valid
TEST_F(EndpointTest, IPv4_MCDC_D3_BoundaryOctets) {
    EXPECT_TRUE(Endpoint("0.0.0.0", 1).is_ipv4());
    EXPECT_TRUE(Endpoint("255.255.255.255", 1).is_ipv4());
}

// D4: digit_len>1=T && address[start]=='0'=T  →  leading zero rejected
TEST_F(EndpointTest, IPv4_MCDC_D4_LeadingZero) {
    EXPECT_FALSE(Endpoint("01.2.3.4", 1).is_ipv4());
    EXPECT_FALSE(Endpoint("1.02.3.4", 1).is_ipv4());
    EXPECT_FALSE(Endpoint("1.2.03.4", 1).is_ipv4());
    EXPECT_FALSE(Endpoint("1.2.3.04", 1).is_ipv4());
}

// D4: digit_len>1=T && address[start]=='0'=F  →  multi-digit, no leading zero
TEST_F(EndpointTest, IPv4_MCDC_D4_MultiDigitNoLeadingZero) {
    EXPECT_TRUE(Endpoint("12.34.56.78", 1).is_ipv4());
}

// D4: digit_len>1=F  →  single digit, condition short-circuits
TEST_F(EndpointTest, IPv4_MCDC_D4_SingleDigit) {
    EXPECT_TRUE(Endpoint("1.2.3.4", 1).is_ipv4());
}

// D5: octets!=4  →  too few octets
TEST_F(EndpointTest, IPv4_MCDC_D5_TooFewOctets) {
    EXPECT_FALSE(Endpoint("1.2.3", 1).is_ipv4());
    EXPECT_FALSE(Endpoint("1.2", 1).is_ipv4());
    EXPECT_FALSE(Endpoint("1", 1).is_ipv4());
}

// D5: octets==4 but trailing chars
TEST_F(EndpointTest, IPv4_MCDC_D5_TrailingCharacters) {
    EXPECT_FALSE(Endpoint("1.2.3.4.", 1).is_ipv4());
    EXPECT_FALSE(Endpoint("1.2.3.4.5", 1).is_ipv4());
}

// D5: octets==4 && pos==size  →  valid
TEST_F(EndpointTest, IPv4_MCDC_D5_ExactFourOctets) {
    EXPECT_TRUE(Endpoint("1.2.3.4", 1).is_ipv4());
}

TEST_F(EndpointTest, IPv4_NonDigitCharacter) {
    EXPECT_FALSE(Endpoint("1.2.a.4", 1).is_ipv4());
    EXPECT_FALSE(Endpoint("abc.def.ghi.jkl", 1).is_ipv4());
}

// ============================================================================
// IPv6 validation — MC/DC
//
// is_valid_ipv6 key decisions:
//   D1: address.empty() || address.size() > 39
//   D2: !isxdigit(c) && c != ':'
//   D3: double-colon count > 1
//   D4: no "::" and leading/trailing ':'
//   D5: ":::" present
//   D6: group len > 4
//   D7: with "::" → groups <= 7; without → groups == 8
// ============================================================================

TEST_F(EndpointTest, IPv6Valid_FullAddress) {
    EXPECT_TRUE(Endpoint("2001:0db8:85a3:0000:0000:8a2e:0370:7334", 1).is_valid());
    EXPECT_TRUE(Endpoint("2001:db8:85a3:0:0:8a2e:370:7334", 1).is_valid());
}

TEST_F(EndpointTest, IPv6Valid_Loopback) {
    EXPECT_TRUE(Endpoint("::1", 1).is_valid());
    EXPECT_TRUE(Endpoint("::1", 1).is_ipv6());
}

TEST_F(EndpointTest, IPv6Valid_AllZeros) {
    EXPECT_TRUE(Endpoint("::", 1).is_ipv6());
}

// D1: empty  →  false
TEST_F(EndpointTest, IPv6_MCDC_D1_Empty) {
    Endpoint ep("", 1);
    EXPECT_FALSE(ep.is_ipv6());
}

// D1: too long (>39 chars)  →  false
TEST_F(EndpointTest, IPv6_MCDC_D1_TooLong) {
    // 40 chars: "2001:0db8:85a3:0000:0000:8a2e:0370:73341"
    Endpoint ep("2001:0db8:85a3:0000:0000:8a2e:0370:73341", 1);
    EXPECT_FALSE(ep.is_ipv6());
}

// D2: non-hex, non-colon character  →  false
TEST_F(EndpointTest, IPv6_MCDC_D2_InvalidChar) {
    EXPECT_FALSE(Endpoint("2001:db8:85a3::8a2e:370:xyz1", 1).is_ipv6());
    EXPECT_FALSE(Endpoint("2001:db8:85a3::8a2e:370:733g", 1).is_ipv6());
}

// D2: valid chars (hex digits + colons only)
TEST_F(EndpointTest, IPv6_MCDC_D2_AllValidChars) {
    EXPECT_TRUE(Endpoint("abcd:ef01:2345:6789:ABCD:EF01:2345:6789", 1).is_ipv6());
}

// D3: two "::" sequences  →  false
TEST_F(EndpointTest, IPv6_MCDC_D3_MultipleDoubleColons) {
    EXPECT_FALSE(Endpoint("2001::db8::1", 1).is_ipv6());
}

// D4: no "::" but leading ':'
TEST_F(EndpointTest, IPv6_MCDC_D4_LeadingColonNoCompression) {
    EXPECT_FALSE(Endpoint(":2001:db8:85a3:0:0:8a2e:370:7334", 1).is_ipv6());
}

// D4: no "::" but trailing ':'
TEST_F(EndpointTest, IPv6_MCDC_D4_TrailingColonNoCompression) {
    EXPECT_FALSE(Endpoint("2001:db8:85a3:0:0:8a2e:370:7334:", 1).is_ipv6());
}

// D5: triple colon ":::"
TEST_F(EndpointTest, IPv6_MCDC_D5_TripleColon) {
    EXPECT_FALSE(Endpoint("2001:::db8:85a3:0:0:8a2e:370", 1).is_ipv6());
}

// D6: group with >4 hex digits
TEST_F(EndpointTest, IPv6_MCDC_D6_GroupTooLong) {
    EXPECT_FALSE(Endpoint("20011:db8:85a3:0:0:8a2e:370:7334", 1).is_ipv6());
}

// D7: without "::" → needs exactly 8 groups
TEST_F(EndpointTest, IPv6_MCDC_D7_TooFewGroupsNoCompression) {
    EXPECT_FALSE(Endpoint("2001:db8:85a3:0:0:8a2e:370", 1).is_ipv6());  // 7 groups
}

TEST_F(EndpointTest, IPv6_MCDC_D7_TooManyGroupsNoCompression) {
    EXPECT_FALSE(Endpoint("2001:db8:85a3:0:0:8a2e:370:7334:1", 1).is_ipv6());  // 9 groups
}

// D7: with "::" → groups <= 7 is valid
TEST_F(EndpointTest, IPv6_MCDC_D7_CompressionValid) {
    EXPECT_TRUE(Endpoint("2001:db8::1", 1).is_ipv6());       // 3 groups + ::
    EXPECT_TRUE(Endpoint("fe80::1", 1).is_ipv6());            // 2 groups + ::
    EXPECT_TRUE(Endpoint("::ffff:192:168", 1).is_ipv6());     // 3 groups + ::
}

// D7: with "::" but 8+ groups → invalid (too many groups even with compression)
TEST_F(EndpointTest, IPv6_MCDC_D7_CompressionTooManyGroups) {
    EXPECT_FALSE(Endpoint("1:2:3:4:5:6:7::8", 1).is_ipv6()); // 8 groups + ::
}

// Regression: leading/trailing ':' that is NOT part of "::"
TEST_F(EndpointTest, IPv6_Regression_LeadingColonNotPartOfCompression) {
    EXPECT_FALSE(Endpoint(":1::", 1).is_ipv6());
    EXPECT_FALSE(Endpoint(":1:2:3:4:5::", 1).is_ipv6());
}

TEST_F(EndpointTest, IPv6_Regression_TrailingColonNotPartOfCompression) {
    EXPECT_FALSE(Endpoint("1::2:", 1).is_ipv6());
    EXPECT_FALSE(Endpoint("::1:", 1).is_ipv6());
}

// ============================================================================
// is_valid() — MC/DC for the top-level disjunction
//
//   is_valid() = is_valid_ipv4(addr) || is_valid_ipv6(addr)
//   MC/DC requires:
//     ipv4=T ⇒ result=T  (regardless of ipv6)
//     ipv4=F, ipv6=T ⇒ result=T
//     ipv4=F, ipv6=F ⇒ result=F
// ============================================================================

TEST_F(EndpointTest, IsValid_MCDC_IPv4True) {
    EXPECT_TRUE(Endpoint("10.0.0.1", 1).is_valid());
}

TEST_F(EndpointTest, IsValid_MCDC_IPv4FalseIPv6True) {
    EXPECT_TRUE(Endpoint("::1", 1).is_valid());
}

TEST_F(EndpointTest, IsValid_MCDC_BothFalse) {
    EXPECT_FALSE(Endpoint("not-an-ip", 1).is_valid());
    EXPECT_FALSE(Endpoint("", 1).is_valid());
    EXPECT_FALSE(Endpoint("999.999.999.999", 1).is_valid());
}

// ============================================================================
// Multicast detection
// ============================================================================

TEST_F(EndpointTest, Multicast_ValidRange) {
    EXPECT_TRUE(Endpoint("224.0.0.0", 1).is_multicast());
    EXPECT_TRUE(Endpoint("224.0.0.1", 1).is_multicast());
    EXPECT_TRUE(Endpoint("239.255.255.255", 1).is_multicast());
    EXPECT_TRUE(Endpoint("232.1.2.3", 1).is_multicast());
}

TEST_F(EndpointTest, Multicast_BelowRange) {
    EXPECT_FALSE(Endpoint("223.255.255.255", 1).is_multicast());
    EXPECT_FALSE(Endpoint("192.168.1.1", 1).is_multicast());
}

TEST_F(EndpointTest, Multicast_AboveRange) {
    EXPECT_FALSE(Endpoint("240.0.0.1", 1).is_multicast());
    EXPECT_FALSE(Endpoint("255.255.255.255", 1).is_multicast());
}

TEST_F(EndpointTest, Multicast_InvalidAddressIsNotMulticast) {
    EXPECT_FALSE(Endpoint("not-an-ip", 1).is_multicast());
    EXPECT_FALSE(Endpoint("::1", 1).is_multicast());
}

// ============================================================================
// to_string
// ============================================================================

TEST_F(EndpointTest, ToString_UDP) {
    Endpoint ep("192.168.1.1", 30490, TransportProtocol::UDP);
    EXPECT_EQ(ep.to_string(), "udp://192.168.1.1:30490");
}

TEST_F(EndpointTest, ToString_TCP) {
    Endpoint ep("10.0.0.1", 5000, TransportProtocol::TCP);
    EXPECT_EQ(ep.to_string(), "tcp://10.0.0.1:5000");
}

TEST_F(EndpointTest, ToString_Multicast) {
    Endpoint ep("239.1.2.3", 30490, TransportProtocol::MULTICAST_UDP);
    EXPECT_EQ(ep.to_string(), "multicast://239.1.2.3:30490");
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST_F(EndpointTest, EqualityOperator) {
    Endpoint a("10.0.0.1", 5000, TransportProtocol::UDP);
    Endpoint b("10.0.0.1", 5000, TransportProtocol::UDP);
    Endpoint c("10.0.0.2", 5000, TransportProtocol::UDP);
    Endpoint d("10.0.0.1", 5001, TransportProtocol::UDP);
    Endpoint e("10.0.0.1", 5000, TransportProtocol::TCP);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);   // different address
    EXPECT_FALSE(a == d);   // different port
    EXPECT_FALSE(a == e);   // different protocol
}

TEST_F(EndpointTest, InequalityOperator) {
    Endpoint a("10.0.0.1", 5000, TransportProtocol::UDP);
    Endpoint b("10.0.0.1", 5000, TransportProtocol::UDP);
    Endpoint c("10.0.0.2", 5000, TransportProtocol::UDP);

    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a != c);
}

TEST_F(EndpointTest, LessThanOperator_ProtocolOrdering) {
    Endpoint udp("10.0.0.1", 5000, TransportProtocol::UDP);
    Endpoint tcp("10.0.0.1", 5000, TransportProtocol::TCP);
    Endpoint mcast("10.0.0.1", 5000, TransportProtocol::MULTICAST_UDP);

    EXPECT_TRUE(udp < tcp);
    EXPECT_TRUE(tcp < mcast);
    EXPECT_FALSE(mcast < udp);
}

TEST_F(EndpointTest, LessThanOperator_AddressOrdering) {
    Endpoint a("10.0.0.1", 5000, TransportProtocol::UDP);
    Endpoint b("10.0.0.2", 5000, TransportProtocol::UDP);

    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}

TEST_F(EndpointTest, LessThanOperator_PortOrdering) {
    Endpoint a("10.0.0.1", 5000, TransportProtocol::UDP);
    Endpoint b("10.0.0.1", 5001, TransportProtocol::UDP);

    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}

// ============================================================================
// Hash function
// ============================================================================

TEST_F(EndpointTest, Hash_SameEndpointsSameHash) {
    Endpoint a("10.0.0.1", 5000, TransportProtocol::UDP);
    Endpoint b("10.0.0.1", 5000, TransportProtocol::UDP);

    Endpoint::Hash hasher;
    EXPECT_EQ(hasher(a), hasher(b));
}

TEST_F(EndpointTest, Hash_ConsistentForSameEndpoint) {
    Endpoint a("10.0.0.1", 5000, TransportProtocol::UDP);
    Endpoint b("10.0.0.2", 5000, TransportProtocol::UDP);

    Endpoint::Hash hasher;
    EXPECT_EQ(hasher(a), hasher(a));
    EXPECT_EQ(hasher(b), hasher(b));
}

TEST_F(EndpointTest, Hash_UsableInUnorderedSet) {
    std::unordered_set<Endpoint, Endpoint::Hash> endpoints;

    endpoints.insert(Endpoint("10.0.0.1", 5000, TransportProtocol::UDP));
    endpoints.insert(Endpoint("10.0.0.1", 5000, TransportProtocol::UDP));
    endpoints.insert(Endpoint("10.0.0.2", 5000, TransportProtocol::UDP));

    EXPECT_EQ(endpoints.size(), 2u);
}

// ============================================================================
// Predefined endpoints
// ============================================================================

TEST_F(EndpointTest, PredefinedEndpoints) {
    EXPECT_EQ(SOMEIP_SD_MULTICAST_ENDPOINT.get_address(), "239.118.122.69");
    EXPECT_EQ(SOMEIP_SD_MULTICAST_ENDPOINT.get_port(), 30490);
    EXPECT_EQ(SOMEIP_SD_MULTICAST_ENDPOINT.get_protocol(), TransportProtocol::MULTICAST_UDP);
    EXPECT_TRUE(SOMEIP_SD_MULTICAST_ENDPOINT.is_multicast());
    EXPECT_TRUE(SOMEIP_SD_MULTICAST_ENDPOINT.is_valid());

    EXPECT_EQ(SOMEIP_DEFAULT_UDP_ENDPOINT.get_address(), "127.0.0.1");
    EXPECT_EQ(SOMEIP_DEFAULT_UDP_ENDPOINT.get_port(), 30490);
    EXPECT_EQ(SOMEIP_DEFAULT_UDP_ENDPOINT.get_protocol(), TransportProtocol::UDP);

    EXPECT_EQ(SOMEIP_DEFAULT_TCP_ENDPOINT.get_address(), "127.0.0.1");
    EXPECT_EQ(SOMEIP_DEFAULT_TCP_ENDPOINT.get_port(), 30490);
    EXPECT_EQ(SOMEIP_DEFAULT_TCP_ENDPOINT.get_protocol(), TransportProtocol::TCP);
}

// ============================================================================
// IPv4 edge cases
// ============================================================================

TEST_F(EndpointTest, IPv4_SingleZeroOctet) {
    EXPECT_TRUE(Endpoint("0.0.0.0", 1).is_ipv4());
}

TEST_F(EndpointTest, IPv4_MaxLengthValid) {
    EXPECT_TRUE(Endpoint("255.255.255.255", 1).is_ipv4());  // 15 chars exactly
}

TEST_F(EndpointTest, IPv4_DotOnly) {
    EXPECT_FALSE(Endpoint("...", 1).is_ipv4());
}

TEST_F(EndpointTest, IPv4_StartsWithDot) {
    EXPECT_FALSE(Endpoint(".1.2.3", 1).is_ipv4());
}

TEST_F(EndpointTest, IPv4_EndsWithDot) {
    EXPECT_FALSE(Endpoint("1.2.3.", 1).is_ipv4());
}

// ============================================================================
// IPv6 edge cases
// ============================================================================

TEST_F(EndpointTest, IPv6_SingleGroup) {
    EXPECT_FALSE(Endpoint("2001", 1).is_ipv6());
}

TEST_F(EndpointTest, IPv6_CompressionAtStart) {
    EXPECT_TRUE(Endpoint("::1:2:3:4:5:6", 1).is_ipv6());    // 6 groups + ::
}

TEST_F(EndpointTest, IPv6_CompressionAtEnd) {
    EXPECT_TRUE(Endpoint("1:2:3:4:5:6::", 1).is_ipv6());     // 6 groups + ::
}

TEST_F(EndpointTest, IPv6_CompressionInMiddle) {
    EXPECT_TRUE(Endpoint("1:2::5:6:7:8", 1).is_ipv6());      // 6 groups + ::
}

TEST_F(EndpointTest, IPv6_MaxGroupsWithCompression) {
    EXPECT_TRUE(Endpoint("1:2:3:4:5:6::7", 1).is_ipv6());    // 7 groups + :: = ok
}

// ============================================================================
// is_ipv4 / is_ipv6 mutual exclusivity
// ============================================================================

TEST_F(EndpointTest, IPv4_IsNotIPv6) {
    Endpoint ep("192.168.1.1", 1);
    EXPECT_TRUE(ep.is_ipv4());
    EXPECT_FALSE(ep.is_ipv6());
}

TEST_F(EndpointTest, IPv6_IsNotIPv4) {
    Endpoint ep("::1", 1);
    EXPECT_FALSE(ep.is_ipv4());
    EXPECT_TRUE(ep.is_ipv6());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
