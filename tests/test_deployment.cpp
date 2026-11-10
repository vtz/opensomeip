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
#include <cstring>
#include <gtest/gtest.h>
#include <string>

#include "sd/deployment.h"

using someip::Result;
using someip::sd::Deployment;
using someip::sd::load_deployment;
using someip::sd::load_deployment_file;

namespace {

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
    const char* path = "/tmp/opensomeip-382-deployment.json";
    std::FILE* file = std::fopen(path, "wb");
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(std::fwrite(kValid, 1, std::strlen(kValid), file), std::strlen(kValid));
    std::fclose(file);

    Deployment deployment;
    EXPECT_EQ(load_deployment_file(path, deployment), Result::SUCCESS);
    EXPECT_EQ(deployment.provided_services.size(), 2u);
    EXPECT_EQ(deployment.sd.multicast_port, 30490);
    std::remove(path);

    EXPECT_EQ(load_deployment_file("/tmp/opensomeip-382-deployment-missing.json", deployment),
              Result::INVALID_ARGUMENT);
    EXPECT_TRUE(deployment.provided_services.empty());
}
