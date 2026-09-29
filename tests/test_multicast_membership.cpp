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
 * @file test_multicast_membership.cpp
 * @brief Unit tests for bounded multicast join retry state machine
 *
 * @tests REQ_TRANSPORT_011_E01, REQ_TRANSPORT_011_E03
 */

#include "sd/multicast_membership.h"

#include <gtest/gtest.h>

#include <chrono>

using someip::sd::MulticastMembership;
using someip::sd::MulticastRejoinConfig;
using someip::sd::MulticastState;

namespace {

MulticastMembership::clock::time_point t0() {
    return MulticastMembership::clock::time_point{};
}

}  // namespace

/**
 * @test_case TC_MC_MEMBERSHIP_001
 * @tests REQ_TRANSPORT_011_E01
 */
TEST(MulticastMembershipTest, SuccessfulJoinReportsJoined) {
    MulticastMembership m;
    m.note_join_success();
    EXPECT_EQ(m.state(), MulticastState::JOINED);
    EXPECT_FALSE(m.is_retrying());
}

/**
 * @test_case TC_MC_MEMBERSHIP_002
 * @tests REQ_TRANSPORT_011_E01, REQ_TRANSPORT_011_E03
 */
TEST(MulticastMembershipTest, FailureArmsRetrying) {
    MulticastMembership m;
    MulticastRejoinConfig cfg;
    cfg.max_attempts = 3;
    cfg.interval = std::chrono::milliseconds(100);
    m.note_join_failure(cfg, t0());
    EXPECT_EQ(m.state(), MulticastState::RETRYING);
    EXPECT_FALSE(m.due(t0()));
    EXPECT_TRUE(m.due(t0() + std::chrono::milliseconds(100)));
}

/**
 * @test_case TC_MC_MEMBERSHIP_003
 * @tests REQ_TRANSPORT_011_E03
 */
TEST(MulticastMembershipTest, ZeroBoundDisablesRetryAndReportsExhausted) {
    MulticastMembership m;
    MulticastRejoinConfig cfg;
    cfg.max_attempts = 0;
    m.note_join_failure(cfg, t0());
    EXPECT_EQ(m.state(), MulticastState::EXHAUSTED);
    EXPECT_FALSE(m.due(t0() + std::chrono::hours(1)));
}

/**
 * @test_case TC_MC_MEMBERSHIP_004
 * @tests REQ_TRANSPORT_011_E03
 */
TEST(MulticastMembershipTest, FailOnceThenSucceedReachesJoined) {
    MulticastMembership m;
    MulticastRejoinConfig cfg;
    cfg.max_attempts = 3;
    cfg.interval = std::chrono::milliseconds(10);

    m.note_join_failure(cfg, t0());
    EXPECT_EQ(m.state(), MulticastState::RETRYING);

    const auto due_at = t0() + cfg.interval;
    ASSERT_TRUE(m.due(due_at));
    m.note_rejoin_result(true, cfg, due_at);
    EXPECT_EQ(m.state(), MulticastState::JOINED);
    EXPECT_FALSE(m.due(due_at + cfg.interval));
}

/**
 * @test_case TC_MC_MEMBERSHIP_005
 * @tests REQ_TRANSPORT_011_E03
 */
TEST(MulticastMembershipTest, PersistentFailureReportsExhaustion) {
    MulticastMembership m;
    MulticastRejoinConfig cfg;
    cfg.max_attempts = 2;
    cfg.interval = std::chrono::milliseconds(1);

    auto now = t0();
    m.note_join_failure(cfg, now);
    now += cfg.interval;
    m.note_rejoin_result(false, cfg, now);
    EXPECT_EQ(m.state(), MulticastState::RETRYING);

    now += cfg.interval;
    m.note_rejoin_result(false, cfg, now);
    EXPECT_EQ(m.state(), MulticastState::EXHAUSTED);
    EXPECT_FALSE(m.due(now + cfg.interval));
}

/**
 * @test_case TC_MC_MEMBERSHIP_006
 * @tests REQ_TRANSPORT_011_E03
 */
TEST(MulticastMembershipTest, LeaveCancelsPendingReattempt) {
    MulticastMembership m;
    MulticastRejoinConfig cfg;
    cfg.max_attempts = 5;
    cfg.interval = std::chrono::milliseconds(50);

    m.note_join_failure(cfg, t0());
    ASSERT_EQ(m.state(), MulticastState::RETRYING);
    m.cancel();
    EXPECT_EQ(m.state(), MulticastState::JOINED);
    EXPECT_FALSE(m.due(t0() + std::chrono::seconds(10)));
}
