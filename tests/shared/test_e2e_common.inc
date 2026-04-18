/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * Shared E2E protection tests for RTOS and Zephyr test suites.
 *
 * Platform-independent subset of the host test_e2e.cpp suite covering:
 *   - E2E header serialization/deserialization
 *   - CRC-8 SAE-J1850, CRC-16 ITU-T X.25, CRC-32
 *   - Profile registry
 *   - Protect/validate roundtrip
 *   - CRC corruption detection
 *
 * Requires:  CHECK(cond, name)  macro defined before inclusion.
 *            test_e2e() calls initialize_basic_profile() internally.
 */

#ifndef SOMEIP_TEST_E2E_COMMON_INC
#define SOMEIP_TEST_E2E_COMMON_INC

#include "e2e/e2e_protection.h"
#include "e2e/e2e_header.h"
#include "e2e/e2e_config.h"
#include "e2e/e2e_crc.h"
#include "e2e/e2e_profile_registry.h"
#include "e2e/e2e_profiles/standard_profile.h"
#include "someip/message.h"
#include "common/result.h"
#include <cstdio>
#include <vector>

static void test_e2e() {
    using namespace someip;
    using namespace someip::e2e;

    printf("\n--- E2E protection tests ---\n");

    initialize_basic_profile();

    // Header serialization/deserialization roundtrip
    {
        E2EHeader header(0x12345678, 0xABCDEF00, 0x1234, 0x5678);
        std::vector<uint8_t> serialized = header.serialize();
        CHECK(serialized.size() == E2EHeader::get_header_size(),
              "e2e_header_serialize_size");

        E2EHeader deserialized;
        CHECK(deserialized.deserialize(serialized), "e2e_header_deserialize");
        CHECK(deserialized.crc == header.crc, "e2e_header_crc_match");
        CHECK(deserialized.counter == header.counter, "e2e_header_counter_match");
        CHECK(deserialized.data_id == header.data_id, "e2e_header_data_id_match");
        CHECK(deserialized.freshness_value == header.freshness_value,
              "e2e_header_freshness_match");
    }

    // Header size is exactly 12 bytes
    CHECK(E2EHeader::get_header_size() == 12u, "e2e_header_size_12");

    // Header boundary value roundtrip
    {
        E2EHeader zero(0, 0, 0, 0);
        E2EHeader max_vals(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF, 0xFFFF);

        auto sz = zero.serialize();
        auto sm = max_vals.serialize();

        E2EHeader dz, dm;
        CHECK(dz.deserialize(sz), "e2e_zero_roundtrip");
        CHECK(dm.deserialize(sm), "e2e_max_roundtrip");
        CHECK(dz.crc == 0u, "e2e_zero_crc");
        CHECK(dm.crc == 0xFFFFFFFFu, "e2e_max_crc");
        CHECK(dm.data_id == 0xFFFFu, "e2e_max_data_id");
    }

    // Header deserialization fails on short buffer
    {
        E2EHeader header;
        std::vector<uint8_t> short_buf(8, 0x00);
        CHECK(!header.deserialize(short_buf), "e2e_short_buffer_fails");
    }

    // CRC-8 SAE-J1850
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        uint8_t crc = e2ecrc::calculate_crc8_sae_j1850(data);
        CHECK(crc != 0, "crc8_nonzero");

        std::vector<uint8_t> empty;
        uint8_t crc_empty = e2ecrc::calculate_crc8_sae_j1850(empty);
        CHECK(crc_empty == 0xFF, "crc8_empty_init_value");

        // Determinism
        uint8_t crc_b = e2ecrc::calculate_crc8_sae_j1850(data);
        CHECK(crc == crc_b, "crc8_deterministic");
    }

    // CRC-16 ITU-T X.25
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        uint16_t crc = e2ecrc::calculate_crc16_itu_x25(data);
        CHECK(crc != 0, "crc16_nonzero");

        std::vector<uint8_t> empty;
        uint16_t crc_empty = e2ecrc::calculate_crc16_itu_x25(empty);
        CHECK(crc_empty == 0xFFFF, "crc16_empty_init_value");
    }

    // CRC-32
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        uint32_t crc = e2ecrc::calculate_crc32(data);
        CHECK(crc != 0, "crc32_nonzero");
    }

    // CRC type dispatch and bounds guard
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        auto crc8  = e2ecrc::calculate_crc(data, 0, 4, 0);
        auto crc16 = e2ecrc::calculate_crc(data, 0, 4, 1);
        auto crc32 = e2ecrc::calculate_crc(data, 0, 4, 2);
        auto unk   = e2ecrc::calculate_crc(data, 0, 4, 255);

        CHECK(crc8.has_value() && crc8.value() == static_cast<uint32_t>(e2ecrc::calculate_crc8_sae_j1850(data)),
              "crc_dispatch_type0");
        CHECK(crc16.has_value() && crc16.value() == static_cast<uint32_t>(e2ecrc::calculate_crc16_itu_x25(data)),
              "crc_dispatch_type1");
        CHECK(crc32.has_value() && crc32.value() == e2ecrc::calculate_crc32(data), "crc_dispatch_type2");
        CHECK(!unk.has_value(), "crc_unknown_type_nullopt");

        auto oob = e2ecrc::calculate_crc(data, 2, 5, 0);
        CHECK(!oob.has_value(), "crc_oob_returns_nullopt");
    }

    // Profile registry
    {
        E2EProfileRegistry& registry = E2EProfileRegistry::instance();
        E2EProfile* profile = registry.get_default_profile();
        CHECK(profile != nullptr, "e2e_default_profile_exists");
        if (profile) {
            CHECK(profile->get_profile_id() == 0, "e2e_profile_id_zero");
            CHECK(profile->get_profile_name() == "basic", "e2e_profile_name_basic");
        }

        // Unknown profile returns nullptr
        CHECK(registry.get_profile(static_cast<uint32_t>(999)) == nullptr,
              "e2e_unknown_profile_null");
    }

    // Protect and validate roundtrip
    {
        E2EProtection protection;
        Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
        msg.set_payload({0x01, 0x02, 0x03, 0x04});

        E2EConfig config(0x1234);
        config.enable_crc = true;
        config.enable_counter = true;
        config.enable_freshness = false;
        config.crc_type = 1;

        Result result = protection.protect(msg, config);
        CHECK(result == Result::SUCCESS, "e2e_protect_success");
        CHECK(msg.has_e2e_header(), "e2e_header_present");

        result = protection.validate(msg, config);
        CHECK(result == Result::SUCCESS, "e2e_validate_success");
    }

    // CRC corruption is detected
    {
        E2EProtection protection;
        Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
        msg.set_payload({0x01, 0x02, 0x03, 0x04});

        E2EConfig config(0x1234);
        config.enable_crc = true;
        config.enable_counter = false;
        config.enable_freshness = false;
        config.crc_type = 1;

        Result setup = protection.protect(msg, config);
        CHECK(setup == Result::SUCCESS, "e2e_corrupt_setup_protect_success");

        auto header_opt = msg.get_e2e_header();
        CHECK(header_opt.has_value(), "e2e_corrupt_setup_header_present");

        if (setup == Result::SUCCESS && header_opt.has_value()) {
            E2EHeader corrupted = header_opt.value();
            corrupted.crc = 0xDEADBEEF;
            msg.set_e2e_header(corrupted);

            Result result = protection.validate(msg, config);
            CHECK(result != Result::SUCCESS, "e2e_corrupted_crc_rejected");
        }
    }

    // Validate without header fails
    {
        E2EProtection protection;
        Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
        msg.set_payload({0x01, 0x02});

        E2EConfig config(0x4444);
        Result result = protection.validate(msg, config);
        CHECK(result == Result::INVALID_ARGUMENT, "e2e_no_header_fails");
    }

    // All CRC types protect/validate
    {
        for (uint8_t crc_type = 0; crc_type <= 2; ++crc_type) {
            E2EProtection protection;
            Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
            msg.set_payload({0x01, 0x02, 0x03, 0x04});

            E2EConfig config(static_cast<uint16_t>(0xAA00 + crc_type));
            config.enable_crc = true;
            config.enable_counter = false;
            config.enable_freshness = false;
            config.crc_type = crc_type;

            Result r = protection.protect(msg, config);
            bool ok = (r == Result::SUCCESS) &&
                      (protection.validate(msg, config) == Result::SUCCESS);
            const char* names[] = {
                "e2e_crc_type0_roundtrip",
                "e2e_crc_type1_roundtrip",
                "e2e_crc_type2_roundtrip"
            };
            CHECK(ok, names[crc_type]);
        }
    }
}

#endif // SOMEIP_TEST_E2E_COMMON_INC
