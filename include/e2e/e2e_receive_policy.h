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

#ifndef E2E_RECEIVE_POLICY_H
#define E2E_RECEIVE_POLICY_H

#include "common/result.h"
#include "someip/message.h"

#include <array>
#include <cstddef>
#include <cstdint>

/**
 * @brief Bound on E2E receive bindings stored in one transport config.
 *
 * Adding past this cap returns Result::RESOURCE_EXHAUSTED from the table.
 * It is not reported through on_message_rejected.
 */
#ifndef SOMEIP_MAX_E2E_RECEIVE_BINDINGS
#define SOMEIP_MAX_E2E_RECEIVE_BINDINGS 16  // NOLINT(cppcoreguidelines-macro-usage)
#endif

namespace someip::e2e {

/**
 * @brief Who runs E2EProtection::validate on a configured receive.
 *
 * Both policies deserialize the default layout (Offset 64, 12-byte header)
 * so those bytes are not application payload. This receive path does not
 * accept a non-default Offset or a non-12-byte header.
 */
enum class E2EReceivePolicy : uint8_t {
    STACK_MANAGED,        ///< Validate before the application handler
    APPLICATION_MANAGED   ///< Deliver parsed header metadata; the app validates
};

/**
 * @brief One protected service method or event.
 *
 * Keyed by Service ID and Method/Event ID. profile_id 0 selects the basic
 * profile and registers it if needed. Checks use the default Offset-64
 * 12-byte layout only.
 */
struct E2EReceiveBinding {
    uint16_t service_id{0};
    uint16_t method_id{0};
    uint16_t data_id{0};
    uint32_t profile_id{0};
    E2EReceivePolicy policy{E2EReceivePolicy::STACK_MANAGED};
    bool enable_crc{true};
    bool enable_counter{true};
    bool enable_freshness{false};
    uint8_t crc_type{1};
    /// Copied into the stack-managed E2EConfig. Same default as E2EConfig.
    uint32_t max_counter_value{0xFFFFFFFF};
    /// Copied into the stack-managed E2EConfig. Same default as E2EConfig.
    uint32_t freshness_timeout_ms{1000};
};

inline constexpr size_t MAX_E2E_RECEIVE_BINDINGS = SOMEIP_MAX_E2E_RECEIVE_BINDINGS;

/**
 * @brief Fixed table of receive bindings. Copy it into a transport config
 * before start(); receive loops only read it.
 *
 * @implements REQ_E2E_RECEIVE_001
 */
class E2EReceiveTable {
public:
    Result add(const E2EReceiveBinding& binding);
    const E2EReceiveBinding* find(uint16_t service_id, uint16_t method_id) const;
    bool empty() const { return count_ == 0; }
    size_t size() const { return count_; }

private:
    E2EReceiveBinding& slot(size_t index);
    const E2EReceiveBinding& slot(size_t index) const;

    std::array<E2EReceiveBinding, MAX_E2E_RECEIVE_BINDINGS> entries_{};
    size_t count_{0};
};

enum class E2EReceiveStatus : uint8_t {
    NOT_CONFIGURED,  ///< No binding; caller keeps the historical deserialize
    ACCEPTED,        ///< @p message is ready for the application handler
    REJECTED         ///< Do not deliver; report @p result
};

struct E2EReceiveOutcome {
    E2EReceiveStatus status{E2EReceiveStatus::NOT_CONFIGURED};
    Result result{Result::SUCCESS};
    /// True when status is Rejected because validate() failed (E2E_INTEGRITY).
    /// False when the protected frame itself failed to deserialize.
    bool integrity_failure{false};
};

/**
 * @brief Apply the default-layout receive policy to one complete SOME/IP frame.
 *
 * Call this only after TP reassembly. A TP segment is not an E2E frame.
 * An empty table returns NotConfigured without touching @p message.
 *
 * @implements REQ_E2E_RECEIVE_001
 */
E2EReceiveOutcome receive_if_e2e_protected(const E2EReceiveTable& table, const uint8_t* data,
                                           size_t size, Message& message);

}  // namespace someip::e2e

#endif // E2E_RECEIVE_POLICY_H
