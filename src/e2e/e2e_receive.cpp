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

#include "e2e/e2e_receive_policy.h"

#include "common/result.h"
#include "e2e/e2e_config.h"
#include "e2e/e2e_profile_registry.h"
#include "e2e/e2e_profiles/standard_profile.h"
#include "e2e/e2e_protection.h"
#include "someip/message.h"

#include <cstddef>
#include <cstdint>

namespace someip::e2e {

E2EReceiveBinding& E2EReceiveTable::slot(size_t index) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    return entries_[index];
}

const E2EReceiveBinding& E2EReceiveTable::slot(size_t index) const {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    return entries_[index];
}

Result E2EReceiveTable::add(const E2EReceiveBinding& binding) {
    if (binding.service_id == 0) {
        return Result::INVALID_ARGUMENT;
    }
    if (find(binding.service_id, binding.method_id) != nullptr) {
        return Result::INVALID_ARGUMENT;
    }
    if (count_ >= MAX_E2E_RECEIVE_BINDINGS) {
        return Result::RESOURCE_EXHAUSTED;
    }
    if (binding.profile_id == 0 &&
        E2EProfileRegistry::instance().get_profile(0) == nullptr) {
        initialize_basic_profile();
    }
    slot(count_) = binding;
    ++count_;
    return Result::SUCCESS;
}

const E2EReceiveBinding* E2EReceiveTable::find(uint16_t service_id, uint16_t method_id) const {
    for (size_t i = 0; i < count_; ++i) {
        const E2EReceiveBinding& entry = slot(i);
        if (entry.service_id == service_id && entry.method_id == method_id) {
            return &entry;
        }
    }
    return nullptr;
}

/** @implements REQ_E2E_RECEIVE_001 */
E2EReceiveOutcome receive_if_e2e_protected(const E2EReceiveTable& table, const uint8_t* data,
                                           size_t size, Message& message) {
    E2EReceiveOutcome outcome;
    if (table.empty() || data == nullptr || size < 4) {
        return outcome;
    }

    const auto service_id = static_cast<uint16_t>(
        (static_cast<uint32_t>(data[0]) << 8U) | static_cast<uint32_t>(data[1]));
    const auto method_id = static_cast<uint16_t>(
        (static_cast<uint32_t>(data[2]) << 8U) | static_cast<uint32_t>(data[3]));
    const E2EReceiveBinding* const binding = table.find(service_id, method_id);
    if (binding == nullptr) {
        return outcome;
    }

    const Result parsed = message.try_deserialize(data, size, /*expect_e2e=*/true);
    if (parsed != Result::SUCCESS) {
        outcome.status = E2EReceiveStatus::REJECTED;
        outcome.result = parsed;
        outcome.integrity_failure = false;
        return outcome;
    }

    if (binding->policy == E2EReceivePolicy::APPLICATION_MANAGED) {
        outcome.status = E2EReceiveStatus::ACCEPTED;
        outcome.result = Result::SUCCESS;
        return outcome;
    }

    if (binding->profile_id != 0 &&
        E2EProfileRegistry::instance().get_profile(binding->profile_id) == nullptr) {
        outcome.status = E2EReceiveStatus::REJECTED;
        outcome.result = Result::NOT_INITIALIZED;
        outcome.integrity_failure = true;
        return outcome;
    }

    E2EConfig config;
    config.profile_id = binding->profile_id;
    if (binding->profile_id == 0) {
        config.profile_name = "basic";
    }
    else {
        config.profile_name.clear();
    }
    config.data_id = binding->data_id;
    config.offset_bits = E2EConfig::DEFAULT_OFFSET_BITS;
    config.enable_crc = binding->enable_crc;
    config.enable_counter = binding->enable_counter;
    config.enable_freshness = binding->enable_freshness;
    config.crc_type = binding->crc_type;

    E2EProtection protection;
    const Result checked = protection.validate(message, config);
    if (checked != Result::SUCCESS) {
        outcome.status = E2EReceiveStatus::REJECTED;
        outcome.result = checked;
        outcome.integrity_failure = true;
        return outcome;
    }

    outcome.status = E2EReceiveStatus::ACCEPTED;
    outcome.result = Result::SUCCESS;
    return outcome;
}

}  // namespace someip::e2e
