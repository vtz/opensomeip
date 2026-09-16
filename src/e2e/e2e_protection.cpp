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

#include "e2e/e2e_protection.h"

#include "e2e/e2e_config.h"
#include "e2e/e2e_profile.h"
#include "e2e/e2e_profile_registry.h"
#include "e2e/e2e_header.h"
#include "someip/message.h"
#include "common/result.h"

#include <optional>

namespace someip::e2e {

namespace {

/**
 * Resolve the active profile and reject layouts Message cannot represent.
 * @implements REQ_E2E_PLUGIN_005
 * @satisfies feat_req_someip_102
 * @satisfies feat_req_someip_103
 */
Result resolve_supported_profile(const E2EConfig& config, E2EProfile*& profile) {
    if (config.offset != E2EConfig::kDefaultOffsetBits) {
        return Result::INVALID_ARGUMENT;
    }

    E2EProfileRegistry& registry = E2EProfileRegistry::instance();
    profile = registry.get_profile(config.profile_id);
    if (profile == nullptr) {
        profile = registry.get_profile(config.profile_name);
    }
    if (profile == nullptr) {
        profile = registry.get_default_profile();
    }
    if (profile == nullptr) {
        return Result::NOT_INITIALIZED;
    }
    if (profile->get_header_size() != E2EHeader::get_header_size()) {
        return Result::INVALID_ARGUMENT;
    }
    return Result::SUCCESS;
}

}  // namespace

/**
 * @brief Add E2E protection to a SOME/IP message
 * @implements REQ_E2E_PLUGIN_001
 * @implements REQ_E2E_PLUGIN_004
 * @implements REQ_E2E_PLUGIN_005
 * @satisfies feat_req_someip_102
 * @satisfies feat_req_someip_103
 */
Result E2EProtection::protect(Message& message, const E2EConfig& config) {
    E2EProfile* profile = nullptr;
    Result const layout = resolve_supported_profile(config, profile);
    if (layout != Result::SUCCESS) {
        return layout;
    }
    return profile->protect(message, config);
}

/**
 * @brief Validate E2E protection of a SOME/IP message
 * @implements REQ_E2E_PLUGIN_001
 * @implements REQ_E2E_PLUGIN_004
 * @implements REQ_E2E_PLUGIN_005
 * @satisfies feat_req_someip_102
 * @satisfies feat_req_someip_103
 */
Result E2EProtection::validate(const Message& message, const E2EConfig& config) {
    E2EProfile* profile = nullptr;
    Result const layout = resolve_supported_profile(config, profile);
    if (layout != Result::SUCCESS) {
        return layout;
    }
    return profile->validate(message, config);
}

std::optional<E2EHeader> E2EProtection::extract_header(const Message& message) {
    return message.get_e2e_header();
}

bool E2EProtection::has_e2e_protection(const Message& message) const {
    return message.has_e2e_header();
}

}  // namespace someip::e2e
