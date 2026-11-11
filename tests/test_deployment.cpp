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

/**
 * @file test_deployment.cpp
 * @brief Deployment document loads into SdConfig and ServiceInstance values.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <string>

#include "sd/deployment.h"

using someip::Result;
using someip::sd::DEPLOYMENT_FILE_MAX_BYTES;
using someip::sd::Deployment;
using someip::sd::SdConfig;
using someip::sd::load_deployment;
using someip::sd::load_deployment_file;
using someip::sd::pick_initial_wait_ms;

namespace {

std::string deployment_temp_path(const char* name)
{
    const char* dir = std::getenv("TEMP");
    if (dir == nullptr || dir[0] == '\0') {
        dir = std::getenv("TMPDIR");
    }
    if (dir == nullptr || dir[0] == '\0') {
#ifdef _WIN32
        dir = ".";
#else
        dir = "/tmp";
#endif
    }
    std::string path(dir);
    const char last = path.back();
    if (last != '/' && last != '\\') {
#ifdef _WIN32
        path.push_back('\\');
#else
        path.push_back('/');
#endif
    }
    path += name;
    return path;
}

constexpr char kValid[] = R"({
  "sd": {
    "multicast_address": "239.255.255.251",
    "multicast_port": 30490,
    "unicast_address": "127.0.0.1",
    "initial_delay_min_ms": 10,
    "initial_delay_max_ms": 50,
    "repetition_base_ms": 200,
    "repetition_max_ms": 1000,
    "repetition_multiplier": 2,
    "cyclic_offer_ms": 5000,
    "ttl_ms": 60000
  },
  "provided_services": [
    {
      "service_id": "0x1000",
      "instance_id": "0x0001",
      "major_version": 1,
      "minor_version": 7,
      "unreliable_port": 30500,
      "reliable_port": 30501,
      "eventgroups": [1, 2]
    },
    {
      "service_id": 4097,
      "instance_id": 1,
      "major_version": 1,
      "minor_version": 0,
      "unreliable_port": 0,
      "reliable_port": 40000,
      "eventgroups": []
    }
  ],
  "required_services": [
    {
      "service_id": "0x1000",
      "instance_id": 1,
      "major_version": 1,
      "minor_version": 7,
      "unreliable_port": 0,
      "reliable_port": 0,
      "eventgroups": [1]
    }
  ]
})";

}  // namespace

/**
 * @test_case TC_DEPLOY_001
 * @brief A valid document fills SdConfig and the service objects the offer API uses.
 */
TEST(DeploymentTest, ValidDocumentMapsOntoExistingTypes)
{
    Deployment deployment;
    ASSERT_EQ(load_deployment(kValid, std::strlen(kValid), deployment), Result::SUCCESS);

    EXPECT_EQ(deployment.sd.multicast_address, "239.255.255.251");
    EXPECT_EQ(deployment.sd.multicast_port, 30490);
    EXPECT_EQ(deployment.sd.unicast_address, "127.0.0.1");
    EXPECT_EQ(deployment.sd.initial_delay_min, std::chrono::milliseconds(10));
    EXPECT_EQ(deployment.sd.initial_delay_max, std::chrono::milliseconds(50));
    EXPECT_EQ(deployment.sd.initial_delay, deployment.sd.initial_delay_max);
    EXPECT_EQ(deployment.sd.repetition_base, std::chrono::milliseconds(200));
    EXPECT_EQ(deployment.sd.repetition_max, std::chrono::milliseconds(1000));
    EXPECT_EQ(deployment.sd.repetition_multiplier, 2);
    EXPECT_EQ(deployment.sd.cyclic_offer, std::chrono::milliseconds(5000));
    EXPECT_EQ(deployment.sd.ttl, std::chrono::milliseconds(60000));

    ASSERT_EQ(deployment.provided_services.size(), 2u);
    const auto& udp = deployment.provided_services[0];
    EXPECT_EQ(udp.instance.service_id, 0x1000);
    EXPECT_EQ(udp.instance.instance_id, 0x0001);
    EXPECT_EQ(udp.instance.major_version, 1);
    EXPECT_EQ(udp.instance.minor_version, 7u);
    EXPECT_EQ(udp.instance.ip_address, "127.0.0.1");
    EXPECT_EQ(udp.instance.port, 30500);
    EXPECT_EQ(udp.instance.protocol, 0x11);
    EXPECT_EQ(udp.unreliable_port, 30500);
    EXPECT_EQ(udp.reliable_port, 30501);
    ASSERT_EQ(udp.eventgroups.size(), 2u);
    EXPECT_EQ(udp.eventgroups[0], 1);
    EXPECT_EQ(udp.eventgroups[1], 2);

    const auto& tcp = deployment.provided_services[1];
    EXPECT_EQ(tcp.instance.service_id, 4097);
    EXPECT_EQ(tcp.instance.port, 40000);
    EXPECT_EQ(tcp.instance.protocol, 0x06);
    EXPECT_TRUE(tcp.eventgroups.empty());

    ASSERT_EQ(deployment.required_services.size(), 1u);
    const auto& required = deployment.required_services[0];
    EXPECT_EQ(required.instance.service_id, 0x1000);
    EXPECT_EQ(required.instance.instance_id, 1);
    EXPECT_TRUE(required.instance.ip_address.empty());
    EXPECT_EQ(required.instance.port, 0);
    ASSERT_EQ(required.eventgroups.size(), 1u);
    EXPECT_EQ(required.eventgroups[0], 1);
}

/**
 * @test_case TC_DEPLOY_002
 * @brief The same service id and instance id cannot be listed twice on one side.
 */
TEST(DeploymentTest, DuplicateInstanceIsRejected)
{
    const char* text = R"({
      "sd": {
        "multicast_address": "239.255.255.251",
        "multicast_port": 30490,
        "initial_delay_min_ms": 0,
        "initial_delay_max_ms": 100,
        "repetition_base_ms": 2000,
        "repetition_max_ms": 3600000,
        "repetition_multiplier": 2,
        "cyclic_offer_ms": 30000,
        "ttl_ms": 3600000
      },
      "provided_services": [
        {
          "service_id": 1, "instance_id": 1, "major_version": 1, "minor_version": 0,
          "unreliable_port": 30500, "reliable_port": 0, "eventgroups": []
        },
        {
          "service_id": 1, "instance_id": 1, "major_version": 1, "minor_version": 0,
          "unreliable_port": 30501, "reliable_port": 0, "eventgroups": []
        }
      ],
      "required_services": []
    })";
    Deployment deployment;
    deployment.sd.multicast_port = 9;
    EXPECT_EQ(load_deployment(text, std::strlen(text), deployment), Result::INVALID_ARGUMENT);
    EXPECT_TRUE(deployment.provided_services.empty());
    EXPECT_NE(deployment.sd.multicast_port, 9);
}

/**
 * @test_case TC_DEPLOY_003
 * @brief Syntax errors, unknown keys, and missing required keys fail the load.
 */
TEST(DeploymentTest, MalformedTextUnknownKeyAndMissingFieldFail)
{
    Deployment deployment;
    EXPECT_EQ(load_deployment("{", 1, deployment), Result::MALFORMED_MESSAGE);

    std::string unknown(kValid);
    const auto sd = unknown.find("\"sd\"");
    ASSERT_NE(sd, std::string::npos);
    unknown.insert(sd, "\"extra\": 1, ");
    EXPECT_EQ(load_deployment(unknown.data(), unknown.size(), deployment),
              Result::INVALID_ARGUMENT);

    std::string missing(kValid);
    const auto port = missing.find("\"multicast_port\": 30490,");
    ASSERT_NE(port, std::string::npos);
    missing.erase(port, std::strlen("\"multicast_port\": 30490,"));
    EXPECT_EQ(load_deployment(missing.data(), missing.size(), deployment),
              Result::INVALID_ARGUMENT);

    const char* no_port = R"({
      "sd": {
        "multicast_address": "239.255.255.251",
        "multicast_port": 30490,
        "initial_delay_min_ms": 0,
        "initial_delay_max_ms": 1,
        "repetition_base_ms": 1,
        "repetition_max_ms": 1,
        "repetition_multiplier": 1,
        "cyclic_offer_ms": 1,
        "ttl_ms": 1000
      },
      "provided_services": [{
        "service_id": 1, "instance_id": 1, "major_version": 1, "minor_version": 0,
        "unreliable_port": 0, "reliable_port": 0, "eventgroups": []
      }],
      "required_services": []
    })";
    EXPECT_EQ(load_deployment(no_port, std::strlen(no_port), deployment), Result::INVALID_ARGUMENT);
}

/**
 * @test_case TC_DEPLOY_004
 * @brief A file on disk loads the same document. Parsing does not need an SD socket.
 */
TEST(DeploymentTest, ValidFileLoadsAndMissingFileFails)
{
    const std::string path = deployment_temp_path("opensomeip-deployment.json");
    const std::string missing = deployment_temp_path("opensomeip-deployment-missing.json");
    std::remove(missing.c_str());

    std::FILE* file = std::fopen(path.c_str(), "wb");
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(std::fwrite(kValid, 1, std::strlen(kValid), file), std::strlen(kValid));
    std::fclose(file);

    Deployment deployment;
    EXPECT_EQ(load_deployment_file(path.c_str(), deployment), Result::SUCCESS);
    EXPECT_EQ(deployment.provided_services.size(), 2u);
    EXPECT_EQ(deployment.sd.multicast_port, 30490);
    std::remove(path.c_str());

    EXPECT_EQ(load_deployment_file(missing.c_str(), deployment), Result::INVALID_ARGUMENT);
    EXPECT_TRUE(deployment.provided_services.empty());
}

constexpr char kTiming[] = R"({
  "sd": {
    "multicast_address": "239.255.255.251",
    "multicast_port": 30490,
    "initial_delay_min_ms": 0,
    "initial_delay_max_ms": 100,
    "repetition_base_ms": 2000,
    "repetition_max_ms": 3600000,
    "repetition_multiplier": 2,
    "cyclic_offer_ms": 5000,
    "ttl_ms": 3600000
  },
  "provided_services": [{
    "service_id": 1, "instance_id": 1, "major_version": 1, "minor_version": 0,
    "unreliable_port": 0, "reliable_port": 40000, "eventgroups": []
  }],
  "required_services": []
})";

std::string replace_once(const char* from, const char* to)
{
    std::string text(kTiming);
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos);
    if (pos == std::string::npos) {
        return text;
    }
    text.replace(pos, std::strlen(from), to);
    return text;
}

void expect_rejected(const std::string& text)
{
    Deployment deployment;
    deployment.sd.multicast_port = 9;
    EXPECT_EQ(load_deployment(text.data(), text.size(), deployment), Result::INVALID_ARGUMENT) << text;
    EXPECT_NE(deployment.sd.multicast_port, 9);
    EXPECT_TRUE(deployment.provided_services.empty());
}

/**
 * @test_case TC_DEPLOY_005
 * @brief A reliable-port-only service is TCP, and an omitted unicast address keeps the default.
 */
TEST(DeploymentTest, ReliablePortOnlyServiceIsTcp)
{
    Deployment deployment;
    ASSERT_EQ(load_deployment(kTiming, std::strlen(kTiming), deployment), Result::SUCCESS);
    ASSERT_EQ(deployment.provided_services.size(), 1u);
    const auto& tcp = deployment.provided_services[0];
    EXPECT_EQ(tcp.unreliable_port, 0);
    EXPECT_EQ(tcp.reliable_port, 40000);
    EXPECT_EQ(tcp.instance.port, 40000);
    EXPECT_EQ(tcp.instance.protocol, 0x06);
    EXPECT_EQ(tcp.instance.ip_address, "127.0.0.1");
    EXPECT_EQ(deployment.sd.initial_delay_min, std::chrono::milliseconds(0));
}

/**
 * @test_case TC_DEPLOY_006
 * @brief Timing, address, and span values the offer loop cannot run are rejected.
 */
TEST(DeploymentTest, UnsafeTimingAndEmptyAddressesAreRejected)
{
    expect_rejected(replace_once("\"ttl_ms\": 3600000", "\"ttl_ms\": 999"));
    expect_rejected(replace_once("\"repetition_multiplier\": 2", "\"repetition_multiplier\": 0"));
    expect_rejected(replace_once("\"repetition_base_ms\": 2000", "\"repetition_base_ms\": 0"));
    expect_rejected(replace_once("\"cyclic_offer_ms\": 5000", "\"cyclic_offer_ms\": 0"));
    expect_rejected(replace_once("\"initial_delay_max_ms\": 100",
                                 "\"initial_delay_max_ms\": 4294967295"));
    expect_rejected(replace_once("\"initial_delay_min_ms\": 0", "\"initial_delay_min_ms\": 200"));
    expect_rejected(replace_once("\"multicast_address\": \"239.255.255.251\"",
                                 "\"multicast_address\": \"\""));
    expect_rejected(replace_once("\"multicast_address\": \"239.255.255.251\"",
                                 "\"multicast_address\": \"239.255.255.251\", \"unicast_address\": \"\""));

    const std::string ttl_ok = replace_once("\"ttl_ms\": 3600000", "\"ttl_ms\": 1000");
    Deployment deployment;
    EXPECT_EQ(load_deployment(ttl_ok.data(), ttl_ok.size(), deployment), Result::SUCCESS);
    EXPECT_EQ(deployment.sd.ttl, std::chrono::milliseconds(1000));
}

/**
 * @test_case TC_DEPLOY_007
 * @brief One leading UTF-8 BOM is skipped. A BOM after the first byte is malformed.
 */
TEST(DeploymentTest, LeadingBomIsSkipped)
{
    const std::string bom = std::string("\xEF\xBB\xBF", 3);
    const std::string leading = bom + kTiming;
    Deployment deployment;
    EXPECT_EQ(load_deployment(leading.data(), leading.size(), deployment), Result::SUCCESS);
    EXPECT_EQ(deployment.sd.multicast_port, 30490);

    const std::string after_space = std::string(" ") + bom + kTiming;
    deployment.sd.multicast_port = 9;
    EXPECT_EQ(load_deployment(after_space.data(), after_space.size(), deployment),
              Result::MALFORMED_MESSAGE);
    EXPECT_NE(deployment.sd.multicast_port, 9);

    std::string after_brace(kTiming);
    const auto brace = after_brace.find('{');
    ASSERT_NE(brace, std::string::npos);
    after_brace.insert(brace + 1, bom);
    EXPECT_EQ(load_deployment(after_brace.data(), after_brace.size(), deployment),
              Result::MALFORMED_MESSAGE);
}

/**
 * @test_case TC_DEPLOY_008
 * @brief A file one byte past the cap is not parsed. A file of exactly the cap is.
 */
TEST(DeploymentTest, FilePastCapIsOverflowAndExactCapLoads)
{
    const std::string exact_path = deployment_temp_path("opensomeip-deployment-exact.json");
    const std::string over_path = deployment_temp_path("opensomeip-deployment-over.json");
    std::string exact(kTiming);
    ASSERT_LT(exact.size(), DEPLOYMENT_FILE_MAX_BYTES);
    exact.append(DEPLOYMENT_FILE_MAX_BYTES - exact.size(), ' ');
    const std::string over = exact + " ";

    std::FILE* exact_file = std::fopen(exact_path.c_str(), "wb");
    ASSERT_NE(exact_file, nullptr);
    ASSERT_EQ(std::fwrite(exact.data(), 1, exact.size(), exact_file), exact.size());
    std::fclose(exact_file);

    std::FILE* over_file = std::fopen(over_path.c_str(), "wb");
    ASSERT_NE(over_file, nullptr);
    ASSERT_EQ(std::fwrite(over.data(), 1, over.size(), over_file), over.size());
    std::fclose(over_file);

    Deployment deployment;
    EXPECT_EQ(load_deployment_file(exact_path.c_str(), deployment), Result::SUCCESS);
    EXPECT_EQ(deployment.provided_services.size(), 1u);
    EXPECT_EQ(deployment.provided_services[0].instance.protocol, 0x06);

    deployment.sd.multicast_port = 9;
    EXPECT_EQ(load_deployment_file(over_path.c_str(), deployment), Result::BUFFER_OVERFLOW);
    EXPECT_NE(deployment.sd.multicast_port, 9);
    EXPECT_TRUE(deployment.provided_services.empty());

    std::remove(exact_path.c_str());
    std::remove(over_path.c_str());
}

/**
 * @test_case TC_DEPLOY_009
 * @brief The full uint32 initial-wait span does not divide by zero.
 */
TEST(DeploymentTest, FullUint32InitialWaitDoesNotDivideByZero)
{
    SdConfig config;
    config.initial_delay_min = std::chrono::milliseconds(0);
    config.initial_delay_max = std::chrono::milliseconds(0xFFFFFFFFLL);
    config.initial_delay = config.initial_delay_max;
    for (int i = 0; i < 8; ++i) {
        const uint32_t wait = pick_initial_wait_ms(config);
        EXPECT_LE(wait, 0xFFFFFFFFu);
    }

    expect_rejected(replace_once("\"initial_delay_max_ms\": 100",
                                 "\"initial_delay_max_ms\": 4294967295"));
}

std::string deployment_document(int service_count, int eventgroup_count)
{
    std::string text =
        R"({"sd":{"multicast_address":"239.255.255.251","multicast_port":30490,)"
        R"("initial_delay_min_ms":0,"initial_delay_max_ms":100,"repetition_base_ms":2000,)"
        R"("repetition_max_ms":3600000,"repetition_multiplier":2,"cyclic_offer_ms":5000,)"
        R"("ttl_ms":3600000},"provided_services":[)";
    for (int service = 0; service < service_count; ++service) {
        if (service != 0) {
            text += ',';
        }
        text += R"({"service_id":)";
        text += std::to_string(service + 1);
        text += R"(,"instance_id":1,"major_version":1,"minor_version":0,)";
        text += R"("unreliable_port":30500,"reliable_port":0,"eventgroups":[)";
        for (int group = 0; group < eventgroup_count; ++group) {
            if (group != 0) {
                text += ',';
            }
            text += std::to_string(group + 1);
        }
        text += "]}";
    }
    text += R"(],"required_services":[]})";
    return text;
}

void expect_overflow(const std::string& text)
{
    Deployment deployment;
    deployment.sd.multicast_port = 9;
    EXPECT_EQ(load_deployment(text.data(), text.size(), deployment), Result::BUFFER_OVERFLOW) << text;
    EXPECT_NE(deployment.sd.multicast_port, 9);
    EXPECT_TRUE(deployment.provided_services.empty());
    EXPECT_TRUE(deployment.required_services.empty());
}

/**
 * @test_case TC_DEPLOY_010
 * @brief Service and eventgroup caps apply on every allocation backend.
 */
TEST(DeploymentTest, ServiceAndEventgroupCapsAreEnforced)
{
    expect_overflow(deployment_document(5, 1));
    expect_overflow(deployment_document(1, 9));

    const std::string four = deployment_document(4, 1);
    Deployment services;
    ASSERT_EQ(load_deployment(four.data(), four.size(), services), Result::SUCCESS);
    EXPECT_EQ(services.provided_services.size(), 4u);

    const std::string eight = deployment_document(1, 8);
    Deployment groups;
    ASSERT_EQ(load_deployment(eight.data(), eight.size(), groups), Result::SUCCESS);
    ASSERT_EQ(groups.provided_services.size(), 1u);
    EXPECT_EQ(groups.provided_services[0].eventgroups.size(), 8u);
}

/**
 * @test_case TC_DEPLOY_011
 * @brief A zero repetition maximum would collapse the offer loop to 1 ms.
 */
TEST(DeploymentTest, ZeroRepetitionMaxIsRejected)
{
    expect_rejected(replace_once("\"repetition_max_ms\": 3600000", "\"repetition_max_ms\": 0"));

    const std::string positive = replace_once("\"repetition_max_ms\": 3600000", "\"repetition_max_ms\": 1");
    Deployment deployment;
    ASSERT_EQ(load_deployment(positive.data(), positive.size(), deployment), Result::SUCCESS);
    EXPECT_EQ(deployment.sd.repetition_max, std::chrono::milliseconds(1));
}
