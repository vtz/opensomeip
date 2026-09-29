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

#ifndef SOMEIP_SD_MULTICAST_MEMBERSHIP_H
#define SOMEIP_SD_MULTICAST_MEMBERSHIP_H

#include "platform/containers.h"

#include <chrono>
#include <cstdint>

namespace someip::sd {

/**
 * @brief Local multicast membership state
 *
 * Distinct from remote subscription acceptance: a peer may have acknowledged a
 * SubscribeEventgroup while the local membership needed to receive those events
 * has failed. The two facts must not be conflated.
 */
enum class MulticastState : uint8_t {
    JOINED,    ///< Membership is active (or not required)
    RETRYING,  ///< Join failed; bounded re-attempts are in progress
    EXHAUSTED  ///< Re-attempts exhausted; multicast reception is unavailable
};

/**
 * @brief Bounded re-attempt policy for a failed multicast join
 *
 * @implements REQ_TRANSPORT_011_E03
 */
struct MulticastRejoinConfig {
    /// Re-attempts after the initial failure (0 disables re-attempt).
    uint8_t max_attempts{5};
    /// Minimum wall-clock spacing between re-attempts.
    std::chrono::milliseconds interval{1000};
};

/**
 * @brief Single-group membership retry state machine
 *
 * Used by SdServer for the SD multicast group and by SdClient for each
 * owned eventgroup multicast group. Leave/unsubscribe must call cancel() so a
 * pending re-attempt cannot outlive ownership of the group.
 *
 * @implements REQ_TRANSPORT_011_E01, REQ_TRANSPORT_011_E03
 */
class MulticastMembership {
public:
    using clock = std::chrono::steady_clock;

    [[nodiscard]] MulticastState state() const noexcept { return state_; }

    [[nodiscard]] bool is_retrying() const noexcept {
        return state_ == MulticastState::RETRYING;
    }

    /**
     * @brief Record a successful join (clears any prior failure state)
     */
    void note_join_success() noexcept {
        state_ = MulticastState::JOINED;
        attempts_ = 0;
        next_attempt_ = clock::time_point{};
    }

    /**
     * @brief Record an initial join failure and arm bounded re-attempts
     */
    void note_join_failure(const MulticastRejoinConfig& cfg, clock::time_point now) noexcept {
        attempts_ = 0;
        if (cfg.max_attempts == 0) {
            state_ = MulticastState::EXHAUSTED;
            next_attempt_ = clock::time_point{};
            return;
        }
        state_ = MulticastState::RETRYING;
        next_attempt_ = now + cfg.interval;
    }

    /**
     * @brief Cancel pending re-attempts (leave / ownership released)
     */
    void cancel() noexcept {
        state_ = MulticastState::JOINED;
        attempts_ = 0;
        next_attempt_ = clock::time_point{};
    }

    /**
     * @brief True when a re-attempt should be issued at @p now
     */
    [[nodiscard]] bool due(clock::time_point now) const noexcept {
        return state_ == MulticastState::RETRYING && now >= next_attempt_;
    }

    /**
     * @brief Apply the result of a scheduled re-attempt
     */
    void note_rejoin_result(bool success, const MulticastRejoinConfig& cfg,
                            clock::time_point now) noexcept {
        if (success) {
            note_join_success();
            return;
        }
        if (state_ != MulticastState::RETRYING) {
            return;
        }
        ++attempts_;
        if (attempts_ >= cfg.max_attempts) {
            state_ = MulticastState::EXHAUSTED;
            next_attempt_ = clock::time_point{};
            return;
        }
        next_attempt_ = now + cfg.interval;
    }

private:
    MulticastState state_{MulticastState::JOINED};
    uint8_t attempts_{0};
    clock::time_point next_attempt_{};
};

/**
 * @brief Aggregate membership state across a set of MulticastMembership values
 *
 * Priority: any RETRYING, else any EXHAUSTED, else JOINED.
 */
template <typename Iterator>
[[nodiscard]] MulticastState aggregate_multicast_state(Iterator begin, Iterator end) noexcept {
    bool any_exhausted = false;
    for (auto it = begin; it != end; ++it) {
        const MulticastState s = it->state();
        if (s == MulticastState::RETRYING) {
            return MulticastState::RETRYING;
        }
        if (s == MulticastState::EXHAUSTED) {
            any_exhausted = true;
        }
    }
    return any_exhausted ? MulticastState::EXHAUSTED : MulticastState::JOINED;
}

}  // namespace someip::sd

#endif  // SOMEIP_SD_MULTICAST_MEMBERSHIP_H
